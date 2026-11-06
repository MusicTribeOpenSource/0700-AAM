// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The whole input path: fake hardware, scan loop, transport, UART bytes.
#include <gtest/gtest.h>

#include <memory>

#include "kick_mcu/app/firmware.h"
#include "sim_board.h"
#include "sim_clock.h"
#include "sim_uart.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace testing_support;

namespace
{
struct Rig : ::testing::Test
{
    sim::SimClock clock;
    sim::SimUart  uart{ 4096 };
    sim::SimBoard board;
    std::unique_ptr<app::Firmware> fw;

    void SetUp() override { build( {} ); }

    void build( const app::FirmwareConfig& cfg )
    {
        fw = std::make_unique<app::Firmware>( uart, clock, app::InputHardware{ board.matrix, board.pads, board.encoders, board.strip }, cfg );
    }

    void host( uint8_t type, uint8_t cmd, const Bytes& payload ) { uart.injectRx( fromHost( type, cmd, payload ) ); }

    void handshake()
    {
        host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
        host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } );
        host( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } );
        step( 1 );
        ASSERT_TRUE( fw->link().session().streaming() );
        uart.drain();
        uart.clearWire();
    }

    // n passes of the main loop, 250 us apart.
    void step( int n )
    {
        for( int i = 0; i < n; ++i )
        {
            fw->step();
            clock.advanceUs( 250 );
        }
    }

    std::vector<Decoded> sent()
    {
        uart.drain();
        link::RxParser parser;
        Collector got;
        got.attach( parser );
        feedAll( parser, uart.wire() );
        return got.frames;
    }

    std::vector<Decoded> sentOfType( uint8_t type )
    {
        std::vector<Decoded> out;
        for( auto& f : sent() )
            if( f.type == type )
                out.push_back( f );
        return out;
    }
};
} // namespace

TEST_F( Rig, AButtonPressAndReleaseReachTheWireAsTwoFrames )
{
    handshake();
    board.matrix.press( contract::kBtnShift + 1 );
    step( 12 );
    board.matrix.release( contract::kBtnShift + 1 );
    step( 40 );

    const auto b = sentOfType( contract::PKT_BUTTON );
    ASSERT_EQ( 2u, b.size() );
    EXPECT_EQ( ( Bytes{ 32, 1 } ), b[0].payload );
    EXPECT_EQ( ( Bytes{ 32, 0 } ), b[1].payload );
    EXPECT_EQ( contract::BTN_STATUS, b[0].cmd );
    EXPECT_EQ( contract::DIR_REQUEST, b[0].dir );
}

TEST_F( Rig, EveryButtonCodeProducesItsOwnPressAndRelease )
{
    handshake();
    for( unsigned code = 0; code < contract::kBtnCodeCount; ++code )
    {
        board.matrix.press( code );
        step( 12 );
        board.matrix.release( code );
        step( 40 );
    }
    const auto b = sentOfType( contract::PKT_BUTTON );
    ASSERT_EQ( 2u * contract::kBtnCodeCount, b.size() );
    for( unsigned code = 0; code < contract::kBtnCodeCount; ++code )
    {
        EXPECT_EQ( ( Bytes{ (uint8_t)code, 1 } ), b[2 * code].payload );
        EXPECT_EQ( ( Bytes{ (uint8_t)code, 0 } ), b[2 * code + 1].payload );
    }
}

TEST_F( Rig, ShiftGoesFirstWhenItArrivesInTheSameTickAsAnotherKey )
{
    handshake();
    board.matrix.press( 5 );
    board.matrix.press( contract::kBtnShift );
    board.matrix.press( 2 );
    step( 12 );
    const auto b = sentOfType( contract::PKT_BUTTON );
    ASSERT_EQ( 3u, b.size() );
    EXPECT_EQ( contract::kBtnShift, b[0].payload[0] );
    EXPECT_EQ( 2, b[1].payload[0] );
    EXPECT_EQ( 5, b[2].payload[0] );
}

TEST_F( Rig, AContactGlitchShorterThanTheWindowSendsNothing )
{
    handshake();
    board.matrix.glitchOnce( 9 );
    step( 1 );
    board.matrix.clearGlitch();
    step( 40 );
    EXPECT_TRUE( sentOfType( contract::PKT_BUTTON ).empty() );
}

