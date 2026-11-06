// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Replays emu/tests/traces/smoke.trace, captured from the unmodified Kick host talking to the
// emulator Surface (KICK_EMU_TRACE, see emu/README.md), through the firmware.
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "kick_mcu/link/link.h"
#include "sim_clock.h"
#include "sim_uart.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace kick_mcu::link;
using namespace testing_support;

namespace
{
struct TraceLine
{
    double      ms{ 0 };
    std::string tag; // TX, RX, DEC
    Bytes       bytes;      // TX, RX
    Decoded     decoded{};  // DEC
};

Bytes parseHex( std::istringstream& in )
{
    Bytes       out;
    std::string tok;
    while( in >> tok )
        out.push_back( (uint8_t)std::stoul( tok, nullptr, 16 ) );
    return out;
}

std::vector<TraceLine> loadTrace( const std::string& path )
{
    std::ifstream          file( path );
    std::vector<TraceLine> lines;
    std::string            text;
    while( std::getline( file, text ) )
    {
        if( text.empty() || text[0] == '#' )
            continue;
        std::istringstream in( text );
        TraceLine          line;
        in >> line.ms >> line.tag;
        if( line.tag == "DEC" )
        {
            // DEC t<type> d<dir> c<cmd> [<byte> ...]
            std::string t, d, c;
            in >> t >> d >> c;
            line.decoded.type = (uint8_t)std::stoi( t.substr( 1 ) );
            line.decoded.dir  = (uint8_t)std::stoi( d.substr( 1 ) );
            line.decoded.cmd  = (uint8_t)std::stoi( c.substr( 1 ) );
            std::string rest;
            std::getline( in, rest );
            for( char& ch : rest )
                if( ch == '[' || ch == ']' )
                    ch = ' ';
            std::istringstream values( rest );
            int                v;
            while( values >> v )
                line.decoded.payload.push_back( (uint8_t)v );
        }
        else
            line.bytes = parseHex( in );
        lines.push_back( std::move( line ) );
    }
    return lines;
}

const std::string kSmokeTrace = std::string( KICK_TRACE_DIR ) + "/smoke.trace";
} // namespace

TEST( TraceReplay, TheTraceIsPresentAndHasBothDirections )
{
    const auto lines = loadTrace( kSmokeTrace );
    ASSERT_FALSE( lines.empty() ) << kSmokeTrace;
    size_t tx = 0, rx = 0, dec = 0;
    for( const auto& l : lines )
        ( l.tag == "TX" ? tx : l.tag == "RX" ? rx : dec )++;
    EXPECT_GT( tx, 0u );
    EXPECT_GT( rx, 0u );
    EXPECT_GT( dec, 0u );
}

TEST( TraceReplay, ParserProducesTheSameFramesTheSurfaceDecoded )
{
    const auto lines = loadTrace( kSmokeTrace );

    std::vector<Decoded> expected;
    Bytes                hostToMcu;
    for( const auto& l : lines )
    {
        if( l.tag == "DEC" )
            expected.push_back( l.decoded );
        else if( l.tag == "RX" )
            hostToMcu.insert( hostToMcu.end(), l.bytes.begin(), l.bytes.end() );
    }
    ASSERT_FALSE( expected.empty() );

    RxParser  parser;
    Collector got;
    got.attach( parser );
    feedAll( parser, hostToMcu );

    ASSERT_EQ( expected.size(), got.frames.size() );
    for( size_t i = 0; i < expected.size(); ++i )
        EXPECT_EQ( expected[i], got.frames[i] ) << "frame " << i;
    EXPECT_EQ( 0u, parser.stats().crcErrors );
    EXPECT_EQ( 0u, parser.stats().garbageBytes );
    EXPECT_EQ( 0u, parser.stats().resyncs );
}

TEST( TraceReplay, EveryFrameTheSurfaceSentReEncodesToTheSameBytes )
{
    const auto lines = loadTrace( kSmokeTrace );
    size_t     checked = 0;
    for( const auto& l : lines )
    {
        if( l.tag != "TX" )
            continue;

        RxParser  parser;
        Collector got;
        got.attach( parser );
        feedAll( parser, l.bytes );
        ASSERT_EQ( 1u, got.frames.size() ) << "TX line at " << l.ms;

        const Decoded& d = got.frames[0];
        EXPECT_EQ( l.bytes, encode( Wire::McuToHost, d.type, d.cmd, d.payload, d.dir ) ) << "TX line at " << l.ms;
        ++checked;
    }
    EXPECT_GT( checked, 0u );
}

TEST( TraceReplay, FirmwareAnswersTheHandshakeByteForByteLikeTheSurface )
{
    const auto lines = loadTrace( kSmokeTrace );

    sim::SimUart  uart( 256 );
    sim::SimClock clock;
    Link          link( uart, clock ); // default version 1.0, as the Surface reports

    const double t0 = lines.front().ms;
    for( const auto& l : lines )
    {
        if( l.tag != "RX" )
            continue;

        // Step time forward in 1 ms ticks so the heartbeat has the chance to fire between lines.
        const double target = l.ms - t0;
        while( (double)clock.nowMs() < target )
        {
            clock.advanceMs( 1 );
            link.poll();
            uart.drain();
        }
        uart.injectRx( l.bytes );
        link.poll();
        uart.drain();
    }

    // Everything the firmware put on the wire, split into frames.
    RxParser  parser;
    Collector firmware;
    firmware.attach( parser );
    feedAll( parser, uart.wire() );

    // The Surface's SYSTEM frames, in order. Its pad, button and slider frames come from the
    // script, not from the handshake, so they have no counterpart here.
    std::vector<Bytes> surfaceSystem;
    for( const auto& l : lines )
        if( l.tag == "TX" && l.bytes.size() > 2 && l.bytes[2] == contract::PKT_SYSTEM )
            surfaceSystem.push_back( l.bytes );
    ASSERT_GE( surfaceSystem.size(), 4u );

    // The handshake: version, status [1], status [2]. Byte for byte, including the trailer.
    ASSERT_GE( firmware.frames.size(), 3u );
    for( size_t i = 0; i < 3; ++i )
    {
        const Decoded& d = firmware.frames[i];
        EXPECT_EQ( surfaceSystem[i], encode( Wire::McuToHost, d.type, d.cmd, d.payload, d.dir ) ) << "handshake frame " << i;
    }
    EXPECT_TRUE( link.session().streaming() );

    // After that only heartbeats, every one the same bytes the Surface sends.
    for( size_t i = 3; i < firmware.frames.size(); ++i )
        EXPECT_EQ( surfaceSystem.back(), encode( Wire::McuToHost, firmware.frames[i].type, firmware.frames[i].cmd, firmware.frames[i].payload, firmware.frames[i].dir ) ) << "frame " << i;
}
