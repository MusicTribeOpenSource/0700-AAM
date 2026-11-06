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

#include "kick_gesture/config.h"
#include "kick_gesture/engine.h"
#include "kick_gesture/frame_reader.h"
#include "kick_gesture/guard.h"
#include "kick_gesture/landmark_model.h"
#include "kick_gesture/record.h"
#include "kick_gesture/sink.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace kickgest
{
    struct ServiceOptions
    {
        double rateHz = 12.0;             // inferences per second at full rate; half of it while degraded
        uint32_t statusIntervalMs = 5000; // a status line this often, and at every state change
        uint32_t idleCheckMs = 200;       // how often to look for the camera while it is off or the service is disabled
        uint32_t guardIntervalMs = 1000;
    };

    // Camera frames in, gesture events out. Everything in here runs on one thread: step() is the whole loop body, run() just
    // calls it with a real clock, and tests call it with a clock of their own.
    class GestureService
    {
    public:
        struct Stats
        {
            uint64_t framesProcessed = 0;
            uint64_t modelErrors = 0;
            uint64_t convertErrors = 0;
            uint64_t eventsSent = 0;
        };

        // None of the pointers is owned. `recorder` may be null. `reader` supplies frames, `resources` the CPU and
        // temperature numbers for the guard.
        GestureService(ILandmarkModel* model, const GestureConfig& gesture, const ServiceOptions& options, const GuardLimits& limits, IResourceReader* resources, IFrameReader* reader,
                       std::vector<IEventSink*> sinks, LandmarkWriter* recorder = nullptr);

        // Does whatever is due at monotonic time nowNs. Returns how many nanoseconds the caller may sleep before the next call.
        uint64_t step(uint64_t nowNs);

        // Loops until `stop` is set, then closes an open pinch and sends a final status.
        void run(const std::atomic<bool>& stop);

        // Closes an open pinch (pinch_end) and reports "stopped".
        void shutdown(uint64_t nowNs);

        const Stats& stats() const { return stats_; }
        const GestureEngine& engine() const { return engine_; }
        GuardState guardState() const { return guardStatus_.state; }

    private:
        void send(const std::string& line);
        void sendEvents(std::vector<GestureEvent>& events);
        void sendStatus(uint64_t nowMs, const std::string& state, const std::string& reason);
        void closeSession(uint64_t nowMs);
        bool handleFrame(const kickcam::FrameMeta& meta);

        ILandmarkModel* model_;
        GestureEngine engine_;
        ServiceOptions opt_;
        ResourceGuard guard_;
        IResourceReader* resources_;
        IFrameReader* reader_;
        std::vector<IEventSink*> sinks_;
        LandmarkWriter* recorder_;

        GuardStatus guardStatus_;
        uint64_t nextGuardNs_ = 0;
        uint64_t nextFrameNs_ = 0;
        uint64_t nextStatusNs_ = 0;
        enum class Activity
        {
            Unknown,
            Running,
            Idle,
            NoCamera,
            Error
        };
        Activity activity_ = Activity::Unknown;
        std::string errorReason_;
        std::string lastState_;
        std::string lastReason_;
        bool haveSession_ = false;

        std::vector<uint8_t> frameBytes_;
        RgbImage rgb_;
        std::vector<Hand> hands_;
        std::vector<GestureEvent> events_;
        Stats stats_;
    };
}
