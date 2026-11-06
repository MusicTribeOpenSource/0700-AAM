// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include "kick_mcu/link/tx_queue.h"
#include "sim_uart.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace kick_mcu::link;
using namespace testing_support;

namespace
{
// Splits the bytes the queue wrote into frames, so tests can see what order they came in.
std::vector<Decoded> framesOnWire( const sim::SimUart& uart )
{
    RxParser  parser;
    Collector got;
    got.attach( parser );
    feedAll( parser, uart.wire() );
    return got.frames;
}

void enqueueButton( TxQueue& q, uint8_t id, uint8_t state )
{
    const uint8_t p[2] = { id, state };
    q.enqueue( Lane::Button, contract::PKT_BUTTON, 0, contract::BTN_STATUS, p, 2 );
}

void enqueuePad( TxQueue& q, Lane lane, uint8_t pad, uint8_t state, uint8_t vel, uint8_t at )
{
    const uint8_t p[4] = { pad, state, vel, at };
    q.enqueue( lane, contract::PKT_PAD, 0, contract::PAD_STATE, p, 4 );
}
} // namespace

TEST( TxQueue, SendsAFrameExactlyAsTheCodecEncodesIt )
{
    TxQueue       q;
    sim::SimUart  uart;
    const uint8_t p[1] = { 2 };
    ASSERT_TRUE( q.enqueue( Lane::System, contract::PKT_SYSTEM, 0, contract::SYS_CHECK_COMM_STATUS, p, 1 ) );
    EXPECT_EQ( 1u, q.pump( uart ) );
    uart.drain();
    EXPECT_EQ( toHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } ), uart.wire() );
    EXPECT_TRUE( q.empty() );
}

TEST( TxQueue, KeepsOrderWithinALane )
{
    TxQueue      q;
    sim::SimUart uart( 1024 );
    for( uint8_t i = 0; i < 10; ++i )
        enqueueButton( q, i, 1 );
    q.pump( uart );
    uart.drain();

    const auto frames = framesOnWire( uart );
    ASSERT_EQ( 10u, frames.size() );
    for( uint8_t i = 0; i < 10; ++i )
        EXPECT_EQ( i, frames[i].payload[0] );
}

TEST( TxQueue, DrainsHigherPriorityLanesFirst )
{
    TxQueue      q;
    sim::SimUart uart( 1024 );
    enqueuePad( q, Lane::Aftertouch, 1, 1, 90, 40 );
    enqueuePad( q, Lane::PadKnob, 2, 1, 100, 0 );
    enqueueButton( q, 7, 1 );
    const uint8_t v[2] = { 1, 0 };
    q.enqueue( Lane::System, contract::PKT_SYSTEM, 0, contract::SYS_GET_SW_VER, v, 2 );

    q.pump( uart );
    uart.drain();

    const auto frames = framesOnWire( uart );
    ASSERT_EQ( 4u, frames.size() );
    EXPECT_EQ( contract::PKT_SYSTEM, frames[0].type );
    EXPECT_EQ( contract::PKT_BUTTON, frames[1].type );
    EXPECT_EQ( 2, frames[2].payload[0] ); // the pad strike
    EXPECT_EQ( 1, frames[3].payload[0] ); // the aftertouch
}

TEST( TxQueue, WritesWholeFramesOnly )
{
    TxQueue      q;
    sim::SimUart uart( 10 ); // a button frame is 10 bytes with its trailer
    enqueueButton( q, 1, 1 );
    enqueueButton( q, 2, 1 );

    EXPECT_EQ( 1u, q.pump( uart ) );
    EXPECT_EQ( 10u, uart.fifoBytes() );
    EXPECT_EQ( 0u, q.pump( uart ) ) << "no room for the second frame, so it must not be split";
    EXPECT_EQ( 10u, uart.fifoBytes() );
    EXPECT_EQ( 1u, q.depth( Lane::Button ) );

    uart.drain();
    EXPECT_EQ( 1u, q.pump( uart ) );
    uart.drain();
    EXPECT_EQ( 2u, framesOnWire( uart ).size() );
}

