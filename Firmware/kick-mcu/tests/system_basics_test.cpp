// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CRC-32, the image header, and the boot-control record.
#include <gtest/gtest.h>

#include <cstring>

#include "system_support.h"

using namespace system_support;

TEST( Crc32, MatchesTheStandardCheckValue )
{
    const char* text = "123456789";
    EXPECT_EQ( 0xCBF43926u, crc32( reinterpret_cast<const uint8_t*>( text ), 9 ) );
}

TEST( Crc32, StreamingEqualsOneShot )
{
    const Bytes data = makeImage( { 1, 2, 3 }, 200 );
    uint32_t    s    = crc32Init();
    s                = crc32Update( s, data.data(), 77 );
    s                = crc32Update( s, data.data() + 77, data.size() - 77 );
    EXPECT_EQ( crc32( data.data(), data.size() ), crc32Final( s ) );
    EXPECT_EQ( 0u, crc32( nullptr, 0 ) );
}

TEST( ImageHeader, RoundTripsEveryField )
{
    ImageHeader h;
    h.imageBytes   = 1234;
    h.payloadCrc32 = 0xDEADBEEF;
    h.version      = { 3, 14, 15 };
    h.swRevision   = 0x0A0B;
    uint8_t raw[kImageHeaderBytes];
    encodeImageHeader( h, raw );

    ImageHeader back;
    ASSERT_EQ( ImageStatus::Ok, decodeImageHeader( raw, back ) );
    EXPECT_EQ( kFlashValid, back.flashState );
    EXPECT_EQ( 1234u, back.imageBytes );
    EXPECT_EQ( 0xDEADBEEFu, back.payloadCrc32 );
    EXPECT_EQ( ( VersionNo{ 3, 14, 15 } ), back.version );
    EXPECT_EQ( 0x0A0B, back.swRevision );
    // little endian, as documented in image.h
    EXPECT_EQ( 0x78, raw[0] );
    EXPECT_EQ( 0x12, raw[3] );
    for( size_t i = 32; i < kImageHeaderBytes; ++i )
        EXPECT_EQ( 0, raw[i] ) << "the signature field is reserved and stays zero";
}

TEST( ImageHeader, EachCorruptionIsReportedAsItself )
{
    uint8_t good[kImageHeaderBytes];
    ImageHeader h;
    h.imageBytes = 100;
    encodeImageHeader( h, good );

    ImageHeader out;
    uint8_t     bad[kImageHeaderBytes];

    std::memset( bad, 0xFF, sizeof( bad ) );
    EXPECT_EQ( ImageStatus::Erased, decodeImageHeader( bad, out ) );

    std::memcpy( bad, good, sizeof( bad ) );
    bad[0] ^= 1;
    EXPECT_EQ( ImageStatus::BadMarker, decodeImageHeader( bad, out ) );

    std::memcpy( bad, good, sizeof( bad ) );
    bad[6] = 9; // header version
    EXPECT_EQ( ImageStatus::BadHeaderVersion, decodeImageHeader( bad, out ) );

    std::memcpy( bad, good, sizeof( bad ) );
    bad[8] ^= 0x40; // imageBytes, covered by the header CRC
    EXPECT_EQ( ImageStatus::BadHeaderCrc, decodeImageHeader( bad, out ) );
}

TEST( VerifyImage, GoodImagePasses )
{
    auto        flash = makeFlash();
    const auto  layout = testLayout();
    const Bytes image  = makeImage( { 1, 0, 0 }, 100 );
    ASSERT_TRUE( writeSlot( *flash, layout, Slot::A, image ) );
    const ImageCheck c = verifyImage( *flash, layout.baseOf( Slot::A ), layout.slotBytes );
    EXPECT_EQ( ImageStatus::Ok, c.status );
    EXPECT_EQ( c.header.payloadCrc32, c.computedCrc32 );
}

TEST( VerifyImage, ErasedSlotIsNotAnImage )
{
    auto       flash  = makeFlash();
    const auto layout = testLayout();
    EXPECT_EQ( ImageStatus::Erased, verifyImage( *flash, layout.baseOf( Slot::B ), layout.slotBytes ).status );
}

