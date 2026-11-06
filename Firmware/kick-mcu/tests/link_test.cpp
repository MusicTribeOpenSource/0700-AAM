// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include "kick_mcu/link/link.h"
#include "sim_clock.h"
#include "sim_uart.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace kick_mcu::link;
using namespace testing_support;

namespace
{
struct LinkTest : ::testing::Test
{
    sim::SimUart  uart{ 256 };
    sim::SimClock clock;
    Link          link{ uart, clock };

    void rx( const Bytes& b ) { uart.injectRx( b ); }

    std::vector<Decoded> wire()
    {
        link.poll();
        uart.drain();
        RxParser  p;
        Collector c;
        c.attach( p );
        feedAll( p, uart.wire() );
        uart.clearWire();
        return c.frames;
    }

    void handshake()
    {
        rx( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } ) );
        link.poll();
        rx( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } ) );
        link.poll();
        rx( fromHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } ) );
        wire();
    }
};
} // namespace

TEST_F( LinkTest, RunsTheHandshakeFromRawUartBytes )
{
    rx( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } ) );
    auto out = wire();
    ASSERT_EQ( 2u, out.size() );
    EXPECT_EQ( contract::SYS_GET_SW_VER, out[0].cmd );
    EXPECT_EQ( contract::SYS_CHECK_COMM_STATUS, out[1].cmd );

    rx( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } ) );
    out = wire();
    ASSERT_EQ( 1u, out.size() );
    EXPECT_EQ( ( Bytes{ 2 } ), out[0].payload );

    rx( fromHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } ) );
    link.poll();
    EXPECT_TRUE( link.session().streaming() );
}

TEST_F( LinkTest, EventsAreRefusedUntilStreaming )
{
    const uint8_t p[2] = { 1, 1 };
    EXPECT_FALSE( link.send( Lane::Button, contract::PKT_BUTTON, contract::BTN_STATUS, p, 2 ) );
    EXPECT_EQ( 1u, link.stats().droppedNotStreaming );
    EXPECT_TRUE( wire().empty() );

    handshake();
    EXPECT_TRUE( link.send( Lane::Button, contract::PKT_BUTTON, contract::BTN_STATUS, p, 2 ) );
    const auto out = wire();
    ASSERT_EQ( 1u, out.size() );
    EXPECT_EQ( contract::PKT_BUTTON, out[0].type );
}

TEST_F( LinkTest, ATruncatedFrameIsFlushedAfterTheInterByteTimeout )
{
    handshake();
    // A frame that promises 30 bytes, then a complete button frame, then silence.
    rx( Bytes{ 'M', 'T', 1, 0, 0, 31, 1, 2, 3 } );
    rx( fromHost( contract::PKT_SYSTEM, contract::SYS_RECONNECT, {} ) );
    link.poll();
    EXPECT_TRUE( link.session().streaming() ) << "the RECONNECT is stuck inside the stalled frame";

    clock.advanceMs( 9 );
    link.poll();
    EXPECT_TRUE( link.session().streaming() ) << "9 ms is inside the timeout";

    clock.advanceMs( 2 );
    link.poll();
    EXPECT_FALSE( link.session().streaming() ) << "after the timeout the RECONNECT is recovered and acted on";
    EXPECT_EQ( 1u, link.parser().stats().timeouts );
}

TEST_F( LinkTest, ASlowButSteadyFrameIsNotTimedOut )
{
    const Bytes f = fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
    for( uint8_t b : f )
    {
        uart.injectRx( &b, 1 );
        link.poll();
        clock.advanceMs( 5 ); // 5 ms between bytes: slow, but under the 10 ms timeout
    }
    EXPECT_EQ( 0u, link.parser().stats().timeouts );
    EXPECT_EQ( SessionState::Connected, link.session().state() );
}

TEST_F( LinkTest, ReceiveRingOverflowIsCounted )
{
    // Never polling, so nothing drains the ring.
    const Bytes junk( 400, 0x00 );
    rx( junk );
    EXPECT_EQ( 400u - 255u, link.stats().rxOverruns );
}

TEST_F( LinkTest, HeartbeatsComeFromPollWithoutAnyInput )
{
    handshake();
    clock.advanceMs( 3000 );
    const auto out = wire();
    ASSERT_EQ( 1u, out.size() );
    EXPECT_EQ( contract::SYS_CHECK_COMM_STATUS, out[0].cmd );
}

TEST_F( LinkTest, FramesTheSessionIgnoresReachThePassThrough )
{
    struct Seen
    {
        int count{ 0 };
        static void on( void* c, const Frame& ) { ++static_cast<Seen*>( c )->count; }
    } seen;
    link.setPassThrough( &Seen::on, &seen );
    rx( fromHost( contract::PKT_LED, contract::LED_SET_ALL, { 1 } ) );
    link.poll();
    EXPECT_EQ( 1, seen.count );
}

TEST_F( LinkTest, DetachesItsInterruptHandlerOnDestruction )
{
    sim::SimUart  u2( 64 );
    sim::SimClock c2;
    {
        Link l2( u2, c2 );
    }
    const Bytes f = fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
    u2.injectRx( f ); // must not touch the destroyed Link
    SUCCEED();
}
