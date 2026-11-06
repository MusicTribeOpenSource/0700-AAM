// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include "kick_mcu/contract.h"
#include "kick_mcu/input/debounce.h"

using namespace kick_mcu;
using namespace kick_mcu::input;

namespace
{
constexpr uint64_t bit( unsigned n ) { return uint64_t( 1 ) << n; }

// Feeds `raw` for `forUs`, in 250 us steps, and returns the state after the last step.
uint64_t hold( Debouncer& d, uint32_t& now, uint64_t raw, uint32_t forUs )
{
    uint64_t state = d.state();
    for( uint32_t t = 0; t < forUs; t += 250 )
    {
        now += 250;
        state = d.update( raw, now );
    }
    return state;
}
} // namespace

TEST( Debounce, APressNeedsTheWholeWindow )
{
    Debouncer d( { 1000, 4000 } );
    uint32_t  now = 0;
    d.reset( now );

    hold( d, now, bit( 5 ), 750 );
    EXPECT_EQ( 0u, d.state() ) << "750 us of a 1000 us window";
    hold( d, now, bit( 5 ), 250 );
    EXPECT_EQ( bit( 5 ), d.state() );
}

TEST( Debounce, AReleaseNeedsItsOwnLongerWindow )
{
    Debouncer d( { 1000, 4000 } );
    uint32_t  now = 0;
    d.reset( now );
    hold( d, now, bit( 5 ), 2000 );
    ASSERT_EQ( bit( 5 ), d.state() );

    hold( d, now, 0, 3750 );
    EXPECT_EQ( bit( 5 ), d.state() ) << "3750 us of a 4000 us window";
    hold( d, now, 0, 250 );
    EXPECT_EQ( 0u, d.state() );
}

TEST( Debounce, ABounceInsideTheWindowIsForgivenNotRestarted )
{
    Debouncer d( { 2000, 4000 } );
    uint32_t  now = 0;
    d.reset( now );

    // Closed 1 ms, open 0.25 ms (a bounce), closed again: the counter dips but does not restart.
    hold( d, now, bit( 3 ), 1000 );
    hold( d, now, 0, 250 );
    hold( d, now, bit( 3 ), 750 );
    EXPECT_EQ( 0u, d.state() ) << "net 1.5 ms closed so far";
    hold( d, now, bit( 3 ), 500 );
    EXPECT_EQ( bit( 3 ), d.state() ) << "net 2 ms closed";
}

TEST( Debounce, ANoiseSpikeShorterThanTheWindowNeverSurfaces )
{
    Debouncer d( { 1000, 4000 } );
    uint32_t  now = 0;
    d.reset( now );
    hold( d, now, bit( 9 ), 500 );
    hold( d, now, 0, 5000 );
    EXPECT_EQ( 0u, d.state() );
}

TEST( Debounce, KeysAreIndependent )
{
    Debouncer d( { 1000, 4000 } );
    uint32_t  now = 0;
    d.reset( now );
    hold( d, now, bit( 1 ) | bit( 40 ), 1000 );
    EXPECT_EQ( bit( 1 ) | bit( 40 ), d.state() );
    hold( d, now, bit( 40 ), 4000 );
    EXPECT_EQ( bit( 40 ), d.state() );
}

TEST( Debounce, EveryButtonCodeIncludingShiftAndColourEditCanPressAndRelease )
{
    for( unsigned code = 0; code < contract::kBtnCodeCount; ++code )
    {
        Debouncer d( { 1000, 4000 } );
        uint32_t  now = 0;
        d.reset( now );
        hold( d, now, bit( code ), 1000 );
        EXPECT_EQ( bit( code ), d.state() ) << "code " << code << " press";
        hold( d, now, 0, 4000 );
        EXPECT_EQ( 0u, d.state() ) << "code " << code << " release";
    }
}

TEST( Debounce, AStalledLoopDoesNotTurnOneReadingIntoALongRun )
{
    Debouncer d( { 1000, 4000 } );
    uint32_t  now = 0;
    d.reset( now );

    // A one second stall, then one closed reading: it counts for at most 500 us (half the
    // shorter window), so the key is not pressed yet. A second reading 500 us later completes it.
    now += 1000000;
    d.update( bit( 2 ), now );
    EXPECT_EQ( 0u, d.state() );
    now += 500;
    d.update( bit( 2 ), now );
    EXPECT_EQ( bit( 2 ), d.state() );
}

TEST( Debounce, ZeroWindowsReportImmediately )
{
    Debouncer d( { 0, 0 } );
    uint32_t  now = 0;
    d.reset( now );
    now += 250;
    EXPECT_EQ( bit( 7 ), d.update( bit( 7 ), now ) );
    now += 250;
    EXPECT_EQ( 0u, d.update( 0, now ) );
}

TEST( Debounce, TheClockMayWrap )
{
    Debouncer d( { 1000, 4000 } );
    uint32_t  now = 0xFFFFFFFFu - 500u;
    d.reset( now );
    hold( d, now, bit( 4 ), 1000 ); // crosses 2^32
    EXPECT_EQ( bit( 4 ), d.state() );
}
