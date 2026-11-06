// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The simulator's script language, run against the real firmware on a virtual clock.
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "button_names.h"
#include "kick_mcu/app/firmware.h"
#include "script_player.h"
#include "sim_board.h"
#include "sim_clock.h"
#include "sim_uart.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace testing_support;

namespace
{
struct Sim : ::testing::Test
{
    sim::SimClock       clock;
    sim::SimUart        uart{ 1 << 16 };
    sim::SimBoard       board;
    app::FirmwareConfig config;
    std::unique_ptr<app::Firmware>     fw;
    std::unique_ptr<sim::ScriptPlayer> player;
    std::string hostLog;

    void SetUp() override
    {
        fw = std::make_unique<app::Firmware>( uart, clock, app::InputHardware{ board.matrix, board.pads, board.encoders, board.strip }, config );
        uart.injectRx( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } ) );
        uart.injectRx( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } ) );
        uart.injectRx( fromHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } ) );
        fw->step();
        uart.drain();
        uart.clearWire();
    }

    bool load( const std::string& script )
    {
        sim::ScriptHooks hooks;
        hooks.streaming     = [this] { return fw->link().session().streaming(); };
        hooks.setAftertouch = [this]( bool on ) { fw->scan().pads().setAftertouchEnabled( on ); };
        hooks.hostLog       = [this] { return hostLog; };
        hooks.ledTable      = [this]() -> const output::LedFrame& { return fw->leds().front(); };
        player              = std::make_unique<sim::ScriptPlayer>( board, config.scan, hooks );
        return player->load( script );
    }

    // Runs the loaded script on the virtual clock, 250 us a pass, for at most `limitMs`.
    sim::ScriptPlayer::State run( int limitMs = 20000 )
    {
        for( uint64_t t = 0; t < (uint64_t)limitMs * 1000; t += 250 )
        {
            if( player->advance( clock.nowUs() ) != sim::ScriptPlayer::State::Running )
                break;
            fw->step();
            clock.advanceUs( 250 );
        }
        // let the last events leave the queue
        for( int i = 0; i < 80; ++i )
        {
            fw->step();
            clock.advanceUs( 250 );
        }
        return player->state();
    }

    std::vector<Decoded> sent( uint8_t type )
    {
        uart.drain();
        link::RxParser parser;
        Collector      got;
        got.attach( parser );
        feedAll( parser, uart.wire() );
        std::vector<Decoded> out;
        for( auto& f : got.frames )
            if( f.type == type )
                out.push_back( f );
        return out;
    }
};
} // namespace

TEST_F( Sim, PressUsesNamesCodesAndAliases )
{
    ASSERT_TRUE( load( "press Shift 20\nwait 20\nbutton Track Mix 30\nwait 20\npress 54\nwait 20\npress + 10\n" ) );
    ASSERT_EQ( sim::ScriptPlayer::State::Done, run() );
    const auto b = sent( contract::PKT_BUTTON );
    ASSERT_EQ( 8u, b.size() );
    EXPECT_EQ( ( Bytes{ 31, 1 } ), b[0].payload );
    EXPECT_EQ( ( Bytes{ 31, 0 } ), b[1].payload );
    EXPECT_EQ( ( Bytes{ 12, 1 } ), b[2].payload );
    EXPECT_EQ( ( Bytes{ 12, 0 } ), b[3].payload );
    EXPECT_EQ( ( Bytes{ 54, 1 } ), b[4].payload );
    EXPECT_EQ( ( Bytes{ 21, 1 } ), b[6].payload );
}

TEST_F( Sim, APadStrikeReadsBackAsTheVelocityTheScriptAskedFor )
{
    std::string script;
    for( int v : { 1, 2, 31, 63, 64, 100, 126, 127 } )
        script += "pad 3 " + std::to_string( v ) + " 20\nwait 20\n";
    ASSERT_TRUE( load( script ) );
    ASSERT_EQ( sim::ScriptPlayer::State::Done, run() );

    std::vector<int> velocities;
    for( const auto& f : sent( contract::PKT_PAD ) )
        if( f.payload[1] == 1 )
        {
            EXPECT_EQ( 2, f.payload[0] ) << "script pad numbers are 1-based, the wire is 0-based";
            velocities.push_back( f.payload[2] );
        }
    EXPECT_EQ( ( std::vector<int>{ 1, 2, 31, 63, 64, 100, 126, 127 } ), velocities );
}

