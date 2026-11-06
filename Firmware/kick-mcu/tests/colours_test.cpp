// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include <cmath>

#include "kick_mcu/output/colours.h"

using namespace kick_mcu;
using namespace kick_mcu::output;

TEST( ColourTable, HasAnEntryForEveryHostColourIdAndTransIsDark )
{
    ColourTable t;
    for( unsigned id = 0; id < contract::kPadColourCount; ++id )
    {
        const Rgb c = t.lookup( (uint8_t)id );
        if( id == contract::kPadColourTrans )
            EXPECT_EQ( 0, c.r + c.g + c.b );
        else
            EXPECT_GT( c.r + c.g + c.b, 0 ) << "colour " << id << " would render dark";
    }
    EXPECT_TRUE( t.isTransparent( contract::kPadColourTrans ) );
    EXPECT_FALSE( t.isTransparent( 0 ) );
}

TEST( ColourTable, AnchorsMatchTheNames )
{
    ColourTable t;
    EXPECT_EQ( 0x8b, t.lookup( 0 ).r ) << "DarkRed";
    EXPECT_EQ( 0, t.lookup( 0 ).g );
    EXPECT_EQ( 255, t.lookup( 7 ).g ) << "Aqua";
    EXPECT_EQ( 255, t.lookup( 7 ).b );
    EXPECT_EQ( 0, t.lookup( 7 ).r );
    EXPECT_EQ( 255, t.lookup( 12 ).r ) << "Red";
    EXPECT_EQ( 255, t.lookup( 16 ).r ) << "NeutralWhite";
    EXPECT_EQ( 255, t.lookup( 16 ).g );
    EXPECT_EQ( 255, t.lookup( 16 ).b );
    EXPECT_EQ( 128, t.lookup( 17 ).r ) << "NeutralGrey";
}

TEST( ColourTable, AnIdPastTheEndIsClampedToTheLastEntry )
{
    ColourTable t;
    EXPECT_EQ( t.lookup( 17 ).r, t.lookup( 18 ).r );
    EXPECT_EQ( t.lookup( 17 ).g, t.lookup( 255 ).g );
}

TEST( ColourTable, EntriesCanBeReplacedExceptTransAndOutOfRange )
{
    ColourTable t;
    EXPECT_TRUE( t.set( 3, { 1, 2, 3 } ) );
    EXPECT_EQ( 2, t.lookup( 3 ).g );
    EXPECT_FALSE( t.set( contract::kPadColourTrans, { 9, 9, 9 } ) );
    EXPECT_FALSE( t.set( 18, { 9, 9, 9 } ) );
    EXPECT_EQ( 0, t.lookup( contract::kPadColourTrans ).r );
}

TEST( GammaTable, EndpointsAreFixedAndTheCurveIsMonotonic )
{
    GammaTable g;
    EXPECT_EQ( 0, g.apply( 0 ) );
    EXPECT_EQ( 255, g.apply( 255 ) );
    for( unsigned i = 1; i < 256; ++i )
        EXPECT_GE( g.apply( (uint8_t)i ), g.apply( (uint8_t)( i - 1 ) ) ) << i;
}

TEST( GammaTable, DefaultIs2Point2AndMidGreyIsAboutAFifth )
{
    GammaTable g;
    EXPECT_FLOAT_EQ( 2.2f, g.gamma() );
    // 255 * 0.5 ^ 2.2 = 55.5
    EXPECT_EQ( 56, g.apply( 128 ) ) << "128 / 255 is just over a half: 255 * 0.502 ^ 2.2 rounds to 56";
    EXPECT_NEAR( 255.0 * std::pow( 64.0 / 255.0, 2.2 ), g.apply( 64 ), 0.51 );
}

TEST( GammaTable, ExponentOneIsTheIdentity )
{
    GammaTable g( 1.0f );
    for( unsigned i = 0; i < 256; ++i )
        EXPECT_EQ( i, g.apply( (uint8_t)i ) );
}

TEST( GammaTable, ItIsAdjustableAndRefusesNonsense )
{
    GammaTable g;
    EXPECT_TRUE( g.setGamma( 2.8f ) );
    EXPECT_LT( g.apply( 128 ), GammaTable( 2.2f ).apply( 128 ) );
    EXPECT_FALSE( g.setGamma( 0.5f ) );
    EXPECT_FALSE( g.setGamma( 9.0f ) );
    EXPECT_FALSE( g.setGamma( std::nanf( "" ) ) );
    EXPECT_FLOAT_EQ( 2.8f, g.gamma() ) << "a refused value changes nothing";
}

TEST( GammaTable, ConstructorFallsBackToDefaultOnABadExponent )
{
    GammaTable g( 0.5f );
    EXPECT_FLOAT_EQ( 2.2f, g.gamma() );
    EXPECT_EQ( GammaTable( 2.2f ).apply( 128 ), g.apply( 128 ) );
    EXPECT_EQ( 255, g.apply( 255 ) );
}
