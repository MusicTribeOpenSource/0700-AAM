// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include "kick_mcu/link/rx_parser.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace kick_mcu::link;
using namespace testing_support;

namespace
{
struct RxParserTest : ::testing::Test
{
    RxParser  parser;
    Collector got;

    void SetUp() override { got.attach( parser ); }

    Bytes button( uint8_t id, uint8_t state ) { return fromHost( contract::PKT_BUTTON, contract::BTN_STATUS, { id, state } ); }
    Decoded buttonFrame( uint8_t id, uint8_t state ) { return { contract::PKT_BUTTON, 0, contract::BTN_STATUS, { id, state } }; }
};
} // namespace

TEST_F( RxParserTest, DeliversAValidFrame )
{
    feedAll( parser, button( 12, 1 ) );
    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 12, 1 ), got.frames[0] );
    EXPECT_FALSE( parser.inFrame() );
    EXPECT_EQ( 1u, parser.stats().frames );
}

TEST_F( RxParserTest, ReadsFramesWithTheMcuToHostTrailer )
{
    // The trailer follows the CRC, so it is a stray byte between frames.
    feedAll( parser, cat( { toHost( contract::PKT_BUTTON, contract::BTN_STATUS, { 3, 1 } ), toHost( contract::PKT_BUTTON, contract::BTN_STATUS, { 3, 0 } ) } ) );
    ASSERT_EQ( 2u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 3, 1 ), got.frames[0] );
    EXPECT_EQ( buttonFrame( 3, 0 ), got.frames[1] );
    EXPECT_EQ( 2u, parser.stats().garbageBytes );
}

TEST_F( RxParserTest, DeliversBackToBackFrames )
{
    Bytes stream;
    for( uint8_t i = 0; i < 20; ++i )
    {
        const Bytes f = button( i, i & 1 );
        stream.insert( stream.end(), f.begin(), f.end() );
    }
    feedAll( parser, stream );

    ASSERT_EQ( 20u, got.frames.size() );
    for( uint8_t i = 0; i < 20; ++i )
        EXPECT_EQ( buttonFrame( i, i & 1 ), got.frames[i] ) << (int)i;
    EXPECT_EQ( 0u, parser.stats().garbageBytes );
    EXPECT_EQ( 0u, parser.stats().resyncs );
}

TEST_F( RxParserTest, SkipsGarbageBetweenFrames )
{
    feedAll( parser, cat( { Bytes{ 0x00, 0xFF, 'T', 'M', 0x12 }, button( 1, 1 ), Bytes{ 'M', 0x99, 'T' }, button( 1, 0 ), Bytes{ 0x55 } } ) );
    ASSERT_EQ( 2u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 1, 1 ), got.frames[0] );
    EXPECT_EQ( buttonFrame( 1, 0 ), got.frames[1] );
}

TEST_F( RxParserTest, TreatsMMTAsAFrameStart )
{
    feedAll( parser, cat( { Bytes{ 'M' }, button( 9, 1 ) } ) );
    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 9, 1 ), got.frames[0] );
}

TEST_F( RxParserTest, RejectsABadCrcAndRecoversOnTheNextFrame )
{
    Bytes bad = button( 5, 1 );
    bad.back() ^= 0x40;
    feedAll( parser, cat( { bad, button( 5, 0 ) } ) );

    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 5, 0 ), got.frames[0] );
    EXPECT_EQ( 1u, parser.stats().crcErrors );
}

TEST_F( RxParserTest, ACorruptedPayloadByteFailsTheCrc )
{
    Bytes f = button( 5, 1 );
    f[6] ^= 0x01;
    feedAll( parser, f );
    EXPECT_TRUE( got.frames.empty() );
    EXPECT_EQ( 1u, parser.stats().crcErrors );
}

TEST_F( RxParserTest, ATruncatedFrameIsAbsorbedAndTheNextOneIsStillFound )
{
    // Cut the first frame short by two bytes. The next frame's bytes fill the gap, the CRC fails,
    // and the re-scan finds the real frame inside.
    Bytes cut = button( 7, 1 );
    cut.resize( cut.size() - 2 );
    feedAll( parser, cat( { cut, button( 7, 0 ) } ) );

    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 7, 0 ), got.frames[0] );
    EXPECT_GE( parser.stats().resyncs, 1u );
}

