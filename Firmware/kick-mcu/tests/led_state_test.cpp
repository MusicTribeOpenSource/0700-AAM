// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include "kick_mcu/output/led_state.h"

using namespace kick_mcu;
using namespace kick_mcu::output;

TEST( LedState, PowersUpDark )
{
    LedState s;
    for( const auto& b : s.front().buttons )
        EXPECT_EQ( contract::STATE_OFF, b.state );
    for( const auto& p : s.front().pads )
    {
        EXPECT_EQ( contract::STATE_OFF, p.state );
        EXPECT_EQ( contract::kPadColourTrans, p.colour );
        EXPECT_EQ( contract::kPadColourTrans, p.offColour );
        EXPECT_EQ( 127, p.velocity );
    }
    EXPECT_EQ( 0, s.front().strip.percent );
}

TEST( LedState, AnUpdateIsInvisibleUntilCommit )
{
    LedState  s;
    LedFrame& back = s.beginUpdate();
    for( auto& p : back.pads )
    {
        p.state  = contract::STATE_ON;
        p.colour = 4;
    }
    for( const auto& p : s.front().pads )
        EXPECT_EQ( contract::STATE_OFF, p.state ) << "nothing is visible before commit";

    s.commit();
    for( const auto& p : s.front().pads )
    {
        EXPECT_EQ( contract::STATE_ON, p.state ) << "all 16 pads appear together";
        EXPECT_EQ( 4, p.colour );
    }
}

TEST( LedState, TheBackBufferStartsAsACopyOfTheFront )
{
    LedState s;
    s.beginUpdate().pads[3].colour = 9;
    s.commit();

    LedFrame& back = s.beginUpdate();
    EXPECT_EQ( 9, back.pads[3].colour ) << "an update builds on what is showing, not on the older buffer";
    back.pads[4].colour = 2;
    s.commit();
    EXPECT_EQ( 9, s.front().pads[3].colour );
    EXPECT_EQ( 2, s.front().pads[4].colour );
}

TEST( LedState, AnAbandonedUpdateLeavesNoTrace )
{
    LedState s;
    s.beginUpdate().pads[0].colour = 7; // never committed
    EXPECT_EQ( contract::kPadColourTrans, s.front().pads[0].colour );
    EXPECT_EQ( contract::kPadColourTrans, s.beginUpdate().pads[0].colour ) << "the next update starts from the front again";
}

TEST( LedState, ResetGoesDarkAndCommitsAreCounted )
{
    LedState s;
    s.beginUpdate().buttons[1].state = contract::STATE_ON;
    s.commit();
    EXPECT_EQ( 1u, s.commits() );
    s.reset();
    EXPECT_EQ( contract::STATE_OFF, s.front().buttons[1].state );
}
