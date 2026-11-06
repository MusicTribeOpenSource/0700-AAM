// Unit tests for AudIO DSP primitives: pan laws, LFO, and the ADSR envelope.

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "Dsp/adsr.h"
#include "Dsp/lfo.h"
#include "Dsp/pan.h"

using namespace AudIO;

// ------------------------------------------------------------------- panner

namespace
{
    Panner<double> make( Panner<double>::Type t, double pan, double gain = 1.0 )
    {
        Panner<double>::Params p;
        p.pan  = pan;
        p.gain = gain;
        p.type = t;
        return Panner<double>( p );
    }
}

TEST( Panner, CentreIsSymmetricForEveryLaw )
{
    using T = Panner<double>::Type;
    for( auto t : { T::BALANCED, T::LINEAR, T::EQUAL_POWER, T::LOG_3DB, T::LOG4_5dB, T::LOG_6DB } )
    {
        auto p = make( t, 0.0 );
        EXPECT_NEAR( p.getLeft(), p.getRight(), 1e-9 ) << "law " << static_cast<int>( t );
        EXPECT_GT( p.getLeft(), 0.0 );
    }
}

TEST( Panner, BalancedKeepsUnityOnTheFavouredSide )
{
    auto centre = make( Panner<double>::Type::BALANCED, 0.0 );
    EXPECT_DOUBLE_EQ( centre.getLeft(), 1.0 );
    EXPECT_DOUBLE_EQ( centre.getRight(), 1.0 );

    auto left = make( Panner<double>::Type::BALANCED, -1.0 );
    EXPECT_DOUBLE_EQ( left.getLeft(), 1.0 );
    EXPECT_DOUBLE_EQ( left.getRight(), 0.0 );

    auto right = make( Panner<double>::Type::BALANCED, 1.0 );
    EXPECT_DOUBLE_EQ( right.getLeft(), 0.0 );
    EXPECT_DOUBLE_EQ( right.getRight(), 1.0 );
}

TEST( Panner, EqualPowerCentreIsMinus3dBAndPowerIsConstant )
{
    auto c = make( Panner<double>::Type::EQUAL_POWER, 0.0 );
    EXPECT_NEAR( c.getLeft(), std::sqrt( 0.5 ), 1e-9 );

    for( double pan = -1.0; pan <= 1.0; pan += 0.125 )
    {
        auto   p     = make( Panner<double>::Type::EQUAL_POWER, pan );
        double power = p.getLeft() * p.getLeft() + p.getRight() * p.getRight();
        EXPECT_NEAR( power, 1.0, 1e-9 ) << "pan " << pan;
    }
}

TEST( Panner, HardPanSilencesOppositeChannel )
{
    auto l = make( Panner<double>::Type::EQUAL_POWER, -1.0 );
    EXPECT_NEAR( l.getLeft(), 1.0, 1e-9 );
    EXPECT_NEAR( l.getRight(), 0.0, 1e-9 );
    auto r = make( Panner<double>::Type::EQUAL_POWER, 1.0 );
    EXPECT_NEAR( r.getLeft(), 0.0, 1e-9 );
    EXPECT_NEAR( r.getRight(), 1.0, 1e-9 );
}

TEST( Panner, LinearLawSumsToTwoUnits )
{
    for( double pan = -1.0; pan <= 1.0; pan += 0.25 )
    {
        auto p = make( Panner<double>::Type::LINEAR, pan );
        EXPECT_NEAR( p.getLeft() + p.getRight(), 2.0, 1e-9 ) << pan;
    }
}

TEST( Panner, GainScalesBothChannels )
{
    auto a = make( Panner<double>::Type::EQUAL_POWER, 0.3, 1.0 );
    auto b = make( Panner<double>::Type::EQUAL_POWER, 0.3, 0.5 );
    EXPECT_NEAR( b.getLeft(), a.getLeft() * 0.5, 1e-9 );
    EXPECT_NEAR( b.getRight(), a.getRight() * 0.5, 1e-9 );
}

TEST( Panner, PanIsClampedToUnitRange )
{
    auto p = make( Panner<double>::Type::BALANCED, 5.0 );
    EXPECT_DOUBLE_EQ( p.getPan(), 1.0 );
    p.setPan( -9.0 );
    EXPECT_DOUBLE_EQ( p.getPan(), -1.0 );
}

TEST( Panner, NegativeGainIsFlooredAtZero )
{
    auto p = make( Panner<double>::Type::BALANCED, 0.0, -2.0 );
    EXPECT_DOUBLE_EQ( p.getGain(), 0.0 );
    EXPECT_DOUBLE_EQ( p.getLeft(), 0.0 );
}

