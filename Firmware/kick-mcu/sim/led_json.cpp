// SPDX-License-Identifier: AGPL-3.0-or-later
#include "led_json.h"

namespace kick_mcu
{
namespace sim
{
    std::string ledTableJson( const output::LedFrame& frame )
    {
        std::string out = "{\"buttons\":[";
        for( unsigned i = 0; i < output::kButtonLeds; ++i )
        {
            if( i != 0 )
                out += ',';
            out += std::to_string( frame.buttons[i].state );
        }
        out += "],\"pads\":[";
        for( unsigned i = 0; i < output::kPadLeds; ++i )
        {
            const output::PadLed& p = frame.pads[i];
            if( i != 0 )
                out += ',';
            out += '[' + std::to_string( p.state ) + ',' + std::to_string( p.colour ) + ',' + std::to_string( p.offColour ) + ',' + std::to_string( p.velocity ) + ']';
        }
        out += "],\"slider\":" + std::to_string( frame.strip.percent ) + "}\n";
        return out;
    }

} // namespace sim
} // namespace kick_mcu
