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

#include "gesture_mapping.h"

namespace kick_gesture
{
    // What the gate needs to know about the app. The adapter fills it in right before each event is handled.
    struct AppState
    {
        // The "gestures enabled" switch on the camera settings page.
        bool gesturesEnabled{ false };
        // True while anything is recording or overdubbing: the sequencer, the looper or the sampler.
        bool recording{ false };
        // The selected pad bank, 0 (A) to 7 (H). Relative bank actions start from it.
        int currentBank{ 0 };
    };

    enum class GateDecision : int
    {
        Allow = 0,
        BlockedDisabled,      // gestures are switched off
        BlockedLowConfidence, // below the sensitivity setting
        BlockedRecording      // would stop, restart or change a recording
    };

    const char* gateDecisionName( GateDecision d );

    // True for actions that must not run while something is recording or overdubbing: every transport command, and
    // the transport and erase buttons. Knob turns and bank selection stay allowed, because neither one ends a take.
    bool blockedWhileRecording( const Action& a );

    // Decides, in this order: gestures off, confidence under `minConfidence`, then the recording rule above.
    GateDecision gate( const Action& a, const AppState& s, float confidence, float minConfidence );
}
