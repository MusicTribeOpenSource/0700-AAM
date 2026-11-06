// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/link/frame_codec.h"

namespace kick_mcu
{
namespace link
{
    uint8_t crc8Update( uint8_t crc, uint8_t byte )
    {
        crc ^= byte;
        for( int bit = 0; bit < 8; ++bit )
            crc = ( crc & 0x80 ) ? (uint8_t)( ( crc << 1 ) ^ contract::kCrcPoly ) : (uint8_t)( crc << 1 );
        return crc;
    }

    uint8_t crc8( const uint8_t* data, size_t length )
    {
        uint8_t crc = contract::kCrcInit;
        for( size_t i = 0; i < length; ++i )
            crc = crc8Update( crc, data[i] );
        return crc;
    }

    namespace
    {
        size_t payloadLimit( Wire wire )
        {
            return wire == Wire::McuToHost ? contract::kMaxPayloadToHost : contract::kMaxPayloadFromHost;
        }
    } // namespace

    size_t encodedSize( Wire wire, size_t payloadLength )
    {
        if( payloadLength > payloadLimit( wire ) )
            return 0;
        return contract::kHeaderBytes + payloadLength + contract::kCrcBytes + ( wire == Wire::McuToHost ? 1 : 0 );
    }

    size_t encodeFrame( uint8_t* out, size_t capacity, Wire wire, uint8_t type, uint8_t dir, uint8_t cmd, const uint8_t* payload, size_t length )
    {
        const size_t total = encodedSize( wire, length );
        if( total == 0 || total > capacity )
            return 0;

        size_t n = 0;
        out[n++] = contract::kStartByte1;
        out[n++] = contract::kStartByte2;
        out[n++] = type;
        out[n++] = dir;
        out[n++] = cmd;
        out[n++] = (uint8_t)( length + contract::kCrcBytes );
        for( size_t i = 0; i < length; ++i )
            out[n++] = payload[i];
        out[n]     = crc8( out, n );
        n += contract::kCrcBytes;
        if( wire == Wire::McuToHost )
            out[n++] = contract::kMcuToHostTrailer;
        return n;
    }

} // namespace link
} // namespace kick_mcu
