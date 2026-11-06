// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include <memory>

#include "input_support.h"
#include "kick_mcu/input/pads.h"

using namespace kick_mcu;
using namespace kick_mcu::input;
using namespace testing_support;

namespace
{
struct PadFixture : ::testing::Test
{
    PadConfig     config;
    RecordingSink sink;
    uint16_t      pressure[16]{};
    uint32_t      now{ 0 };

    std::unique_ptr<PadScanner> scanner;

    void SetUp() override { rebuild(); }

    void rebuild() { scanner = std::make_unique<PadScanner>( config ); }

    // One tick of 250 us.
    void tick() { now += 250; scanner->update( pressure, now, sink ); }
    void ticks( int n ) { for( int i = 0; i < n; ++i ) tick(); }

    std::vector<RecordingSink::Event> pad() const { return sink.ofType( contract::PKT_PAD ); }
};
} // namespace

TEST_F( PadFixture, AStrikeReportsThePeakOfTheWindowAfterTheWindowEnds )
{
    // Rising over four ticks: the first reading crosses the threshold, the last is the peak.
    pressure[2] = 200;
    tick();
    pressure[2] = 1800;
    tick();
    pressure[2] = 3600; // full scale
    tick();
    pressure[2] = 2500;
    tick();
    EXPECT_TRUE( pad().empty() ) << "still inside the 1000 us window";
    tick();

    ASSERT_EQ( 1u, pad().size() );
    const auto frames = pad();
    const auto& e = frames[0];
    EXPECT_EQ( link::Lane::PadKnob, e.lane );
    EXPECT_EQ( contract::PAD_STATE, e.cmd );
    EXPECT_EQ( ( Bytes{ 2, 1, 127, 0 } ), e.payload );
}

TEST_F( PadFixture, VelocityIsScaledBetweenTheThresholdAndFullScaleThenCurved )
{
    // Peak halfway between on (150) and full (3600): strength 63.
    pressure[0] = (uint16_t)( 150 + ( 3600 - 150 ) / 2 );
    ticks( 6 );
    ASSERT_EQ( 1u, pad().size() );
    EXPECT_EQ( 63, pad()[0].payload[2] );

    sink.clear();
    pressure[0] = 0;
    ticks( 2 );
    sink.clear();

    scanner->curve().load( VelocityCurve::Preset::Hard );
    pressure[0] = (uint16_t)( 150 + ( 3600 - 150 ) / 2 );
    ticks( 6 );
    const auto hits = pad();
    ASSERT_FALSE( hits.empty() );
    EXPECT_LT( hits[0].payload[2], 63 ) << "the hard curve makes the same strike quieter";
}

TEST_F( PadFixture, AReadingJustBelowTheThresholdIsNotAStrike )
{
    pressure[5] = 149;
    ticks( 20 );
    EXPECT_TRUE( pad().empty() );
}

TEST_F( PadFixture, VelocityIsNeverZero )
{
    pressure[1] = 150; // exactly the threshold: strength 0
    ticks( 6 );
    ASSERT_EQ( 1u, pad().size() );
    EXPECT_EQ( 1, pad()[0].payload[2] );
}

TEST_F( PadFixture, ReleaseSendsPadUpOnce )
{
    pressure[3] = 2000;
    ticks( 6 );
    sink.clear();

    pressure[3] = 60; // below the off threshold (70)
    tick();
    ASSERT_EQ( 1u, pad().size() );
    EXPECT_EQ( ( Bytes{ 3, 0, 0, 0 } ), pad()[0].payload );
    tick();
    EXPECT_EQ( 1u, pad().size() );
}

TEST_F( PadFixture, HysteresisKeepsAPadDownBetweenTheTwoThresholds )
{
    pressure[3] = 2000;
    ticks( 6 );
    sink.clear();

    pressure[3] = 100; // under on (150) but over off (70)
    ticks( 10 );
    EXPECT_TRUE( pad().empty() );
    EXPECT_TRUE( scanner->isDown( 3 ) );
}

TEST_F( PadFixture, ATapShorterThanTheWindowStillReportsDownThenUp)
{
    pressure[4] = 2000;
    tick();
    pressure[4] = 0;
    tick();
    ASSERT_EQ( 2u, pad().size() );
    EXPECT_EQ( 1, pad()[0].payload[1] );
    EXPECT_EQ( 0, pad()[1].payload[1] );
    EXPECT_GT( pad()[0].payload[2], 1 ) << "carries the peak it saw";
}

TEST_F( PadFixture, PadsAreIndependentAndReportTheirOwnNumber )
{
    pressure[0]  = 1000;
    pressure[15] = 3000;
    ticks( 6 );
    ASSERT_EQ( 2u, pad().size() );
    EXPECT_EQ( 0, pad()[0].payload[0] );
    EXPECT_EQ( 15, pad()[1].payload[0] );
}

TEST_F( PadFixture, NothingIsSentWhileTheLinkIsDownAndNothingStaleFollows )
{
    sink.accept = false;
    pressure[6] = 2000;
    ticks( 6 );
    pressure[6] = 0;
    ticks( 2 );
    sink.accept = true;
    ticks( 4 );
    EXPECT_TRUE( pad().empty() ) << "a press and release that happened before the link came up are not replayed";
}

// ---- refusal and retry --------------------------------------------------------------------

TEST_F( PadFixture, ARefusedDownIsRetriedUntilItGoes )
{
    sink.refuse = true;
    pressure[7] = 2000;
    ticks( 10 );
    EXPECT_TRUE( pad().empty() );
    EXPECT_GT( scanner->stats().retries, 0u );

    sink.refuse = false;
    tick();
    ASSERT_EQ( 1u, pad().size() );
    EXPECT_EQ( 1, pad()[0].payload[1] );
}

