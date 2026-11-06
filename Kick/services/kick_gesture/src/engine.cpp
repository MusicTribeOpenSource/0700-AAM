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
#include "kick_gesture/engine.h"

#include <algorithm>
#include <cmath>

namespace kickgest
{
    namespace
    {
        constexpr uint64_t kHistoryMs = 3000;
        constexpr uint64_t kSpeedWindowMs = 250;
        constexpr uint64_t kMinSwipeStrokeMs = 100;

        float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
    }

    void GestureEngine::PoseTimer::update(bool now, uint64_t t, uint32_t exitMs)
    {
        if (now)
        {
            if (!running)
            {
                running = true;
                since = t;
            }
            lastTrue = t;
        }
        else if (running && t - lastTrue >= exitMs)
        {
            running = false;
            fired = false;
        }
    }

    GestureEngine::GestureEngine(const GestureConfig& cfg) : cfg_(cfg) {}

    void GestureEngine::clearTrack()
    {
        tracking_ = false;
        hist_.clear();
        swipeBuf_.clear();
        voteCount_ = 0;
        voteHead_ = 0;
        palm_.reset();
        fist_.reset();
        swipeArmed_ = true;
        slowing_ = false;
        pinchArming_ = false;
        pinchActive_ = false;
        pinchReleasing_ = false;
    }

    void GestureEngine::endSession(uint64_t tMs, std::vector<GestureEvent>& out)
    {
        if (pinchActive_)
        {
            // Not rate limited: a pinch_start must always get its pinch_end.
            // Scored over the last 300 ms in which a hand was seen, which can be well before tMs.
            const float c = meanScoreSince(lastHandMs_ > 300 ? lastHandMs_ - 300 : 0);
            GestureEvent e;
            e.type = GestureType::PinchEnd;
            e.hasValue = true;
            e.value = pinchLastValue_;
            e.hand = handLabel();
            e.confidence = c;
            e.tMs = tMs;
            out.push_back(e);
            lastEmitMs_[static_cast<size_t>(GestureType::PinchEnd)] = tMs;
            hasEmitted_[static_cast<size_t>(GestureType::PinchEnd)] = true;
        }
        clearTrack();
    }

    void GestureEngine::noHand(uint64_t t, std::vector<GestureEvent>& out)
    {
        ++stats_.framesWithoutHand;
        if (tracking_ && t - lastHandMs_ >= cfg_.lostMs) endSession(t, out);
    }

    void GestureEngine::process(const LandmarkFrame& frame, std::vector<GestureEvent>& out)
    {
        const uint64_t t = frame.tMs;
        if (haveFrame_ && t < lastFrameMs_)
        {
            // The clock restarted (service restart, replay of a second file). Close what is open at the old time.
            ++stats_.outOfOrderFrames;
            endSession(lastFrameMs_, out);
        }
        else if (haveFrame_ && t == lastFrameMs_)
        {
            ++stats_.outOfOrderFrames;
            return;
        }
        haveFrame_ = true;
        lastFrameMs_ = t;

        const Hand* best = nullptr;
        HandFeatures f;
        for (const Hand& h : frame.hands)
        {
            if (!(h.score >= cfg_.minHandScore)) continue;
            if (best != nullptr && !(h.score > best->score)) continue;
            HandFeatures cand;
            if (!computeFeatures(h, frame.aspect, cfg_, cand))
            {
                ++stats_.nonFiniteFrames;
                continue;
            }
            best = &h;
            f = cand;
        }
        if (best == nullptr)
        {
            noHand(t, out);
            return;
        }

        ++stats_.framesWithHand;
        if (tracking_ && t - lastHandMs_ >= cfg_.lostMs) endSession(t, out);
        tracking_ = true;
        lastHandMs_ = t;

        votes_[static_cast<size_t>(voteHead_)] = best->rightProb;
        voteHead_ = (voteHead_ + 1) % kVotes;
        if (voteCount_ < kVotes) ++voteCount_;

        const Sample s{t, f.cx, f.cy, f.palm, best->score, f.extCount};
        hist_.push_back(s);
        while (!hist_.empty() && t - hist_.front().tMs > kHistoryMs) hist_.pop_front();

        if (updatePinch(f, t, out))
        {
            // A pinch is starting or active: nothing else may fire, and no half-built pose or stroke survives it.
            palm_.reset();
            fist_.reset();
            swipeBuf_.clear();
            return;
        }
        updatePoses(f, t, out);
        updateSwipe(f, t, out);
    }

