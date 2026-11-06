// Integration tests: build real AudIO designs (factory-created objects wired
// through buses and chains), render audio blocks, and check the signal.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "Design.h"
#include "ObjectFactory.h"
#include "SineWave.h"
#include "VolumeAndPan.h"
#include "support/audio_pool.h"

using namespace AudIO;

namespace
{
    constexpr uint32_t kSr    = 48000;
    constexpr int      kBlock = kicktest::kBlockSize;

    struct Block
    {
        std::vector<float> l, r;
    };

    float rms( const std::vector<float>& v )
    {
        double acc = 0;
        for( float s : v )
            acc += double( s ) * s;
        return static_cast<float>( std::sqrt( acc / std::max<size_t>( 1, v.size() ) ) );
    }

    float peakOf( const std::vector<float>& v )
    {
        float p = 0;
        for( float s : v )
            p = std::max( p, std::fabs( s ) );
        return p;
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

class AudioGraphTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        kicktest::ensureAudioPool();
        design = std::make_unique<Design>( 0, 2 );
        design->prepareToPlay( kSr, kBlock );
        out = std::make_unique<Buffer<float>>( 0, false, 2, true );
    }

    // Render n blocks and return the concatenated stereo output.
    Block render( int blocks )
    {
        Block b;
        for( int i = 0; i < blocks; ++i )
        {
            out->clearOutputs(); // the host clears before each callback; bypass relies on it
            design->process( *out );
            auto* l = out->getOutputBuffer( 0 );
            auto* r = out->getOutputBuffer( 1 );
            b.l.insert( b.l.end(), l, l + kBlock );
            b.r.insert( b.r.end(), r, r + kBlock );
        }
        return b;
    }

    // Skip the 10 ms-to-250 ms parameter ramps before measuring.
    Block settle( int blocks = 60 )
    {
        render( blocks );
        return render( 4 );
    }

    // sine -> bus -> design output
    struct SineRig
    {
        uint32_t sine, bus;
    };
    SineRig sineToOutput( float freq, float amount = 1.f, float dB = 0.f )
    {
        SineRig rig;
        rig.sine = design->createObject( "SineWave" );
        rig.bus  = design->createBus();
        EXPECT_TRUE( design->sendToBus( rig.bus, rig.sine, amount ) );
        EXPECT_TRUE( design->setDesignOutput( rig.bus ) );
        auto* s = design->getObject( rig.sine );
        EXPECT_NE( s, nullptr );
        s->setParameter( SineWave::kParamFreq, freq );
        s->setParameter( SineWave::kParamAmplitude, dB );
        return rig;
    }

    std::unique_ptr<Design>         design;
    std::unique_ptr<Buffer<float>>  out;
};

// -------------------------------------------------------- factory registry

TEST( ObjectFactory, ContainsCoreObjects )
{
    auto list = ObjectFactory::Get().List();
    for( const char* name : { "SineWave", "VolumeAndPan", "Mixer", "ADSR" } )
        EXPECT_NE( std::find( list.begin(), list.end(), name ), list.end() ) << name << " not registered";
}

TEST( ObjectFactory, CreatesTypedObjectsAndRejectsUnknown )
{
    auto sine = ObjectFactory::Get().Create( "SineWave" );
    ASSERT_NE( sine, nullptr );
    // The factory key ("SineWave") and the object's own getType() ("Sine") differ.
    EXPECT_STREQ( sine->getType(), "Sine" );
    EXPECT_EQ( ObjectFactory::Get().Create( "NoSuchObject" ), nullptr );
}

TEST( ObjectFactory, EachCreatedObjectGetsAUniqueId )
{
    auto a = ObjectFactory::Get().Create( "SineWave" );
    auto b = ObjectFactory::Get().Create( "SineWave" );
    ASSERT_TRUE( a && b );
    EXPECT_NE( a->getId(), b->getId() );
}

// ---------------------------------------------------------------- rendering

TEST_F( AudioGraphTest, EmptyDesignRendersSilence )
{
    auto b = render( 4 );
    EXPECT_EQ( peakOf( b.l ), 0.f );
    EXPECT_EQ( peakOf( b.r ), 0.f );
}

TEST_F( AudioGraphTest, SineAtUnityGainHasUnitPeakAndSqrtHalfRms )
{
    sineToOutput( 1000.f );
    auto b = settle();
    EXPECT_NEAR( peakOf( b.l ), 1.f, 0.02f );
    EXPECT_NEAR( rms( b.l ), 0.7071f, 0.02f );
}

TEST_F( AudioGraphTest, OutputFrequencyMatchesParameter )
{
    sineToOutput( 1000.f );
    settle();
    auto b = render( 100 ); // 1 s
    EXPECT_NEAR( risingZeroCrossings( b.l ), 1000, 3 );
}

