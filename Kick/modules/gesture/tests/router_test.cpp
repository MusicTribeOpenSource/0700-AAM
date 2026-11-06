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
#include <gtest/gtest.h>

using contracts::GestureEvent;
using contracts::GestureKind;
using namespace kick_gesture;

namespace
{
    GestureEvent ev( GestureKind k, float confidence = 0.95f, uint64_t t = 1000 )
    {
        GestureEvent e;
        e.kind       = k;
        e.confidence = confidence;
        e.tMs        = t;
        return e;
    }

    GestureEvent pinch( GestureKind k, float value, uint64_t t, float confidence = 0.95f )
    {
        GestureEvent e = ev( k, confidence, t );
        e.hasValue     = true;
        e.value        = value;
        return e;
    }

    AppState enabled( int bank = 0, bool recording = false )
    {
        AppState s;
        s.gesturesEnabled = true;
        s.currentBank     = bank;
        s.recording       = recording;
        return s;
    }

    int totalSteps( const Outcome& o )
    {
        int n = 0;
        for( const auto& c : o.commands )
            if( c.type == Command::Type::KnobSteps )
                n += c.steps;
        return n;
    }
}

TEST( Router, NothingHappensWhileGesturesAreOff )
{
    GestureRouter r;
    const auto    o = r.handle( ev( GestureKind::PalmHold ), AppState{} );
    EXPECT_TRUE( o.mapped );
    EXPECT_EQ( GateDecision::BlockedDisabled, o.decision );
    EXPECT_TRUE( o.commands.empty() );
}

TEST( Router, PalmHoldPlaysAndFistStopsByDefault )
{
    GestureRouter r;
    auto          o = r.handle( ev( GestureKind::PalmHold ), enabled() );
    ASSERT_EQ( 1u, o.commands.size() );
    EXPECT_EQ( ( Command{ Command::Type::ButtonTap, panel::kPlay, 0 } ), o.commands[0] );
    o = r.handle( ev( GestureKind::Fist ), enabled() );
    ASSERT_EQ( 1u, o.commands.size() );
    EXPECT_EQ( ( Command{ Command::Type::ButtonTap, panel::kStop, 0 } ), o.commands[0] );
}

TEST( Router, FistCannotStopARecording )
{
    GestureRouter r;
    const auto    o = r.handle( ev( GestureKind::Fist ), enabled( 0, true ) );
    EXPECT_EQ( GateDecision::BlockedRecording, o.decision );
    EXPECT_TRUE( o.commands.empty() );
}

TEST( Router, SwipesStepTheBankAndWrap )
{
    GestureRouter r;
    auto          o = r.handle( ev( GestureKind::SwipeRight ), enabled( 2 ) );
    ASSERT_EQ( 1u, o.commands.size() );
    EXPECT_EQ( panel::kBankA + 3, o.commands[0].id );
    o = r.handle( ev( GestureKind::SwipeRight ), enabled( 7 ) );
    EXPECT_EQ( panel::kBankA, o.commands.at( 0 ).id );
    o = r.handle( ev( GestureKind::SwipeLeft ), enabled( 0 ) );
    EXPECT_EQ( panel::kBankH, o.commands.at( 0 ).id );
    o = r.handle( ev( GestureKind::SwipeLeft ), enabled( 5 ) );
    EXPECT_EQ( panel::kBankA + 4, o.commands.at( 0 ).id );
}

TEST( Router, BanksStayAvailableWhileRecording )
{
    GestureRouter r;
    const auto    o = r.handle( ev( GestureKind::SwipeRight ), enabled( 0, true ) );
    EXPECT_EQ( GateDecision::Allow, o.decision );
    EXPECT_EQ( 1u, o.commands.size() );
}

TEST( Router, AnAbsoluteBankIgnoresTheCurrentOne )
{
    GestureRouter r;
    MappingTable  m;
    std::string   why;
    ASSERT_TRUE( m.set( GestureKind::SwipeLeft, { ActionKind::Bank, 5, 0, 1.f }, why ) );
    r.setMapping( m );
    EXPECT_EQ( panel::kBankA + 5, r.handle( ev( GestureKind::SwipeLeft ), enabled( 1 ) ).commands.at( 0 ).id );
}

TEST( Router, AnOutOfRangeCurrentBankIsClamped )
{
    GestureRouter r;
    EXPECT_EQ( panel::kBankA, r.handle( ev( GestureKind::SwipeRight ), enabled( 99 ) ).commands.at( 0 ).id ); // 7 + 1 wraps
    EXPECT_EQ( panel::kBankA + 1, r.handle( ev( GestureKind::SwipeRight ), enabled( -3 ) ).commands.at( 0 ).id );
}

