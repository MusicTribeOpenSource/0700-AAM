/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "kick_gesture/service.h"

#include "kick_gesture/event_json.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace kickgest
{
    GestureService::GestureService(ILandmarkModel* model, const GestureConfig& gesture, const ServiceOptions& options, const GuardLimits& limits, IResourceReader* resources,
                                   IFrameReader* reader, std::vector<IEventSink*> sinks, LandmarkWriter* recorder)
        : model_(model), engine_(gesture), opt_(options), guard_(limits), resources_(resources), reader_(reader), sinks_(std::move(sinks)), recorder_(recorder)
    {
        if (!(opt_.rateHz >= 1.0)) opt_.rateHz = 1.0;
        if (opt_.rateHz > 30.0) opt_.rateHz = 30.0;
    }

    void GestureService::send(const std::string& line)
    {
        for (IEventSink* s : sinks_) s->send(line);
    }

    void GestureService::sendEvents(std::vector<GestureEvent>& events)
    {
        for (const GestureEvent& e : events)
        {
            send(eventToJson(e));
            ++stats_.eventsSent;
        }
        events.clear();
    }

    void GestureService::sendStatus(uint64_t nowMs, const std::string& state, const std::string& reason)
    {
        StatusMessage m;
        m.state = state;
        m.reason = reason;
        m.rateHz = guardStatus_.rateDivisor == 0 ? 0.0 : opt_.rateHz / static_cast<double>(guardStatus_.rateDivisor);
        m.cpu = guardStatus_.cpu;
        m.tempC = guardStatus_.tempC;
        m.tMs = nowMs;
        send(statusToJson(m));
        lastState_ = state;
        lastReason_ = reason;
    }

    void GestureService::closeSession(uint64_t nowMs)
    {
        if (!haveSession_) return;
        engine_.endSession(nowMs, events_);
        sendEvents(events_);
        haveSession_ = false;
    }

    bool GestureService::handleFrame(const kickcam::FrameMeta& meta)
    {
        // A frame that cannot be read counts as a frame without a hand, so an open pinch still gets its pinch_end
        // after lost_ms instead of staying open for as long as the failures last.
        auto failFrame = [&]() {
            LandmarkFrame none;
            none.tMs = meta.timestampNs / 1000000ull;
            none.aspect = 1.f;
            haveSession_ = true;
            engine_.process(none, events_);
            sendEvents(events_);
            return false;
        };
        if (!convertToRgb(frameBytes_.data(), frameBytes_.size(), meta.format, rgb_))
        {
            ++stats_.convertErrors;
            errorReason_ = "unsupported_frame";
            return failFrame();
        }
        std::string err;
        if (!model_->detect(rgb_, hands_, err))
        {
            ++stats_.modelErrors;
            errorReason_ = "model_failed";
            return failFrame();
        }
        LandmarkFrame lf;
        lf.tMs = meta.timestampNs / 1000000ull;
        lf.aspect = static_cast<float>(meta.format.width) / static_cast<float>(meta.format.height);
        lf.hands = hands_;
        lf.quantize();
        if (recorder_ != nullptr) recorder_->write(lf);
        haveSession_ = true;
        engine_.process(lf, events_);
        sendEvents(events_);
        ++stats_.framesProcessed;
        return true;
    }

    uint64_t GestureService::step(uint64_t nowNs)
    {
        const uint64_t nowMs = nowNs / 1000000ull;
        constexpr uint64_t kMs = 1000000ull;

        if (nowNs >= nextGuardNs_)
        {
            nextGuardNs_ = nowNs + static_cast<uint64_t>(opt_.guardIntervalMs) * kMs;
            const GuardState before = guardStatus_.state;
            guardStatus_ = guard_.update(nowMs, resources_->cpuSeconds(), resources_->tempC());
            // An open pinch must not outlive the decision to stop looking.
            if (guardStatus_.state == GuardState::Disabled && before != GuardState::Disabled) closeSession(nowMs);
        }

        if (guardStatus_.state == GuardState::Disabled)
        {
            // No inference and no camera reads. The guard decides when to try again.
            nextFrameNs_ = nowNs + static_cast<uint64_t>(opt_.idleCheckMs) * kMs;
        }
        else if (nowNs >= nextFrameNs_)
        {
            kickcam::FrameMeta meta;
            const IFrameReader::Result r = reader_->poll(frameBytes_, meta);
            const double rate = opt_.rateHz / static_cast<double>(guardStatus_.rateDivisor == 0 ? 1 : guardStatus_.rateDivisor);
            const uint64_t period = static_cast<uint64_t>(1e9 / rate);
            switch (r)
            {
                case IFrameReader::Result::Frame:
                    nextFrameNs_ = nowNs + period;
                    activity_ = handleFrame(meta) ? Activity::Running : Activity::Error;
                    break;
                case IFrameReader::Result::NoNewFrame:
                    // Look again soon, not a whole period later, so a frame is not left waiting.
                    nextFrameNs_ = nowNs + std::min<uint64_t>(period / 4, 20 * kMs);
                    break;
                case IFrameReader::Result::NotCapturing:
                    nextFrameNs_ = nowNs + static_cast<uint64_t>(opt_.idleCheckMs) * kMs;
                    closeSession(nowMs);
                    activity_ = Activity::Idle;
                    break;
                case IFrameReader::Result::Unavailable:
                    nextFrameNs_ = nowNs + static_cast<uint64_t>(opt_.idleCheckMs) * kMs;
                    closeSession(nowMs);
                    activity_ = Activity::NoCamera;
                    break;
            }
        }

        // What to tell the world: said at every change and every statusIntervalMs.
        std::string state;
        std::string reason;
        if (guardStatus_.state == GuardState::Disabled)
        {
            state = "disabled";
            reason = guardStatus_.reason;
        }
        else
            switch (activity_)
            {
                case Activity::Idle: state = "idle"; reason = "camera_off"; break;
                case Activity::NoCamera: state = "no_camera"; reason = "camera_service_missing"; break;
                case Activity::Error: state = "error"; reason = errorReason_; break;
                default: state = guardStateName(guardStatus_.state); reason = guardStatus_.reason; break;
            }
        if (activity_ != Activity::Unknown || guardStatus_.state == GuardState::Disabled)
        {
            if (state != lastState_ || reason != lastReason_ || nowNs >= nextStatusNs_)
            {
                sendStatus(nowMs, state, reason);
                nextStatusNs_ = nowNs + static_cast<uint64_t>(opt_.statusIntervalMs) * kMs;
            }
        }

        const uint64_t due = std::min(nextFrameNs_, std::min(nextGuardNs_, nextStatusNs_ == 0 ? nextGuardNs_ : nextStatusNs_));
        return due > nowNs ? due - nowNs : 0;
    }

    void GestureService::shutdown(uint64_t nowNs)
    {
        const uint64_t nowMs = nowNs / 1000000ull;
        closeSession(nowMs);
        sendStatus(nowMs, "stopped", "");
    }

    void GestureService::run(const std::atomic<bool>& stop)
    {
        while (!stop.load())
        {
            const uint64_t now = kickcam::monotonicNs();
            const uint64_t sleepNs = std::min<uint64_t>(step(now), 50ull * 1000000ull);
            if (sleepNs > 0) std::this_thread::sleep_for(std::chrono::nanoseconds(sleepNs));
        }
        shutdown(kickcam::monotonicNs());
    }
}
