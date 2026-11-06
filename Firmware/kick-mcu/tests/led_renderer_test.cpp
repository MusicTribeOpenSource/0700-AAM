// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "kick_mcu/output/led_renderer.h"
#include "sim_clock.h"
#include "sim_leds.h"

using namespace kick_mcu;
using namespace kick_mcu::output;
using contract::STATE_OFF;
using contract::STATE_OFF_BLINK;
using contract::STATE_ON;
using contract::STATE_ON_BLINK;
using contract::STATE_SAMPLE_ASSIGNED;
using contract::STATE_SAMPLE_NOT_ASSIGNED;

namespace
{
struct Render : ::testing::Test
{
    sim::SimClock               clock;
    sim::SimLedDriver           driver;
    LedState                    state;
    BlinkClock                  blink{ clock };
    RenderConfig                config;
    std::unique_ptr<LedRefresh> refresh;
    GammaTable                  gamma;

    void SetUp() override { build(); }
    void build() { refresh = std::make_unique<LedRefresh>( driver, state, blink, clock, config ); }

    // Renders a frame at the given blink phase.
    RenderedFrame draw( const LedFrame& f, bool blinkOn = true )
    {
        RenderedFrame out;
        refresh->render( f, blinkOn, out );
        return out;
    }

    uint8_t g( unsigned v ) const { return gamma.apply( (uint8_t)v ); }
};
} // namespace

TEST_F( Render, ADarkTableIsDark )
{
    const auto out = draw( LedFrame{} );
    for( auto v : out.buttons )
        EXPECT_EQ( 0, v );
    for( auto& p : out.pads )
        EXPECT_EQ( 0, p[0] + p[1] + p[2] );
    for( auto v : out.strip )
        EXPECT_EQ( 0, v );
}

TEST_F( Render, AnOnPadIsItsColourThroughGamma )
{
    LedFrame f;
    f.pads[2]      = { STATE_ON, 12, contract::kPadColourTrans, 127 }; // Red ff2020
    const auto out = draw( f );
    EXPECT_EQ( g( 0xff ), out.pads[2][0] );
    EXPECT_EQ( g( 0x20 ), out.pads[2][1] );
    EXPECT_EQ( g( 0x20 ), out.pads[2][2] );
    EXPECT_LT( out.pads[2][1], 8 ) << "gamma pulls a dim channel well down";
    EXPECT_EQ( 0, out.pads[3][0] + out.pads[3][1] + out.pads[3][2] );
}

TEST_F( Render, TransparentAndOffAndUnknownStatesAreDark )
{
    LedFrame f;
    f.pads[0]      = { STATE_ON, contract::kPadColourTrans, contract::kPadColourTrans, 127 };
    f.pads[1]      = { STATE_OFF, 12, 12, 127 };
    f.pads[2]      = { 6, 12, 12, 127 };
    f.pads[3]      = { 200, 12, 12, 127 };
    const auto out = draw( f );
    for( int i = 0; i < 4; ++i )
        EXPECT_EQ( 0, out.pads[i][0] + out.pads[i][1] + out.pads[i][2] ) << i;
}

TEST_F( Render, SampleStatesAreDimmedBeforeGamma )
{
    LedFrame f;
    f.pads[0]      = { STATE_SAMPLE_ASSIGNED, 16, 15, 127 }; // white
    f.pads[1]      = { STATE_SAMPLE_NOT_ASSIGNED, 16, 15, 127 };
    const auto out = draw( f );
    EXPECT_EQ( g( 255 * 192 >> 8 ), out.pads[0][0] ); // 0.75
    EXPECT_EQ( g( 255 * 90 >> 8 ), out.pads[1][0] );  // about 0.35
    EXPECT_GT( out.pads[0][0], out.pads[1][0] );
    EXPECT_LT( out.pads[0][0], 255 );
}

TEST_F( Render, VelocityScalesTheColour )
{
    LedFrame f;
    f.pads[0]      = { STATE_ON, 16, 15, 127 };
    f.pads[1]      = { STATE_ON, 16, 15, 64 };
    f.pads[2]      = { STATE_ON, 16, 15, 0 };
    f.pads[3]      = { STATE_ON, 16, 15, 255 }; // past 127: clamped
    const auto out = draw( f );
    EXPECT_EQ( 255, out.pads[0][0] );
    EXPECT_EQ( g( 255 * ( 64 * 256 / 127 ) >> 8 ), out.pads[1][0] );
    EXPECT_NEAR( 128, 255 * ( 64 * 256 / 127 ) >> 8, 1 ) << "half velocity is half the colour value";
    EXPECT_EQ( 0, out.pads[2][0] );
    EXPECT_EQ( 255, out.pads[3][0] );
}

