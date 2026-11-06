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

#include <gtest/gtest.h>

using namespace kick_gesture;

namespace
{
    const Action kStop{ ActionKind::Transport, static_cast<int>( TransportCommand::Stop ), 0, 1.f };
    const Action kPlay{ ActionKind::Transport, static_cast<int>( TransportCommand::Play ), 0, 1.f };
    const Action kBankNext{ ActionKind::Bank, 0, 1, 1.f };
    const Action kKnob{ ActionKind::Parameter, 1, 1, 1.f };

    AppState on( bool recording = false )
    {
        AppState s;
        s.gesturesEnabled = true;
        s.recording       = recording;
        return s;
    }
}

TEST( Gate, OffByDefault )
{
    EXPECT_EQ( GateDecision::BlockedDisabled, gate( kPlay, AppState{}, 1.f, 0.6f ) );
}

TEST( Gate, DisabledWinsOverEverythingElse )
{
    AppState s;
    s.recording = true;
    EXPECT_EQ( GateDecision::BlockedDisabled, gate( kStop, s, 0.f, 0.9f ) );
}

TEST( Gate, ConfidenceBelowTheMinimumIsBlocked )
{
    EXPECT_EQ( GateDecision::BlockedLowConfidence, gate( kPlay, on(), 0.59f, 0.6f ) );
    EXPECT_EQ( GateDecision::Allow, gate( kPlay, on(), 0.6f, 0.6f ) );
}

TEST( Gate, ConfidenceIsCheckedBeforeTheRecordingRule )
{
    EXPECT_EQ( GateDecision::BlockedLowConfidence, gate( kStop, on( true ), 0.1f, 0.6f ) );
}

TEST( Gate, AStraySwipeCannotStopARecording )
{
    // The case the issue names. fist is mapped to Stop by default, and a take is running.
    EXPECT_EQ( GateDecision::BlockedRecording, gate( kStop, on( true ), 0.99f, 0.6f ) );
    EXPECT_EQ( GateDecision::Allow, gate( kStop, on( false ), 0.99f, 0.6f ) );
}

TEST( Gate, EveryTransportCommandIsBlockedWhileRecording )
{
    for( int c = 0; c < static_cast<int>( TransportCommand::Count ); ++c )
    {
        const Action a{ ActionKind::Transport, c, 0, 1.f };
        EXPECT_EQ( GateDecision::BlockedRecording, gate( a, on( true ), 1.f, 0.6f ) ) << c;
    }
}

TEST( Gate, TransportAndEraseButtonsAreBlockedWhileRecording )
{
    for( int id : { panel::kRecord, panel::kOverdub, panel::kStop, panel::kPlay, panel::kPlayStart, panel::kErase } )
        EXPECT_EQ( GateDecision::BlockedRecording, gate( { ActionKind::Button, id, 0, 1.f }, on( true ), 1.f, 0.6f ) ) << id;
    // An unrelated button is not.
    EXPECT_EQ( GateDecision::Allow, gate( { ActionKind::Button, 9, 0, 1.f }, on( true ), 1.f, 0.6f ) );
}

TEST( Gate, KnobsAndBanksStayAvailableWhileRecording )
{
    EXPECT_EQ( GateDecision::Allow, gate( kKnob, on( true ), 1.f, 0.6f ) );
    EXPECT_EQ( GateDecision::Allow, gate( kBankNext, on( true ), 1.f, 0.6f ) );
}

TEST( Gate, DecisionNamesAreDistinct )
{
    EXPECT_STRNE( gateDecisionName( GateDecision::Allow ), gateDecisionName( GateDecision::BlockedRecording ) );
    EXPECT_STRNE( gateDecisionName( GateDecision::BlockedDisabled ), gateDecisionName( GateDecision::BlockedLowConfidence ) );
}
