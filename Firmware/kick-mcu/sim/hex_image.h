// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Intel HEX in, flat bytes out, for loading a stamped firmware image into the simulated flash.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kick_mcu
{
namespace sim
{
    /// Reads record types 00 (data), 01 (end of file) and 04 (extended linear address) and returns
    /// the bytes from address 0 to the highest one, gaps 0xFF. Anything else, a bad checksum or a
    /// missing end-of-file record sets `error` and returns an empty vector. These are the same
    /// types tools/mcu-image-header.py writes, and the only ones the host updater handles.
    std::vector<uint8_t> hexToImage( const std::string& text, std::string& error );

} // namespace sim
} // namespace kick_mcu