TEST_F( RxParserTest, ARealFrameInsideARejectedFrameIsNotLost )
{
    // A frame header claims 10 payload bytes, then a complete valid frame follows inside them.
    const Bytes inner = button( 20, 1 );
    Bytes       stream = { 'M', 'T', 1, 0, 0, 11 };
    stream.insert( stream.end(), inner.begin(), inner.end() );
    stream.resize( 6 + 10 + 1, 0x00 ); // pad out to the claimed length so the CRC byte arrives
    feedAll( parser, stream );

    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 20, 1 ), got.frames[0] );
}

TEST_F( RxParserTest, RescansAnMInsideTheRejectedFrame )
{
    // "M T 1 0 0 3 'M' 'T' ..." : the first header says 2 payload bytes, which are the start of a
    // second header. The first frame fails its CRC; the second must still come out.
    const Bytes real = button( 2, 1 );
    feedAll( parser, cat( { Bytes{ 'M', 'T', 1, 0, 0, 3 }, real } ) );
    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 2, 1 ), got.frames[0] );
}

TEST_F( RxParserTest, RejectsSizeZero )
{
    feedAll( parser, cat( { Bytes{ 'M', 'T', 1, 0, 0, 0 }, button( 4, 1 ) } ) );
    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 4, 1 ), got.frames[0] );
    EXPECT_EQ( 1u, parser.stats().sizeErrors );
}

TEST_F( RxParserTest, RejectsAPayloadLargerThanTheReceiveBuffer )
{
    // size 0xFF would mean 254 payload bytes. The parser must refuse it at the size byte, not
    // swallow 254 bytes of the following traffic.
    feedAll( parser, cat( { Bytes{ 'M', 'T', 1, 0, 0, 0xFF }, button( 4, 1 ) } ) );
    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 4, 1 ), got.frames[0] );
    EXPECT_EQ( 1u, parser.stats().sizeErrors );
}

TEST_F( RxParserTest, AcceptsTheLargestFrameTheBufferHolds )
{
    const Bytes payload( contract::kMaxPayloadFromHost, 0x5A );
    feedAll( parser, fromHost( contract::PKT_PAD, contract::PAD_SET_STEP_ALL_PAD, payload ) );
    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( payload, got.frames[0].payload );
}

TEST_F( RxParserTest, AcceptsAFrameWithNoPayload )
{
    feedAll( parser, fromHost( contract::PKT_SYSTEM, contract::SYS_RECONNECT, {} ) );
    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_TRUE( got.frames[0].payload.empty() );
}

TEST_F( RxParserTest, DeliversTheHostsLargestRealFrame )
{
    const Bytes payload( contract::kLargestHostPayload, 3 );
    feedAll( parser, fromHost( contract::PKT_PAD, contract::PAD_SET_STEP_ALL_PAD, payload ) );
    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( payload, got.frames[0].payload );
}

TEST_F( RxParserTest, ExpireFlushesAFrameThatWillNeverComplete )
{
    // A frame promises 30 bytes, then a short valid frame arrives and the line goes quiet. Without
    // expire() the valid frame would sit unreported behind the stalled one.
    const Bytes stalled = { 'M', 'T', 3, 0, 4, 31, 1, 2, 3 };
    feedAll( parser, cat( { stalled, button( 6, 1 ) } ) );
    EXPECT_TRUE( got.frames.empty() );
    EXPECT_TRUE( parser.inFrame() );

    parser.expire();
    ASSERT_EQ( 1u, got.frames.size() );
    EXPECT_EQ( buttonFrame( 6, 1 ), got.frames[0] );
    EXPECT_EQ( 1u, parser.stats().timeouts );
    EXPECT_FALSE( parser.inFrame() );
}

TEST_F( RxParserTest, ExpireWhenIdleDoesNothing )
{
    parser.expire();
    EXPECT_EQ( 0u, parser.stats().timeouts );
    EXPECT_TRUE( got.frames.empty() );
}

