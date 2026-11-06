// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A full-rate pad storm on a simulated 230400 baud line.
#include <gtest/gtest.h>

#include <algorithm>

#include "kick_mcu/link/link.h"
#include "sim_clock.h"
#include "sim_uart.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace kick_mcu::link;
using namespace testing_support;

namespace
{
// 230400 baud, 8N1 is 10 bits a byte: 23 040 bytes a second, 23.04 a millisecond. The test
// drains 23 or 24 bytes a tick so the average is right.
size_t bytesForTick( uint32_t tick ) { return ( tick % 25 ) < 1 ? 24 : 23; }

struct Result
{
    size_t buttonsAttempted{ 0 }, padEventsAttempted{ 0 }, aftertouchAttempted{ 0 };
    size_t buttonsOnWire{ 0 }, padEventsOnWire{ 0 }, aftertouchOnWire{ 0 };
    size_t maxButtonDepth{ 0 };
    size_t garbageOnWire{ 0 };
};
} // namespace

TEST( Storm, SixteenPadsWithAftertouchNeverCostAButtonFrame )
{
    // The UART FIFO is the only thing a button can wait behind once it is at the head of the
    // queue, so its size sets the worst-case wait: 32 bytes is 1.4 ms of line.
    sim::SimUart  uart( 32 );
    sim::SimClock clock;
    Link          link( uart, clock );

    // Open the link the way the host does.
    uart.injectRx( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } ) );
    uart.injectRx( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } ) );
    uart.injectRx( fromHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } ) );
    link.poll();
    uart.drain();
    uart.clearWire();
    ASSERT_TRUE( link.session().streaming() );
    const TxStats before = link.tx().stats();

    Result r;
    const uint32_t kMs = 3000;
    for( uint32_t ms = 0; ms < kMs; ++ms )
    {
        // Every pad reports pressure every millisecond: 16 frames, far more than the line carries.
        for( uint8_t pad = 0; pad < 16; ++pad )
        {
            const uint8_t p[4] = { pad, 1, 0, (uint8_t)( 1 + ms % 127 ) };
            ++r.aftertouchAttempted;
            link.send( Lane::Aftertouch, contract::PKT_PAD, contract::PAD_STATE, p, 4 );
        }

        // Each pad is struck every 40 ms (25 strikes a second, all 16 at once) and released 20 ms later.
        if( ms % 40 == 0 )
            for( uint8_t pad = 0; pad < 16; ++pad )
            {
                const uint8_t p[4] = { pad, 1, (uint8_t)( 1 + ( ms + pad ) % 127 ), 0 };
                ++r.padEventsAttempted;
                link.send( Lane::PadKnob, contract::PKT_PAD, contract::PAD_STATE, p, 4 );
            }
        if( ms % 40 == 20 )
            for( uint8_t pad = 0; pad < 16; ++pad )
            {
                const uint8_t p[4] = { pad, 0, 0, 0 };
                ++r.padEventsAttempted;
                link.send( Lane::PadKnob, contract::PKT_PAD, contract::PAD_STATE, p, 4 );
            }

        // Buttons press and release on a 7 ms beat, interleaved with all of that.
        if( ms % 7 == 0 )
        {
            const uint8_t p[2] = { (uint8_t)( ( ms / 7 ) % 53 ), (uint8_t)( ( ms / 7 ) & 1 ) };
            ++r.buttonsAttempted;
            link.send( Lane::Button, contract::PKT_BUTTON, contract::BTN_STATUS, p, 2 );
        }

        link.poll();
        r.maxButtonDepth = std::max( r.maxButtonDepth, link.tx().depth( Lane::Button ) );
        uart.drain( bytesForTick( ms ) );
        clock.advanceMs( 1 );
    }

    // Let the line run dry. Remaining aftertouch is allowed to be discarded by the test ending.
    for( int i = 0; i < 400; ++i )
    {
        link.poll();
        uart.drain( 23 );
        clock.advanceMs( 1 );
    }
    link.poll();
    uart.drain();

    RxParser  parser;
    Collector got;
    got.attach( parser );
    feedAll( parser, uart.wire() );
    for( const auto& d : got.frames )
    {
        if( d.type == contract::PKT_BUTTON )
            ++r.buttonsOnWire;
        else if( d.type == contract::PKT_PAD && d.payload[1] == 1 && d.payload[2] == 0 )
            ++r.aftertouchOnWire; // pressed, no strike velocity: a pressure update
        else if( d.type == contract::PKT_PAD )
            ++r.padEventsOnWire;
    }

    const TxStats& s = link.tx().stats();
    const size_t   B = (size_t)Lane::Button, P = (size_t)Lane::PadKnob, A = (size_t)Lane::Aftertouch;

    // The requirement: no button frame lost, none delayed behind the storm.
    EXPECT_EQ( 0u, s.dropped[B] );
    EXPECT_EQ( r.buttonsAttempted, r.buttonsOnWire );
    EXPECT_LE( r.maxButtonDepth, 1u ) << "a button waited behind more than the frame already in flight";

    // Pad strikes and releases are not dropped either: they fit in the line with room to spare.
    EXPECT_EQ( 0u, s.dropped[P] );
    EXPECT_EQ( r.padEventsAttempted, r.padEventsOnWire );

    // Aftertouch is what gives way, and every frame it lost is counted: queued = sent + dropped
    // (the drop-oldest policy admits every new frame), and attempted = queued.
    EXPECT_GT( s.dropped[A], 0u );
    EXPECT_EQ( r.aftertouchAttempted, s.queued[A] );
    EXPECT_EQ( s.queued[A], s.sent[A] + s.dropped[A] + link.tx().depth( Lane::Aftertouch ) );
    EXPECT_EQ( 0u, link.stats().rxOverruns );
    EXPECT_EQ( 0u, s.partialWrites );
    EXPECT_EQ( 0u, s.rejected );

    // Everything that reached the wire parses: no frame was split by the queue.
    EXPECT_EQ( 0u, parser.stats().crcErrors );
    EXPECT_EQ( 0u, parser.stats().sizeErrors );
    size_t sentSinceHandshake = 0;
    for( size_t lane = 0; lane < kLaneCount; ++lane )
        sentSinceHandshake += s.sent[lane] - before.sent[lane];
    EXPECT_EQ( sentSinceHandshake, got.frames.size() );
}
