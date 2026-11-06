// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <vector>

#include "kick_mcu/link/frame_codec.h"
#include "kick_mcu/link/rx_parser.h"

namespace testing_support
{
using Bytes = std::vector<uint8_t>;

struct Decoded
{
    uint8_t type, dir, cmd;
    Bytes   payload;

    bool operator==( const Decoded& o ) const { return type == o.type && dir == o.dir && cmd == o.cmd && payload == o.payload; }
};

/// Collects every frame an RxParser delivers.
struct Collector
{
    std::vector<Decoded> frames;

    static void onFrame( void* context, const kick_mcu::link::Frame& f )
    {
        static_cast<Collector*>( context )->frames.push_back( { f.type, f.dir, f.cmd, Bytes( f.payload, f.payload + f.length ) } );
    }

    void attach( kick_mcu::link::RxParser& parser ) { parser.setHandler( &Collector::onFrame, this ); }
};

inline Bytes encode( kick_mcu::link::Wire wire, uint8_t type, uint8_t cmd, const Bytes& payload, uint8_t dir = 0 )
{
    uint8_t buf[128];
    size_t  n = kick_mcu::link::encodeFrame( buf, sizeof( buf ), wire, type, dir, cmd, payload.data(), payload.size() );
    return Bytes( buf, buf + n );
}

inline Bytes toHost( uint8_t type, uint8_t cmd, const Bytes& payload ) { return encode( kick_mcu::link::Wire::McuToHost, type, cmd, payload ); }
inline Bytes fromHost( uint8_t type, uint8_t cmd, const Bytes& payload ) { return encode( kick_mcu::link::Wire::HostToMcu, type, cmd, payload ); }

inline void feedAll( kick_mcu::link::RxParser& parser, const Bytes& bytes )
{
    for( uint8_t b : bytes )
        parser.feed( b );
}

inline Bytes cat( std::initializer_list<Bytes> parts )
{
    Bytes out;
    for( const auto& p : parts )
        out.insert( out.end(), p.begin(), p.end() );
    return out;
}

/// xorshift32, so tests that need noise get the same noise every run.
struct Rng
{
    uint32_t s;
    explicit Rng( uint32_t seed )
        : s( seed ? seed : 1 )
    {
    }
    uint32_t next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    uint32_t below( uint32_t n ) { return next() % n; }
};

} // namespace testing_support