TEST( Panner, PanSamplesHelperAppliesPerChannelGains )
{
    auto [l, r] = AudIO::panSamples<float>( 1.f, 1.f, 0.25f, 0.75f );
    EXPECT_FLOAT_EQ( l, 0.25f );
    EXPECT_FLOAT_EQ( r, 0.75f );
}

TEST( Panner, ResetRestoresUnityCentre )
{
    auto p = make( Panner<double>::Type::EQUAL_POWER, 0.8, 0.2 );
    p.reset();
    EXPECT_DOUBLE_EQ( p.getPan(), 0.0 );
    EXPECT_DOUBLE_EQ( p.getGain(), 1.0 );
}

TEST( Panner, Linear45dBIsSymmetricAndMirrored )
{
    auto c = make( Panner<double>::Type::LINEAR_4_5DB, 0.0 );
    EXPECT_NEAR( c.getLeft(), 0.6025, 1e-9 );
    EXPECT_NEAR( c.getRight(), 0.6025, 1e-9 );
    for( double pan : { -1.0, -0.5, 0.25, 0.8 } )
    {
        auto a = make( Panner<double>::Type::LINEAR_4_5DB, pan );
        auto b = make( Panner<double>::Type::LINEAR_4_5DB, -pan );
        EXPECT_NEAR( a.getLeft(), b.getRight(), 1e-9 ) << pan;
        EXPECT_NEAR( a.getRight(), b.getLeft(), 1e-9 ) << pan;
    }
}

// ---------------------------------------------------------------------- LFO

namespace
{
    std::vector<float> render( LFO& lfo, int n )
    {
        std::vector<float> out( n );
        for( auto& s : out )
            s = lfo.process();
        return out;
    }

    int risingZeroCrossings( const std::vector<float>& v )
    {
        int n = 0;
        for( size_t i = 1; i < v.size(); ++i )
            if( v[i - 1] < 0.f && v[i] >= 0.f )
                ++n;
        return n;
    }
}

TEST( LFO, SineStaysWithinUnitRange )
{
    LFO lfo;
    lfo.setSampleRate( 48000 );
    lfo.setEnabled( true );
    lfo.setType( LFOType::SINE );
    lfo.setFrequency( 440.f );
    for( float s : render( lfo, 4800 ) )
    {
        ASSERT_LE( s, 1.001f );
        ASSERT_GE( s, -1.001f );
    }
}

TEST( LFO, SineFrequencyMatchesRequest )
{
    LFO lfo;
    lfo.setSampleRate( 48000 );
    lfo.setEnabled( true );
    lfo.setType( LFOType::SINE );
    lfo.setFrequency( 100.f );
    // One second of audio at 100 Hz has 100 cycles; allow one for start-up phase.
    EXPECT_NEAR( risingZeroCrossings( render( lfo, 48000 ) ), 100, 1 );
}

TEST( LFO, DoublingFrequencyDoublesCycles )
{
    auto cycles = []( float f ) {
        LFO lfo;
        lfo.setSampleRate( 48000 );
        lfo.setEnabled( true );
        lfo.setType( LFOType::SINE );
        lfo.setFrequency( f );
        return risingZeroCrossings( render( lfo, 48000 ) );
    };
    EXPECT_NEAR( cycles( 200.f ), 2 * cycles( 100.f ), 2 );
}

TEST( LFO, SquareOnlyTakesTwoLevels )
{
    LFO lfo;
    lfo.setSampleRate( 48000 );
    lfo.setEnabled( true );
    lfo.setType( LFOType::SQUARE );
    lfo.setFrequency( 50.f );
    bool sawHigh = false, sawLow = false;
    for( float s : render( lfo, 4800 ) )
    {
        if( s > 0.5f ) sawHigh = true;
        if( s < -0.5f ) sawLow = true;
    }
    EXPECT_TRUE( sawHigh );
    EXPECT_TRUE( sawLow );
}

TEST( LFO, AllShapesProduceFiniteOutput )
{
    for( auto t : { LFOType::SINE, LFOType::TRIANGLE, LFOType::SAW_UP, LFOType::SAW_DOWN, LFOType::SQUARE, LFOType::NOISE, LFOType::SAMPLE_HOLD } )
    {
        LFO lfo;
        lfo.setSampleRate( 48000 );
        lfo.setEnabled( true );
        lfo.setType( t );
        lfo.setFrequency( 7.f );
        for( float s : render( lfo, 2000 ) )
            ASSERT_TRUE( std::isfinite( s ) ) << "shape " << static_cast<int>( t );
    }
}

// --------------------------------------------------------------------- ADSR

namespace
{
    constexpr double kSr = 48000.0;

    // ADSR is non-copyable (IDSP holds atomics), so configure in place.
    void configure( ADSR<double>& env, double a, double d, double s, double r )
    {
        env.setSampleRate( kSr );
        env.setEnabled( true );
        env.setParameters( { a, 0.0, d, s, r } );
    }
}

