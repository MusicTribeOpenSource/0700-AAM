// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pad colours: the 18 PadColourIds as RGB, and the gamma step that turns a colour into PWM duty.
#pragma once

#include <cstdint>

#include "kick_mcu/contract.h"

namespace kick_mcu
{
namespace output
{
    struct Rgb
    {
        uint8_t r{ 0 }, g{ 0 }, b{ 0 };
    };

    /// Maps a PadColourIds value to a colour. The values are the host's enum order
    /// (dcl_contracts.h), and the default RGB is copied from the emulator Surface's table, which
    /// was picked by eye. They are display colours: sRGB-encoded intent, not LED drive levels.
    /// Real pads will not look like this until someone sets them on hardware, which is why the
    /// table is settable.
    ///
    /// Id 15 (PadColourTrans) is "no colour" and renders as dark. An id past the end is clamped to
    /// the last entry, the way the Surface does.
    class ColourTable
    {
    public:
        ColourTable();

        Rgb  lookup( uint8_t id ) const { return entries_[id < contract::kPadColourCount ? id : contract::kPadColourCount - 1]; }
        bool isTransparent( uint8_t id ) const { return id == contract::kPadColourTrans; }

        /// Replaces one entry. Ids past the end and the transparent id are refused.
        bool set( uint8_t id, Rgb colour );

    private:
        Rgb entries_[contract::kPadColourCount];
    };

    /// sRGB-style transfer curve, 8 bits in and 8 bits out: duty = 255 * (value / 255) ^ gamma.
    /// An LED's light output is close to proportional to its PWM duty, and the eye is not, so
    /// without this a colour at half intensity (128) would look about three quarters as bright
    /// as full. The default is 2.2, the usual display gamma. The table is rebuilt only by
    /// setGamma(), which runs powf 256 times, so call it at start-up and not from the refresh path.
    ///
    /// The output resolution is 8 bits because ILedDriver takes 8-bit duty. A driver with more
    /// resolution would want a wider table; that is a BSP decision.
    class GammaTable
    {
    public:
        /// An exponent setGamma() refuses falls back to 2.2, so the table is never left all zero.
        explicit GammaTable( float gamma = 2.2f )
        {
            if( !setGamma( gamma ) )
                setGamma( 2.2f );
        }

        /// Rebuilds the table. The exponent is limited to 1.0 through 4.0; anything else is refused
        /// and returns false.
        bool    setGamma( float gamma );
        float   gamma() const { return gamma_; }
        uint8_t apply( uint8_t value ) const { return table_[value]; }

    private:
        float   gamma_{ 2.2f };
        uint8_t table_[256]{};
    };

} // namespace output
} // namespace kick_mcu
