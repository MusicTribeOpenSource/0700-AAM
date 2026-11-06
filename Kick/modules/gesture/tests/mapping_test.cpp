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

#include "gesture_mapping.h"

#include <gtest/gtest.h>

using contracts::GestureKind;
using namespace kick_gesture;

TEST( Mapping, DefaultsAreTheDocumentedOnes )
{
    const MappingTable m;
    EXPECT_EQ( ( Action{ ActionKind::Bank, 0, -1, 1.f } ), m.get( GestureKind::SwipeLeft ) );
    EXPECT_EQ( ( Action{ ActionKind::Bank, 0, 1, 1.f } ), m.get( GestureKind::SwipeRight ) );
    EXPECT_EQ( ( Action{ ActionKind::Transport, static_cast<int>( TransportCommand::Play ), 0, 1.f } ), m.get( GestureKind::PalmHold ) );
    EXPECT_EQ( ( Action{ ActionKind::Transport, static_cast<int>( TransportCommand::Stop ), 0, 1.f } ), m.get( GestureKind::Fist ) );
    EXPECT_EQ( ActionKind::None, m.get( GestureKind::PinchStart ).kind );
    EXPECT_EQ( ActionKind::None, m.get( GestureKind::PinchEnd ).kind );
    EXPECT_EQ( ( Action{ ActionKind::Parameter, 1, 1, 32.f } ), m.get( GestureKind::PinchChange ) );
}

TEST( Mapping, DefaultTextIsStable )
{
    // This string is what the preferences file holds for an untouched mapping. Changing it changes stored data.
    EXPECT_EQ( "swipe_left=bank:0:-1:1;swipe_right=bank:0:1:1;palm_hold=transport:0:0:1;fist=transport:1:0:1;"
               "pinch_start=none:0:0:1;pinch_change=parameter:1:1:32;pinch_end=none:0:0:1",
               MappingTable::defaults().toString() );
}

TEST( Mapping, TextRoundTrips )
{
    MappingTable m;
    std::string  why;
    ASSERT_TRUE( m.set( GestureKind::SwipeLeft, { ActionKind::Parameter, 3, -1, 2.5f }, why ) ) << why;
    ASSERT_TRUE( m.set( GestureKind::Fist, { ActionKind::Button, 52, 0, 1.f }, why ) ) << why;
    ASSERT_TRUE( m.set( GestureKind::PalmHold, { ActionKind::Bank, 6, 0, 1.f }, why ) ) << why;
    ASSERT_TRUE( m.set( GestureKind::PinchChange, { ActionKind::Parameter, 16, -1, 0.5f }, why ) ) << why;
    ASSERT_TRUE( m.set( GestureKind::SwipeRight, { ActionKind::None, 0, 0, 1.f }, why ) ) << why;

    MappingTable back;
    std::string  problems;
    EXPECT_TRUE( MappingTable::fromString( m.toString(), back, problems ) ) << problems;
    EXPECT_EQ( m, back );
}

TEST( Mapping, EmptyTextGivesDefaults )
{
    MappingTable m;
    std::string  problems;
    EXPECT_TRUE( MappingTable::fromString( "", m, problems ) );
    EXPECT_EQ( MappingTable::defaults(), m );
}

TEST( Mapping, SetStoresNoneAsAllZeroFields )
{
    MappingTable m;
    std::string  why;
    ASSERT_TRUE( m.set( GestureKind::Fist, { ActionKind::None, 9, 1, 7.f }, why ) );
    EXPECT_EQ( ( Action{ ActionKind::None, 0, 0, 1.f } ), m.get( GestureKind::Fist ) );
}

TEST( Mapping, BadEntriesAreSkippedAndTheDefaultStays )
{
    MappingTable m;
    std::string  problems;
    const bool   ok = MappingTable::fromString(
        "swipe_left=parameter:99:1:1;"   // knob out of range
        "bogus=bank:0:1:1;"              // unknown gesture
        "fist=button:44;"                // too few fields
        "palm_hold=transport:1:0:1;"     // fine: Stop
        "pinch_change=bank:0:1:1;"       // pinch_change only takes a knob
        "swipe_right=bank:abc:1:1",      // not a number
        m, problems );
    EXPECT_FALSE( ok );
    EXPECT_EQ( MappingTable::defaults().get( GestureKind::SwipeLeft ), m.get( GestureKind::SwipeLeft ) );
    EXPECT_EQ( MappingTable::defaults().get( GestureKind::SwipeRight ), m.get( GestureKind::SwipeRight ) );
    EXPECT_EQ( MappingTable::defaults().get( GestureKind::Fist ), m.get( GestureKind::Fist ) );
    EXPECT_EQ( MappingTable::defaults().get( GestureKind::PinchChange ), m.get( GestureKind::PinchChange ) );
    EXPECT_EQ( ( Action{ ActionKind::Transport, 1, 0, 1.f } ), m.get( GestureKind::PalmHold ) );
    for( const char* part : { "swipe_left", "bogus", "fist", "pinch_change", "swipe_right" } )
        EXPECT_NE( std::string::npos, problems.find( part ) ) << "problems should mention " << part << ": " << problems;
    EXPECT_EQ( std::string::npos, problems.find( "palm_hold" ) ) << problems;
}

