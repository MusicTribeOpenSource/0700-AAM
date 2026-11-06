// SPDX-License-Identifier: AGPL-3.0-or-later
#include <gtest/gtest.h>

#include "kick_mcu/link/frame_codec.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace kick_mcu::link;
using namespace testing_support;

TEST( FrameCodec, CrcMatchesFramesCapturedFromTheEmulator )
{
    // Both frames are from emu/tests/traces/smoke.trace, written by the Surface's own crc8.
    const uint8_t status2[] = { 'M', 'T', 0x00, 0x00, 0x04, 0x02, 0x02 };
    EXPECT_EQ( 0xf3, crc8( status2, sizeof( status2 ) ) );

    const uint8_t commInit1[] = { 'M', 'T', 0x00, 0x00, 0x02, 0x02, 0x01 };
    EXPECT_EQ( 0x05, crc8( commInit1, sizeof( commInit1 ) ) );
}

TEST( FrameCodec, CrcStartsAtFFAndEmptyInputKeepsIt )
{
    EXPECT_EQ( 0xFF, crc8( nullptr, 0 ) );
}

TEST( FrameCodec, UpdateAgreesWithOneShot )
{
    const Bytes data = { 'M', 'T', 3, 0, 4, 5, 16, 4, 4, 4 };
    uint8_t     crc  = contract::kCrcInit;
    for( uint8_t b : data )
        crc = crc8Update( crc, b );
    EXPECT_EQ( crc8( data.data(), data.size() ), crc );
}

TEST( FrameCodec, McuToHostFrameCarriesATrailerAndHostToMcuDoesNot )
{
    const Bytes up   = toHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } );
    const Bytes down = fromHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } );

    // Frame length is size + 6 on the wire; the MCU-to-host frame is one longer.
    ASSERT_EQ( 9u, up.size() );
    ASSERT_EQ( 8u, down.size() );
    EXPECT_EQ( contract::kMcuToHostTrailer, up.back() );
    EXPECT_EQ( Bytes( up.begin(), up.end() - 1 ), down );

    // The size byte is payload length + 1, and size + 7 is what the host reads per frame.
    EXPECT_EQ( 2u, up[5] );
    EXPECT_EQ( (size_t)up[5] + 7, up.size() );
}

TEST( FrameCodec, EncodedBytesMatchTheEmulatorHeartbeat )
{
    const Bytes expected = { 0x4d, 0x54, 0x00, 0x00, 0x04, 0x02, 0x02, 0xf3, 0x0a };
    EXPECT_EQ( expected, toHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } ) );
}

TEST( FrameCodec, ZeroLengthPayloadHasSizeOne )
{
    const Bytes f = toHost( contract::PKT_SYSTEM, contract::SYS_RECONNECT, {} );
    ASSERT_EQ( 8u, f.size() );
    EXPECT_EQ( 1u, f[5] );
}

TEST( FrameCodec, McuToHostPayloadIsLimitedToNineteenBytes )
{
    EXPECT_NE( 0u, encodedSize( Wire::McuToHost, 19 ) );
    EXPECT_EQ( 0u, encodedSize( Wire::McuToHost, 20 ) );

    uint8_t buf[64];
    const Bytes big( 20, 0xAA );
    EXPECT_EQ( 0u, encodeFrame( buf, sizeof( buf ), Wire::McuToHost, 1, 0, 0, big.data(), big.size() ) );
}

TEST( FrameCodec, HostToMcuAcceptsTheLargestHostFrame )
{
    const Bytes payload( contract::kLargestHostPayload, 7 );
    EXPECT_NE( 0u, encodedSize( Wire::HostToMcu, payload.size() ) );
    EXPECT_NE( 0u, encodedSize( Wire::HostToMcu, contract::kMaxPayloadFromHost ) );
    EXPECT_EQ( 0u, encodedSize( Wire::HostToMcu, contract::kMaxPayloadFromHost + 1 ) );
}

TEST( FrameCodec, RefusesWhenTheBufferIsTooSmall )
{
    uint8_t     buf[8];
    const Bytes payload = { 1, 2, 3 };
    EXPECT_EQ( 0u, encodeFrame( buf, sizeof( buf ), Wire::McuToHost, 1, 0, 0, payload.data(), payload.size() ) );
    EXPECT_EQ( 0u, encodeFrame( buf, 0, Wire::McuToHost, 1, 0, 0, nullptr, 0 ) );
}

TEST( FrameCodec, RoundTripsThroughTheParserForEveryPayloadLength )
{
    for( size_t len = 0; len <= contract::kMaxPayloadFromHost; ++len )
    {
        Bytes payload( len );
        for( size_t i = 0; i < len; ++i )
            payload[i] = (uint8_t)( 0x100 - i * 3 );

        RxParser  parser;
        Collector got;
        got.attach( parser );
        feedAll( parser, fromHost( contract::PKT_PAD, contract::PAD_SET_STEP_ALL_PAD, payload ) );

        ASSERT_EQ( 1u, got.frames.size() ) << "len " << len;
        EXPECT_EQ( payload, got.frames[0].payload ) << "len " << len;
    }
}