TEST_F( Rig, AShiftReleaseTheQueueRefusedIsRetriedNotLost )
{
    handshake();
    board.matrix.press( contract::kBtnShift );
    step( 12 );
    ASSERT_EQ( 1u, sentOfType( contract::PKT_BUTTON ).size() );
    uart.clearWire();

    // Fill the button lane without letting the transport run, so the release has nowhere to go.
    const size_t depth = link::TxQueue::kLaneDepth[(size_t)link::Lane::Button];
    for( size_t i = 0; i < depth; ++i )
    {
        const uint8_t p[2] = { 40, 1 };
        ASSERT_TRUE( fw->link().send( link::Lane::Button, contract::PKT_BUTTON, contract::BTN_STATUS, p, 2 ) );
    }
    board.matrix.release( contract::kBtnShift );
    for( int i = 0; i < 20; ++i ) // debounce the release with the transport stopped, so the lane stays full
    {
        fw->scan().tick();
        clock.advanceUs( 250 );
    }
    EXPECT_GT( fw->scan().stats().buttonRetries, 0u );

    step( 4 ); // now the transport drains and the retry gets through
    const auto b = sentOfType( contract::PKT_BUTTON );
    ASSERT_FALSE( b.empty() );
    EXPECT_EQ( ( Bytes{ contract::kBtnShift, 0 } ), b.back().payload ) << "after the filler, the release";
}

TEST_F( Rig, NothingIsSentBeforeTheHandshakeAndNothingStaleFollows )
{
    board.matrix.press( 3 );
    step( 12 );
    board.matrix.release( 3 );
    step( 40 );
    board.matrix.press( 7 ); // still held when the link comes up
    step( 12 );
    EXPECT_TRUE( sent().empty() ) << "no handshake yet: only the host speaks first";

    handshake();
    step( 40 );
    EXPECT_TRUE( sentOfType( contract::PKT_BUTTON ).empty() ) << "the held key is not announced late";
    board.matrix.release( 7 );
    step( 40 );
    EXPECT_TRUE( sentOfType( contract::PKT_BUTTON ).empty() ) << "and its release is not sent either";

    board.matrix.press( 7 );
    step( 12 );
    board.matrix.release( 7 );
    step( 40 );
    EXPECT_EQ( 2u, sentOfType( contract::PKT_BUTTON ).size() ) << "the next press is reported normally";
}

TEST_F( Rig, APadHeldWhenTheLinkComesUpSendsNoOrphanRelease )
{
    board.pads.set( 2, 2000 );
    step( 12 );
    handshake();
    board.pads.set( 2, 0 );
    step( 8 );
    EXPECT_TRUE( sentOfType( contract::PKT_PAD ).empty() );
}

TEST_F( Rig, APadStrikeAndReleaseReachTheWire )
{
    handshake();
    board.pads.set( 4, 2000 );
    step( 8 );
    board.pads.set( 4, 0 );
    step( 4 );
    const auto p = sentOfType( contract::PKT_PAD );
    ASSERT_EQ( 2u, p.size() );
    EXPECT_EQ( 4, p[0].payload[0] );
    EXPECT_EQ( 1, p[0].payload[1] );
    EXPECT_GT( p[0].payload[2], 1 );
    EXPECT_EQ( ( Bytes{ 4, 0, 0, 0 } ), p[1].payload );
}

TEST_F( Rig, TheHostCanTurnAftertouchOnWithPadSetAfterTouch )
{
    handshake();
    board.pads.set( 1, 3000 );
    step( 8 );
    board.pads.set( 1, 1500 );
    step( 200 ); // 50 ms
    ASSERT_EQ( 1u, sentOfType( contract::PKT_PAD ).size() ) << "the strike only";

    host( contract::PKT_PAD, contract::PAD_SET_AFTER_TOUCH, { 1 } );
    step( 100 );
    const auto p = sentOfType( contract::PKT_PAD );
    ASSERT_GE( p.size(), 2u );
    EXPECT_EQ( contract::kPadStateAftertouch, p[1].payload[1] );

    host( contract::PKT_PAD, contract::PAD_SET_AFTER_TOUCH, { 0 } );
    step( 4 );
    uart.clearWire();
    board.pads.set( 1, 2500 );
    step( 400 );
    EXPECT_TRUE( sentOfType( contract::PKT_PAD ).empty() ) << "off again";
}

TEST_F( Rig, KnobsAndTheStripReachTheWire )
{
    handshake();
    for( int i = 0; i < 8; ++i )
    {
        board.encoders.step( 2, true );
        step( 1 );
    }
    board.encoders.step( 0, false );
    step( 1 );
    board.strip.touch( 1023 );
    step( 2 );
    board.strip.lift();
    step( 20 );

    const auto k = sentOfType( contract::PKT_KNOBS );
    ASSERT_EQ( 2u, k.size() );
    EXPECT_EQ( ( Bytes{ 2, 1, 1 } ), k[0].payload );
    const auto s = sentOfType( contract::PKT_SLIDER );
    ASSERT_EQ( 2u, s.size() );
    EXPECT_EQ( ( Bytes{ 100 } ), s[0].payload );
    EXPECT_EQ( ( Bytes{ 0 } ), s[1].payload );
}

TEST_F( Rig, UnhandledHostFramesAreCountedNotActedOn )
{
    handshake();
    host( contract::PKT_KNOBS, contract::KNOB_ROTARY, { 3, 1 } ); // LED, PAD and SLIDER frames belong to the output decoder now
    step( 1 );
    EXPECT_EQ( 1u, fw->hostFramesIgnored() );
}
