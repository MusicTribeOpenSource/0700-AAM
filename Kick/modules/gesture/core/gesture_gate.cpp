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

#include "gesture_gate.h"

namespace kick_gesture
{
    const char* gateDecisionName( GateDecision d )
    {
        switch( d )
        {
            case GateDecision::Allow: return "allowed";
            case GateDecision::BlockedDisabled: return "gestures are off";
            case GateDecision::BlockedLowConfidence: return "below sensitivity";
            case GateDecision::BlockedRecording: return "blocked while recording";
        }
        return "allowed";
    }

    bool blockedWhileRecording( const Action& a )
    {
        switch( a.kind )
        {
            case ActionKind::Transport: return true;
            case ActionKind::Button:
                return a.id == panel::kRecord || a.id == panel::kOverdub || a.id == panel::kStop || a.id == panel::kPlay || a.id == panel::kPlayStart ||
                       a.id == panel::kErase;
            case ActionKind::None:
            case ActionKind::Parameter:
            case ActionKind::Bank: return false;
        }
        return false;
    }

    GateDecision gate( const Action& a, const AppState& s, float confidence, float minConfidence )
    {
        if( !s.gesturesEnabled )
            return GateDecision::BlockedDisabled;
        if( confidence < minConfidence )
            return GateDecision::BlockedLowConfidence;
        if( s.recording && blockedWhileRecording( a ) )
            return GateDecision::BlockedRecording;
        return GateDecision::Allow;
    }
}
