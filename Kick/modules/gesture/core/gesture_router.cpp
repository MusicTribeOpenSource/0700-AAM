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

#include "gesture_router.h"

#include <algorithm>
#include <cmath>

namespace kick_gesture
{
    using contracts::GestureKind;

    int clampSensitivity( int s ) { return std::max( 1, std::min( 10, s ) ); }

    float minConfidenceForSensitivity( int s ) { return 0.96f - 0.04f * static_cast<float>( clampSensitivity( s ) - 1 ); }

    float pinchTauMsForSensitivity( int s ) { return 400.f - 40.f * static_cast<float>( clampSensitivity( s ) - 1 ); }

    GestureRouter::GestureRouter()
    {
        smoother_.configure( pinchTauMsForSensitivity( sensitivity_ ), kPinchMaxSlewPerSec );
    }

    void GestureRouter::setSensitivity( int s )
    {
        sensitivity_ = clampSensitivity( s );
        smoother_.configure( pinchTauMsForSensitivity( sensitivity_ ), kPinchMaxSlewPerSec );
    }

    void GestureRouter::beginPinch( float value, uint64_t tMs )
    {
        smoother_.reset( value, tMs );
        lastSmoothed_  = smoother_.value();
        stepRemainder_ = 0.f;
        pinchTracking_ = true;
    }

    namespace
    {
        // Commands for a discrete gesture.
        void discreteCommands( const Action& a, const AppState& s, std::vector<Command>& out )
        {
            switch( a.kind )
            {
                case ActionKind::None: break;
                case ActionKind::Parameter:
                {
                    const int steps = std::max( 1, static_cast<int>( std::lround( a.scale ) ) );
                    out.push_back( { Command::Type::KnobSteps, a.id, a.direction * steps } );
                    break;
                }
                case ActionKind::Button: out.push_back( { Command::Type::ButtonTap, a.id, 0 } ); break;
                case ActionKind::Transport: out.push_back( { Command::Type::ButtonTap, buttonIdFor( static_cast<TransportCommand>( a.id ) ), 0 } ); break;
                case ActionKind::Bank:
                {
                    int target = a.id;
                    if( a.direction != 0 )
                    {
                        const int current = std::max( panel::kBankA, std::min( panel::kBankH, s.currentBank ) );
                        target            = ( current + a.direction + panel::kNumBanks ) % panel::kNumBanks;
                    }
                    out.push_back( { Command::Type::ButtonTap, panel::kBankA + target, 0 } );
                    break;
                }
            }
        }
    }

    Outcome GestureRouter::handle( const contracts::GestureEvent& e, const AppState& s )
    {
        Outcome out;
        const Action&  action  = mapping_.get( e.kind );
        const float    minConf = minConfidenceForSensitivity( sensitivity_ );

        if( e.kind == GestureKind::PinchStart )
        {
            // The pinch reference is independent of what pinch_start itself is mapped to.
            pinchTracking_ = false;
            if( e.hasValue )
                beginPinch( e.value, e.tMs );
        }
        else if( e.kind == GestureKind::PinchEnd )
        {
            pinchTracking_ = false;
            stepRemainder_ = 0.f;
        }

        if( e.kind == GestureKind::PinchChange )
        {
            if( action.kind != ActionKind::Parameter || !e.hasValue )
                return out;
            out.mapped   = true;
            out.decision = gate( action, s, e.confidence, minConf );
            if( out.decision != GateDecision::Allow )
            {
                // Whatever the hand does while blocked must not count: the next allowed change sets a new reference.
                pinchTracking_ = false;
                return out;
            }
            if( !pinchTracking_ )
            {
                beginPinch( e.value, e.tMs );
                return out;
            }
            const float smoothed = smoother_.update( e.value, e.tMs );
            stepRemainder_ += ( smoothed - lastSmoothed_ ) * action.scale * static_cast<float>( action.direction );
            lastSmoothed_ = smoothed;
            const float whole = std::trunc( stepRemainder_ );
            if( whole != 0.f )
            {
                stepRemainder_ -= whole;
                out.commands.push_back( { Command::Type::KnobSteps, action.id, static_cast<int>( whole ) } );
            }
            return out;
        }

        if( action.kind == ActionKind::None )
            return out;
        out.mapped   = true;
        out.decision = gate( action, s, e.confidence, minConf );
        if( out.decision == GateDecision::Allow )
            discreteCommands( action, s, out.commands );
        return out;
    }
}
