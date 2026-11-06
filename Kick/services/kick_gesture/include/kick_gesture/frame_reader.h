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

#include "kick_camera/shm_region.h"
#include "kick_camera/shm_ring.h"

#include <cstdint>
#include <string>
#include <vector>

namespace kickgest
{
    // Latest frame from the camera service, or the reason there is none.
    class IFrameReader
    {
    public:
        enum class Result
        {
            Frame,        // `data` and `meta` hold a frame newer than the last one returned
            NoNewFrame,   // the camera is on and has nothing newer
            NotCapturing, // the camera service is there and the camera is off
            Unavailable   // there is no camera service (shared memory missing or not a ring this build understands)
        };
        virtual ~IFrameReader() = default;
        virtual Result poll(std::vector<uint8_t>& data, kickcam::FrameMeta& meta) = 0;
    };

    // Reads a ring that already lives in this process (tests, and anything that embeds the camera service).
    class RingFrameReader final : public IFrameReader
    {
    public:
        RingFrameReader(const void* base, size_t bytes);
        Result poll(std::vector<uint8_t>& data, kickcam::FrameMeta& meta) override;

    private:
        kickcam::RingReader reader_;
        bool attached_ = false;
        uint64_t lastSeen_ = 0;
    };

    // Reads the camera service's POSIX shared memory, read-only. The service owns the object and may come and go,
    // so this reopens it when it is missing and when a camera that claims to be capturing has delivered nothing
    // for staleMs (the service died, or restarted and replaced the object).
    class ShmFrameReader final : public IFrameReader
    {
    public:
        explicit ShmFrameReader(std::string name, uint32_t retryMs = 1000, uint32_t staleMs = 3000) : name_(std::move(name)), retryMs_(retryMs), staleMs_(staleMs) {}
        Result poll(std::vector<uint8_t>& data, kickcam::FrameMeta& meta) override;

    private:
        bool open();
        void close();

        std::string name_;
        uint32_t retryMs_;
        uint32_t staleMs_;
        kickcam::ShmRegion region_;
        kickcam::RingReader reader_;
        bool open_ = false;
        uint64_t lastSeen_ = 0;
        uint64_t lastOpenAttemptMs_ = 0;
        bool attempted_ = false;
        uint64_t lastFrameMs_ = 0;
    };
}