TEST_F( PadFixture, ARefusedUpIsRetriedSoNoNoteIsLeftOn )
{
    pressure[7] = 2000;
    ticks( 6 );
    sink.clear();

    sink.refuse = true;
    pressure[7] = 0;
    ticks( 5 );
    EXPECT_TRUE( pad().empty() );
    sink.refuse = false;
    tick();
    ASSERT_EQ( 1u, pad().size() );
    EXPECT_EQ( 0, pad()[0].payload[1] );
}

TEST_F( PadFixture, APadReleasedBeforeItsDownWasQueuedSendsNeitherFrame )
{
    sink.refuse = true;
    pressure[8] = 2000;
    ticks( 6 );
    pressure[8] = 0;
    ticks( 2 );
    sink.refuse = false;
    ticks( 4 );
    EXPECT_TRUE( pad().empty() ) << "the host never saw the press, so a release would be an orphan";
    EXPECT_EQ( 1u, scanner->stats().discarded );
}

TEST_F( PadFixture, ANewStrikeWaitsForThePreviousFramesSoDownAndUpAlternate )
{
    pressure[9] = 2000;
    ticks( 6 );
    sink.clear();

    sink.refuse = true; // the up cannot go
    pressure[9] = 0;
    ticks( 2 );
    pressure[9] = 2500; // struck again
    ticks( 10 );
    EXPECT_TRUE( pad().empty() );

    sink.refuse = false;
    ticks( 8 );
    const auto frames = pad();
    ASSERT_GE( frames.size(), 2u );
    EXPECT_EQ( 0, frames[0].payload[1] ) << "the owed release comes first";
    EXPECT_EQ( 1, frames[1].payload[1] ) << "then the new strike";
}

// ---- aftertouch --------------------------------------------------------------------------

TEST_F( PadFixture, AftertouchIsOffByDefault )
{
    pressure[0] = 2000;
    ticks( 400 ); // 100 ms
    ASSERT_EQ( 1u, pad().size() );
    EXPECT_EQ( 0u, scanner->stats().aftertouch );
}

TEST_F( PadFixture, AftertouchWaitsForTheHoldoffThenFollowsPressureOnTheLowLane )
{
    config.aftertouchEnabled = true;
    rebuild();

    pressure[0] = 3000;
    ticks( 6 );
    pressure[0] = 1800; // settled
    sink.clear();
    ticks( 100 ); // 25 ms: inside the 30 ms holdoff
    EXPECT_TRUE( pad().empty() );

    ticks( 40 ); // up to 35 ms
    const auto frames = pad();
    ASSERT_GE( frames.size(), 1u );
    EXPECT_EQ( link::Lane::Aftertouch, frames[0].lane );
    EXPECT_EQ( contract::kPadStateAftertouch, frames[0].payload[1] );
    EXPECT_NE( 1, frames[0].payload[1] ) << "must not look like a note on";
    EXPECT_EQ( 0, frames[0].payload[0] );
    const int expected = (int)( ( 1800 - config.offThreshold ) * 127 / ( config.fullScale - config.offThreshold ) );
    EXPECT_EQ( expected, frames[0].payload[3] );
}

TEST_F( PadFixture, AftertouchIsRateLimitedPerPad )
{
    config.aftertouchEnabled = true;
    rebuild();

    pressure[0] = 3000;
    ticks( 6 );
    sink.clear();

    // Pressure changes every tick for one second; at 50 Hz that is about 50 frames, not 4000.
    uint16_t level = 500;
    for( int i = 0; i < 4000; ++i )
    {
        level = (uint16_t)( 500 + ( i % 40 ) * 60 );
        pressure[0] = level;
        tick();
    }
    const size_t n = pad().size();
    EXPECT_GE( n, 40u );
    EXPECT_LE( n, 51u );
}

TEST_F( PadFixture, AftertouchOnlySendsWhenTheValueMovesByTheDeadband )
{
    config.aftertouchEnabled = true;
    config.aftertouchDeadband = 5;
    rebuild();

    pressure[0] = 3000;
    ticks( 6 );
    pressure[0] = 2000;
    ticks( 200 ); // past the holdoff
    sink.clear();

    // A wiggle of about 2 units of the 0 to 127 scale: ignored.
    for( int i = 0; i < 400; ++i )
    {
        pressure[0] = (uint16_t)( 2000 + ( i % 2 ) * 50 );
        tick();
    }
    EXPECT_TRUE( pad().empty() );
}

TEST_F( PadFixture, ReleasingAPadPullsItsQueuedAftertouchOutOfTheQueue )
{
    config.aftertouchEnabled = true;
    rebuild();

    pressure[11] = 3000;
    ticks( 6 );
    sink.clear();
    pressure[11] = 0;
    tick();
    ASSERT_EQ( 1u, sink.discards.size() );
    EXPECT_EQ( link::Lane::Aftertouch, sink.discards[0].lane );
    EXPECT_EQ( contract::PKT_PAD, sink.discards[0].type );
    EXPECT_EQ( 11, sink.discards[0].firstByte );
}

TEST_F( PadFixture, TurningAftertouchOnAtRunTimeTakesEffectForPadsAlreadyHeld )
{
    pressure[0] = 3000;
    ticks( 6 );
    pressure[0] = 2000;
    ticks( 200 );
    sink.clear();

    scanner->setAftertouchEnabled( true );
    ticks( 100 );
    EXPECT_FALSE( pad().empty() );
}