TEST( ADSR, StartsIdle )
{
    ADSR<double> env( ADSR<double>::Type::LINEAR );
    EXPECT_EQ( env.getCurrentState(), ADSR<double>::State::IDLE );
    EXPECT_FALSE( env.isActive() );
}

TEST( ADSR, KeyOnEntersAttackAndRisesMonotonically )
{
    ADSR<double> env( ADSR<double>::Type::LINEAR );
    configure( env, 0.01, 0.01, 0.5, 0.01 );
    env.keyOn();
    EXPECT_EQ( env.getCurrentState(), ADSR<double>::State::ATTACK );
    double prev = 0.0;
    for( int i = 0; i < 400; ++i ) // 400 < 480 attack samples
    {
        double v = env.process();
        ASSERT_GE( v, prev ) << "sample " << i;
        prev = v;
    }
    EXPECT_LE( prev, 1.0001 );
}

TEST( ADSR, ExponentialAttackReachesFullScale )
{
    ADSR<double> env( ADSR<double>::Type::EXPONENTIAL );
    configure( env, 0.01, 0.01, 0.5, 0.01 );
    env.keyOn();
    double peak = 0;
    for( int i = 0; i < 480; ++i )
        peak = std::max( peak, env.process() );
    EXPECT_GT( peak, 0.8 );
    EXPECT_LE( peak, 1.0001 );
}

TEST( ADSR, LinearAttackReachesFullScale )
{
    ADSR<double> env( ADSR<double>::Type::LINEAR );
    configure( env, 0.01, 0.01, 0.5, 0.01 );
    env.keyOn();
    double peak = 0;
    for( int i = 0; i < 480; ++i )
        peak = std::max( peak, env.process() );
    EXPECT_GT( peak, 0.95 );
    EXPECT_LE( peak, 1.05 );
}

TEST( ADSR, ReachesSustainAndHoldsLevel )
{
    ADSR<double> env( ADSR<double>::Type::LINEAR );
    configure( env, 0.005, 0.005, 0.4, 0.01 );
    env.keyOn();
    double v = 0;
    for( int i = 0; i < 4800; ++i )
        v = env.process();
    EXPECT_EQ( env.getCurrentState(), ADSR<double>::State::SUSTAIN );
    EXPECT_NEAR( v, 0.4, 0.01 );
    for( int i = 0; i < 1000; ++i )
        ASSERT_NEAR( env.process(), 0.4, 0.01 ) << "sustain must hold";
}

TEST( ADSR, KeyOffReleasesToIdle )
{
    ADSR<double> env( ADSR<double>::Type::LINEAR );
    configure( env, 0.002, 0.002, 0.6, 0.01 );
    env.keyOn();
    for( int i = 0; i < 2400; ++i )
        env.process();
    ASSERT_EQ( env.getCurrentState(), ADSR<double>::State::SUSTAIN );

    env.keyOff();
    EXPECT_EQ( env.getCurrentState(), ADSR<double>::State::RELEASE );
    double prev = 1e9;
    for( int i = 0; i < 480; ++i )
    {
        double v = env.process();
        ASSERT_LE( v, prev + 1e-9 ) << "release must not rise";
        prev = v;
    }
    for( int i = 0; i < 480; ++i )
        env.process();
    EXPECT_EQ( env.getCurrentState(), ADSR<double>::State::IDLE );
}

TEST( ADSR, ResetReturnsToIdle )
{
    ADSR<double> env( ADSR<double>::Type::EXPONENTIAL );
    configure( env, 0.01, 0.01, 0.5, 0.01 );
    env.keyOn();
    env.process();
    env.reset();
    EXPECT_EQ( env.getCurrentState(), ADSR<double>::State::IDLE );
}

TEST( ADSR, DisabledEnvelopeIgnoresKeyOn )
{
    ADSR<double> env( ADSR<double>::Type::LINEAR );
    env.setSampleRate( kSr );
    env.keyOn();
    EXPECT_EQ( env.getCurrentState(), ADSR<double>::State::IDLE );
}

TEST( ADSR, ExponentialAndLinearBothStayBounded )
{
    for( auto type : { ADSR<double>::Type::LINEAR, ADSR<double>::Type::EXPONENTIAL } )
    {
        ADSR<double> env( type );
    configure( env, 0.005, 0.02, 0.3, 0.02 );
        env.keyOn();
        for( int i = 0; i < 20000; ++i )
        {
            const double v = env.process();
            ASSERT_TRUE( std::isfinite( v ) );
            ASSERT_GE( v, 0.0 );
            ASSERT_LE( v, 1.01 );
            if( i == 10000 )
                env.keyOff();
        }
    }
}
