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
#include "kick_gesture/frame_reader.h"

namespace kickgest
{
    using Result = IFrameReader::Result;

    namespace
    {
        uint64_t nowMs() { return kickcam::monotonicNs() / 1000000ull; }

        Result map(kickcam::RingReader::Result r)
        {
            switch (r)
            {
                case kickcam::RingReader::Result::Ok: return Result::Frame;
                case kickcam::RingReader::Result::NotCapturing: return Result::NotCapturing;
                default: return Result::NoNewFrame; // NoNewFrame, or Torn: the next poll tries again
            }
        }
    }

    RingFrameReader::RingFrameReader(const void* base, size_t bytes) { attached_ = reader_.attach(base, bytes); }

    Result RingFrameReader::poll(std::vector<uint8_t>& data, kickcam::FrameMeta& meta)
    {
        if (!attached_) return Result::Unavailable;
        const Result r = map(reader_.readLatest(lastSeen_, data, meta));
        if (r == Result::Frame) lastSeen_ = meta.frameNo;
        return r;
    }

    bool ShmFrameReader::open()
    {
        const uint64_t now = nowMs();
        if (attempted_ && now - lastOpenAttemptMs_ < retryMs_) return false;
        attempted_ = true;
        lastOpenAttemptMs_ = now;
        std::string err;
        if (!region_.openReadOnly(name_, err)) return false;
        if (!reader_.attach(region_.data(), region_.size()))
        {
            region_.close();
            return false;
        }
        open_ = true;
        lastSeen_ = 0;
        lastFrameMs_ = now;
        return true;
    }

    void ShmFrameReader::close()
    {
        region_.close();
        open_ = false;
    }

    Result ShmFrameReader::poll(std::vector<uint8_t>& data, kickcam::FrameMeta& meta)
    {
        if (!open_ && !open()) return Result::Unavailable;
        const Result r = map(reader_.readLatest(lastSeen_, data, meta));
        const uint64_t now = nowMs();
        if (r == Result::Frame)
        {
            lastSeen_ = meta.frameNo;
            lastFrameMs_ = now;
        }
        else if (r == Result::NotCapturing)
        {
            lastFrameMs_ = now; // off is a state, not a stall
            // The mapping may belong to a service that has since gone away and been replaced. While the camera is off, look the
            // object up by name again once per retry period; nothing is lost by it and the next enable finds the new ring.
            if (now - lastOpenAttemptMs_ >= retryMs_)
            {
                close();
                if (!open()) return Result::Unavailable;
                return Result::NotCapturing; // the next poll reads whatever the new ring holds
            }
        }
        else if (now - lastFrameMs_ >= staleMs_)
        {
            // Capturing but silent: reopen by name, which finds a restarted service's new object.
            close();
            return Result::Unavailable;
        }
        return r;
    }
}
