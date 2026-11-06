// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include "kick_mcu/input/velocity_curve.h"

using namespace kick_mcu::input;

TEST( VelocityCurve, LinearIsTheIdentityExceptThatZeroBecomesOne )
{
    VelocityCurve c( VelocityCurve::Preset::Linear );
    EXPECT_EQ( 1, c.map( 0 ) ) << "velocity 0 would be a note-off";
    for( unsigned i = 1; i < 128; ++i )
        EXPECT_EQ( i, c.map( i ) );
}

TEST( VelocityCurve, EveryPresetStaysInOneToOneTwentySevenAndNeverFalls )
{
    for( auto preset : { VelocityCurve::Preset::Linear, VelocityCurve::Preset::Soft, VelocityCurve::Preset::Hard } )
    {
        VelocityCurve c( preset );
        for( unsigned i = 0; i < 128; ++i )
        {
            EXPECT_GE( c.map( i ), 1 );
            EXPECT_LE( c.map( i ), 127 );
            if( i > 0 )
                EXPECT_GE( c.map( i ), c.map( i - 1 ) ) << "at " << i;
        }
        EXPECT_EQ( 127, c.map( 127 ) ) << "the top of the range is reachable";
    }
}

TEST( VelocityCurve, SoftIsLouderAndHardIsQuieterThanLinearInTheMiddle )
{
    VelocityCurve linear( VelocityCurve::Preset::Linear ), soft( VelocityCurve::Preset::Soft ), hard( VelocityCurve::Preset::Hard );
    for( unsigned i : { 20u, 40u, 64u, 90u } )
    {
        EXPECT_GT( soft.map( i ), linear.map( i ) ) << i;
        EXPECT_LT( hard.map( i ), linear.map( i ) ) << i;
    }
}

TEST( VelocityCurve, ATableCanBeReplacedAndIsClampedIntoRange )
{
    VelocityCurve c;
    uint8_t       table[128];
    for( int i = 0; i < 128; ++i )
        table[i] = (uint8_t)( i * 2 ); // 0 .. 254: both ends out of range
    ASSERT_TRUE( c.set( table, 128 ) );
    EXPECT_EQ( 1, c.map( 0 ) );
    EXPECT_EQ( 100, c.map( 50 ) );
    EXPECT_EQ( 127, c.map( 127 ) );
}

TEST( VelocityCurve, AShortTableRepeatsItsLastEntry )
{
    VelocityCurve c;
    const uint8_t table[3] = { 10, 20, 30 };
    ASSERT_TRUE( c.set( table, 3 ) );
    EXPECT_EQ( 20, c.map( 1 ) );
    EXPECT_EQ( 30, c.map( 2 ) );
    EXPECT_EQ( 30, c.map( 100 ) );
}

TEST( VelocityCurve, ANullTableIsRefusedAndTheCurveKeepsItsShape )
{
    VelocityCurve c( VelocityCurve::Preset::Hard );
    const uint8_t before = c.map( 64 );
    EXPECT_FALSE( c.set( nullptr, 128 ) );
    EXPECT_EQ( before, c.map( 64 ) );
}

TEST( VelocityCurve, AnOutOfRangeInputIsTreatedAsTheTop )
{
    VelocityCurve c;
    EXPECT_EQ( 127, c.map( 5000 ) );
}