TEST( Router, SensitivityMovesTheConfidenceGate )
{
    GestureRouter r;
    r.setSensitivity( 1 );
    EXPECT_EQ( GateDecision::BlockedLowConfidence, r.handle( ev( GestureKind::PalmHold, 0.7f ), enabled() ).decision );
    r.setSensitivity( 10 );
    EXPECT_EQ( GateDecision::Allow, r.handle( ev( GestureKind::PalmHold, 0.7f ), enabled() ).decision );
}

TEST( Router, SensitivityTable )
{
    EXPECT_FLOAT_EQ( 0.96f, minConfidenceForSensitivity( 1 ) );
    EXPECT_FLOAT_EQ( 0.80f, minConfidenceForSensitivity( 5 ) );
    EXPECT_NEAR( 0.60f, minConfidenceForSensitivity( 10 ), 1e-6f );
    EXPECT_FLOAT_EQ( 400.f, pinchTauMsForSensitivity( 1 ) );
    EXPECT_FLOAT_EQ( 240.f, pinchTauMsForSensitivity( 5 ) );
    EXPECT_FLOAT_EQ( 40.f, pinchTauMsForSensitivity( 10 ) );
    EXPECT_EQ( 1, clampSensitivity( -4 ) );
    EXPECT_EQ( 10, clampSensitivity( 77 ) );
}

TEST( Router, AnUnmappedGestureIsNotMapped )
{
    GestureRouter r;
    const auto    o = r.handle( pinch( GestureKind::PinchStart, 0.2f, 1000 ), enabled() );
    EXPECT_FALSE( o.mapped );
    EXPECT_TRUE( o.commands.empty() );
}

TEST( Router, ADiscreteKnobMappingTurnsByTheRoundedScale )
{
    GestureRouter r;
    MappingTable  m;
    std::string   why;
    ASSERT_TRUE( m.set( GestureKind::SwipeLeft, { ActionKind::Parameter, 2, -1, 2.6f }, why ) );
    ASSERT_TRUE( m.set( GestureKind::SwipeRight, { ActionKind::Parameter, 2, 1, 0.2f }, why ) );
    r.setMapping( m );
    EXPECT_EQ( ( Command{ Command::Type::KnobSteps, 2, -3 } ), r.handle( ev( GestureKind::SwipeLeft ), enabled() ).commands.at( 0 ) );
    // A scale under one step still moves the knob by one.
    EXPECT_EQ( ( Command{ Command::Type::KnobSteps, 2, 1 } ), r.handle( ev( GestureKind::SwipeRight ), enabled() ).commands.at( 0 ) );
}

TEST( Router, PinchOpeningTurnsTheKnobUpAndClosingTurnsItDown )
{
    GestureRouter r;
    r.setSensitivity( 10 ); // 40 ms smoothing, so the output follows closely
    EXPECT_TRUE( r.handle( pinch( GestureKind::PinchStart, 0.2f, 1000 ), enabled() ).commands.empty() );
    int up = 0;
    for( int i = 1; i <= 20; ++i )
        up += totalSteps( r.handle( pinch( GestureKind::PinchChange, 0.2f + 0.03f * static_cast<float>( i ), 1000 + 50u * static_cast<uint64_t>( i ) ), enabled() ) );
    EXPECT_GT( up, 8 ) << "0.6 of the range at 32 steps is about 19, minus the smoothing lag";
    EXPECT_LE( up, 20 );
    int down = 0;
    for( int i = 1; i <= 40; ++i )
        down += totalSteps( r.handle( pinch( GestureKind::PinchChange, 0.8f - 0.03f * static_cast<float>( i ), 2000 + 50u * static_cast<uint64_t>( i ) ), enabled() ) );
    EXPECT_LT( down, -8 );
}

TEST( Router, AnInvertedPinchMappingReversesTheKnob )
{
    GestureRouter r;
    r.setSensitivity( 10 );
    MappingTable m;
    std::string  why;
    ASSERT_TRUE( m.set( GestureKind::PinchChange, { ActionKind::Parameter, 3, -1, 32.f }, why ) );
    r.setMapping( m );
    r.handle( pinch( GestureKind::PinchStart, 0.2f, 1000 ), enabled() );
    int total = 0;
    Outcome o;
    for( int i = 1; i <= 20; ++i )
    {
        o = r.handle( pinch( GestureKind::PinchChange, 0.2f + 0.03f * static_cast<float>( i ), 1000 + 50u * static_cast<uint64_t>( i ) ), enabled() );
        for( const auto& c : o.commands )
            EXPECT_EQ( 3, c.id );
        total += totalSteps( o );
    }
    EXPECT_LT( total, -8 );
}