TEST( VerifyImage, OneFlippedBitAnywhereInThePayloadFailsTheCrc )
{
    const auto layout = testLayout();
    const Bytes image = makeImage( { 1, 0, 0 }, 100 );
    for( size_t i = kImageHeaderBytes; i < image.size(); ++i )
    {
        auto flash = makeFlash();
        ASSERT_TRUE( writeSlot( *flash, layout, Slot::A, image ) );
        flash->contents()[layout.baseOf( Slot::A ) + i] ^= 0x10;
        ASSERT_EQ( ImageStatus::BadPayloadCrc, verifyImage( *flash, layout.baseOf( Slot::A ), layout.slotBytes ).status ) << "byte " << i;
    }
}

TEST( VerifyImage, LengthLargerThanTheSlotOrSmallerThanTheHeaderIsRejected )
{
    const auto layout = testLayout();
    for( uint32_t length : { 64u, 10u, (uint32_t)layout.slotBytes + 1 } )
    {
        auto  flash = makeFlash();
        Bytes image = makeImage( { 1, 0, 0 }, 100 );
        ImageHeader h;
        ASSERT_EQ( ImageStatus::Ok, decodeImageHeader( image.data(), h ) );
        h.imageBytes = length;
        encodeImageHeader( h, image.data() );
        ASSERT_TRUE( writeSlot( *flash, layout, Slot::A, image ) );
        EXPECT_EQ( ImageStatus::BadLength, verifyImage( *flash, layout.baseOf( Slot::A ), layout.slotBytes ).status ) << length;
    }
}

TEST( SimFlash, WriteCanOnlyClearBits )
{
    sim::SimFlash flash( 128, 64 );
    const uint8_t a = 0xF0, b = 0x0F, c = 0x30;
    ASSERT_TRUE( flash.write( 0, &a, 1 ) );
    EXPECT_FALSE( flash.write( 0, &b, 1 ) ) << "needs bits to go from 0 to 1";
    EXPECT_EQ( 1u, flash.illegalWrites() );
    EXPECT_TRUE( flash.write( 0, &c, 1 ) ) << "clearing more bits is fine";
    uint8_t back = 0;
    ASSERT_TRUE( flash.read( 0, &back, 1 ) );
    EXPECT_EQ( 0x30, back );
    ASSERT_TRUE( flash.eraseBlock( 0 ) );
    ASSERT_TRUE( flash.read( 0, &back, 1 ) );
    EXPECT_EQ( 0xFF, back );
}

TEST( SimFlash, PowerCutLeavesAPrefixOfAWriteAndKillsTheDevice )
{
    sim::SimFlash flash( 128, 64 );
    const uint8_t data[4] = { 1, 2, 3, 4 };
    flash.cutPowerAfter( 2 );
    EXPECT_FALSE( flash.write( 0, data, 4 ) );
    EXPECT_TRUE( flash.powerLost() );
    uint8_t back[1];
    EXPECT_FALSE( flash.read( 0, back, 1 ) ) << "a dead device does not answer";
    flash.restorePower();
    EXPECT_EQ( ( Bytes{ 1, 2, 0xFF, 0xFF } ), Bytes( flash.contents().begin(), flash.contents().begin() + 4 ) );
}

TEST( SimFlash, PowerCutDuringEraseLeavesTheRestOfTheBlockAsItWas )
{
    sim::SimFlash flash( 128, 64 );
    const Bytes   zeros( 64, 0 );
    ASSERT_TRUE( flash.write( 0, zeros.data(), 64 ) );
    flash.cutPowerAfter( 10 );
    EXPECT_FALSE( flash.eraseBlock( 0 ) );
    flash.restorePower();
    for( size_t i = 0; i < 64; ++i )
        EXPECT_EQ( i < 10 ? 0xFF : 0x00, flash.contents()[i] ) << i;
}

// ---- boot control ---------------------------------------------------------------------------

struct BootRig : ::testing::Test
{
    FlashLayout                    layout{ testLayout() };
    std::unique_ptr<sim::SimFlash> flash{ makeFlash( layout ) };
    BootControl                    control{ *flash, layout };
    Bytes                          v1{ makeImage( { 1, 0, 0 }, 100, 1 ) };
    Bytes                          v2{ makeImage( { 1, 1, 0 }, 120, 2 ) };
};

