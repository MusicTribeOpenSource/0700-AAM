// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Input latency in simulation, on a 230400 baud line modelled byte by byte.
//
// What is measured, per event:
//   contact  the moment the fake hardware changes (a key closes, a pad is struck)
//   decision the start of the scan tick in which the firmware decided the event and queued it
//            (after the debounce window for a key, after the velocity window for a pad)
//   wire     the moment the last bit of the frame's last byte, the trailer, has left the UART
//
// "decision to wire" is the figure the 2 ms budget applies to: it is what the transport and
// the line cost. "contact to wire" adds the debounce or velocity window and the wait for the
// next scan tick, which are tunable settings and are reported separately.
//
// The clock is virtual, so CPU time is not measured. The test charges the scan a fixed
// kAssumedScanCpuUs between sampling and sending. That number is an assumption about the target,
// not a measurement, and it is the first thing to replace once the MCU exists.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

#include "kick_mcu/app/firmware.h"
#include "paced_uart.h"
#include "sim_board.h"
#include "sim_clock.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace testing_support;

namespace
{
constexpr uint32_t kTickUs           = 250; // main loop period
constexpr uint32_t kAssumedScanCpuUs = 100; // ASSUMPTION: scan work between sampling and queuing, on the target
constexpr size_t   kUartFifoBytes    = 24;  // BSP requirement: two frames of the usual size
constexpr double   kBudgetUs         = 2000.0;

struct WireByte
{
    uint8_t byte;
    double  endUs;
};

struct FrameTimes
{
    uint8_t type;
    Bytes   payload;
    double  endUs; // trailer byte complete
};

struct Rig
{
    sim::SimClock  clock;
    sim::PacedUart uart{ clock, kUartFifoBytes };
    sim::SimBoard  board;
    app::Firmware  fw;
    std::vector<WireByte> wire;

    // Start of the first tick, since armDecisions(), in which each lane gained a frame; -1 if none yet.
    double   decisionUs[link::kLaneCount];
    uint32_t queuedBefore[link::kLaneCount]{};

    explicit Rig( const app::FirmwareConfig& config = {} )
        : fw( uart, clock, app::InputHardware{ board.matrix, board.pads, board.encoders, board.strip }, config )
    {
        uart.setSink( [this]( uint8_t b, double t ) { wire.push_back( { b, t } ); } );
        send( contract::SYS_COMM_INIT, { 1 } );
        send( contract::SYS_COMM_INIT, { 2 } );
        send( contract::SYS_CHECK_COMM_STATUS, { 2 } );
        for( int i = 0; i < 400; ++i ) // 100 ms: handshake done, line idle
            tick();
        wire.clear();
        armDecisions();
    }

    void armDecisions()
    {
        for( double& d : decisionUs )
            d = -1;
    }

    void send( uint8_t cmd, const Bytes& payload ) { uart.injectRx( fromHost( contract::PKT_SYSTEM, cmd, payload ) ); }

    // One pass of the main loop: sample and queue, spend the assumed CPU time, then let the
    // transport send. The tick takes kTickUs in all.
    void tick()
    {
        const uint32_t start = clock.nowUs();
        fw.scan().tick();
        clock.advanceUs( kAssumedScanCpuUs );
        fw.link().poll();
        for( size_t i = 0; i < link::kLaneCount; ++i )
        {
            const uint32_t queued = fw.link().tx().stats().queued[i];
            if( queued != queuedBefore[i] && decisionUs[i] < 0 )
                decisionUs[i] = (double)start;
            queuedBefore[i] = queued;
        }
        clock.set( (uint64_t)start + kTickUs );
        uart.service();
    }

    // Runs 20 ms, long enough for any event in these tests to be decided and sent.
    void settle()
    {
        for( int i = 0; i < 80; ++i )
            tick();
    }

    // Frames on the wire from byte index `from` on.
    std::vector<FrameTimes> frames( size_t from = 0 ) const
    {
        std::vector<FrameTimes> out;
        size_t i = from;
        while( i + 6 < wire.size() )
        {
            if( wire[i].byte != 'M' || wire[i + 1].byte != 'T' )
            {
                ++i;
                continue;
            }
            const size_t total = (size_t)wire[i + 5].byte + 7;
            if( i + total > wire.size() )
                break;
            Bytes payload;
            for( size_t k = 0; k + 1 < wire[i + 5].byte; ++k )
                payload.push_back( wire[i + 6 + k].byte );
            out.push_back( { wire[i + 2].byte, payload, wire[i + total - 1].endUs } );
            i += total;
        }
        return out;
    }
};

struct Result
{
    double decisionToWireUs;
    double contactToWireUs;
};

void report( const char* name, const Result& r )
{
    std::printf( "[latency] %-34s decision->wire %7.1f us   contact->wire %7.1f us\n", name, r.decisionToWireUs, r.contactToWireUs );
}
} // namespace

