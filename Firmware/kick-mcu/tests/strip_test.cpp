// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include <memory>

#include "input_support.h"
#include "kick_mcu/input/strip.h"
#include "sim_board.h"

using namespace kick_mcu;
using namespace kick_mcu::input;
using namespace testing_support;

namespace
{
struct StripFixture : ::testing::Test
{
    StripConfig   config;
    RecordingSink sink;
    sim::SimStrip hw;
    uint32_t      now{ 0 };
    std::unique_ptr<StripScanner> strip;

    void SetUp() override { strip = std::make_unique<StripScanner>( config ); }
    void rebuild() { strip = std::make_unique<StripScanner>( config ); }

    void tick() { now += 250; strip->update( hw, now, sink ); }
    void ticks( int n ) { for( int i = 0; i < n; ++i ) tick(); }

    std::vector<uint8_t> values() const
    {
        std::vector<uint8_t> v;
        for( const auto& e : sink.ofType( contract::PKT_SLIDER ) )
            v.push_back( e.payload[0] );
        return v;
    }
};
} // namespace

TEST( StripMapping, TheBottomIsOneAndTheTopIsOneHundredNeverZero )
{
    EXPECT_EQ( 1, StripScanner::toPercent( 0 ) );
    EXPECT_EQ( 100, StripScanner::toPercent( 1023 ) );
    EXPECT_EQ( 100, StripScanner::toPercent( 5000 ) );
    for( unsigned p = 0; p < 1024; ++p )
    {
        EXPECT_GE( StripScanner::toPercent( (uint16_t)p ), 1 );
        EXPECT_LE( StripScanner::toPercent( (uint16_t)p ), 100 );
    }
}

TEST_F( StripFixture, TheFirstTouchIsSentImmediately )
{
    hw.touch( 512 );
    tick();
    ASSERT_EQ( 1u, values().size() );
    EXPECT_EQ( StripScanner::toPercent( 512 ), values()[0] );
    EXPECT_EQ( contract::SLIDER_TOUCH_INPUT, sink.events[0].cmd );
    EXPECT_EQ( link::Lane::PadKnob, sink.events[0].lane );
}

TEST_F( StripFixture, HoldingStillSendsNothingMore )
{
    hw.touch( 512 );
    ticks( 50 );
    EXPECT_EQ( 1u, values().size() );
}

TEST_F( StripFixture, ADragSendsEachChange )
{
    hw.touch( 100 );
    tick();
    hw.touch( 300 );
    tick();
    hw.touch( 900 );
    tick();
    ASSERT_EQ( 3u, values().size() );
    EXPECT_LT( values()[0], values()[1] );
    EXPECT_LT( values()[1], values()[2] );
}

TEST_F( StripFixture, TheDeadbandSwallowsSmallMoves )
{
    config.deadband = 5;
    rebuild();
    hw.touch( 500 );
    tick();
    const uint8_t first = values()[0];
    hw.touch( 520 ); // 2 percent
    tick();
    EXPECT_EQ( 1u, values().size() );
    hw.touch( 700 );
    tick();
    ASSERT_EQ( 2u, values().size() );
    EXPECT_GT( values()[1], first + 5 );
}

TEST_F( StripFixture, LiftSendsZeroOnceAfterTheDebounce )
{
    hw.touch( 500 );
    tick();
    hw.lift();
    ticks( 11 ); // 2750 us of the 3000 us debounce
    EXPECT_EQ( 1u, values().size() );
    tick();
    ASSERT_EQ( 2u, values().size() );
    EXPECT_EQ( 0, values()[1] );
    ticks( 50 );
    EXPECT_EQ( 2u, values().size() ) << "one lift only";
    EXPECT_FALSE( strip->touched() );
}

TEST_F( StripFixture, OneDroppedReadingInTheMiddleOfADragIsNotALift )
{
    hw.touch( 500 );
    tick();
    hw.lift();
    tick();
    hw.touch( 500 );
    ticks( 40 );
    EXPECT_EQ( 1u, values().size() );
}

TEST_F( StripFixture, TouchingTheSamePlaceAgainAfterALiftIsSentAgain )
{
    hw.touch( 500 );
    tick();
    hw.lift();
    ticks( 20 );
    ASSERT_EQ( 2u, values().size() );
    hw.touch( 500 );
    tick();
    ASSERT_EQ( 3u, values().size() );
    EXPECT_EQ( StripScanner::toPercent( 500 ), values()[2] );
}

TEST_F( StripFixture, ARefusedLiftIsRetriedUntilItGoes )
{
    hw.touch( 500 );
    tick();
    sink.refuse = true;
    hw.lift();
    ticks( 30 );
    EXPECT_EQ( 1u, values().size() );
    EXPECT_GT( strip->stats().retries, 0u );
    sink.refuse = false;
    tick();
    ASSERT_EQ( 2u, values().size() );
    EXPECT_EQ( 0, values()[1] );
}

TEST_F( StripFixture, ARefusedPositionIsRetriedOnTheNextTick )
{
    sink.refuse = true;
    hw.touch( 500 );
    ticks( 3 );
    EXPECT_TRUE( values().empty() );
    sink.refuse = false;
    tick();
    EXPECT_EQ( 1u, values().size() );
}

TEST_F( StripFixture, ATouchThatNeverReachedTheHostSendsNoLift )
{
    sink.accept = false;
    hw.touch( 500 );
    ticks( 4 );
    hw.lift();
    ticks( 20 );
    sink.accept = true;
    ticks( 4 );
    EXPECT_TRUE( values().empty() );
}

TEST_F( StripFixture, AFingerAlreadyDownWhenTheLinkComesUpIsSentAtOnce )
{
    sink.accept = false;
    hw.touch( 500 );
    ticks( 4 );
    sink.accept = true;
    tick();
    EXPECT_EQ( 1u, values().size() );
}
