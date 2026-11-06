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

#include "kick_camera/config.h"
#include "kick_camera/control_server.h"
#include "kick_camera/controls.h"
#include "kick_camera/frame_source.h"
#include "kick_camera/shm_region.h"
#include "kick_camera/shm_ring.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace kickcam
{
    using SourceFactory = std::function<std::unique_ptr<FrameSource>(const Config&)>;

    // The backend named by cfg.source, or nullptr (with error set) when this build does not have it.
    std::unique_ptr<FrameSource> makeSource(const Config& cfg, std::string& error);

    // Owns the shared-memory ring, the control socket and the capture thread.
    //
    // Privacy by construction:
    //  - After start() the camera is closed. Only "enable" opens the device.
    //  - ring "capturing" is raised before the device is opened and lowered after it is closed, so the on-screen
    //    indicator can never read "off" while the hardware is on.
    //  - "disable" (and any source failure) closes the device and zeroes every pixel in the ring.
    //  - Frames reach disk only when cfg.debugDumpDir is set, and then only debugDumpMax of them.
    //
    // Control commands (one per line on the control socket):
    //   enable | disable | status | lock_exposure | unlock_exposure | lock_wb | unlock_wb | preset lit_room|dim_stage
    // Replies are "ok", "err <reason>", or for status one line of JSON.
    class CameraService
    {
    public:
        explicit CameraService(Config cfg, SourceFactory factory = nullptr);
        ~CameraService();

        // Creates the ring and the control socket. The camera stays closed.
        bool start(std::string& error);
        // Disables the camera, removes the socket and the ring.
        void stop();

        bool enable(std::string& error);
        void disable();

        std::string handleCommand(const std::string& line);
        std::string statusJson();

        // The ring, for in-process readers (tests).
        const void* ringData() const { return region_.data(); }
        size_t ringSize() const { return region_.size(); }

    private:
        void captureLoop(FrameSource* source);
        void finishFromCaptureThread(const char* state, const std::string& message);
        void dumpFrame(const FrameView& v);
        void setStateLocked(const char* state, const std::string& message);

        Config cfg_;
        SourceFactory factory_;
        ShmRegion region_;
        RingWriter ring_;
        ControlServer server_;

        std::mutex lifecycle_; // serialises enable / disable / stop, held across thread joins
        std::mutex mu_;        // guards everything below
        std::unique_ptr<FrameSource> source_;
        std::thread capture_;
        std::atomic<bool> stopCapture_{false};
        std::string state_ = "off";
        std::string error_;
        FrameFormat format_;
        uint32_t fps_ = 0;
        ControlState controls_;
        std::atomic<uint64_t> frames_{0};
        std::atomic<uint64_t> lastFrameNs_{0};
        std::atomic<uint64_t> dropped_{0};
        std::atomic<uint32_t> dumped_{0};
        bool dumpWarned_ = false;
        bool started_ = false;
    };
}