TEST( Mapping, SetLeavesTheTableAloneOnFailure )
{
    MappingTable m;
    std::string  why;
    EXPECT_FALSE( m.set( GestureKind::Fist, { ActionKind::Transport, 9, 0, 1.f }, why ) );
    EXPECT_FALSE( why.empty() );
    EXPECT_EQ( MappingTable::defaults(), m );
}

TEST( Mapping, ValidationRules )
{
    std::string why;
    EXPECT_FALSE( validateAction( GestureKind::Fist, { ActionKind::Parameter, 0, 1, 1.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::Fist, { ActionKind::Parameter, 17, 1, 1.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::Fist, { ActionKind::Parameter, 1, 0, 1.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::Fist, { ActionKind::Parameter, 1, 1, 0.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::Fist, { ActionKind::Parameter, 1, 1, 129.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::Fist, { ActionKind::Button, 53, 0, 1.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::Fist, { ActionKind::Button, -1, 0, 1.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::Fist, { ActionKind::Bank, 8, 0, 1.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::Fist, { ActionKind::Bank, 0, 2, 1.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::Fist, { ActionKind::Transport, 4, 0, 1.f }, why ) );
    EXPECT_TRUE( validateAction( GestureKind::Fist, { ActionKind::Button, 52, 0, 1.f }, why ) );
    EXPECT_TRUE( validateAction( GestureKind::Fist, { ActionKind::Bank, 7, 0, 1.f }, why ) );
    EXPECT_TRUE( validateAction( GestureKind::PinchChange, { ActionKind::Parameter, 4, -1, 16.f }, why ) );
    EXPECT_TRUE( validateAction( GestureKind::PinchChange, { ActionKind::None, 0, 0, 1.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::PinchChange, { ActionKind::Button, 1, 0, 1.f }, why ) );
    EXPECT_FALSE( validateAction( GestureKind::PinchChange, { ActionKind::Transport, 0, 0, 1.f }, why ) );
}

TEST( Mapping, TransportButtonsMatchThePanel )
{
    EXPECT_EQ( 45, buttonIdFor( TransportCommand::Play ) );
    EXPECT_EQ( 44, buttonIdFor( TransportCommand::Stop ) );
    EXPECT_EQ( 42, buttonIdFor( TransportCommand::Record ) );
    EXPECT_EQ( 43, buttonIdFor( TransportCommand::Overdub ) );
}

TEST( Mapping, EveryChoiceIsValidAndStartsWithNone )
{
    for( int i = 0; i < contracts::kGestureKindCount; ++i )
    {
        const auto  g       = static_cast<GestureKind>( i );
        const auto& choices = actionChoices( g );
        ASSERT_FALSE( choices.empty() );
        EXPECT_EQ( ActionKind::None, choices.front().kind );
        for( const auto& a : choices )
        {
            std::string why;
            EXPECT_TRUE( validateAction( g, a, why ) ) << describeAction( g, a ) << ": " << why;
        }
    }
}

TEST( Mapping, DefaultsAreAmongTheChoices )
{
    const MappingTable m;
    for( int i = 0; i < contracts::kGestureKindCount; ++i )
    {
        const auto  g       = static_cast<GestureKind>( i );
        const auto& choices = actionChoices( g );
        bool        found   = false;
        for( const auto& a : choices )
            found = found || a == m.get( g );
        EXPECT_TRUE( found ) << contracts::gestureKindName( g );
    }
}

TEST( Mapping, Descriptions )
{
    EXPECT_EQ( "None", describeAction( GestureKind::Fist, {} ) );
    EXPECT_EQ( "Bank next", describeAction( GestureKind::SwipeRight, { ActionKind::Bank, 0, 1, 1.f } ) );
    EXPECT_EQ( "Bank previous", describeAction( GestureKind::SwipeLeft, { ActionKind::Bank, 0, -1, 1.f } ) );
    EXPECT_EQ( "Bank C", describeAction( GestureKind::Fist, { ActionKind::Bank, 2, 0, 1.f } ) );
    EXPECT_EQ( "Play", describeAction( GestureKind::PalmHold, { ActionKind::Transport, 0, 0, 1.f } ) );
    EXPECT_EQ( "Stop", describeAction( GestureKind::Fist, { ActionKind::Transport, 1, 0, 1.f } ) );
    EXPECT_EQ( "Knob 2 down", describeAction( GestureKind::Fist, { ActionKind::Parameter, 2, -1, 1.f } ) );
    EXPECT_EQ( "Knob 1 follows pinch", describeAction( GestureKind::PinchChange, { ActionKind::Parameter, 1, 1, 32.f } ) );
    EXPECT_EQ( "Knob 1 follows pinch, inverted", describeAction( GestureKind::PinchChange, { ActionKind::Parameter, 1, -1, 32.f } ) );
    EXPECT_EQ( "Button 52", describeAction( GestureKind::Fist, { ActionKind::Button, 52, 0, 1.f } ) );
}