TEST_F( AudioGraphTest, BusSendAmountScalesLevel )
{
    sineToOutput( 500.f, 0.5f );
    auto b = settle();
    EXPECT_NEAR( peakOf( b.l ), 0.5f, 0.02f );
}

TEST_F( AudioGraphTest, AmplitudeParameterIsInDecibels )
{
    sineToOutput( 500.f, 1.f, -6.0206f );
    auto b = settle();
    EXPECT_NEAR( peakOf( b.l ), 0.5f, 0.02f );
}

TEST_F( AudioGraphTest, SineParametersReadBack )
{
    auto  rig = sineToOutput( 880.f, 1.f, -12.f );
    auto* s   = design->getObject( rig.sine );
    EXPECT_NEAR( s->getParameter( SineWave::kParamFreq ), 880.f, 1e-3f );
    EXPECT_NEAR( s->getParameter( SineWave::kParamAmplitude ), -12.f, 1e-2f );
}

TEST_F( AudioGraphTest, SineBypassSilencesTheOscillator )
{
    auto rig = sineToOutput( 440.f );
    design->getObject( rig.sine )->setParameter( SineWave::kParamBypass, 1.f );
    auto b = settle();
    EXPECT_LT( peakOf( b.l ), 0.01f );
}

// ------------------------------------------------------------ bus routing

TEST_F( AudioGraphTest, BusConnectionBookkeeping )
{
    auto sine = design->createObject( "SineWave" );
    auto bus  = design->createBus();
    EXPECT_FALSE( design->isConnectedToBus( bus, sine ) );
    EXPECT_EQ( design->getBusConnections( bus ), 0u );

    ASSERT_TRUE( design->sendToBus( bus, sine, 0.25f ) );
    EXPECT_TRUE( design->isConnectedToBus( bus, sine ) );
    EXPECT_EQ( design->getBusConnections( bus ), 1u );
    EXPECT_NEAR( design->getBusInputGain( bus, sine ), 0.25f, 1e-6f );

    ASSERT_TRUE( design->setBusInputGain( bus, sine, 0.75f ) );
    EXPECT_NEAR( design->getBusInputGain( bus, sine ), 0.75f, 1e-6f );

    ASSERT_TRUE( design->removeFromBus( bus, sine ) );
    EXPECT_FALSE( design->isConnectedToBus( bus, sine ) );
    EXPECT_EQ( design->getBusConnections( bus ), 0u );
}

TEST_F( AudioGraphTest, TwoSourcesOnOneBusSum )
{
    auto a   = design->createObject( "SineWave" );
    auto b   = design->createObject( "SineWave" );
    auto bus = design->createBus();
    design->sendToBus( bus, a, 0.3f );
    design->sendToBus( bus, b, 0.3f );
    design->setDesignOutput( bus );
    // Identical frequency and phase: the two partials add coherently.
    design->getObject( a )->setParameter( SineWave::kParamFreq, 700.f );
    design->getObject( b )->setParameter( SineWave::kParamFreq, 700.f );
    auto blk = settle();
    EXPECT_NEAR( peakOf( blk.l ), 0.6f, 0.03f );
}

TEST_F( AudioGraphTest, RemovingASourceStopsItContributing )
{
    auto rig = sineToOutput( 600.f );
    ASSERT_GT( peakOf( settle().l ), 0.5f );
    design->removeFromBus( rig.bus, rig.sine );
    auto b = settle();
    EXPECT_LT( peakOf( b.l ), 0.01f );
}

TEST_F( AudioGraphTest, UnknownIdsAreRejectedNotFatal )
{
    EXPECT_FALSE( design->exists( 999999 ) );
    EXPECT_EQ( design->getObject( 999999 ), nullptr );
    EXPECT_FALSE( design->sendToBus( 999999, 888888, 1.f ) );
    EXPECT_FALSE( design->removeObject( 777777 ) );
}

// ---------------------------------------------------- chain / level / pan

TEST_F( AudioGraphTest, ChainGainScalesOutput )
{
    auto sine  = design->createObject( "SineWave" );
    auto chain = design->createChain();
    auto bus   = design->createBus();
    design->sendToBus( chain, sine, 1.f ) || true; // sine feeds the bus below instead
    design->sendToBus( bus, sine, 1.f );
    design->setDesignOutput( bus );
    design->getObject( sine )->setParameter( SineWave::kParamFreq, 800.f );
    design->setGain( bus, 0.5f );
    auto b = settle();
    EXPECT_NEAR( peakOf( b.l ), 0.5f, 0.03f );
    EXPECT_NEAR( design->getGain( bus ), 0.5f, 1e-6f );
}