    float GestureEngine::speedPalmPerSec(uint64_t t) const
    {
        if (hist_.size() < 2) return 0.f;
        const Sample& last = hist_.back();
        const Sample* first = &last;
        for (auto it = hist_.rbegin(); it != hist_.rend(); ++it)
        {
            if (t - it->tMs > kSpeedWindowMs) break;
            first = &*it;
        }
        const uint64_t dt = last.tMs - first->tMs;
        if (dt < 50) return 0.f;
        const float d = std::hypot(last.cx - first->cx, last.cy - first->cy);
        return d / last.palm / (static_cast<float>(dt) / 1000.f);
    }

    float GestureEngine::meanScoreSince(uint64_t t0) const
    {
        float sum = 0.f;
        int n = 0;
        for (auto it = hist_.rbegin(); it != hist_.rend(); ++it)
        {
            if (it->tMs < t0) break;
            sum += it->score;
            ++n;
        }
        return n > 0 ? clamp01(sum / static_cast<float>(n)) : 0.f;
    }

    Handedness GestureEngine::handLabel() const
    {
        if (voteCount_ == 0) return Handedness::Unknown;
        float sum = 0.f;
        for (int i = 0; i < voteCount_; ++i) sum += votes_[static_cast<size_t>(i)];
        const bool right = (sum / static_cast<float>(voteCount_) > 0.5f) != cfg_.swapHandedness;
        return right ? Handedness::Right : Handedness::Left;
    }

    bool GestureEngine::canEmit(GestureType type, uint64_t t) const
    {
        const size_t i = static_cast<size_t>(slot(type));
        if (!hasEmitted_[i]) return true;
        const uint32_t gap = cfg_.minIntervalMs(type);
        return t - lastEmitMs_[i] >= gap;
    }

    bool GestureEngine::confident(float c)
    {
        if (c >= cfg_.minEventConfidence) return true;
        ++stats_.confidenceSuppressed;
        return false;
    }

    void GestureEngine::emit(GestureType type, bool hasValue, float value, float confidence, uint64_t t, std::vector<GestureEvent>& out)
    {
        GestureEvent e;
        e.type = type;
        e.hasValue = hasValue;
        e.value = value;
        e.hand = handLabel();
        e.confidence = confidence;
        e.tMs = t;
        out.push_back(e);
        const size_t i = static_cast<size_t>(slot(type));
        lastEmitMs_[i] = t;
        hasEmitted_[i] = true;
    }

    bool GestureEngine::updatePinch(const HandFeatures& f, uint64_t t, std::vector<GestureEvent>& out)
    {
        const float span = cfg_.pinchOff - cfg_.pinchMin;
        const float value = clamp01((f.pinchRatio - cfg_.pinchMin) / span);

        if (!pinchActive_)
        {
            const bool pose = f.pinchRatio <= cfg_.pinchOn && f.indexReach >= cfg_.pinchMinReach;
            if (!pose)
            {
                pinchArming_ = false;
                return false;
            }
            if (!pinchArming_)
            {
                pinchArming_ = true;
                pinchArmSince_ = t;
            }
            if (t - pinchArmSince_ >= cfg_.pinchStartMs)
            {
                if (!canEmit(GestureType::PinchStart, t))
                    ++stats_.rateSuppressed;
                else if (confident(meanScoreSince(pinchArmSince_)))
                {
                    pinchActive_ = true;
                    pinchArming_ = false;
                    pinchReleasing_ = false;
                    pinchLastValue_ = value;
                    emit(GestureType::PinchStart, true, value, meanScoreSince(pinchArmSince_), t, out);
                }
            }
            return true;
        }

        if (f.pinchRatio >= cfg_.pinchOff)
        {
            if (!pinchReleasing_)
            {
                pinchReleasing_ = true;
                pinchReleaseSince_ = t;
            }
            if (t - pinchReleaseSince_ >= cfg_.pinchEndMs)
            {
                const float c = meanScoreSince(t > 300 ? t - 300 : 0);
                emit(GestureType::PinchEnd, true, pinchLastValue_, c, t, out);
                pinchActive_ = false;
                pinchReleasing_ = false;
                pinchArming_ = false;
                return false;
            }
            return true;
        }

        pinchReleasing_ = false;
        if (std::fabs(value - pinchLastValue_) >= cfg_.pinchDeadband)
        {
            if (!canEmit(GestureType::PinchChange, t))
                ++stats_.rateSuppressed;
            else
            {
                pinchLastValue_ = value;
                    emit(GestureType::PinchChange, true, value, meanScoreSince(t > 300 ? t - 300 : 0), t, out);
            }
        }
        return true;
    }

