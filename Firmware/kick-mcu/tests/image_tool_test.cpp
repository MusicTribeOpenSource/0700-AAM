// SPDX-License-Identifier: AGPL-3.0-or-later
//
// tools/mcu-image-header.py and the firmware have to agree on the image header. This test takes
// the tool's output (built by CMake from tests/fixtures/payload.hex) and checks it with the
// firmware's own code, then sends it through the Updater the way the host would.
#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include "system_support.h"

#ifndef KICK_STAMPED_HEX
#error "KICK_STAMPED_HEX must name the stamped hex the tool produced"
#endif

using namespace system_support;
namespace c = kick_mcu::contract;

namespace
{
struct HexRecord
{
    unsigned type;
    unsigned address;
    Bytes    data;
};

std::vector<HexRecord> readHex( const std::string& path )
{
    std::ifstream          in( path );
    std::vector<HexRecord> out;
    std::string            line;
    while( std::getline( in, line ) )
    {
        if( line.empty() )
            continue;
        EXPECT_EQ( ':', line[0] );
        auto byteAt = [&]( size_t i ) { return (unsigned)std::stoul( line.substr( 1 + 2 * i, 2 ), nullptr, 16 ); };
        const unsigned count = byteAt( 0 );
        HexRecord      r;
        r.address = ( byteAt( 1 ) << 8 ) | byteAt( 2 );
        r.type    = byteAt( 3 );
        unsigned sum = count + byteAt( 1 ) + byteAt( 2 ) + r.type;
        for( unsigned i = 0; i < count; ++i )
        {
            r.data.push_back( (uint8_t)byteAt( 4 + i ) );
            sum += r.data.back();
        }
        EXPECT_EQ( 0u, ( sum + byteAt( 4 + count ) ) & 0xFF ) << "checksum of: " << line;
        out.push_back( r );
    }
    return out;
}

Bytes flatten( const std::vector<HexRecord>& records )
{
    Bytes    image;
    uint32_t upper = 0;
    for( auto& r : records )
    {
        if( r.type == 4 )
            upper = ( r.data[0] << 8 ) | r.data[1];
        if( r.type != 0 )
            continue;
        const size_t at = ( (size_t)upper << 16 ) + r.address;
        if( image.size() < at + r.data.size() )
            image.resize( at + r.data.size(), 0xFF );
        std::copy( r.data.begin(), r.data.end(), image.begin() + (std::ptrdiff_t)at );
    }
    return image;
}
} // namespace

TEST( ImageTool, TheFirmwareAcceptsAnImageTheToolStamped )
{
    const Bytes image = flatten( readHex( KICK_STAMPED_HEX ) );
    ASSERT_EQ( 64u + 150u, image.size() ) << "header plus the fixture's 150-byte payload";

    ImageHeader h;
    ASSERT_EQ( ImageStatus::Ok, decodeImageHeader( image.data(), h ) );
    EXPECT_EQ( kFlashValid, h.flashState );
    EXPECT_EQ( ( VersionNo{ 3, 1, 4 } ), h.version );
    EXPECT_EQ( 15u, h.swRevision );
    EXPECT_EQ( image.size(), h.imageBytes );
    EXPECT_EQ( crc32( image.data() + 64, image.size() - 64 ), h.payloadCrc32 );

    auto flash = makeFlash();
    ASSERT_TRUE( writeSlot( *flash, testLayout(), Slot::A, image ) );
    EXPECT_EQ( ImageStatus::Ok, verifyImage( *flash, testLayout().baseOf( Slot::A ), testLayout().slotBytes ).status );
}

TEST( ImageTool, TheUpdaterInstallsTheToolsHexRecordByRecord )
{
    const auto records = readHex( KICK_STAMPED_HEX );
    const Bytes image  = flatten( records );

    UpdaterRig rig;
    provision( *rig.flash, rig.layout, makeImage( { 1, 0, 0 }, 100 ) );
    rig.start( Slot::A );
    ASSERT_TRUE( rig.initiate() );

    size_t sent = 0;
    for( auto& r : records )
    {
        if( r.type == 4 )
            rig.dataBase( (uint16_t)( ( r.data[0] << 8 ) | r.data[1] ) );
        else if( r.type == 0 )
            rig.data( r.address, r.data.data(), r.data.size() ), ++sent;
        else if( r.type == 1 )
            rig.send( { c::SW_UPDATE_END } );
    }
    EXPECT_GT( sent, 10u );
    EXPECT_EQ( UpdateState::Committed, rig.updater->state() );
    EXPECT_EQ( 0u, rig.replies.count( c::SW_UPDATE_FAILURE ) );

    const BootDecision d = selectBoot( *rig.flash, rig.layout );
    ASSERT_TRUE( d.bootable );
    EXPECT_EQ( Slot::B, d.slot );
    EXPECT_EQ( ( VersionNo{ 3, 1, 4 } ), d.image.header.version );
}
