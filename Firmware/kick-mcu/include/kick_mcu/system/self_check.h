// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The boot-time check of the running image, and the report the host can ask for.
#pragma once

#include <cstddef>
#include <cstdint>

#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/system/boot_control.h"
#include "kick_mcu/system/flash_layout.h"
#include "kick_mcu/system/image.h"

namespace kick_mcu
{
namespace system
{
    /// What the firmware knows about its own image. Computed once at boot and kept, so asking
    /// for it later costs nothing and does not touch flash. It does not depend on the update path:
    /// it is filled in whether or not an update is ever attempted.
    struct SelfCheckResult
    {
        ImageStatus status{ ImageStatus::NoFlash };
        uint32_t    flashState{ 0 };      ///< the header's marker (FLASH_VALID) when a header was read, else 0
        uint32_t    storedCrc32{ 0 };     ///< payload CRC-32 recorded in the header
        uint32_t    computedCrc32{ 0 };   ///< payload CRC-32 computed from flash just now
        VersionNo   version{};            ///< from the header when status is Ok, else the build version
        uint16_t    swRevision{ 0 };
        Slot        slot{ Slot::A };
        bool        trial{ false };       ///< running a trial image that has not confirmed itself yet
        bool        reverted{ false };    ///< the bootloader gave up on a trial on this boot
        bool        fellBack{ false };    ///< the recorded good slot was damaged and this is the other one

        bool ok() const { return status == ImageStatus::Ok; }
    };

    /// Verifies the slot the bootloader chose against its header: marker, header CRC, length and
    /// the CRC-32 of the whole payload, read back from flash. `flash` may be null (a sim run with
    /// no flash), which gives status NoFlash and the build version.
    SelfCheckResult runSelfCheck( bsp::IFlash* flash, const FlashLayout& layout, const BootDecision& boot, const VersionNo& buildVersion );

    /// The extended version report, 15 bytes, big endian (the order the host uses for BPM):
    ///
    ///    0  major
    ///    1  minor
    ///    2  bug
    ///    3  ImageStatus (0 is ok)
    ///    4  flashState, 4 bytes (FLASH_VALID when a header was read)
    ///    8  payload CRC-32 computed from flash, 4 bytes
    ///   12  swRevision, 2 bytes
    ///   14  flags: bit 0 slot B is running, bit 1 trial image, bit 2 bootloader reverted, bit 3 fell back
    ///
    /// The first two bytes are what the handshake's GET_SW_VER carries and what the host decodes
    /// ("major.minor", kick_AppHome.cpp), so a host that reads this frame the way it reads the
    /// handshake one still gets the right version. The 16-bit typeSoftwareInfo::ui16ProgramCheckSum
    /// is the low half of the CRC.
    constexpr size_t kVersionReportBytes = 15;
    size_t           encodeVersionReport( const SelfCheckResult& r, uint8_t out[kVersionReportBytes] );

} // namespace system
} // namespace kick_mcu
