// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include "input_support.h"
#include "kick_mcu/input/encoders.h"
#include "sim_board.h"

using namespace kick_mcu;
using namespace kick_mcu::input;
using namespace testing_support;

namespace
{
struct EncFixture : ::testing::Test
{
    EncoderBank      bank;
    RecordingSink    sink;
    sim::SimEncoders hw;

    void SetUp() override { bank.update( hw.readPhases(), sink ); } // records the starting positions

    void scan() { bank.update( hw.readPhases(), sink ); }

    // Moves an encoder `transitions` times, scanning after each.
    void turn( unsigned encoder, bool clockwise, int transitions )
    {
        for( int i = 0; i < transitions; ++i )
        {
            hw.step( encoder, clockwise );
            scan();
        }
    }

    std::vector<RecordingSink::Event> knobs() const { return sink.ofType( contract::PKT_KNOBS ); }
};
} // namespace

TEST_F( EncFixture, FourTransitionsAreOneStepClockwise )
{
    turn( 3, true, 3 );
    EXPECT_TRUE( knobs().empty() ) << "three of four transitions";
    turn( 3, true, 1 );
    ASSERT_EQ( 1u, knobs().size() );
    EXPECT_EQ( ( Bytes{ 3, 1, 1 } ), knobs()[0].payload );
    EXPECT_EQ( contract::KNOB_QLINK, knobs()[0].cmd );
    EXPECT_EQ( link::Lane::PadKnob, knobs()[0].lane );
}

TEST_F( EncFixture, AnticlockwiseHasDirectionZero )
{
    turn( 5, false, 8 );
    ASSERT_EQ( 2u, knobs().size() );
    EXPECT_EQ( ( Bytes{ 5, 0, 1 } ), knobs()[0].payload );
}

TEST_F( EncFixture, TheJogWheelUsesTheRotaryCommandAndKnobZero )
{
    turn( 0, true, 4 );
    ASSERT_EQ( 1u, knobs().size() );
    EXPECT_EQ( contract::KNOB_ROTARY, knobs()[0].cmd );
    EXPECT_EQ( 0, knobs()[0].payload[0] );
}

TEST_F( EncFixture, TheTopKnobIsSixteen )
{
    turn( 16, true, 4 );
    ASSERT_EQ( 1u, knobs().size() );
    EXPECT_EQ( 16, knobs()[0].payload[0] );
    EXPECT_EQ( contract::KNOB_QLINK, knobs()[0].cmd );
}

TEST_F( EncFixture, ABounceThatGoesForwardAndBackCountsForNothing )
{
    turn( 2, true, 1 );
    turn( 2, false, 1 );
    turn( 2, true, 1 );
    turn( 2, false, 1 );
    EXPECT_TRUE( knobs().empty() );
    turn( 2, true, 4 );
    ASSERT_EQ( 1u, knobs().size() ) << "and the position is still right afterwards";
}

TEST_F( EncFixture, EncodersDoNotDisturbEachOther )
{
    for( int i = 0; i < 4; ++i )
    {
        hw.step( 1, true );
        hw.step( 2, false );
        scan();
    }
    ASSERT_EQ( 2u, knobs().size() );
    EXPECT_EQ( ( Bytes{ 1, 1, 1 } ), knobs()[0].payload );
    EXPECT_EQ( ( Bytes{ 2, 0, 1 } ), knobs()[1].payload );
}

TEST_F( EncFixture, AMissedTransitionIsCountedAndTreatedAsTwoInTheLastDirection )
{
    turn( 4, true, 2 );
    // Two transitions between scans: the decoder sees a two-position jump.
    hw.step( 4, true );
    hw.step( 4, true );
    scan();
    EXPECT_EQ( 1u, bank.stats().skipped );
    ASSERT_EQ( 1u, knobs().size() ) << "2 + 2 = four transitions";
    EXPECT_EQ( 1, knobs()[0].payload[1] );
}

