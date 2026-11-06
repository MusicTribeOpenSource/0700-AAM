// SPDX-License-Identifier: AGPL-3.0-or-later
#include "trace_replay.h"

#include <cstdlib>
#include <sstream>
#include <vector>

namespace kick_mcu
{
namespace sim
{
    TraceReplayer::TraceReplayer()
    {
        firmware_ = std::make_unique<app::Firmware>( uart_, clock_, app::InputHardware{ board_.matrix, board_.pads, board_.encoders, board_.strip }, app::OutputHardware{ driver_ } );
    }

    void TraceReplayer::pump( int passes )
    {
        for( int i = 0; i < passes; ++i )
        {
            firmware_->step();
            clock_.advanceUs( 250 );
            uart_.drain();
            uart_.clearWire();
        }
    }

    bool TraceReplayer::run( const std::string& traceText )
    {
        error_.clear();
        std::istringstream in( traceText );
        std::string        line;
        int                lineNo = 0;
        while( std::getline( in, line ) )
        {
            ++lineNo;
            if( line.empty() || line[0] == '#' )
                continue;
            std::istringstream fields( line );
            std::string        ms, tag;
            fields >> ms >> tag;
            if( tag != "RX" )
                continue;

            std::vector<uint8_t> bytes;
            std::string          tok;
            while( fields >> tok )
            {
                char*      end = nullptr;
                const long v   = std::strtol( tok.c_str(), &end, 16 );
                if( end == tok.c_str() || *end != '\0' || v < 0 || v > 255 )
                {
                    error_ = "trace line " + std::to_string( lineNo ) + ": bad hex byte '" + tok + "'";
                    return false;
                }
                bytes.push_back( (uint8_t)v );
            }
            ++rxLines_;
            rxBytes_ += bytes.size();

            // The receive ring holds 255 bytes; hand it half that at a time.
            for( size_t at = 0; at < bytes.size(); at += 120 )
            {
                const size_t n = bytes.size() - at < 120 ? bytes.size() - at : 120;
                uart_.injectRx( bytes.data() + at, n );
                pump( 8 );
            }
        }
        if( rxLines_ == 0 )
        {
            error_ = "the trace has no RX lines";
            return false;
        }
        pump( 100 ); // let the refresh draw the final table
        return true;
    }

} // namespace sim
} // namespace kick_mcu
