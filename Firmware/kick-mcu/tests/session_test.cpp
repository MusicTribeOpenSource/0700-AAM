// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include "kick_mcu/link/session.h"
#include "sim_uart.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace kick_mcu::link;
using namespace testing_support;

namespace
{
struct SessionTest : ::testing::Test
{
    TxQueue      tx;
    Session      session{ tx, SessionConfig{} };
    sim::SimUart uart{ 4096 };
    uint32_t     now{ 0 };

    // Hands the session a frame as the host would send it.
    void host( uint8_t type, uint8_t cmd, const Bytes& payload, uint8_t dir = contract::DIR_REQUEST )
    {
        Frame f;
        f.type   = type;
        f.dir    = dir;
        f.cmd    = cmd;
        f.length = (uint8_t)payload.size();
        for( size_t i = 0; i < payload.size(); ++i )
            f.payload[i] = payload[i];
        session.onFrame( f, now );
    }

    // What the MCU transmitted since the last call.
    std::vector<Decoded> sent()
    {
        tx.pump( uart );
        uart.drain();
        RxParser  p;
        Collector c;
        c.attach( p );
        feedAll( p, uart.wire() );
        uart.clearWire();
        return c.frames;
    }

    Decoded sys( uint8_t cmd, Bytes payload ) { return { contract::PKT_SYSTEM, 0, cmd, payload }; }

    void handshake()
    {
        host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
        host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } );
        host( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } );
        sent();
    }
};
} // namespace

TEST_F( SessionTest, StartsNotConnectedAndSaysNothing )
{
    EXPECT_EQ( SessionState::NotConnected, session.state() );
    EXPECT_TRUE( sent().empty() );
}

TEST_F( SessionTest, CommInitOneAnswersWithVersionThenStatusOne )
{
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
    EXPECT_EQ( SessionState::Connected, session.state() );

    const auto out = sent();
    ASSERT_EQ( 2u, out.size() );
    EXPECT_EQ( sys( contract::SYS_GET_SW_VER, { 1, 0 } ), out[0] );
    EXPECT_EQ( sys( contract::SYS_CHECK_COMM_STATUS, { 1 } ), out[1] );
}

TEST_F( SessionTest, ReportsTheConfiguredVersion )
{
    TxQueue tx2;
    Session s2( tx2, SessionConfig{ 3, 14, 3000 } );
    Frame   f;
    f.type = contract::PKT_SYSTEM; f.cmd = contract::SYS_COMM_INIT; f.length = 1; f.payload[0] = 1;
    s2.onFrame( f, 0 );
    sim::SimUart u( 256 );
    tx2.pump( u );
    u.drain();
    RxParser  p;
    Collector c;
    c.attach( p );
    feedAll( p, u.wire() );
    ASSERT_EQ( 2u, c.frames.size() );
    EXPECT_EQ( ( Bytes{ 3, 14 } ), c.frames[0].payload );
}

TEST_F( SessionTest, CommInitTwoAnswersWithStatusTwo )
{
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
    sent();
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } );

    const auto out = sent();
    ASSERT_EQ( 1u, out.size() );
    EXPECT_EQ( sys( contract::SYS_CHECK_COMM_STATUS, { 2 } ), out[0] );
    EXPECT_EQ( SessionState::Connected, session.state() );
}

TEST_F( SessionTest, TheHostsStatusEchoStartsStreaming )
{
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } );
    EXPECT_FALSE( session.streaming() );
    host( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } );
    EXPECT_TRUE( session.streaming() );
    EXPECT_EQ( 1u, session.stats().handshakes );
}

TEST_F( SessionTest, StatusFromTheHostBeforeAnyHandshakeIsIgnored )
{
    host( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } );
    EXPECT_EQ( SessionState::NotConnected, session.state() );
    EXPECT_EQ( 1u, session.stats().unexpected );
}

TEST_F( SessionTest, HeartbeatRunsEveryThreeSecondsOnceStreaming )
{
    handshake();
    for( int ms = 0; ms < 2999; ++ms )
    {
        ++now;
        session.poll( now );
    }
    EXPECT_TRUE( sent().empty() );

    ++now;
    session.poll( now );
    auto out = sent();
    ASSERT_EQ( 1u, out.size() );
    EXPECT_EQ( sys( contract::SYS_CHECK_COMM_STATUS, { 2 } ), out[0] );

    now += 3000;
    session.poll( now );
    EXPECT_EQ( 1u, sent().size() );
    EXPECT_EQ( 2u, session.stats().heartbeats );
}

