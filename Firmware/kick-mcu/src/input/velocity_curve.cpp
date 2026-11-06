// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/input/velocity_curve.h"

#include <cmath>

namespace kick_mcu
{
namespace input
{
    namespace
    {
        uint8_t clampVelocity( double v )
        {
            const double r = std::floor( v + 0.5 );
            return r < 1.0 ? 1 : r > 127.0 ? 127 : (uint8_t)r;
        }
    } // namespace

    void VelocityCurve::load( Preset preset )
    {
        // The exponent is applied once here, at load time, so scanning does only a table lookup.
        const double exponent = preset == Preset::Soft ? 0.6 : preset == Preset::Hard ? 1.6 : 1.0;
        for( size_t i = 0; i < kSize; ++i )
            table_[i] = clampVelocity( 127.0 * std::pow( (double)i / 127.0, exponent ) );
    }

    bool VelocityCurve::set( const uint8_t* table, size_t count )
    {
        if( table == nullptr )
            return false;
        for( size_t i = 0; i < kSize; ++i )
        {
            // A short table repeats its last entry, so a partial curve still covers every input.
            const uint8_t v = count == 0 ? 1 : table[i < count ? i : count - 1];
            table_[i]       = v < 1 ? 1 : v > 127 ? 127 : v;
        }
        return true;
    }

} // namespace input
} // namespace kick_mcu