TEST( TxQueue, ABlockedHighLaneIsNotOvertakenByASmallerLowFrame )
{
    TxQueue      q;
    sim::SimUart uart( 12 );
    const uint8_t big[15] = {};
    q.enqueue( Lane::System, contract::PKT_SYSTEM, 0, contract::SYS_GET_SW_VER, big, 15 ); // 23 bytes, never fits
    const uint8_t z[1] = { 0 };
    q.enqueue( Lane::Aftertouch, contract::PKT_PAD, 0, contract::PAD_STATE, z, 1 ); // 9 bytes, would fit
    EXPECT_EQ( 0u, q.pump( uart ) );
    EXPECT_EQ( 0u, uart.fifoBytes() );
}

TEST( TxQueue, DropsNewFramesWhenAButtonLaneIsFullAndCountsThem )
{
    TxQueue      q;
    const size_t depth = TxQueue::kLaneDepth[(size_t)Lane::Button];
    for( size_t i = 0; i < depth + 5; ++i )
        enqueueButton( q, (uint8_t)i, 1 );

    EXPECT_EQ( depth, q.depth( Lane::Button ) );
    EXPECT_EQ( 5u, q.stats().dropped[(size_t)Lane::Button] );
    EXPECT_EQ( depth, q.stats().queued[(size_t)Lane::Button] );

    // What was kept is the oldest: a button press must not be replaced by a later one.
    sim::SimUart uart( 4096 );
    q.pump( uart );
    uart.drain();
    const auto frames = framesOnWire( uart );
    ASSERT_EQ( depth, frames.size() );
    EXPECT_EQ( 0, frames.front().payload[0] );
}

TEST( TxQueue, AftertouchKeepsTheNewestValueWhenFull )
{
    TxQueue      q;
    const size_t depth = TxQueue::kLaneDepth[(size_t)Lane::Aftertouch];
    for( size_t i = 0; i < depth + 4; ++i )
    {
        const uint8_t p[4] = { 0, 1, 0, (uint8_t)i };
        EXPECT_TRUE( q.enqueue( Lane::Aftertouch, contract::PKT_PAD, 0, contract::PAD_STATE, p, 4 ) );
    }

    EXPECT_EQ( depth, q.depth( Lane::Aftertouch ) );
    EXPECT_EQ( 4u, q.stats().dropped[(size_t)Lane::Aftertouch] );

    sim::SimUart uart( 4096 );
    q.pump( uart );
    uart.drain();
    const auto frames = framesOnWire( uart );
    ASSERT_EQ( depth, frames.size() );
    EXPECT_EQ( 4, frames.front().payload[3] ) << "the four oldest values were discarded";
    EXPECT_EQ( depth + 3, frames.back().payload[3] );
}

TEST( TxQueue, RefusesAPayloadTooLongForTheHost )
{
    TxQueue       q;
    const uint8_t big[20] = {};
    EXPECT_FALSE( q.enqueue( Lane::Button, contract::PKT_BUTTON, 0, 0, big, 20 ) );
    EXPECT_EQ( 1u, q.stats().rejected );
    EXPECT_TRUE( q.empty() );
}

TEST( TxQueue, ClearDiscardsWithoutCountingADrop )
{
    TxQueue q;
    enqueueButton( q, 1, 1 );
    q.clear( Lane::Button );
    EXPECT_TRUE( q.empty() );
    EXPECT_EQ( 0u, q.stats().dropped[(size_t)Lane::Button] );
}

TEST( TxQueue, WrapsAroundItsRingForManyFrames )
{
    TxQueue      q;
    sim::SimUart uart( 4096 );
    for( int round = 0; round < 50; ++round )
    {
        for( uint8_t i = 0; i < 5; ++i )
            enqueueButton( q, i, (uint8_t)( round & 1 ) );
        q.pump( uart );
    }
    uart.drain();
    EXPECT_EQ( 250u, framesOnWire( uart ).size() );
    EXPECT_EQ( 0u, q.stats().dropped[(size_t)Lane::Button] );
}

