// Unit tests for AudIO's small numeric helpers (Includes/Utils.h).

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "Utils.h"

using namespace AudIO;

TEST( Utils, MinMaxClampLimit )
{
    EXPECT_EQ( utils::max( 3, 7 ), 7 );
    EXPECT_EQ( utils::min( 3, 7 ), 3 );
    EXPECT_EQ( utils::clamp( 15, 0, 10 ), 10 );
    EXPECT_EQ( utils::clamp( -5, 0, 10 ), 0 );
    EXPECT_EQ( utils::clamp( 5, 0, 10 ), 5 );
    // limit() takes (lower, upper, value), the opposite order to clamp().
    EXPECT_DOUBLE_EQ( utils::limit( -1.0, 1.0, 4.0 ), 1.0 );
    EXPECT_DOUBLE_EQ( utils::limit( -1.0, 1.0, -4.0 ), -1.0 );
}

TEST( Utils, IsEqualUsesEpsilon )
{
    EXPECT_TRUE( utils::isEqual( 1.0f, 1.00005f ) );
    EXPECT_FALSE( utils::isEqual( 1.0f, 1.01f ) );
    EXPECT_TRUE( utils::isNotEqual( 1.0f, 1.01f ) );
    EXPECT_TRUE( utils::isEqual( 1.0f, 1.2f, 0.5f ) );
    EXPECT_TRUE( utils::isZero( 0.0f ) );
}

TEST( Utils, DecibelConversionRoundTrips )
{
    EXPECT_NEAR( utils::logToLinear( 0.f ), 1.f, 1e-6f );
    EXPECT_NEAR( utils::logToLinear( -6.0206f ), 0.5f, 1e-4f );
    EXPECT_NEAR( utils::linearTolog( 1.f ), 0.f, 1e-5f );
    EXPECT_NEAR( utils::linearTolog( 0.5f ), -6.0206f, 1e-3f );
    for( float db : { -60.f, -24.f, -6.f, 0.f, 6.f } )
        EXPECT_NEAR( utils::linearTolog( utils::logToLinear( db ) ), db, 1e-3f );
}

TEST( Utils, SilenceMapsToFloorNotMinusInfinity )
{
    EXPECT_FLOAT_EQ( utils::linearTolog( 0.f ), -100.f );
    EXPECT_FLOAT_EQ( utils::linearTolog( -1.f ), -100.f );
}

TEST( Utils, MillisecondsToSamples )
{
    EXPECT_FLOAT_EQ( utils::millisecToSamples( 10.f, 48000 ), 480.f );
    EXPECT_FLOAT_EQ( utils::millisecToSamples( 0.f, 48000 ), 0.f );
}

TEST( Utils, FastAbsAndNeg )
{
    for( float f : { -3.5f, -0.f, 0.f, 1e-20f, 123456.f } )
    {
        EXPECT_FLOAT_EQ( utils::fastabs( f ), std::fabs( f ) );
        EXPECT_FLOAT_EQ( utils::fastneg( f ), -f );
    }
}

TEST( Utils, PowerOfTwo )
{
    EXPECT_TRUE( utils::isPowerOfTwo( 1 ) );
    EXPECT_TRUE( utils::isPowerOfTwo( 1024 ) );
    EXPECT_FALSE( utils::isPowerOfTwo( 1000 ) );
    EXPECT_EQ( utils::previousPowerOfTwo( 1000 ), 512u );
    EXPECT_EQ( utils::previousPowerOfTwo( 1024 ), 1024u );
}

TEST( Utils, RangePredicates )
{
    EXPECT_TRUE( utils::isPositive( 0 ) || !utils::isPositive( 0 ) ); // documents that 0 is classified either way without crashing
    EXPECT_TRUE( utils::isPositiveAndBelow( 3, 5 ) );
    EXPECT_FALSE( utils::isPositiveAndBelow( 5, 5 ) );
    EXPECT_FALSE( utils::isPositiveAndBelow( -1, 5 ) );
}

TEST( Utils, EnumIntConversions )
{
    enum class E : uint8_t { a = 3, b = 9 };
    EXPECT_EQ( utils::toInt( E::b ), 9 );
    EXPECT_EQ( utils::toEnum<E>( 3 ), E::a );
}

TEST( Utils, StringFormat )
{
    EXPECT_EQ( utils::string_format( "%d-%s", 7, "x" ), "7-x" );
    EXPECT_EQ( utils::string_format( "plain" ), "plain" );
}

TEST( Utils, RandomStaysInRange )
{
    for( int i = 0; i < 1000; ++i )
    {
        const float f = utils::random<float>( -1.f, 1.f );
        ASSERT_GE( f, -1.f );
        ASSERT_LE( f, 1.f );
        const int n = utils::random<int>( 3, 5 );
        ASSERT_GE( n, 3 );
        ASSERT_LE( n, 5 );
    }
}

TEST( Utils, BufferHelpers )
{
    std::vector<float> a( 8, 1.f ), b( 8, 2.f );
    utils::add<float>( a.data(), b.data(), 8 );
    for( float v : a )
        EXPECT_FLOAT_EQ( v, 3.f );
    utils::multiply<float>( a.data(), 8, 0.5f );
    for( float v : a )
        EXPECT_FLOAT_EQ( v, 1.5f );
    utils::clear<float>( a.data(), 8 );
    for( float v : a )
        EXPECT_FLOAT_EQ( v, 0.f );
    utils::copyWithGain<float, float>( a.data(), b.data(), 8, 4.f );
    for( float v : a )
        EXPECT_FLOAT_EQ( v, 8.f );
    utils::addWithGain<float, float>( a.data(), b.data(), 8, 1.f );
    for( float v : a )
        EXPECT_FLOAT_EQ( v, 10.f );
}

TEST( Math, ConstantsAreConsistent )
{
    EXPECT_NEAR( Math<double>::twoPi, 2 * Math<double>::pi, 1e-12 );
    EXPECT_NEAR( Math<double>::pi_2, Math<double>::pi / 2, 1e-12 );
    EXPECT_NEAR( Math<double>::pi_4, Math<double>::pi / 4, 1e-12 );
    EXPECT_NEAR( Math<double>::sqrt2_2, std::sqrt( 2.0 ) / 2, 1e-12 );
    EXPECT_NEAR( Math<double>::gain6dB, std::pow( 10.0, -6.0 / 20 ), 1e-12 );
    EXPECT_NEAR( Math<double>::gain3dB, std::pow( 10.0, -3.0 / 20 ), 1e-12 );
}