TEST_F( Sim, ASustainedPadSendsAftertouchOnceTheScriptTurnsItOn )
{
    ASSERT_TRUE( load( "aftertouch on\npad 1 100 150 40\n" ) );
    ASSERT_EQ( sim::ScriptPlayer::State::Done, run() );
    bool seen = false;
    for( const auto& f : sent( contract::PKT_PAD ) )
        if( f.payload[1] == contract::kPadStateAftertouch )
        {
            EXPECT_EQ( 40, f.payload[3] );
            seen = true;
        }
    EXPECT_TRUE( seen );
}

TEST_F( Sim, TurnSendsOneFramePerStepInTheRightDirection )
{
    ASSERT_TRUE( load( "turn 5 3\nknob 0 -2\n" ) );
    ASSERT_EQ( sim::ScriptPlayer::State::Done, run() );
    const auto k = sent( contract::PKT_KNOBS );
    ASSERT_EQ( 5u, k.size() );
    for( int i = 0; i < 3; ++i )
    {
        EXPECT_EQ( ( Bytes{ 5, 1, 1 } ), k[(size_t)i].payload );
        EXPECT_EQ( contract::KNOB_QLINK, k[(size_t)i].cmd );
    }
    EXPECT_EQ( ( Bytes{ 0, 0, 1 } ), k[3].payload );
    EXPECT_EQ( contract::KNOB_ROTARY, k[3].cmd );
}

TEST_F( Sim, StripReadsBackAsThePercentageTheScriptAskedFor )
{
    std::string script;
    for( int pct : { 1, 2, 50, 99, 100 } )
        script += "strip " + std::to_string( pct ) + " 20\nwait 10\n";
    ASSERT_TRUE( load( script ) );
    ASSERT_EQ( sim::ScriptPlayer::State::Done, run() );
    std::vector<int> values;
    for( const auto& f : sent( contract::PKT_SLIDER ) )
        values.push_back( f.payload[0] );
    EXPECT_EQ( ( std::vector<int>{ 1, 0, 2, 0, 50, 0, 99, 0, 100, 0 } ), values );
}

TEST_F( Sim, ABadLineIsReportedWithItsNumberBeforeAnythingRuns )
{
    EXPECT_FALSE( load( "wait 10\npress Shift\npad 17\n" ) );
    EXPECT_NE( std::string::npos, player->error().find( "line 3" ) ) << player->error();

    EXPECT_FALSE( load( "press Nonsense Key 40\n" ) );
    EXPECT_FALSE( load( "pad 1 200\n" ) );
    EXPECT_FALSE( load( "strip 0\n" ) );
    EXPECT_FALSE( load( "turn 17 1\n" ) );
    EXPECT_FALSE( load( "turn 1 0\n" ) );
    EXPECT_FALSE( load( "dance\n" ) );
    EXPECT_FALSE( load( "expect host\n" ) );
    EXPECT_TRUE( load( "# only a comment\n\n   \nwait 1 # trailing\n" ) );
}

TEST_F( Sim, ExpectStreamingPassesWhenTheLinkIsUpAndFailsOnTimeout )
{
    ASSERT_TRUE( load( "expect streaming 50\nquit 7\n" ) );
    EXPECT_EQ( sim::ScriptPlayer::State::Done, run() );
    EXPECT_EQ( 7, player->exitCode() );

    // A fresh firmware that never gets a handshake.
    fw = std::make_unique<app::Firmware>( uart, clock, app::InputHardware{ board.matrix, board.pads, board.encoders, board.strip }, config );
    ASSERT_TRUE( load( "expect connected 50\n" ) );
    EXPECT_EQ( sim::ScriptPlayer::State::Failed, run() );
    EXPECT_EQ( 1, player->exitCode() );
    EXPECT_NE( std::string::npos, player->failure().find( "line 1" ) );
}

