// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The LED path end to end: host frames in through the UART, driver levels out.
#include <gtest/gtest.h>

#include <memory>

#include "kick_mcu/app/firmware.h"
#include "sim_board.h"
#include "sim_clock.h"
#include "sim_leds.h"
#include "sim_uart.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace testing_support;

namespace
{
struct LedRig : ::testing::Test
{
    sim::SimClock     clock;
    sim::SimUart      uart{ 4096 };
    sim::SimBoard     board;
    sim::SimLedDriver driver;
    std::unique_ptr<app::Firmware> fw;

    void SetUp() override
    {
        fw = std::make_unique<app::Firmware>( uart, clock, app::InputHardware{ board.matrix, board.pads, board.encoders, board.strip }, app::OutputHardware{ driver } );
        host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
        host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } );
        host( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } );
        step( 1 );
        ASSERT_TRUE( fw->link().session().streaming() );
    }

    void host( uint8_t type, uint8_t cmd, const Bytes& payload ) { uart.injectRx( fromHost( type, cmd, payload ) ); }

    // n passes of the main loop, 250 us apart, with the UART drained so the FIFO never fills.
    void step( int n )
    {
        for( int i = 0; i < n; ++i )
        {
            fw->step();
            clock.advanceUs( 250 );
            uart.drain();
            uart.clearWire();
        }
    }

    // Long enough for the refresh to draw once more.
    void settle() { step( 60 ); }
};
} // namespace

TEST_F( LedRig, TheFirstDrawIsAllDark )
{
    settle();
    EXPECT_GE( driver.commits(), 1u );
    for( auto v : driver.visible().buttons )
        EXPECT_EQ( 0, v );
}

TEST_F( LedRig, AButtonLedCommandLightsTheButton )
{
    host( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 13, 1, 0 } ); // Pad Mix
    settle();
    EXPECT_EQ( 255, driver.visible().buttons[13] );
    host( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 13, 0, 0 } );
    settle();
    EXPECT_EQ( 0, driver.visible().buttons[13] );
}

TEST_F( LedRig, BanksBehaveAsARadioGroupOnTheDriver )
{
    host( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 1, 1, 1 } );
    host( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 5, 1, 1 } );
    settle();
    int lit = 0;
    for( unsigned i = 0; i < 8; ++i )
        lit += driver.visible().buttons[i] != 0;
    EXPECT_EQ( 1, lit );
    EXPECT_NE( 0, driver.visible().buttons[5] );
}

TEST_F( LedRig, ClearThenColourThePads )
{
    host( contract::PKT_PAD, contract::PAD_SET_ALL_SINGLE_COLOUR, { 0, contract::kPadColourTrans } ); // clearAllPadsMessage
    Bytes multi{ 1 };
    for( uint8_t i = 0; i < 16; ++i )
        multi.push_back( 16 ); // white
    host( contract::PKT_PAD, contract::PAD_SET_ALL_MULTI_COLOUR, multi );
    settle();
    for( auto& p : driver.visible().pads )
        EXPECT_GT( p[0], 100 );
    host( contract::PKT_PAD, contract::PAD_SET_ALL_SINGLE_COLOUR, { 0, contract::kPadColourTrans } );
    settle();
    for( auto& p : driver.visible().pads )
        EXPECT_EQ( 0, p[0] + p[1] + p[2] );
}

TEST_F( LedRig, AWhitePadWallIsHeldUnderTheCurrentCap )
{
    host( contract::PKT_PAD, contract::PAD_SET_ALL_SINGLE_COLOUR, { 1, 16 } );
    settle();
    EXPECT_EQ( 160, fw->refresh().stats().scaleQ8 );
    EXPECT_LE( fw->refresh().stats().outputMa, 600u );
    EXPECT_GE( fw->refresh().stats().limited, 1u );
}

TEST_F( LedRig, TheSliderShowsOnTheStripLeds )
{
    host( contract::PKT_SLIDER, contract::SLIDER_BAR_LED, { 50 } );
    settle();
    int lit = 0;
    for( auto v : driver.visible().strip )
        lit += v != 0;
    EXPECT_EQ( 6, lit );
}

TEST_F( LedRig, MalformedAndUnknownCommandsAreCountedAndHarmless )
{
    host( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 1 } );
    host( contract::PKT_LED, 77, {} );
    host( contract::PKT_PAD, contract::PAD_SET_SINGLE_PAD, { 99, 1, 1 } );
    host( contract::PKT_SLIDER, contract::SLIDER_TOUCH_INPUT, { 10 } );
    settle();
    const auto& s = fw->commands().stats();
    EXPECT_EQ( 1u, s.malformed );
    EXPECT_EQ( 2u, s.unknown );
    EXPECT_EQ( 1u, s.outOfRange );
    for( auto v : driver.visible().buttons )
        EXPECT_EQ( 0, v );
    EXPECT_EQ( 0u, fw->hostFramesIgnored() ) << "rejected output commands are counted by the decoder, not as ignored";
}

TEST_F( LedRig, AftertouchEnableStillReachesTheInputLayer )
{
    host( contract::PKT_PAD, contract::PAD_SET_AFTER_TOUCH, { 1 } );
    settle();
    EXPECT_TRUE( fw->scan().pads().aftertouchEnabled() );
    EXPECT_EQ( 0u, fw->hostFramesIgnored() );
}

TEST_F( LedRig, CommandsNobodyHandlesAreStillCounted )
{
    // BPM, shutdown, update and version belong to the system layer now; RESET_GD has no handler.
    host( contract::PKT_SYSTEM, contract::SYS_RESET_GD, {} );
    settle();
    EXPECT_EQ( 1u, fw->hostFramesIgnored() );
}

TEST_F( LedRig, LedsKeepTheirStateWhenTheHostReconnects )
{
    host( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 13, 1, 0 } );
    settle();
    host( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
    settle();
    EXPECT_EQ( 255, driver.visible().buttons[13] );
}

TEST_F( LedRig, ABlinkingPadFlipsEvery250ms )
{
    host( contract::PKT_PAD, contract::PAD_SET_BLINKING, { 0, 4, 16, 15 } );
    std::vector<int> samples;
    for( int ms = 0; ms < 1000; ms += 25 )
    {
        step( 100 ); // 25 ms
        samples.push_back( driver.visible().pads[0][0] != 0 );
    }
    int edges = 0;
    for( size_t i = 1; i < samples.size(); ++i )
        edges += samples[i] != samples[i - 1];
    EXPECT_GE( edges, 3 );
    EXPECT_LE( edges, 4 );
}

TEST( LedsFirmware, AFirmwareBuiltWithoutLedsStillTracksTheHost )
{
    sim::SimClock clock;
    sim::SimUart  uart( 4096 );
    sim::SimBoard board;
    app::Firmware fw( uart, clock, app::InputHardware{ board.matrix, board.pads, board.encoders, board.strip } );
    uart.injectRx( fromHost( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 3, 1, 0 } ) );
    for( int i = 0; i < 60; ++i )
    {
        fw.step();
        clock.advanceUs( 250 );
    }
    EXPECT_EQ( 1, fw.leds().front().buttons[3].state );
}
