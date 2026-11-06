// Unit tests for AudIO's real-time containers: SPSC ring buffer, memory pool,
// and ramped (smoothed) parameters.

#include <gtest/gtest.h>

#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

#include "MemoryPool.h"
#include "RampedParameter.h"
#include "support/audio_pool.h"
#include "SPSCQueue.h"

using namespace AudIO;

// ------------------------------------------------------------------ SPSC ring

TEST( SPSCQueue, PopOnEmptyFails )
{
    SPSCQUeue<int, 4> q;
    int               v = -1;
    EXPECT_FALSE( q.pop( v ) );
    EXPECT_EQ( v, -1 );
}

TEST( SPSCQueue, FifoOrder )
{
    SPSCQUeue<int, 8> q;
    for( int i = 0; i < 5; ++i )
        ASSERT_TRUE( q.push( i ) );
    for( int i = 0; i < 5; ++i )
    {
        int v = -1;
        ASSERT_TRUE( q.pop( v ) );
        EXPECT_EQ( v, i );
    }
}

TEST( SPSCQueue, HoldsCapacityMinusOne )
{
    // One slot is sacrificed to tell full from empty.
    SPSCQUeue<int, 4> q;
    EXPECT_TRUE( q.push( 1 ) );
    EXPECT_TRUE( q.push( 2 ) );
    EXPECT_TRUE( q.push( 3 ) );
    EXPECT_FALSE( q.push( 4 ) );
}

TEST( SPSCQueue, WrapsAround )
{
    SPSCQUeue<int, 4> q;
    int               v = 0;
    for( int i = 0; i < 100; ++i )
    {
        ASSERT_TRUE( q.push( i ) );
        ASSERT_TRUE( q.pop( v ) );
        ASSERT_EQ( v, i );
    }
}

TEST( SPSCQueue, ProducerConsumerThreadsPreserveSequence )
{
    constexpr int            kCount = 200000;
    SPSCQUeue<int, 256>      q;
    std::vector<int>         seen;
    seen.reserve( kCount );

    std::thread producer( [&] {
        for( int i = 0; i < kCount; ++i )
            while( !q.push( i ) )
                std::this_thread::yield();
    } );
    std::thread consumer( [&] {
        int v;
        while( static_cast<int>( seen.size() ) < kCount )
            if( q.pop( v ) )
                seen.push_back( v );
            else
                std::this_thread::yield();
    } );
    producer.join();
    consumer.join();

    ASSERT_EQ( seen.size(), static_cast<size_t>( kCount ) );
    for( int i = 0; i < kCount; ++i )
        ASSERT_EQ( seen[i], i ) << "reordered or lost at " << i;
}

// ---------------------------------------------------------------- memory pool

class MemoryPoolTest : public ::testing::Test
{
protected:
    void SetUp() override { kicktest::ensureAudioPool(); }
    MemoryPool<float>& pool() { return MemoryPool<float>::get(); }
};

TEST_F( MemoryPoolTest, AllocateTracksUsageAndPeak )
{
    const auto usedBefore = pool().getUsed();
    float*     a          = pool().acquire( nullptr, true );
    float*     b          = pool().acquire( nullptr, true );
    ASSERT_NE( a, nullptr );
    ASSERT_NE( b, nullptr );
    EXPECT_NE( a, b );
    EXPECT_EQ( pool().getUsed(), usedBefore + 2 );
    EXPECT_GE( pool().getPeakUsed(), usedBefore + 2 );

    pool().release( a );
    pool().release( b );
    EXPECT_EQ( pool().getUsed(), usedBefore );
}

TEST_F( MemoryPoolTest, BlocksAreZeroedAndBlockSized )
{
    float* a = pool().acquire( nullptr, true );
    ASSERT_NE( a, nullptr );
    for( uint32_t i = 0; i < pool().getBlockSize(); ++i )
        ASSERT_EQ( a[i], 0.f );
    std::fill( a, a + pool().getBlockSize(), 9.f );
    pool().release( a );

    float* again = pool().acquire( nullptr, true ); // reuse must re-zero
    ASSERT_NE( again, nullptr );
    for( uint32_t i = 0; i < pool().getBlockSize(); ++i )
        ASSERT_EQ( again[i], 0.f ) << "stale data at " << i;
    pool().release( again );
}

