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

#include "gesture_gate.h"
#include "gesture_mapping.h"
#include "gesture_smoother.h"

#include <vector>

namespace kick_gesture
{
    // One thing for the adapter to do on the message thread.
    struct Command
    {
        enum class Type : int
        {
            ButtonTap, // press and release front panel button `id`
            KnobSteps  // turn Q-Link knob `id` by `steps` (positive is clockwise)
        };
        Type type{ Type::ButtonTap };
        int  id{ 0 };
        int  steps{ 0 };

        bool operator==( const Command& o ) const { return type == o.type && id == o.id && steps == o.steps; }
    };

    struct Outcome
    {
        // True when the gesture has an action. False for None, and for pinch_change when it is not mapped to a knob.
        bool                 mapped{ false };
        GateDecision         decision{ GateDecision::Allow };
        std::vector<Command> commands;
    };

    // Sensitivity runs from 1 (least) to 10 (most) and is stored in the preferences.
    //   minimum confidence: 0.96 at 1, 0.80 at 5, 0.60 at 10 (the service never sends below 0.6)
    //   pinch smoothing time constant: 400 ms at 1, 240 ms at 5, 40 ms at 10
    int   clampSensitivity( int s );
    float minConfidenceForSensitivity( int s );
    float pinchTauMsForSensitivity( int s );
    // The slew limit for the pinch value, in units of its 0 to 1 range per second.
    constexpr float kPinchMaxSlewPerSec = 3.f;

    // Turns gesture events into commands: looks up the mapping, applies the gate, smooths the pinch.
    // Not thread-safe. The adapter calls it from the message thread only.
    class GestureRouter
    {
    public:
        GestureRouter();

        void                setMapping( const MappingTable& m ) { mapping_ = m; }
        const MappingTable& mapping() const { return mapping_; }
        void                setSensitivity( int s );
        int                 sensitivity() const { return sensitivity_; }

        Outcome handle( const contracts::GestureEvent& e, const AppState& s );

    private:
        void beginPinch( float value, uint64_t tMs );

        MappingTable  mapping_;
        int           sensitivity_{ 5 };
        PinchSmoother smoother_;
        // Set by pinch_start, which also sets the reference value. A pinch_change that the gate blocks clears it, and
        // the next change the gate allows then sets a new reference without turning the knob. Only a change that
        // arrives while this is set turns the knob.
        bool  pinchTracking_{ false };
        float lastSmoothed_{ 0.f };
        float stepRemainder_{ 0.f };
    };
}
