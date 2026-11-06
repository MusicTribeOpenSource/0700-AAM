// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CRC-32 for the firmware image and the boot-control record.
#pragma once

#include <cstddef>
#include <cstdint>

namespace kick_mcu
{
namespace system
{
    /// CRC-32/ISO-HDLC, the one zlib, Ethernet and Python's binascii.crc32 compute: reflected
    /// polynomial 0xEDB88320, initial value 0xFFFFFFFF, final xor 0xFFFFFFFF. Check value for
    /// "123456789" is 0xCBF43926.
    ///
    /// Streaming use: start with crc32Init(), feed chunks through crc32Update(), finish with
    /// crc32Final(). It works a nibble at a time with a 16-entry table, so it costs 64 bytes of
    /// flash rather than the 1 KiB a byte table needs, which matters on a small MCU. The sixteen
    /// entries are the single-nibble CRC steps of the polynomial.
    inline uint32_t crc32Init() { return 0xFFFFFFFFu; }

    inline uint32_t crc32Update( uint32_t state, const uint8_t* data, size_t length )
    {
        static const uint32_t kNibble[16] = {
            0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
            0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
        };
        for( size_t i = 0; i < length; ++i )
        {
            state ^= data[i];
            state = ( state >> 4 ) ^ kNibble[state & 0x0Fu];
            state = ( state >> 4 ) ^ kNibble[state & 0x0Fu];
        }
        return state;
    }

    inline uint32_t crc32Final( uint32_t state ) { return state ^ 0xFFFFFFFFu; }

    inline uint32_t crc32( const uint8_t* data, size_t length ) { return crc32Final( crc32Update( crc32Init(), data, length ) ); }

} // namespace system
} // namespace kick_mcu