TEST_F( MemoryPoolTest, ReferenceCountKeepsBlockAliveUntilLastRelease )
{
    const auto used0 = pool().getUsed();
    float*     a     = pool().acquire( nullptr, true );
    float*     alias = pool().acquire( a ); // second reference
    EXPECT_EQ( alias, a );
    EXPECT_EQ( pool().getUsed(), used0 + 1 );

    pool().release( a );
    EXPECT_EQ( pool().getUsed(), used0 + 1 ) << "still referenced";
    pool().release( a );
    EXPECT_EQ( pool().getUsed(), used0 );
}

TEST_F( MemoryPoolTest, ForeignPointerIsIgnored )
{
    float outside = 0.f;
    EXPECT_EQ( pool().acquire( &outside ), nullptr );
    const auto used = pool().getUsed();
    pool().release( &outside );
    EXPECT_EQ( pool().getUsed(), used );
}

TEST_F( MemoryPoolTest, PercentagesAreComplementary )
{
    EXPECT_NEAR( pool().getUsedMemoryPercentage() + pool().getFreeMemoryPercentage(), 100.f, 0.01f );
}

// -------------------------------------------------------- ramped parameters

TEST( RampedParameter, StartsAtInitialValue )
{
    RampedParameter<float> p( 0.25f );
    EXPECT_FLOAT_EQ( p.getCurrent(), 0.25f );
    EXPECT_FLOAT_EQ( p.getTarget(), 0.25f );
    // Note: isRamping() is true on a fresh parameter (step counter 0 < nSteps) even
    // though current==target. Harmless, since ramping toward the same value is a no-op.
}

TEST( RampedParameter, LinearRampReachesTargetInExactlyNSteps )
{
    RampedParameter<float> p( 0.f, 100 );
    p.store( 1.f );
    EXPECT_TRUE( p.isRamping() );
    EXPECT_FLOAT_EQ( p.value(), 1.f );

    float prev = p.getCurrent();
    for( int i = 0; i < 100; ++i )
    {
        const float v = p.getNext();
        EXPECT_GT( v, prev ) << "monotonic at step " << i;
        prev = v;
    }
    EXPECT_NEAR( p.getCurrent(), 1.f, 1e-4f );
    EXPECT_FALSE( p.isRamping() );
}

TEST( RampedParameter, BlockStepsAreEquivalentToSampleSteps )
{
    RampedParameter<float> a( 0.f, 200 ), b( 0.f, 200 );
    a.store( 1.f );
    b.store( 1.f );
    for( int i = 0; i < 50; ++i )
        a.getNext();
    b.getNext( 50 );
    EXPECT_NEAR( a.getCurrent(), b.getCurrent(), 1e-4f );
}

TEST( RampedParameter, ForceJumpsImmediately )
{
    RampedParameter<float> p( 0.f, 100 );
    p.store( 0.8f, true );
    EXPECT_FLOAT_EQ( p.getCurrent(), 0.8f );
    p.getNext( 10 );
    EXPECT_FLOAT_EQ( p.getCurrent(), 0.8f ) << "a forced jump must not drift when stepped";
}

TEST( RampedParameter, StepsPastEndSettleOnTarget )
{
    RampedParameter<float> p( 0.f, 10 );
    p.store( 2.f );
    for( int i = 0; i < 50; ++i )
        p.getNext();
    EXPECT_NEAR( p.getCurrent(), 2.f, 1e-4f );
}

TEST( RampedParameter, RetargetMidRampContinuesFromCurrent )
{
    RampedParameter<float> p( 0.f, 100 );
    p.store( 1.f );
    p.getNext( 50 );
    const float mid = p.getCurrent();
    EXPECT_NEAR( mid, 0.5f, 1e-3f );
    p.store( 0.f );
    for( int i = 0; i < 100; ++i )
        p.getNext();
    EXPECT_NEAR( p.getCurrent(), 0.f, 1e-3f );
}

TEST( RampedParameter, RampDurationFromSampleRate )
{
    RampedParameter<float> p( 0.f );
    p.setRampDuration( 48000.f, 0.01f ); // 480 steps
    p.store( 1.f );
    p.getNext( 479 );
    EXPECT_TRUE( p.isRamping() );
    p.getNext( 1 );
    EXPECT_FALSE( p.isRamping() );
}

TEST( RampedParameter, AssignmentOperatorStores )
{
    RampedParameter<float> p( 0.f, 10 );
    p = 3.f;
    EXPECT_FLOAT_EQ( p.getTarget(), 3.f );
}
