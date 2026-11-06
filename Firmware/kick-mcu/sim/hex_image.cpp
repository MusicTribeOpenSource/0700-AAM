// SPDX-License-Identifier: AGPL-3.0-or-later
#include "hex_image.h"

#include <cstdlib>
#include <sstream>

namespace kick_mcu
{
namespace sim
{
    std::vector<uint8_t> hexToImage( const std::string& text, std::string& error )
    {
        std::vector<uint8_t> image;
        uint32_t             upper = 0;
        bool                 eof   = false;
        std::istringstream   in( text );
        std::string          line;
        size_t               lineNo = 0;

        auto fail = [&]( const std::string& why ) {
            error = "hex line " + std::to_string( lineNo ) + ": " + why;
            return std::vector<uint8_t>{};
        };

        while( std::getline( in, line ) )
        {
            ++lineNo;
            while( !line.empty() && ( line.back() == '\r' || line.back() == ' ' ) )
                line.pop_back();
            if( line.empty() )
                continue;
            if( line[0] != ':' || line.size() < 11 || ( line.size() - 1 ) % 2 != 0 )
                return fail( "not a record" );
            std::vector<unsigned> b;
            for( size_t i = 1; i + 1 < line.size() + 1; i += 2 )
            {
                char* end = nullptr;
                const std::string pair = line.substr( i, 2 );
                const unsigned long v  = std::strtoul( pair.c_str(), &end, 16 );
                if( end != pair.c_str() + 2 )
                    return fail( "not hex" );
                b.push_back( (unsigned)v );
            }
            if( b.size() != b[0] + 5u )
                return fail( "length does not match" );
            unsigned sum = 0;
            for( unsigned v : b )
                sum += v;
            if( ( sum & 0xFF ) != 0 )
                return fail( "bad checksum" );
            if( eof )
                return fail( "data after the end-of-file record" );

            const unsigned type = b[3], address = ( b[1] << 8 ) | b[2];
            if( type == 0 )
            {
                const size_t at = ( (size_t)upper << 16 ) + address;
                if( image.size() < at + b[0] )
                    image.resize( at + b[0], 0xFF );
                for( unsigned i = 0; i < b[0]; ++i )
                    image[at + i] = (uint8_t)b[4 + i];
            }
            else if( type == 1 )
                eof = true;
            else if( type == 4 && b[0] == 2 )
                upper = ( b[4] << 8 ) | b[5];
            else
                return fail( "record type " + std::to_string( type ) + " is not supported" );
        }
        if( !eof )
        {
            error = "no end-of-file record";
            return {};
        }
        return image;
    }

} // namespace sim
} // namespace kick_mcu
