// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The header stamped at the start of every firmware image, and the check that decides whether
// a slot holds a whole, uncorrupted image.
#pragma once

#include <cstddef>
#include <cstdint>

#include "kick_mcu/bsp/bsp.h"

namespace kick_mcu
{
namespace system
{
    /// The valid marker the host's typeSoftwareInfo::ui32FlashState uses (Defines.hpp, FLASH_VALID).
    /// tests/system_contract_test.cpp checks the two stay equal.
    constexpr uint32_t kFlashValid = 0x12345678u;

    constexpr size_t   kImageHeaderBytes   = 64;
    constexpr uint16_t kImageHeaderVersion = 1;

    /// typeVersionNo from Defines.hpp, without the alignment byte. The handshake reports major
    /// and minor only; bug travels in the extended report (see self_check.h).
    struct VersionNo
    {
        uint8_t major{ 0 };
        uint8_t minor{ 0 };
        uint8_t bug{ 0 };

        bool operator==( const VersionNo& o ) const { return major == o.major && minor == o.minor && bug == o.bug; }
    };

    /// Header layout, 64 bytes, little endian (the byte order of the Cortex-M and RISC-V parts
    /// this MCU is likely to be; the value is arbitrary and only tools/mcu-image-header.py and
    /// this file have to agree on it):
    ///
    ///    0  u32  flashState     FLASH_VALID
    ///    4  u16  headerBytes    64
    ///    6  u16  headerVersion  1
    ///    8  u32  imageBytes     header plus payload; the payload is everything after the header
    ///   12  u32  payloadCrc32   CRC-32 over bytes [headerBytes, imageBytes)
    ///   16  u8   major
    ///   17  u8   minor
    ///   18  u8   bug
    ///   19  u8   0
    ///   20  u16  swRevision    typeSoftwareInfo::ui16SwRevisionNo
    ///   22  u16  0
    ///   24  u32  headerCrc32   CRC-32 over bytes 0 to 23
    ///   28  u32  0
    ///   32  u8[32] signature   reserved for a signature over bytes 0 to 31 and the payload CRC. Not
    ///                          written and not checked: see docs/mcu-update-protocol.md, "Image authenticity".
    ///
    /// typeSoftwareInfo::ui16ProgramCheckSum is 16 bits wide. The full 32-bit payloadCrc32 is what
    /// gets checked; the report sends the 32-bit value and a host that wants the 16-bit field takes
    /// the low half.
    struct ImageHeader
    {
        uint32_t  flashState{ kFlashValid };
        uint16_t  headerBytes{ kImageHeaderBytes };
        uint16_t  headerVersion{ kImageHeaderVersion };
        uint32_t  imageBytes{ 0 };
        uint32_t  payloadCrc32{ 0 };
        VersionNo version{};
        uint16_t  swRevision{ 0 };
    };

    enum class ImageStatus : uint8_t
    {
        Ok = 0,
        ReadFailed,       ///< the flash read itself failed
        Erased,           ///< the header area is all 0xFF: nothing was ever written
        BadMarker,        ///< flashState is not FLASH_VALID
        BadHeaderVersion, ///< a header layout this firmware does not know
        BadHeaderCrc,     ///< the header was corrupted or only partly written
        BadLength,        ///< imageBytes smaller than the header or larger than the slot
        BadPayloadCrc,    ///< the payload does not match the CRC in the header
        NoFlash,          ///< the build has no flash attached (sim without --flash); not an image fault
    };

    const char* imageStatusName( ImageStatus status );

    /// Writes the 64-byte header, computing headerCrc32. The signature field is left zero.
    void encodeImageHeader( const ImageHeader& header, uint8_t out[kImageHeaderBytes] );

    /// Reads a header. Checks the marker, header version, header CRC and that headerBytes is 64.
    /// It does not check the length against a slot or the payload; verifyImage() does.
    ImageStatus decodeImageHeader( const uint8_t in[kImageHeaderBytes], ImageHeader& out );

    struct ImageCheck
    {
        ImageStatus status{ ImageStatus::ReadFailed };
        ImageHeader header{};
        uint32_t    computedCrc32{ 0 }; ///< the CRC just computed over the payload, valid once the length checked out
    };

    /// Reads the image at `base` (a slot of `slotBytes`) back from flash and checks it end to end:
    /// header, length, then a CRC-32 over the whole payload. It reads in 64-byte pieces and keeps
    /// no copy of the image. Cost is one pass over the image, so a boot check is a few
    /// milliseconds per 100 KiB at flash read speed.
    ImageCheck verifyImage( bsp::IFlash& flash, size_t base, size_t slotBytes );

} // namespace system
} // namespace kick_mcu
