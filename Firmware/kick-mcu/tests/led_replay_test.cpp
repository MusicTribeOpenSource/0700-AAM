// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Replays emu/tests/traces/leds.trace through the firmware and compares the final LED table with
// emu/tests/traces/leds.surface.json, the table the emulator Surface produced for the same trace.
// With KICK_SURFACE_BIN set to the Surface binary the test also replays the trace through the
// Surface itself and compares live; without it that half is skipped.
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "led_json.h"
#include "trace_replay.h"

namespace
{
std::string slurp( const std::string& path )
{
    std::ifstream     in( path, std::ios::binary );
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string trimEnd( std::string s )
{
    while( !s.empty() && ( s.back() == '\n' || s.back() == '\r' ) )
        s.pop_back();
    return s;
}

std::string firmwareTable( const std::string& tracePath )
{
    kick_mcu::sim::TraceReplayer replayer;
    EXPECT_TRUE( replayer.run( slurp( tracePath ) ) ) << replayer.error();
    return trimEnd( kick_mcu::sim::ledTableJson( replayer.table() ) );
}
} // namespace

TEST( LedReplay, FirmwareMatchesSurfaceGolden )
{
    const std::string dir    = KICK_TRACE_DIR;
    const std::string golden = trimEnd( slurp( dir + "/leds.surface.json" ) );
    ASSERT_FALSE( golden.empty() ) << "missing leds.surface.json";
    EXPECT_EQ( firmwareTable( dir + "/leds.trace" ), golden );
}

TEST( LedReplay, LiveSurfaceMatchesFirmware )
{
    const char* bin = std::getenv( "KICK_SURFACE_BIN" );
    if( !bin )
        GTEST_SKIP() << "KICK_SURFACE_BIN not set";
    const std::string dir = KICK_TRACE_DIR;
    const std::string out = ::testing::TempDir() + "leds.live.json";
    const std::string cmd = std::string( "\"" ) + bin + "\" --replay \"" + dir + "/leds.trace\" --dump-leds \"" + out + "\" >/dev/null 2>&1";
    ASSERT_EQ( std::system( cmd.c_str() ), 0 );
    EXPECT_EQ( trimEnd( slurp( out ) ), firmwareTable( dir + "/leds.trace" ) );
}