TEST_F( BootRig, RecordsAlternateBetweenTheTwoCopiesAndNewestWins )
{
    BootRecord r;
    EXPECT_FALSE( control.load( r ) );
    ASSERT_TRUE( control.confirm( Slot::A ) );
    ASSERT_TRUE( control.load( r ) );
    EXPECT_EQ( 1u, r.seq );
    ASSERT_TRUE( control.commitTrial( Slot::A, Slot::B ) );
    ASSERT_TRUE( control.load( r ) );
    EXPECT_EQ( 2u, r.seq );
    EXPECT_EQ( BootState::TrialPending, r.state );
    EXPECT_EQ( Slot::A, r.good );
    EXPECT_EQ( Slot::B, r.trial );
    // Both copies hold a record now, with different seq.
    EXPECT_NE( flash->contents()[layout.ctlBase[0] + 4], flash->contents()[layout.ctlBase[1] + 4] );
}

TEST_F( BootRig, ACopyDamagedAnywhereIsIgnoredAndTheOtherOneWins )
{
    ASSERT_TRUE( control.confirm( Slot::A ) );        // seq 1 in copy 0
    ASSERT_TRUE( control.commitTrial( Slot::A, Slot::B ) ); // seq 2 in copy 1
    for( size_t i = 0; i < kBootRecordBytes; ++i )
    {
        auto copy = *flash; // SimFlash is copyable: a vector and some counters
        BootControl damaged( copy, layout );
        copy.contents()[layout.ctlBase[1] + i] ^= 0x01;
        BootRecord r;
        ASSERT_TRUE( damaged.load( r ) ) << "byte " << i;
        EXPECT_EQ( 1u, r.seq ) << "byte " << i << ": falls back to the older, still valid copy";
        EXPECT_EQ( BootState::Confirmed, r.state );
    }
}

TEST_F( BootRig, SequenceNumberComparisonSurvivesWraparound )
{
    // Hand-build records with seq 0xFFFFFFFF in copy 0 and 0 in copy 1: 0 is newer.
    auto put = [&]( unsigned copy, uint32_t seq ) {
        uint8_t raw[kBootRecordBytes] = { 'K', 'B', 'C', '1', 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 };
        for( int i = 0; i < 4; ++i )
            raw[4 + i] = (uint8_t)( seq >> ( 8 * i ) );
        const uint32_t crc = crc32( raw, 12 );
        for( int i = 0; i < 4; ++i )
            raw[12 + i] = (uint8_t)( crc >> ( 8 * i ) );
        ASSERT_TRUE( flash->write( layout.ctlBase[copy], raw, sizeof( raw ) ) );
    };
    put( 0, 0xFFFFFFFFu );
    put( 1, 0u );
    BootRecord r;
    ASSERT_TRUE( control.load( r ) );
    EXPECT_EQ( 0u, r.seq );
    ASSERT_TRUE( control.confirm( Slot::A ) );
    ASSERT_TRUE( control.load( r ) );
    EXPECT_EQ( 1u, r.seq ) << "the next record goes into the older copy with seq + 1";
}

TEST_F( BootRig, FirstBootWithNoRecordPicksTheFirstValidSlotAndWritesOne )
{
    ASSERT_TRUE( writeSlot( *flash, layout, Slot::B, v2 ) );
    BootDecision d = selectBoot( *flash, layout );
    ASSERT_TRUE( d.bootable );
    EXPECT_TRUE( d.recordMissing );
    EXPECT_EQ( Slot::B, d.slot );
    BootRecord r;
    ASSERT_TRUE( control.load( r ) );
    EXPECT_EQ( BootState::Confirmed, r.state );
    EXPECT_EQ( Slot::B, r.good );
}

TEST_F( BootRig, NothingValidMeansRecoveryNotAGuess )
{
    const BootDecision d = selectBoot( *flash, layout );
    EXPECT_FALSE( d.bootable );
}

