// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include "kick_mcu/output/blink.h"
#include "sim_clock.h"

using namespace kick_mcu;

TEST( BlinkClock, DefaultIsTwoHertzWithEqualHalves )
{
    sim::SimClock       clock;
    output::BlinkClock  blink( clock );
    EXPECT_EQ( 500u, blink.periodMs() );

    bool seen[2000];
    for( int t = 0; t < 2000; ++t )
    {
        clock.set( (uint64_t)t * 1000 );
        blink.update();
        seen[t] = blink.on();
    }
    for( int t = 0; t < 2000; ++t )
        EXPECT_EQ( ( t / 250 ) % 2 == 0, seen[t] ) << "t = " << t << " ms";
}

TEST( BlinkClock, EveryReaderSeesTheSamePhaseBetweenUpdates )
{
    sim::SimClock      clock;
    output::BlinkClock blink( clock );
    clock.advanceMs( 300 );
    blink.update();
    const bool a = blink.on();
    clock.advanceMs( 100 ); // time moves, the phase does not until the next update
    EXPECT_EQ( a, blink.on() );
}

TEST( BlinkClock, AStalledLoopLandsOnTheRightPhase )
{
    sim::SimClock      clock;
    output::BlinkClock blink( clock );
    clock.advanceMs( 250 * 7 + 10 ); // seven edges: odd, so off
    blink.update();
    EXPECT_FALSE( blink.on() );
    clock.advanceMs( 240 ); // the eighth edge
    blink.update();
    EXPECT_TRUE( blink.on() );
}

TEST( BlinkClock, KeepsPhaseAcrossTheMillisecondWrap )
{
    sim::SimClock clock;
    clock.set( (uint64_t)0xFFFFFF00u * 1000 ); // 256 ms before the wrap
    output::BlinkClock blink( clock );
    bool               expected = true;
    uint32_t           elapsed  = 0;
    for( int step = 0; step < 4000; ++step )
    {
        clock.advanceMs( 1 );
        ++elapsed;
        blink.update();
        expected = ( elapsed / 250 ) % 2 == 0;
        ASSERT_EQ( expected, blink.on() ) << "elapsed " << elapsed;
    }
}

TEST( BlinkClock, ChangingThePeriodRestartsInTheOnHalf )
{
    sim::SimClock      clock;
    output::BlinkClock blink( clock );
    clock.advanceMs( 300 );
    blink.update();
    EXPECT_FALSE( blink.on() );
    blink.setPeriodMs( 1000 );
    EXPECT_TRUE( blink.on() );
    clock.advanceMs( 499 );
    blink.update();
    EXPECT_TRUE( blink.on() );
    clock.advanceMs( 1 );
    blink.update();
    EXPECT_FALSE( blink.on() );
}

TEST( BlinkClock, ATinyPeriodIsRaisedSoNothingDividesByZero )
{
    sim::SimClock      clock;
    output::BlinkClock blink( clock, 0 );
    EXPECT_EQ( 2u, blink.periodMs() );
    clock.advanceMs( 5 );
    blink.update();
    SUCCEED();
}