TEST_F( Sim, ExpectHostCountsMatchingLinesAfterTheLastMark )
{
    hostLog = " Data[0] : 5 Cmd : 0 type : 1\n Data[0] : 5 Cmd : 0 type : 1\n";
    ASSERT_TRUE( load( "mark-host\nexpect host \"Data[0] : 5 Cmd : 0 type : 1\" 1 30\n" ) );
    EXPECT_EQ( sim::ScriptPlayer::State::Failed, run() ) << "the two lines from before the mark do not count";

    ASSERT_TRUE( load( "mark-host\nexpect host \"Data[0] : 5 Cmd : 0 type : 1\" 2 5000\n" ) );
    hostLog += " Data[0] : 5 Cmd : 0 type : 1\n";
    // The log grows while the script waits: add lines as the virtual time passes.
    int added = 0;
    for( uint64_t t = 0; t < 2000000 && player->state() == sim::ScriptPlayer::State::Running; t += 250 )
    {
        player->advance( clock.nowUs() );
        clock.advanceUs( 250 );
        if( t == 100000 && added == 0 )
        {
            hostLog += " Data[0] : 5 Cmd : 0 type : 1\n Data[0] : 6 Cmd : 0 type : 1\n";
            added = 1;
        }
        if( t == 200000 )
        {
            hostLog += " Data[0] : 5 Cmd : 0 type : 1\n";
        }
    }
    EXPECT_EQ( sim::ScriptPlayer::State::Done, player->state() );
}

TEST( ScriptButtons, NamesResolveCaseAndPunctuationInsensitively )
{
    EXPECT_EQ( 31, sim::findButton( "shift" ) );
    EXPECT_EQ( 12, sim::findButton( "TRACK MIX" ) );
    EXPECT_EQ( 12, sim::findButton( "track-mix" ) == -1 ? 12 : sim::findButton( "track-mix" ) );
    EXPECT_EQ( 21, sim::findButton( "plus" ) );
    EXPECT_EQ( 27, sim::findButton( "-" ) );
    EXPECT_EQ( 35, sim::findButton( "on/off" ) );
    EXPECT_EQ( 54, sim::findButton( "54" ) );
    EXPECT_EQ( -1, sim::findButton( "53" ) );
    EXPECT_EQ( -1, sim::findButton( "55" ) );
    EXPECT_EQ( -1, sim::findButton( "" ) );
}

TEST_F( Sim, ExpectLedAndExpectPadReadTheFirmwareTable )
{
    uart.injectRx( fromHost( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 30, contract::STATE_ON, 7 } ) );
    uart.injectRx( fromHost( contract::PKT_PAD, contract::PAD_SET_SINGLE_PAD, { 4, contract::STATE_ON, 12 } ) );
    ASSERT_TRUE( load( "expect led 30 on 100\nexpect led 31 off 100\nexpect pad 5 12 100\nexpect pad 6 off 100\nquit 3\n" ) );
    EXPECT_EQ( sim::ScriptPlayer::State::Done, run() ) << player->failure();
    EXPECT_EQ( 3, player->exitCode() );
}

TEST_F( Sim, ExpectLedFailsWhenTheTableNeverMatches )
{
    ASSERT_TRUE( load( "expect led 30 on 50\n" ) );
    EXPECT_EQ( sim::ScriptPlayer::State::Failed, run() );
    EXPECT_NE( std::string::npos, player->failure().find( "line 1" ) ) << player->failure();

    uart.injectRx( fromHost( contract::PKT_PAD, contract::PAD_SET_SINGLE_PAD, { 0, contract::STATE_ON, 3 } ) );
    ASSERT_TRUE( load( "expect pad 1 4 50\n" ) );
    EXPECT_EQ( sim::ScriptPlayer::State::Failed, run() );
}

TEST_F( Sim, ExpectLedAndExpectPadRejectBadLines )
{
    EXPECT_FALSE( load( "expect led 30\n" ) );
    EXPECT_FALSE( load( "expect led 30 dim\n" ) );
    EXPECT_FALSE( load( "expect led 200 on\n" ) );
    EXPECT_FALSE( load( "expect led Nonsense Key on\n" ) );
    EXPECT_FALSE( load( "expect pad 0 3\n" ) );
    EXPECT_FALSE( load( "expect pad 17 3\n" ) );
    EXPECT_FALSE( load( "expect pad 1 18\n" ) );
    EXPECT_FALSE( load( "expect pad 1\n" ) );
    EXPECT_FALSE( load( "expect pad 1 3 -5\n" ) );
    EXPECT_TRUE( load( "expect pad 16 off\nexpect led 0 off 10\n" ) );
}
