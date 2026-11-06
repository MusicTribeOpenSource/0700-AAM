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
#include "kick_gesture/features.h"
#include "kick_gesture/types.h"

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

// The temporal layer: landmark frames in, gesture events out.
//
// It is a pure function of the frames and their timestamps. It never reads a clock, so the same stream
// replays to the same events whatever the machine is doing.
//
// Contract the engine keeps (docs/gesture-events.md):
//  - every pinch_start is followed by exactly one pinch_end, also when the hand disappears, the clock runs
//    backwards, or endSession() is called;
//  - pinch_change only appears between a pinch_start and its pinch_end;
//  - events of one type are never closer than GestureConfig::minIntervalMs (pinch_end is exempt);
//  - while a pinch is starting or active, swipes, palm_hold and fist are suppressed.
namespace kickgest
{
    class GestureEngine
    {
    public:
        struct Stats
        {
            uint64_t framesWithHand = 0;
            uint64_t framesWithoutHand = 0;
            uint64_t nonFiniteFrames = 0;      // frames with NaN or infinite landmarks, counted as no hand
            uint64_t outOfOrderFrames = 0;     // timestamp equal to or before the previous frame
            uint64_t rateSuppressed = 0;       // events held back by a rate limit
            uint64_t confidenceSuppressed = 0; // events held back by minEventConfidence
        };

        explicit GestureEngine(const GestureConfig& cfg);

        // Appends the events this frame produced to `out`.
        void process(const LandmarkFrame& frame, std::vector<GestureEvent>& out);

        // Ends tracking now: emits the pinch_end of an open pinch and forgets the hand. Use it at the end of a
        // replay and when the camera or the service is switched off.
        void endSession(uint64_t tMs, std::vector<GestureEvent>& out);

        bool pinchActive() const { return pinchActive_; }
        const Stats& stats() const { return stats_; }
        const GestureConfig& config() const { return cfg_; }

    private:
        struct Sample
        {
            uint64_t tMs;
            float cx, cy, palm, score;
            int ext;
        };

        struct PoseTimer
        {
            bool running = false;
            bool fired = false;
            uint64_t since = 0;
            uint64_t lastTrue = 0;
            void update(bool now, uint64_t t, uint32_t exitMs);
            bool due(uint64_t t, uint32_t holdMs) const { return running && !fired && t - since >= holdMs; }
            void reset() { *this = PoseTimer(); }
        };

        void noHand(uint64_t t, std::vector<GestureEvent>& out);
        void clearTrack();
        bool updatePinch(const HandFeatures& f, uint64_t t, std::vector<GestureEvent>& out);
        void updatePoses(const HandFeatures& f, uint64_t t, std::vector<GestureEvent>& out);
        void updateSwipe(const HandFeatures& f, uint64_t t, std::vector<GestureEvent>& out);

        float speedPalmPerSec(uint64_t t) const;
        float meanScoreSince(uint64_t t0) const;
        Handedness handLabel() const;
        bool canEmit(GestureType type, uint64_t t) const;
        bool confident(float c);
        void emit(GestureType type, bool hasValue, float value, float confidence, uint64_t t, std::vector<GestureEvent>& out);

        static int slot(GestureType t) { return t == GestureType::SwipeRight ? static_cast<int>(GestureType::SwipeLeft) : static_cast<int>(t); }

        GestureConfig cfg_;
        Stats stats_;

        bool haveFrame_ = false;
        uint64_t lastFrameMs_ = 0;
        bool tracking_ = false;
        uint64_t lastHandMs_ = 0;

        std::deque<Sample> hist_;
        std::deque<Sample> swipeBuf_;
        static constexpr int kVotes = 15;
        std::array<float, kVotes> votes_{};
        int voteCount_ = 0;
        int voteHead_ = 0;

        PoseTimer palm_;
        PoseTimer fist_;

        bool swipeArmed_ = true;
        bool slowing_ = false;
        uint64_t slowSince_ = 0;

        bool pinchArming_ = false;
        uint64_t pinchArmSince_ = 0;
        bool pinchActive_ = false;
        bool pinchReleasing_ = false;
        uint64_t pinchReleaseSince_ = 0;
        float pinchLastValue_ = 0.f;

        std::array<uint64_t, kGestureTypeCount> lastEmitMs_{};
        std::array<bool, kGestureTypeCount> hasEmitted_{};
    };
}
