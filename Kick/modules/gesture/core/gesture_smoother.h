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

#include <cstdint>

namespace kick_gesture
{
    // Smooths the pinch aperture before it turns a knob, so a jittery hand does not zipper a filter.
    // Two stages, both driven by the event clock (the camera frame time in milliseconds), never by wall-clock time:
    //   1. a one-pole low-pass with time constant tauMs: v += (target - v) * (1 - exp(-dt / tauMs))
    //   2. a slew limit: v never moves faster than maxSlewPerSec (units of the 0 to 1 range per second)
    // The output stays inside 0 to 1.
    class PinchSmoother
    {
    public:
        PinchSmoother() = default;

        void configure( float tauMs, float maxSlewPerSec );

        // Sets the output to `v` (clamped to 0 to 1) at time `tMs` without any filtering. Call it when a pinch starts.
        void reset( float v, uint64_t tMs );

        // Moves toward `target` and returns the new output. A time that is not after the last one (the same frame
        // twice, or the clock going backwards) leaves the output where it is and, for a backwards clock, takes the
        // new time as the reference so the next update has a sane dt.
        float update( float target, uint64_t tMs );

        float value() const { return value_; }
        float tauMs() const { return tauMs_; }
        float maxSlewPerSec() const { return maxSlewPerSec_; }

    private:
        float    tauMs_{ 120.f };
        float    maxSlewPerSec_{ 3.f };
        float    value_{ 0.f };
        uint64_t lastMs_{ 0 };
    };
}