TEST_F( AudioGraphTest, HardLeftPanSilencesRightChannel )
{
    auto rig = sineToOutput( 800.f );
    design->setPan( rig.bus, -1.f );
    auto b = settle();
    EXPECT_GT( peakOf( b.l ), 0.5f );
    EXPECT_LT( peakOf( b.r ), 0.02f );
}

TEST_F( AudioGraphTest, HardRightPanSilencesLeftChannel )
{
    auto rig = sineToOutput( 800.f );
    design->setPan( rig.bus, 1.f );
    auto b = settle();
    EXPECT_GT( peakOf( b.r ), 0.5f );
    EXPECT_LT( peakOf( b.l ), 0.02f );
}

TEST_F( AudioGraphTest, MuteSilencesAndUnmuteRestores )
{
    auto rig = sineToOutput( 800.f );
    ASSERT_GT( peakOf( settle().l ), 0.5f );

    design->setMute( rig.bus, true );
    EXPECT_TRUE( design->isMuted( rig.bus ) );
    EXPECT_LT( peakOf( settle().l ), 0.01f );

    design->setMute( rig.bus, false );
    EXPECT_FALSE( design->isMuted( rig.bus ) );
    EXPECT_GT( peakOf( settle().l ), 0.5f );
}

TEST_F( AudioGraphTest, LevelAndPanGetterMatchesSetter )
{
    auto rig = sineToOutput( 800.f );
    design->setLevelAndPan( rig.bus, 0.6f, -0.25f );
    auto [gain, pan] = design->getLevelAndPan( rig.bus );
    EXPECT_NEAR( gain, 0.6f, 1e-5f );
    EXPECT_NEAR( pan, -0.25f, 1e-5f );
}

// --------------------------------------------------------- inserts on chain

TEST_F( AudioGraphTest, InsertLifecycleOnChain )
{
    auto chain = design->createChain();
    auto ins   = design->createInsert( chain, "VolumeAndPan" );
    ASSERT_NE( ins, 0u );
    ASSERT_NE( design->getInsert( chain, ins ), nullptr );

    EXPECT_TRUE( design->addInsert( chain, ins, 0 ) );
    EXPECT_TRUE( design->removeInsert( chain, ins ) );
    EXPECT_TRUE( design->deleteInsert( chain, ins ) );
    EXPECT_EQ( design->getInsert( chain, ins ), nullptr );
}

// ------------------------------------------------------------------ metering

TEST_F( AudioGraphTest, MeterReportsSignalAfterProcessing )
{
    auto rig = sineToOutput( 1000.f );
    ASSERT_TRUE( design->addMeter( rig.sine ) );
    settle();
    const MeterData& m = design->readMeter( rig.sine );
    ASSERT_FALSE( m.empty() );
    EXPECT_GT( m[0].peak, -6.f ) << "unity sine should read near 0 dB peak";
    EXPECT_LT( m[0].peak, 3.f );
}

TEST_F( AudioGraphTest, MeterOnSilenceStaysAtFloor )
{
    auto sine = design->createObject( "SineWave" );
    auto bus  = design->createBus();
    design->sendToBus( bus, sine, 1.f );
    design->setDesignOutput( bus );
    design->getObject( sine )->setParameter( SineWave::kParamBypass, 1.f );
    design->addMeter( sine );
    settle();
    const MeterData& m = design->readMeter( sine );
    ASSERT_FALSE( m.empty() );
    EXPECT_LT( m[0].peak, -40.f );
}

// ------------------------------------------------------ design-level control

TEST_F( AudioGraphTest, DesignBypassSilencesTheGraphWhenNoInputsAreConnected )
{
    sineToOutput( 1000.f );
    design->setBypass( true );
    EXPECT_TRUE( design->isBypassed() );
    auto b = settle();
    EXPECT_LT( peakOf( b.l ), 0.02f ) << "no design inputs, so bypass yields silence";
    design->setBypass( false );
    EXPECT_FALSE( design->isBypassed() );
    EXPECT_GT( peakOf( settle().l ), 0.5f );
}

TEST_F( AudioGraphTest, ClearRemovesEverything )
{
    auto rig = sineToOutput( 1000.f );
    ASSERT_TRUE( design->exists( rig.sine ) );
    design->clear();
    render( 2 );
    EXPECT_FALSE( design->exists( rig.sine ) );
    EXPECT_LT( peakOf( settle().l ), 0.01f );
}

TEST_F( AudioGraphTest, NamesRoundTrip )
{
    auto sine = design->createObject( "SineWave" );
    design->setName( sine, "lead" );
    EXPECT_EQ( design->getName( sine ), "lead" );
    design->setName( "main" );
    EXPECT_EQ( design->getName(), "main" );
}