TEST_F( Render, BlinkingPadsSwapColoursWithThePhase )
{
    LedFrame f;
    f.pads[0]  = { STATE_ON_BLINK, 12, 7, 127 };                         // red, then aqua
    f.pads[1]  = { STATE_OFF_BLINK, 12, 7, 127 };                        // aqua, then red
    f.pads[2]  = { STATE_ON_BLINK, 16, contract::kPadColourTrans, 127 }; // white, then dark
    const auto on  = draw( f, true );
    const auto off = draw( f, false );

    EXPECT_EQ( 255, on.pads[0][0] );
    EXPECT_EQ( 0, off.pads[0][0] ) << "aqua has no red";
    EXPECT_EQ( 255, off.pads[0][1] );

    EXPECT_EQ( off.pads[0][0], on.pads[1][0] ) << "OFF_BLINK is the mirror of ON_BLINK";
    EXPECT_EQ( off.pads[0][1], on.pads[1][1] );
    EXPECT_EQ( on.pads[0][0], off.pads[1][0] );

    EXPECT_EQ( 255, on.pads[2][0] );
    EXPECT_EQ( 0, off.pads[2][0] ) << "blinking to no colour is blinking off";
}

TEST_F( Render, ButtonsLightForOnSampleAssignedAndTheRightBlinkHalf )
{
    LedFrame f;
    f.buttons[0].state = STATE_ON;
    f.buttons[1].state = STATE_SAMPLE_ASSIGNED;
    f.buttons[2].state = STATE_SAMPLE_NOT_ASSIGNED;
    f.buttons[3].state = STATE_ON_BLINK;
    f.buttons[4].state = STATE_OFF_BLINK;
    f.buttons[5].state = 99;

    auto on = draw( f, true );
    EXPECT_EQ( 255, on.buttons[0] );
    EXPECT_EQ( 255, on.buttons[1] );
    EXPECT_EQ( 0, on.buttons[2] ) << "the Surface leaves this state dark on a button";
    EXPECT_EQ( 255, on.buttons[3] );
    EXPECT_EQ( 0, on.buttons[4] );
    EXPECT_EQ( 0, on.buttons[5] );

    auto off = draw( f, false );
    EXPECT_EQ( 0, off.buttons[3] );
    EXPECT_EQ( 255, off.buttons[4] );
    EXPECT_EQ( 255, off.buttons[0] );
}

TEST_F( Render, TheStripFillsFromTheBottomLikeTheSurfaceDrawsIt )
{
    LedFrame f;
    for( int p = 0; p <= 100; ++p )
    {
        f.strip.percent = (uint8_t)p;
        const auto out  = draw( f );
        const int  want = (int)std::lround( p / 100.0 * 12.0 );
        int        n    = 0;
        for( unsigned i = 0; i < kStripLedCount; ++i )
        {
            n += out.strip[i] != 0;
            EXPECT_EQ( (int)i < want, out.strip[i] != 0 ) << p << " % LED " << i;
        }
        ASSERT_EQ( want, n ) << p << " %";
    }
}

TEST_F( Render, LevelModeDrawsTheSameAsBar )
{
    LedFrame f;
    f.strip.percent = 75;
    const auto bar  = draw( f );
    f.strip.mode    = StripMode::Level;
    EXPECT_EQ( bar, draw( f ) );
}

TEST_F( Render, IndividualStripLevelsSetEachLed )
{
    LedFrame f;
    f.strip.mode          = StripMode::Individual;
    f.strip.individual[2] = 100;
    f.strip.individual[7] = 50;
    const auto out        = draw( f );
    EXPECT_EQ( 255, out.strip[2] );
    EXPECT_EQ( 127, out.strip[7] );
    EXPECT_EQ( 0, out.strip[0] );
}

TEST_F( Render, MasterBrightnessScalesColoursBeforeGamma )
{
    config.masterQ8 = 128;
    build();
    LedFrame f;
    f.pads[0] = { STATE_ON, 16, 15, 127 };
    EXPECT_EQ( g( 127 ), draw( f ).pads[0][0] );
}

// ---- current cap ---------------------------------------------------------------------------

TEST_F( Render, AFullWhiteWallStaysUnderTheCap )
{
    LedFrame f;
    for( auto& p : f.pads )
        p = { STATE_ON, 16, 15, 127 };
    RenderedFrame out;
    RenderInfo    info;
    refresh->render( f, true, out, &info );

    EXPECT_EQ( 960u, info.demandMa );
    EXPECT_EQ( 160, info.scaleQ8 );
    EXPECT_LE( info.outputMa, 600u );
    EXPECT_EQ( 599u, info.outputMa ) << "255 * 160 >> 8 = 159 on 48 channels: 7632 * 20 / 255, rounded up";
    for( auto& p : out.pads )
        for( auto c : p )
            EXPECT_EQ( 159, c );
}

TEST_F( Render, EverythingOnStaysUnderTheCap )
{
    LedFrame f;
    for( auto& p : f.pads )
        p = { STATE_ON, 16, 15, 127 };
    for( auto& b : f.buttons )
        b.state = STATE_ON;
    f.strip.percent = 100;
    RenderedFrame out;
    RenderInfo    info;
    refresh->render( f, true, out, &info );
    EXPECT_EQ( 1285u, info.demandMa );
    EXPECT_EQ( 119, info.scaleQ8 );
    EXPECT_LE( info.outputMa, 600u );
}