TEST( Router, AJitteryHandDoesNotZipperTheKnob )
{
    // The hand holds still at 0.5 and the aperture reading jumps by 0.05 each frame. Unsmoothed, that is 3.2 steps of a
    // 32-step knob every frame. Smoothed, it stays at one step or none, and nets out to nothing.
    GestureRouter r;
    r.setSensitivity( 5 );
    r.handle( pinch( GestureKind::PinchStart, 0.5f, 1000 ), enabled() );
    int net = 0, worst = 0;
    for( int i = 1; i <= 200; ++i )
    {
        const float raw = 0.5f + ( i % 2 ? 0.05f : -0.05f );
        const auto  o   = r.handle( pinch( GestureKind::PinchChange, raw, 1000 + 50u * static_cast<uint64_t>( i ) ), enabled() );
        for( const auto& c : o.commands )
            worst = std::max( worst, std::abs( c.steps ) );
        net += totalSteps( o );
    }
    EXPECT_LE( worst, 1 );
    EXPECT_LE( std::abs( net ), 2 );
}

TEST( Router, TheKnobReachesTheEndOfTheTravelAndNoFurther )
{
    GestureRouter r;
    r.setSensitivity( 10 );
    r.handle( pinch( GestureKind::PinchStart, 0.f, 1000 ), enabled() );
    int total = 0;
    for( int i = 1; i <= 100; ++i )
        total += totalSteps( r.handle( pinch( GestureKind::PinchChange, 1.f, 1000 + 50u * static_cast<uint64_t>( i ) ), enabled() ) );
    // 32 steps over the full travel. The remainder under one step is kept, never rounded up.
    EXPECT_GE( total, 31 );
    EXPECT_LE( total, 32 );
}

TEST( Router, AChangeWithoutAStartOnlySetsTheReference )
{
    // A listener that joins mid-pinch sees pinch_change first (docs/gesture-events.md).
    GestureRouter r;
    EXPECT_TRUE( r.handle( pinch( GestureKind::PinchChange, 0.9f, 1000 ), enabled() ).commands.empty() );
    int total = 0;
    for( int i = 1; i <= 10; ++i )
        total += totalSteps( r.handle( pinch( GestureKind::PinchChange, 0.9f, 1000 + 50u * static_cast<uint64_t>( i ) ), enabled() ) );
    EXPECT_EQ( 0, total );
}

TEST( Router, AfterPinchEndTheNextChangeDoesNotJump )
{
    GestureRouter r;
    r.setSensitivity( 10 );
    r.handle( pinch( GestureKind::PinchStart, 0.1f, 1000 ), enabled() );
    r.handle( pinch( GestureKind::PinchEnd, 0.1f, 1100 ), enabled() );
    // A new pinch starts elsewhere. Without a start it just sets a reference.
    EXPECT_TRUE( r.handle( pinch( GestureKind::PinchChange, 0.9f, 5000 ), enabled() ).commands.empty() );
}

TEST( Router, ABlockedChangeDropsTheReference )
{
    GestureRouter r;
    r.setSensitivity( 10 );
    r.handle( pinch( GestureKind::PinchStart, 0.2f, 1000 ), enabled() );
    // Gestures are switched off for a moment while the hand keeps moving.
    AppState off = enabled();
    off.gesturesEnabled = false;
    EXPECT_EQ( GateDecision::BlockedDisabled, r.handle( pinch( GestureKind::PinchChange, 0.9f, 1050 ), off ).decision );
    // Back on: the first change only sets a reference, so the knob does not jump by the 0.7 the hand moved meanwhile.
    EXPECT_TRUE( r.handle( pinch( GestureKind::PinchChange, 0.9f, 1100 ), enabled() ).commands.empty() );
}

TEST( Router, PinchChangeMappedToNothingDoesNothing )
{
    GestureRouter r;
    MappingTable  m;
    std::string   why;
    ASSERT_TRUE( m.set( GestureKind::PinchChange, {}, why ) );
    r.setMapping( m );
    const auto o = r.handle( pinch( GestureKind::PinchChange, 0.5f, 1000 ), enabled() );
    EXPECT_FALSE( o.mapped );
    EXPECT_TRUE( o.commands.empty() );
}

TEST( Router, PinchChangeWithoutAValueIsIgnored )
{
    GestureRouter r;
    EXPECT_TRUE( r.handle( ev( GestureKind::PinchChange ), enabled() ).commands.empty() );
}

TEST( Router, PinchStartAndEndCanTriggerButtons )
{
    GestureRouter r;
    MappingTable  m;
    std::string   why;
    ASSERT_TRUE( m.set( GestureKind::PinchStart, { ActionKind::Button, 18, 0, 1.f }, why ) ); // tap tempo
    r.setMapping( m );
    const auto o = r.handle( pinch( GestureKind::PinchStart, 0.1f, 1000 ), enabled() );
    ASSERT_EQ( 1u, o.commands.size() );
    EXPECT_EQ( 18, o.commands[0].id );
}