TEST_F( EncFixture, HalfCycleEncodersStepEveryTwoTransitions )
{
    EncoderConfig cfg;
    cfg.transitionsPerStep = 2;
    EncoderBank half( cfg );
    half.update( hw.readPhases(), sink );
    for( int i = 0; i < 4; ++i )
    {
        hw.step( 7, true );
        half.update( hw.readPhases(), sink );
    }
    EXPECT_EQ( 2u, knobs().size() );
}

// ---- coalescing -------------------------------------------------------------------------

TEST_F( EncFixture, StepsFromOneTickAreOneFrame )
{
    bank.addSteps( 9, 3, sink );
    bank.addSteps( 9, 2, sink );
    EXPECT_TRUE( knobs().empty() ) << "nothing goes out until the tick ends";
    bank.flush( sink );
    ASSERT_EQ( 1u, knobs().size() );
    EXPECT_EQ( ( Bytes{ 9, 1, 5 } ), knobs()[0].payload );
}

TEST_F( EncFixture, AReversalFlushesTheEarlierDirectionFirstAndInOrder )
{
    bank.addSteps( 9, 3, sink );
    bank.addSteps( 9, -2, sink );
    bank.flush( sink );
    ASSERT_EQ( 2u, knobs().size() );
    EXPECT_EQ( ( Bytes{ 9, 1, 3 } ), knobs()[0].payload );
    EXPECT_EQ( ( Bytes{ 9, 0, 2 } ), knobs()[1].payload );
}

TEST_F( EncFixture, AReversalThatCannotFlushIsNettedSoThePositionStaysRight )
{
    bank.addSteps( 9, 3, sink );
    sink.refuse = true;
    bank.addSteps( 9, -2, sink );
    sink.refuse = false;
    EXPECT_EQ( 1u, bank.stats().reversalNets );
    bank.flush( sink );
    ASSERT_EQ( 1u, knobs().size() );
    EXPECT_EQ( ( Bytes{ 9, 1, 1 } ), knobs()[0].payload );
}

TEST_F( EncFixture, StepsTheSinkRefusedJoinTheNextTicksAndGoOutTogether )
{
    sink.refuse = true;
    bank.addSteps( 6, 2, sink );
    bank.flush( sink );
    EXPECT_EQ( 2, bank.pending( 6 ) );
    EXPECT_EQ( 1u, bank.stats().held );

    sink.refuse = false;
    bank.addSteps( 6, 3, sink );
    bank.flush( sink );
    ASSERT_EQ( 1u, knobs().size() );
    EXPECT_EQ( ( Bytes{ 6, 1, 5 } ), knobs()[0].payload );
    EXPECT_EQ( 0, bank.pending( 6 ) );
}

TEST_F( EncFixture, MoreThanOneHundredTwentySevenStepsSplitIntoFrames )
{
    bank.addSteps( 1, 300, sink );
    bank.flush( sink );
    ASSERT_EQ( 3u, knobs().size() );
    EXPECT_EQ( 127, knobs()[0].payload[2] );
    EXPECT_EQ( 127, knobs()[1].payload[2] );
    EXPECT_EQ( 46, knobs()[2].payload[2] );
}

TEST_F( EncFixture, DifferentEncodersGetTheirOwnFramesInOneTick )
{
    bank.addSteps( 1, 1, sink );
    bank.addSteps( 0, -4, sink );
    bank.flush( sink );
    ASSERT_EQ( 2u, knobs().size() );
    EXPECT_EQ( ( Bytes{ 0, 0, 4 } ), knobs()[0].payload ) << "lowest encoder number first";
}

TEST_F( EncFixture, NothingBuildsUpWhileTheLinkIsDown )
{
    sink.accept = false;
    bank.addSteps( 2, 40, sink );
    bank.flush( sink );
    EXPECT_EQ( 0, bank.pending( 2 ) );
    sink.accept = true;
    bank.flush( sink );
    EXPECT_TRUE( knobs().empty() );
}

TEST_F( EncFixture, AnOutOfRangeEncoderIsIgnored )
{
    bank.addSteps( 17, 1, sink );
    bank.flush( sink );
    EXPECT_TRUE( knobs().empty() );
}