TEST( Latency, AButtonPressOnAQuietLine )
{
    Result worst{ 0, 0 };
    for( uint32_t phase = 0; phase < kTickUs; phase += 25 ) // contact at every phase within a tick
    {
        Rig rig;
        rig.clock.advanceUs( phase );
        const double contact = (double)rig.clock.nowUs();
        rig.board.matrix.press( 20 );
        rig.settle();

        const auto f = rig.frames();
        ASSERT_EQ( 1u, f.size() ) << "phase " << phase;
        ASSERT_EQ( contract::PKT_BUTTON, f[0].type );
        worst.decisionToWireUs = std::max( worst.decisionToWireUs, f[0].endUs - rig.decisionUs[(size_t)link::Lane::Button] );
        worst.contactToWireUs  = std::max( worst.contactToWireUs, f[0].endUs - contact );
    }
    report( "button press, quiet line (worst)", worst );
    EXPECT_LT( worst.decisionToWireUs, kBudgetUs );
    EXPECT_LT( worst.contactToWireUs, kBudgetUs ) << "with the default 1 ms press debounce";
}

TEST( Latency, APadStrikeOnAQuietLine )
{
    Result worst{ 0, 0 };
    for( uint32_t phase = 0; phase < kTickUs; phase += 25 )
    {
        Rig rig;
        rig.clock.advanceUs( phase );
        const double contact = (double)rig.clock.nowUs();
        // A realistic rise: the sensor reaches its peak over about half a millisecond.
        rig.board.pads.set( 3, 800 );
        rig.tick();
        rig.board.pads.set( 3, 2400 );
        rig.tick();
        rig.board.pads.set( 3, 3000 );
        rig.settle();

        const auto f = rig.frames();
        ASSERT_EQ( 1u, f.size() ) << "phase " << phase;
        ASSERT_EQ( contract::PKT_PAD, f[0].type );
        worst.decisionToWireUs = std::max( worst.decisionToWireUs, f[0].endUs - rig.decisionUs[(size_t)link::Lane::PadKnob] );
        worst.contactToWireUs  = std::max( worst.contactToWireUs, f[0].endUs - contact );
    }
    report( "pad strike, quiet line (worst)", worst );
    EXPECT_LT( worst.decisionToWireUs, kBudgetUs );
    EXPECT_LT( worst.contactToWireUs, kBudgetUs ) << "with the default 1 ms velocity window";
}

TEST( Latency, AButtonPressInTheMiddleOfAFullPadStorm )
{
    // 16 pads held with aftertouch on and pressure changing every tick, and all 16 re-struck
    // every 20 ms: the line is carrying pad frames the whole time. A button must still get
    // through within the budget, because it has its own lane and the UART FIFO is short.
    Result worst{ 0, 0 };
    Rng    rng( 7 );
    for( int trial = 0; trial < 60; ++trial )
    {
        // Aftertouch on, with no settling holdoff and a 2 ms rate limit instead of 20 ms. Every
        // held pad then offers a pressure frame every 2 ms, far more than the line carries, so
        // the aftertouch lane overflows constantly (as in the transport's own storm test).
        app::FirmwareConfig cfg;
        cfg.scan.pads.aftertouchEnabled       = true;
        cfg.scan.pads.aftertouchHoldoffUs     = 0;
        cfg.scan.pads.aftertouchMinIntervalUs = 2000;
        Rig rig( cfg );

        // Run the storm for a while before and during the measurement.
        const int stormTicks = 600 + (int)rng.below( 400 );
        for( int t = 0; t < stormTicks; ++t )
        {
            for( unsigned p = 0; p < 16; ++p )
            {
                const uint16_t level = (uint16_t)( ( t % 80 ) < 8 ? 3200 : 600 + ( ( t * 37 + p * 53 ) % 1800 ) );
                const bool     up    = ( t % 80 ) >= 60; // released briefly so the next strike is a new one
                rig.board.pads.set( p, up ? 0 : level );
            }
            rig.tick();
        }
        // The press lands at a different point in the storm each trial.
        const double contact   = (double)rig.clock.nowUs();
        const size_t wireStart = rig.wire.size();
        rig.armDecisions();
        rig.board.matrix.press( 31 );
        for( int t = 0; t < 400; ++t )
        {
            for( unsigned p = 0; p < 16; ++p )
                rig.board.pads.set( p, (uint16_t)( 600 + ( ( t * 41 + p * 29 ) % 1800 ) ) );
            rig.tick();
        }

        // The storm is real: the pad lanes carried hundreds of frames, and the aftertouch lane overflowed.
        const auto& tx = rig.fw.link().tx().stats();
        ASSERT_GT( tx.sent[(size_t)link::Lane::PadKnob] + tx.sent[(size_t)link::Lane::Aftertouch], 300u ) << "trial " << trial;
        ASSERT_GT( tx.dropped[(size_t)link::Lane::Aftertouch], 0u ) << "trial " << trial << ": the line was not saturated";

        bool found = false;
        for( const auto& f : rig.frames( wireStart ) )
            if( f.type == contract::PKT_BUTTON )
            {
                found = true;
                worst.decisionToWireUs = std::max( worst.decisionToWireUs, f.endUs - rig.decisionUs[(size_t)link::Lane::Button] );
                worst.contactToWireUs  = std::max( worst.contactToWireUs, f.endUs - contact );
                break;
            }
        ASSERT_TRUE( found ) << "trial " << trial << ": the button frame never reached the wire";
    }
    report( "button press, pad storm (worst of 60)", worst );
    EXPECT_LT( worst.decisionToWireUs, kBudgetUs );
}
