// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/output/colours.h"

#include <cmath>

namespace kick_mcu
{
namespace output
{
    ColourTable::ColourTable()
    {
        // Same order as DeviceControlContracts::PadColourIds; values from emu/surface/Main.cpp.
        static constexpr uint32_t kDefaults[contract::kPadColourCount] = {
            0x8b0000, // 0  DarkRed
            0x9f00ff, // 1  VividPurple
            0xb8a000, // 2  DarkYellow
            0x90ee90, // 3  lightGreen1
            0x4cd964, // 4  lightGreen3
            0x2e8b57, // 5  lightGreen4
            0x87cefa, // 6  LightSkyBlue
            0x00ffff, // 7  Aqua
            0x1e5bff, // 8  Blue1
            0xff69b4, // 9  Pink
            0xff4fa3, // 10 Pink1
            0xff1493, // 11 Pink2
            0xff2020, // 12 Red
            0xffa040, // 13 Orange1
            0xff8000, // 14 Orange
            0x000000, // 15 Trans: no colour
            0xffffff, // 16 NeutralWhite
            0x808080, // 17 NeutralGrey
        };
        for( unsigned i = 0; i < contract::kPadColourCount; ++i )
            entries_[i] = { (uint8_t)( kDefaults[i] >> 16 ), (uint8_t)( kDefaults[i] >> 8 ), (uint8_t)kDefaults[i] };
    }

    bool ColourTable::set( uint8_t id, Rgb colour )
    {
        if( id >= contract::kPadColourCount || id == contract::kPadColourTrans )
            return false;
        entries_[id] = colour;
        return true;
    }

    bool GammaTable::setGamma( float gamma )
    {
        if( !( gamma >= 1.0f && gamma <= 4.0f ) ) // also rejects NaN
            return false;
        gamma_ = gamma;
        for( unsigned i = 0; i < 256; ++i )
            table_[i] = (uint8_t)std::lround( 255.0f * std::pow( (float)i / 255.0f, gamma ) );
        return true;
    }

} // namespace output
} // namespace kick_mcu
