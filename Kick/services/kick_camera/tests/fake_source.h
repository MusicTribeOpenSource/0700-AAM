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
#pragma once

#include "kick_camera/frame_source.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace kicktest
{
    using namespace kickcam;

    // What a fake source supports and what happened to it. Shared with the test so it outlives the source.
    struct FakeState
    {
        std::mutex mu;
        std::vector<std::string> calls; // control calls in order, e.g. "auto_exposure=0"
        int opens = 0;
        int closes = 0;
        bool isOpen = false;

        bool failOpen = false;
        bool supportsAutoExposure = true;
        bool supportsAbsolute = true;
        bool supportsKeepFps = true;
        bool supportsWhiteBalance = true;
        int exposure = 321;
        FrameFormat format = {32, 16, 64, kFourccYuyv};
        uint32_t fps = 100;
        int framesBeforeEnd = -1; // -1: never ends
        bool failAfterFrames = false;
        std::function<void()> onOpen; // runs inside open(), before it reports success
        std::function<void()> onClose; // runs inside close(), before the device is marked closed

        std::vector<std::string> callLog()
        {
            std::lock_guard<std::mutex> lock(mu);
            return calls;
        }
        int openCount() { std::lock_guard<std::mutex> lock(mu); return opens; }
        int closeCount() { std::lock_guard<std::mutex> lock(mu); return closes; }
        bool open_() { std::lock_guard<std::mutex> lock(mu); return isOpen; }
    };

    class FakeSource : public FrameSource
    {
    public:
        explicit FakeSource(std::shared_ptr<FakeState> s) : s_(std::move(s)) {}
        ~FakeSource() override { close(); }

        bool open(std::string& error) override
        {
            if (s_->onOpen) s_->onOpen();
            std::lock_guard<std::mutex> lock(s_->mu);
            ++s_->opens;
            if (s_->failOpen)
            {
                error = "fake: open refused";
                return false;
            }
            s_->isOpen = true;
            return true;
        }
        void close() override
        {
            if (s_->isOpen && s_->onClose) s_->onClose();
            std::lock_guard<std::mutex> lock(s_->mu);
            if (!s_->isOpen) return;
            s_->isOpen = false;
            ++s_->closes;
        }
        FrameFormat format() const override { return s_->format; }
        uint32_t fps() const override { return s_->fps; }
        const char* name() const override { return "fake"; }

        ReadResult next(FrameView& out, int, std::string& error) override
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / s_->fps));
            if (s_->framesBeforeEnd >= 0 && count_ >= s_->framesBeforeEnd)
            {
                if (s_->failAfterFrames)
                {
                    error = "fake: device unplugged";
                    return ReadResult::Error;
                }
                return ReadResult::EndOfStream;
            }
            buffer_.assign(frameBytes(s_->format), static_cast<uint8_t>(0x40 + count_ % 16));
            ++count_;
            out.data = buffer_.data();
            out.size = buffer_.size();
            out.timestampNs = monotonicNs();
            return ReadResult::Frame;
        }

        bool setAutoExposure(bool on) override { return record(s_->supportsAutoExposure, std::string("auto_exposure=") + (on ? "1" : "0")); }
        bool setExposureAbsolute(int v) override
        {
            const bool ok = record(s_->supportsAbsolute, "exposure=" + std::to_string(v));
            if (ok) s_->exposure = v;
            return ok;
        }
        bool getExposureAbsolute(int& v) override
        {
            if (!s_->supportsAbsolute) return false;
            v = s_->exposure;
            return true;
        }
        bool setKeepFrameRate(bool on) override { return record(s_->supportsKeepFps, std::string("keep_fps=") + (on ? "1" : "0")); }
        bool setAutoWhiteBalance(bool on) override { return record(s_->supportsWhiteBalance, std::string("awb=") + (on ? "1" : "0")); }

    private:
        bool record(bool supported, const std::string& call)
        {
            std::lock_guard<std::mutex> lock(s_->mu);
            if (!supported) return false;
            s_->calls.push_back(call);
            return true;
        }
        std::shared_ptr<FakeState> s_;
        std::vector<uint8_t> buffer_;
        int count_ = 0;
    };
}