TEST_F( Render, ALightLoadIsNotTouched )
{
    LedFrame f;
    f.pads[0]          = { STATE_ON, 16, 15, 127 };
    f.buttons[3].state = STATE_ON;
    RenderedFrame out;
    RenderInfo    info;
    refresh->render( f, true, out, &info );
    EXPECT_EQ( 256, info.scaleQ8 );
    EXPECT_EQ( 255, out.pads[0][0] );
    EXPECT_EQ( 255, out.buttons[3] );
}

TEST_F( Render, TheCapIsSettable )
{
    config.current.capMa = 10000;
    build();
    LedFrame f;
    for( auto& p : f.pads )
        p = { STATE_ON, 16, 15, 127 };
    RenderInfo    info;
    RenderedFrame out;
    refresh->render( f, true, out, &info );
    EXPECT_EQ( 256, info.scaleQ8 );
}

TEST_F( Render, RandomMixesNeverExceedTheCap )
{
    uint32_t seed = 99;
    auto     rnd  = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return seed >> 8;
    };
    for( int round = 0; round < 500; ++round )
    {
        LedFrame f;
        for( auto& p : f.pads )
            p = { (uint8_t)( rnd() % 6 ), (uint8_t)( rnd() % 20 ), (uint8_t)( rnd() % 20 ), (uint8_t)( rnd() % 140 ) };
        for( auto& b : f.buttons )
            b.state = (uint8_t)( rnd() % 7 );
        f.strip.percent = (uint8_t)( rnd() % 101 );
        RenderedFrame out;
        RenderInfo    info;
        refresh->render( f, ( rnd() & 1 ) != 0, out, &info );
        ASSERT_LE( info.outputMa, 600u ) << round;
    }
}

// ---- the refresh task ----------------------------------------------------------------------

TEST_F( Render, TheFirstServiceDrawsAndAnIdlePanelCausesNoMoreCommits )
{
    refresh->service();
    EXPECT_EQ( 1u, driver.commits() );
    for( int i = 0; i < 200; ++i )
    {
        clock.advanceMs( 10 );
        refresh->service();
    }
    EXPECT_EQ( 1u, driver.commits() ) << "nothing changed, so nothing was sent";
    EXPECT_EQ( 201u, refresh->stats().draws );
}

TEST_F( Render, AChangeReachesTheDriverWithinOneInterval )
{
    refresh->service();
    LedFrame& back = state.beginUpdate();
    back.pads[4]   = { STATE_ON, 7, 15, 127 }; // aqua
    state.commit();

    clock.advanceMs( 3 );
    refresh->service();
    EXPECT_EQ( 1u, driver.commits() ) << "inside the interval: not drawn yet";
    EXPECT_EQ( 0, driver.visible().pads[4][1] );

    clock.advanceMs( 7 );
    refresh->service();
    EXPECT_EQ( 2u, driver.commits() );
    EXPECT_EQ( 255, driver.visible().pads[4][1] );
    EXPECT_EQ( 255, driver.visible().pads[4][2] );
    EXPECT_EQ( 0, driver.visible().pads[4][0] );
    EXPECT_EQ( refresh->last().pads[4][1], driver.visible().pads[4][1] );
}

TEST_F( Render, AMultiPadUpdateNeverShowsHalfDone )
{
    refresh->service();
    LedFrame& back = state.beginUpdate();
    for( auto& p : back.pads )
        p = { STATE_ON, 16, 15, 127 };
    // The decoder is mid-update here. A refresh pass at this moment draws the old table.
    clock.advanceMs( 10 );
    refresh->service();
    for( auto& p : driver.visible().pads )
        EXPECT_EQ( 0, p[0] );
    state.commit();
    clock.advanceMs( 10 );
    refresh->service();
    int lit = 0;
    for( auto& p : driver.visible().pads )
        lit += p[0] != 0;
    EXPECT_EQ( 16, lit );
}

TEST_F( Render, BlinkingAlternatesAtTwoHertzAndAllBlinkersFlipOnTheSameDraw )
{
    LedFrame& back        = state.beginUpdate();
    back.pads[0]          = { STATE_ON_BLINK, 16, 15, 127 };
    back.pads[9]          = { STATE_ON_BLINK, 16, 15, 127 };
    back.buttons[8].state = STATE_ON_BLINK;
    state.commit();

    int  flips = 0;
    bool prev  = true;
    for( int ms = 0; ms < 2000; ms += 10 )
    {
        refresh->service();
        const bool a = driver.visible().pads[0][0] != 0;
        const bool b = driver.visible().pads[9][0] != 0;
        const bool c = driver.visible().buttons[8] != 0;
        ASSERT_EQ( a, b ) << ms;
        ASSERT_EQ( a, c ) << ms;
        if( a != prev )
            ++flips;
        prev = a;
        clock.advanceMs( 10 );
    }
    EXPECT_EQ( 7, flips ) << "2 s at 2 Hz is four cycles: on, then seven edges";
}

TEST_F( Render, DrawNowIgnoresTheInterval )
{
    refresh->service();
    state.beginUpdate().buttons[2].state = STATE_ON;
    state.commit();
    refresh->drawNow();
    EXPECT_EQ( 255, driver.visible().buttons[2] );
}
