// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/system/image.h"

#include "kick_mcu/system/crc32.h"

namespace kick_mcu
{
namespace system
{
    namespace
    {
        void put16( uint8_t* p, uint16_t v )
        {
            p[0] = (uint8_t)( v & 0xFF );
            p[1] = (uint8_t)( v >> 8 );
        }
        void put32( uint8_t* p, uint32_t v )
        {
            p[0] = (uint8_t)( v & 0xFF );
            p[1] = (uint8_t)( v >> 8 );
            p[2] = (uint8_t)( v >> 16 );
            p[3] = (uint8_t)( v >> 24 );
        }
        uint16_t get16( const uint8_t* p ) { return (uint16_t)( p[0] | ( p[1] << 8 ) ); }
        uint32_t get32( const uint8_t* p ) { return (uint32_t)p[0] | ( (uint32_t)p[1] << 8 ) | ( (uint32_t)p[2] << 16 ) | ( (uint32_t)p[3] << 24 ); }

        constexpr size_t kHeaderCrcOffset = 24;
    } // namespace

    const char* imageStatusName( ImageStatus status )
    {
        switch( status )
        {
            case ImageStatus::Ok: return "ok";
            case ImageStatus::ReadFailed: return "read-failed";
            case ImageStatus::Erased: return "erased";
            case ImageStatus::BadMarker: return "bad-marker";
            case ImageStatus::BadHeaderVersion: return "bad-header-version";
            case ImageStatus::BadHeaderCrc: return "bad-header-crc";
            case ImageStatus::BadLength: return "bad-length";
            case ImageStatus::BadPayloadCrc: return "bad-payload-crc";
            case ImageStatus::NoFlash: return "no-flash";
        }
        return "unknown";
    }

    void encodeImageHeader( const ImageHeader& h, uint8_t out[kImageHeaderBytes] )
    {
        for( size_t i = 0; i < kImageHeaderBytes; ++i )
            out[i] = 0;
        put32( out + 0, h.flashState );
        put16( out + 4, h.headerBytes );
        put16( out + 6, h.headerVersion );
        put32( out + 8, h.imageBytes );
        put32( out + 12, h.payloadCrc32 );
        out[16] = h.version.major;
        out[17] = h.version.minor;
        out[18] = h.version.bug;
        put16( out + 20, h.swRevision );
        put32( out + kHeaderCrcOffset, crc32( out, kHeaderCrcOffset ) );
    }

    ImageStatus decodeImageHeader( const uint8_t in[kImageHeaderBytes], ImageHeader& out )
    {
        bool allErased = true;
        for( size_t i = 0; i < kImageHeaderBytes && allErased; ++i )
            allErased = in[i] == 0xFF;
        if( allErased )
            return ImageStatus::Erased;

        if( get32( in ) != kFlashValid )
            return ImageStatus::BadMarker;
        if( get16( in + 6 ) != kImageHeaderVersion )
            return ImageStatus::BadHeaderVersion;
        if( get32( in + kHeaderCrcOffset ) != crc32( in, kHeaderCrcOffset ) )
            return ImageStatus::BadHeaderCrc;
        if( get16( in + 4 ) != kImageHeaderBytes )
            return ImageStatus::BadLength;

        out.flashState    = get32( in );
        out.headerBytes   = get16( in + 4 );
        out.headerVersion = get16( in + 6 );
        out.imageBytes    = get32( in + 8 );
        out.payloadCrc32  = get32( in + 12 );
        out.version       = { in[16], in[17], in[18] };
        out.swRevision    = get16( in + 20 );
        return ImageStatus::Ok;
    }

    ImageCheck verifyImage( bsp::IFlash& flash, size_t base, size_t slotBytes )
    {
        ImageCheck result;
        uint8_t    buffer[kImageHeaderBytes];

        if( !flash.read( base, buffer, kImageHeaderBytes ) )
        {
            result.status = ImageStatus::ReadFailed;
            return result;
        }
        result.status = decodeImageHeader( buffer, result.header );
        if( result.status != ImageStatus::Ok )
            return result;

        const ImageHeader& h = result.header;
        if( h.imageBytes <= h.headerBytes || h.imageBytes > slotBytes )
        {
            result.status = ImageStatus::BadLength;
            return result;
        }

        uint32_t crc       = crc32Init();
        size_t   offset    = h.headerBytes;
        while( offset < h.imageBytes )
        {
            const size_t piece = ( h.imageBytes - offset ) < sizeof( buffer ) ? ( h.imageBytes - offset ) : sizeof( buffer );
            if( !flash.read( base + offset, buffer, piece ) )
            {
                result.status = ImageStatus::ReadFailed;
                return result;
            }
            crc = crc32Update( crc, buffer, piece );
            offset += piece;
        }
        result.computedCrc32 = crc32Final( crc );
        result.status        = result.computedCrc32 == h.payloadCrc32 ? ImageStatus::Ok : ImageStatus::BadPayloadCrc;
        return result;
    }

} // namespace system
} // namespace kick_mcu