TEST_F( RxParserTest, ExpireOnALoneStartByteLeavesTheParserClean )
{
    parser.feed( 'M' );
    EXPECT_TRUE( parser.inFrame() );
    parser.expire();
    EXPECT_FALSE( parser.inFrame() );
    feedAll( parser, button( 1, 1 ) );
    EXPECT_EQ( 1u, got.frames.size() );
}

TEST_F( RxParserTest, ASingleBitFlipAnywhereInAFrameNeverDeliversAWrongFrame )
{
    const Bytes good = button( 33, 1 );
    for( size_t i = 0; i < good.size(); ++i )
        for( int bit = 0; bit < 8; ++bit )
        {
            RxParser  p;
            Collector c;
            c.attach( p );
            Bytes f = good;
            f[i] ^= (uint8_t)( 1 << bit );
            feedAll( p, f );
            p.expire();
            for( const auto& d : c.frames )
                EXPECT_EQ( buttonFrame( 33, 1 ), d ) << "byte " << i << " bit " << bit << " delivered a corrupted frame";
        }
}

TEST_F( RxParserTest, RecoversFromRandomNoiseAndStillSeesEveryCleanFrame )
{
    // Noise with no 'M' in it cannot start a frame, so every clean frame between noise bursts
    // must come out, in order. Run many bursts of different lengths.
    Rng rng( 0xC0FFEE );
    Bytes expectedOrder;
    Bytes stream;
    for( uint8_t i = 0; i < 200; ++i )
    {
        const size_t noise = rng.below( 40 );
        for( size_t n = 0; n < noise; ++n )
        {
            uint8_t b = (uint8_t)rng.next();
            if( b == 'M' )
                b = 'N';
            stream.push_back( b );
        }
        const Bytes f = button( i % 53, i & 1 );
        stream.insert( stream.end(), f.begin(), f.end() );
        expectedOrder.push_back( i );
    }
    feedAll( parser, stream );

    ASSERT_EQ( expectedOrder.size(), got.frames.size() );
    for( uint8_t i = 0; i < 200; ++i )
        EXPECT_EQ( buttonFrame( i % 53, i & 1 ), got.frames[i] ) << (int)i;
}

TEST_F( RxParserTest, SurvivesArbitraryInputWithoutOverrunningItsBuffer )
{
    // Random bytes with 'M' and 'T' common, so the parser keeps entering and abandoning frames,
    // and expire() lands at random points. The assertions are weak on purpose: the value of this
    // test is running 200k bytes of it under ASan and UBSan. Whatever the parser does deliver
    // must be small enough to re-encode.
    Rng rng( 12345 );
    for( int i = 0; i < 200000; ++i )
    {
        const uint32_t r = rng.below( 8 );
        parser.feed( r == 0 ? 'M' : r == 1 ? 'T' : (uint8_t)rng.next() );
        if( rng.below( 5000 ) == 0 )
            parser.expire();
    }
    for( const auto& d : got.frames )
    {
        uint8_t buf[128];
        EXPECT_NE( 0u, encodeFrame( buf, sizeof( buf ), Wire::HostToMcu, d.type, d.dir, d.cmd, d.payload.data(), d.payload.size() ) );
    }
    EXPECT_EQ( got.frames.size(), parser.stats().frames );
}

TEST_F( RxParserTest, RecoversAfterEveryPossibleTruncationPoint )
{
    // Cut a good frame after k bytes, then send a complete frame. For every k the second frame
    // must be delivered.
    const Bytes first  = fromHost( contract::PKT_PAD, contract::PAD_SET_ALL_MULTI_COLOUR, Bytes( 20, 9 ) );
    const Bytes second = button( 40, 1 );
    for( size_t k = 1; k < first.size(); ++k )
    {
        RxParser  p;
        Collector c;
        c.attach( p );
        feedAll( p, Bytes( first.begin(), first.begin() + (std::ptrdiff_t)k ) );
        feedAll( p, second );
        p.expire();
        ASSERT_EQ( 1u, c.frames.size() ) << "cut at " << k;
        EXPECT_EQ( buttonFrame( 40, 1 ), c.frames[0] ) << "cut at " << k;
    }
}