    void GestureEngine::updatePoses(const HandFeatures& f, uint64_t t, std::vector<GestureEvent>& out)
    {
        const float speed = speedPalmPerSec(t);

        const bool open = f.extCount == 4 && f.thumbExt && speed <= cfg_.palmHoldMaxSpeed;
        palm_.update(open, t, cfg_.palmHoldExitMs);
        if (palm_.due(t, cfg_.palmHoldMs))
        {
            if (!canEmit(GestureType::PalmHold, t))
                ++stats_.rateSuppressed;
            else
            {
                const float c = meanScoreSince(palm_.since);
                if (confident(c))
                {
                    palm_.fired = true;
                    emit(GestureType::PalmHold, false, 0.f, c, t, out);
                }
            }
        }

        const bool fist = f.extCount == 0 && speed <= cfg_.fistMaxSpeed;
        fist_.update(fist, t, cfg_.fistExitMs);
        if (fist_.due(t, cfg_.fistMs))
        {
            if (!canEmit(GestureType::Fist, t))
                ++stats_.rateSuppressed;
            else
            {
                const float c = meanScoreSince(fist_.since);
                if (confident(c))
                {
                    fist_.fired = true;
                    emit(GestureType::Fist, false, 0.f, c, t, out);
                }
            }
        }
    }

    void GestureEngine::updateSwipe(const HandFeatures& f, uint64_t t, std::vector<GestureEvent>& out)
    {
        // Re-arm: after a swipe the hand has to slow down for a moment, which is what separates the return
        // stroke of a swipe from a swipe the other way.
        const float speed = speedPalmPerSec(t);
        if (speed <= cfg_.swipeRestSpeed)
        {
            if (!slowing_)
            {
                slowing_ = true;
                slowSince_ = t;
            }
        }
        else
            slowing_ = false;
        if (!swipeArmed_ && slowing_ && t - slowSince_ >= cfg_.swipeRearmMs) swipeArmed_ = true;

        if (f.extCount < static_cast<int>(cfg_.swipeMinExtended))
        {
            swipeBuf_.clear();
            return;
        }
        swipeBuf_.push_back(hist_.back());
        while (!swipeBuf_.empty() && t - swipeBuf_.front().tMs > cfg_.swipeWindowMs) swipeBuf_.pop_front();
        if (!swipeArmed_ || swipeBuf_.size() < 3) return;

        const Sample& cur = swipeBuf_.back();
        size_t iMin = 0;
        size_t iMax = 0;
        for (size_t i = 0; i + 1 < swipeBuf_.size(); ++i)
        {
            if (swipeBuf_[i].cx < swipeBuf_[iMin].cx) iMin = i;
            if (swipeBuf_[i].cx > swipeBuf_[iMax].cx) iMax = i;
        }
        const float dxRight = cur.cx - swipeBuf_[iMin].cx;
        const float dxLeft = swipeBuf_[iMax].cx - cur.cx;
        const bool right = dxRight >= dxLeft;
        const size_t start = right ? iMin : iMax;
        const float dx = right ? dxRight : -dxLeft;
        const Sample& s0 = swipeBuf_[start];

        const uint64_t dur = cur.tMs - s0.tMs;
        if (dur < kMinSwipeStrokeMs) return;
        const float travel = std::fabs(dx) / cur.palm;
        if (travel < cfg_.swipeMinTravel) return;
        if (travel / (static_cast<float>(dur) / 1000.f) < cfg_.swipeMinSpeed) return;
        const float dy = std::fabs(cur.cy - s0.cy);
        if (std::fabs(dx) < cfg_.swipeMinDominance * dy) return;

        int same = 0;
        int opposite = 0;
        for (size_t i = start + 1; i < swipeBuf_.size(); ++i)
        {
            const float step = swipeBuf_[i].cx - swipeBuf_[i - 1].cx;
            if (std::fabs(step) < 1e-4f) continue;
            if ((step > 0.f) == right)
                ++same;
            else
                ++opposite;
        }
        if (same + opposite == 0) return;
        if (static_cast<float>(same) / static_cast<float>(same + opposite) < cfg_.swipeMinMonotonic) return;

        const GestureType type = right ? GestureType::SwipeRight : GestureType::SwipeLeft;
        if (!canEmit(type, t))
        {
            ++stats_.rateSuppressed;
            return;
        }
        const float c = meanScoreSince(s0.tMs);
        if (!confident(c)) return;
        emit(type, false, 0.f, c, t, out);
        swipeArmed_ = false;
        slowing_ = false;
        swipeBuf_.clear();
    }
}