namespace
{
// A UART that claims room for a frame and then takes half of it: the BSP bug partialWrites reports.
class GreedyLiar : public bsp::IUart
{
public:
    size_t write( bsp::ByteSpan bytes ) override { return bytes.size / 2; }
    size_t writableBytes() const override { return 1000; }
    void   onByte( bsp::ByteHandler, void* ) override {}
};
} // namespace

TEST( TxQueue, CountsAFrameThatTheUartOnlyPartlyAccepted )
{
    TxQueue    q;
    GreedyLiar uart;
    enqueueButton( q, 1, 1 );
    enqueueButton( q, 2, 1 );
    q.pump( uart );
    EXPECT_EQ( 2u, q.stats().partialWrites );
    EXPECT_TRUE( q.empty() );
}

TEST( TxQueue, DiscardRemovesMatchingFramesAndKeepsTheOrderOfTheRest )
{
    TxQueue q;
    enqueuePad( q, Lane::Aftertouch, 3, 2, 0, 10 );
    enqueuePad( q, Lane::Aftertouch, 5, 2, 0, 11 );
    enqueuePad( q, Lane::Aftertouch, 3, 2, 0, 12 );
    enqueuePad( q, Lane::Aftertouch, 7, 2, 0, 13 );
    enqueuePad( q, Lane::Aftertouch, 3, 2, 0, 14 );
    enqueueButton( q, 3, 1 ); // another lane, same first byte: untouched

    EXPECT_EQ( 3u, q.discard( Lane::Aftertouch, contract::PKT_PAD, 3 ) );
    EXPECT_EQ( 2u, q.depth( Lane::Aftertouch ) );
    EXPECT_EQ( 1u, q.depth( Lane::Button ) );
    EXPECT_EQ( 0u, q.stats().dropped[(size_t)Lane::Aftertouch] ) << "a requested discard is not a drop";

    sim::SimUart uart( 4096 );
    q.pump( uart );
    uart.drain();
    const auto frames = framesOnWire( uart );
    ASSERT_EQ( 3u, frames.size() );
    EXPECT_EQ( contract::PKT_BUTTON, frames[0].type );
    EXPECT_EQ( 5, frames[1].payload[0] );
    EXPECT_EQ( 7, frames[2].payload[0] );
}

TEST( TxQueue, DiscardWorksWhenTheLaneHasWrappedAround )
{
    TxQueue      q;
    sim::SimUart uart( 4096 );
    const size_t depth = TxQueue::kLaneDepth[(size_t)Lane::Aftertouch];
    for( size_t i = 0; i < depth - 2; ++i )
        enqueuePad( q, Lane::Aftertouch, 1, 2, 0, 0 );
    q.pump( uart ); // empties it and leaves head in the middle of the ring
    uart.drain();
    uart.clearWire();
    for( size_t i = 0; i < depth; ++i )
        enqueuePad( q, Lane::Aftertouch, (uint8_t)( i % 2 ), 2, 0, (uint8_t)i );

    EXPECT_EQ( depth / 2, q.discard( Lane::Aftertouch, contract::PKT_PAD, 0 ) );
    q.pump( uart );
    uart.drain();
    const auto frames = framesOnWire( uart );
    ASSERT_EQ( depth / 2, frames.size() );
    for( const auto& f : frames )
        EXPECT_EQ( 1, f.payload[0] );
}

TEST( TxQueue, DiscardOfNothingChangesNothing )
{
    TxQueue q;
    enqueuePad( q, Lane::Aftertouch, 2, 2, 0, 1 );
    EXPECT_EQ( 0u, q.discard( Lane::Aftertouch, contract::PKT_PAD, 9 ) );
    EXPECT_EQ( 0u, q.discard( Lane::Aftertouch, contract::PKT_BUTTON, 2 ) );
    EXPECT_EQ( 1u, q.depth( Lane::Aftertouch ) );
}
