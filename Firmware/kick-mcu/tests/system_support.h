// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Shared pieces for the system and update tests: a small flash map, image builders, and a host
// that speaks the update protocol to an Updater.
#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "kick_mcu/contract.h"
#include "kick_mcu/system/boot_control.h"
#include "kick_mcu/system/crc32.h"
#include "kick_mcu/system/flash_layout.h"
#include "kick_mcu/system/image.h"
#include "kick_mcu/system/updater.h"
#include "sim_clock.h"
#include "sim_flash.h"

namespace system_support
{
using Bytes = std::vector<uint8_t>;
using namespace kick_mcu;
using namespace kick_mcu::system;

/// 64-byte erase blocks, a 128-byte bootloader, two 256-byte slots, two control blocks: 768 bytes.
/// Small on purpose, so a sweep that cuts power at every byte operation stays cheap.
inline FlashLayout testLayout() { return makeLayout( 64, 128, 256 ); }

inline std::unique_ptr<sim::SimFlash> makeFlash( const FlashLayout& layout = testLayout() ) { return std::make_unique<sim::SimFlash>( layout.endOfFlash(), layout.blockBytes ); }

/// A whole image: 64-byte header and `payloadBytes` of repeatable filler, with a good CRC.
inline Bytes makeImage( VersionNo version, size_t payloadBytes, uint8_t seed = 1, uint16_t revision = 7 )
{
    Bytes payload( payloadBytes );
    for( size_t i = 0; i < payloadBytes; ++i )
        payload[i] = (uint8_t)( seed * 31u + i * 7u + ( i >> 3 ) );
    ImageHeader h;
    h.imageBytes   = (uint32_t)( kImageHeaderBytes + payloadBytes );
    h.payloadCrc32 = crc32( payload.data(), payload.size() );
    h.version      = version;
    h.swRevision   = revision;
    Bytes image( kImageHeaderBytes );
    encodeImageHeader( h, image.data() );
    image.insert( image.end(), payload.begin(), payload.end() );
    return image;
}

/// Puts an image in a slot the way a factory programmer would: erase, then write.
inline bool writeSlot( bsp::IFlash& flash, const FlashLayout& layout, Slot slot, const Bytes& image )
{
    for( size_t a = 0; a < layout.slotBytes; a += layout.blockBytes )
        if( !flash.eraseBlock( layout.baseOf( slot ) + a ) )
            return false;
    return flash.write( layout.baseOf( slot ), image.data(), image.size() );
}

/// Factory state: `image` in slot A, boot control confirming A, slot B erased.
inline void provision( bsp::IFlash& flash, const FlashLayout& layout, const Bytes& image )
{
    ASSERT_TRUE( writeSlot( flash, layout, Slot::A, image ) );
    BootControl control( flash, layout );
    ASSERT_TRUE( control.confirm( Slot::A ) );
}

/// Collects the Updater's replies.
struct Replies
{
    std::vector<Bytes> got;
    static bool        sink( void* context, const uint8_t* payload, size_t length )
    {
        static_cast<Replies*>( context )->got.emplace_back( payload, payload + length );
        return true;
    }
    uint8_t last() const { return got.empty() ? 0xEE : got.back()[0]; }
    size_t  count( uint8_t sub ) const
    {
        size_t n = 0;
        for( auto& r : got )
            n += r.size() == 1 && r[0] == sub;
        return n;
    }
};

/// An Updater on a flash, with a host that speaks the protocol the unmodified host speaks.
struct UpdaterRig
{
    FlashLayout                     layout{ testLayout() };
    std::unique_ptr<sim::SimFlash>  flash{ makeFlash( layout ) };
    sim::SimClock                   clock;
    sim::SimPower                   power;
    Replies                         replies;
    std::unique_ptr<Updater>        updater;
    UpdaterConfig                   config;

    void start( Slot running = Slot::A )
    {
        updater = std::make_unique<Updater>( *flash, layout, running, &power, clock, &Replies::sink, &replies, config );
    }

    void send( const Bytes& payload ) { updater->onFrame( payload.data(), payload.size() ); }

    /// INITIATE, then poll until BEGIN arrives or the transfer ends.
    bool initiate()
    {
        replies.got.clear();
        send( { contract::SW_UPDATE_INITIATE } );
        for( int i = 0; i < 64 && updater->state() == UpdateState::Erasing; ++i )
            updater->poll();
        return updater->state() == UpdateState::Receiving && replies.last() == contract::SW_UPDATE_BEGIN_TRANSFER;
    }

    /// One DATA record as the host builds it: cmd, address, the bytes, and one stale byte.
    void data( uint32_t offset, const uint8_t* bytes, size_t n, uint8_t stale = 0x5A )
    {
        Bytes p{ contract::SW_UPDATE_DATA, (uint8_t)( offset >> 8 ), (uint8_t)offset };
        p.insert( p.end(), bytes, bytes + n );
        p.push_back( stale );
        send( p );
    }

    void dataBase( uint16_t ulba ) { send( { contract::SW_UPDATE_DATABASE, (uint8_t)( ulba >> 8 ), (uint8_t)ulba } ); }

    /// DATABASE 0, then every record of `recordBytes`, then END. Stops early if the transfer ends.
    /// `skipLast` records are left out at the end (a truncated transfer).
    void transfer( const Bytes& image, size_t recordBytes = 16, size_t skipLast = 0, bool sendEnd = true )
    {
        dataBase( 0 );
        size_t records = ( image.size() + recordBytes - 1 ) / recordBytes;
        records -= skipLast;
        for( size_t r = 0; r < records && updater->state() == UpdateState::Receiving; ++r )
        {
            const size_t offset = r * recordBytes;
            const size_t n      = std::min( recordBytes, image.size() - offset );
            data( (uint32_t)offset, image.data() + offset, n );
        }
        if( sendEnd )
            send( { contract::SW_UPDATE_END } );
    }

    /// Runs out the reset delay so a committed update asks for its reset.
    void runResetDelay()
    {
        clock.advanceMs( config.resetDelayMs + 1 );
        updater->poll();
    }
};

} // namespace system_support