TEST_F( BootRig, TrialIsMarkedBootedBeforeItRunsAndRevertsIfNeverConfirmed )
{
    provision( *flash, layout, v1 );
    ASSERT_TRUE( writeSlot( *flash, layout, Slot::B, v2 ) );
    ASSERT_TRUE( control.commitTrial( Slot::A, Slot::B ) );

    BootDecision first = selectBoot( *flash, layout );
    ASSERT_TRUE( first.bootable );
    EXPECT_EQ( Slot::B, first.slot );
    EXPECT_TRUE( first.trial );
    EXPECT_EQ( BootState::TrialBooted, first.record.state );

    BootDecision second = selectBoot( *flash, layout ); // reset without confirming
    ASSERT_TRUE( second.bootable );
    EXPECT_EQ( Slot::A, second.slot );
    EXPECT_FALSE( second.trial );
    EXPECT_TRUE( second.reverted );

    BootDecision third = selectBoot( *flash, layout );
    EXPECT_EQ( Slot::A, third.slot ) << "and it stays reverted";
    EXPECT_FALSE( third.reverted );
}

TEST_F( BootRig, ConfirmedTrialKeepsRunning )
{
    provision( *flash, layout, v1 );
    ASSERT_TRUE( writeSlot( *flash, layout, Slot::B, v2 ) );
    ASSERT_TRUE( control.commitTrial( Slot::A, Slot::B ) );
    BootDecision first = selectBoot( *flash, layout );
    ASSERT_EQ( Slot::B, first.slot );
    ASSERT_TRUE( control.confirm( first.slot ) );
    for( int i = 0; i < 3; ++i )
    {
        const BootDecision d = selectBoot( *flash, layout );
        EXPECT_EQ( Slot::B, d.slot );
        EXPECT_FALSE( d.trial );
        EXPECT_FALSE( d.reverted );
    }
}

TEST_F( BootRig, ATrialSlotThatDoesNotVerifyIsNeverBooted )
{
    provision( *flash, layout, v1 );
    Bytes damaged = v2;
    damaged[100] ^= 0xFF;
    ASSERT_TRUE( writeSlot( *flash, layout, Slot::B, damaged ) );
    ASSERT_TRUE( control.commitTrial( Slot::A, Slot::B ) );
    const BootDecision d = selectBoot( *flash, layout );
    ASSERT_TRUE( d.bootable );
    EXPECT_EQ( Slot::A, d.slot );
    EXPECT_TRUE( d.reverted );
    EXPECT_FALSE( d.trial );
}

TEST_F( BootRig, ADamagedGoodSlotFallsBackToTheOtherOneWhenThatIsValid )
{
    provision( *flash, layout, v1 );
    ASSERT_TRUE( writeSlot( *flash, layout, Slot::B, v2 ) );
    flash->contents()[layout.baseOf( Slot::A ) + 90] ^= 0x01;
    const BootDecision d = selectBoot( *flash, layout );
    ASSERT_TRUE( d.bootable );
    EXPECT_EQ( Slot::B, d.slot );
    EXPECT_TRUE( d.fellBack );
    BootRecord r;
    ASSERT_TRUE( control.load( r ) );
    EXPECT_EQ( Slot::B, r.good );
}

TEST_F( BootRig, ADamagedGoodSlotWithNoValidAlternativeIsNotBootable )
{
    provision( *flash, layout, v1 );
    flash->contents()[layout.baseOf( Slot::A ) + 90] ^= 0x01;
    EXPECT_FALSE( selectBoot( *flash, layout ).bootable );
}

TEST_F( BootRig, ARecordWriteCutAtAnyByteLeavesEitherTheOldRecordOrTheNewOne )
{
    ASSERT_TRUE( control.confirm( Slot::A ) );              // seq 1
    ASSERT_TRUE( control.commitTrial( Slot::A, Slot::B ) ); // seq 2: both copies now hold a record

    // How many byte operations one more store takes: an erase block plus a 16-byte record.
    const uint64_t perStore = layout.blockBytes + kBootRecordBytes;
    for( uint64_t cut = 0; cut <= perStore; ++cut )
    {
        auto copy = *flash;
        BootControl c( copy, layout );
        copy.cutPowerAfter( cut );
        c.confirm( Slot::A ); // seq 3, may be cut
        copy.restorePower();

        BootRecord r;
        ASSERT_TRUE( c.load( r ) ) << "cut after " << cut << ": no valid record at all";
        if( cut < perStore )
        {
            EXPECT_EQ( 2u, r.seq ) << "cut after " << cut << ": the old record must survive";
            EXPECT_EQ( BootState::TrialPending, r.state );
        }
        else
        {
            EXPECT_EQ( 3u, r.seq );
            EXPECT_EQ( BootState::Confirmed, r.state );
        }
    }
}