TEST_F( SessionTest, NoHeartbeatBeforeStreaming )
{
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
    sent();
    now += 10000;
    session.poll( now );
    EXPECT_TRUE( sent().empty() );
}

TEST_F( SessionTest, HeartbeatSurvivesTheClockWrapping )
{
    now = 0xFFFFFF00u;
    handshake();
    now += 3000; // wraps past 2^32
    session.poll( now );
    EXPECT_EQ( 1u, sent().size() );
}

TEST_F( SessionTest, ReconnectDropsToNotConnectedAndStopsTheHeartbeat )
{
    handshake();
    host( contract::PKT_SYSTEM, contract::SYS_RECONNECT, {} );
    EXPECT_EQ( SessionState::NotConnected, session.state() );
    now += 10000;
    session.poll( now );
    EXPECT_TRUE( sent().empty() );
    EXPECT_EQ( 1u, session.stats().reconnects );
}

TEST_F( SessionTest, TheHandshakeRunsAgainAfterReconnect )
{
    handshake();
    host( contract::PKT_SYSTEM, contract::SYS_RECONNECT, {} );
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
    const auto out = sent();
    ASSERT_EQ( 2u, out.size() );
    EXPECT_EQ( sys( contract::SYS_GET_SW_VER, { 1, 0 } ), out[0] );
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } );
    host( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } );
    EXPECT_TRUE( session.streaming() );
    EXPECT_EQ( 2u, session.stats().handshakes );
}

TEST_F( SessionTest, CommInitOneWhileStreamingRestartsTheHandshake )
{
    handshake();
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
    EXPECT_EQ( SessionState::Connected, session.state() );
    EXPECT_EQ( 2u, sent().size() );
}

TEST_F( SessionTest, CommInitNotConnectedDisconnects )
{
    handshake();
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { contract::COMM_NOT_CONNECTED } );
    EXPECT_EQ( SessionState::NotConnected, session.state() );
}

TEST_F( SessionTest, QueuedEventsAreDroppedWhenTheSessionRestarts )
{
    handshake();
    const uint8_t button[2] = { 1, 1 };
    const uint8_t pad[4]    = { 1, 1, 9, 0 };
    tx.enqueue( Lane::Button, contract::PKT_BUTTON, 0, 0, button, 2 );
    tx.enqueue( Lane::PadKnob, contract::PKT_PAD, 0, 0, pad, 4 );
    host( contract::PKT_SYSTEM, contract::SYS_RECONNECT, {} );
    EXPECT_TRUE( sent().empty() );
}

TEST_F( SessionTest, ResponsesAreIgnored )
{
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 }, contract::DIR_RESPONSE );
    EXPECT_EQ( SessionState::NotConnected, session.state() );
    EXPECT_EQ( 1u, session.stats().ignoredResponses );
}

TEST_F( SessionTest, MalformedCommInitDoesNothing )
{
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, {} );
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 9 } );
    EXPECT_EQ( SessionState::NotConnected, session.state() );
    EXPECT_EQ( 2u, session.stats().unexpected );
    EXPECT_TRUE( sent().empty() );
}

namespace
{
struct Seen
{
    std::vector<Decoded> frames;
    static void          on( void* c, const Frame& f ) { static_cast<Seen*>( c )->frames.push_back( { f.type, f.dir, f.cmd, Bytes( f.payload, f.payload + f.length ) } ); }
};
} // namespace

TEST_F( SessionTest, FramesTheSessionDoesNotConsumeGoToThePassThrough )
{
    Seen seen;
    session.setPassThrough( &Seen::on, &seen );
    host( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 3, 1 } );
    host( contract::PKT_SYSTEM, contract::SYS_BPM, { 0x00, 0x78 } );
    host( contract::PKT_SYSTEM, contract::SYS_SHUTDOWN, {} );

    ASSERT_EQ( 3u, seen.frames.size() );
    EXPECT_EQ( contract::PKT_LED, seen.frames[0].type );
    EXPECT_EQ( contract::SYS_BPM, seen.frames[1].cmd );
    EXPECT_EQ( contract::SYS_SHUTDOWN, seen.frames[2].cmd );
    EXPECT_EQ( 3u, session.stats().passedThrough );
}

TEST_F( SessionTest, UnconsumedFramesAreCountedWhenNobodyListens )
{
    host( contract::PKT_LED, contract::LED_SET_ALL, { 0 } );
    EXPECT_EQ( 1u, session.stats().unhandled );
}
