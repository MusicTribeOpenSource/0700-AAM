// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Where things live in the MCU's flash.
#pragma once

#include <cstddef>
#include <cstdint>

#include "kick_mcu/bsp/bsp.h"

namespace kick_mcu
{
namespace system
{
    enum class Slot : uint8_t
    {
        A = 0,
        B = 1,
    };

    inline Slot otherSlot( Slot s ) { return s == Slot::A ? Slot::B : Slot::A; }
    inline const char* slotName( Slot s ) { return s == Slot::A ? "A" : "B"; }

    /// Flash map, in address order:
    ///
    ///   bootloader | slot A | slot B | boot-control copy 0 | boot-control copy 1
    ///
    /// The bootloader is never written by the application, so the update path cannot damage
    /// the code that recovers from a bad update. Each boot-control copy is one erase block. The
    /// two copies are written alternately, so one valid copy always survives a power cut.
    ///
    /// The numbers the sim uses (see makeSimLayout()) are placeholders. The real MCU part is not
    /// public, so its erase block size, slot size and address map are not known yet; only this
    /// struct changes when they are.
    struct FlashLayout
    {
        size_t blockBytes{ 0 };   ///< erase block, as the IFlash reports it
        size_t slotBase[2]{};     ///< byte address of slot A and slot B
        size_t slotBytes{ 0 };    ///< size of one slot, a multiple of blockBytes
        size_t ctlBase[2]{};      ///< byte address of the two boot-control copies

        size_t baseOf( Slot s ) const { return slotBase[(size_t)s]; }
        size_t endOfFlash() const { return ctlBase[1] + blockBytes; }

        /// True when the map fits the device and every region is block aligned and in order.
        bool fits( const bsp::IFlash& flash ) const
        {
            if( blockBytes == 0 || blockBytes != flash.eraseBlockBytes() || slotBytes == 0 || slotBytes % blockBytes != 0 )
                return false;
            if( slotBase[0] % blockBytes != 0 || slotBase[1] != slotBase[0] + slotBytes || ctlBase[0] != slotBase[1] + slotBytes || ctlBase[1] != ctlBase[0] + blockBytes )
                return false;
            return endOfFlash() <= flash.sizeBytes();
        }
    };

    /// bootloader | A | B | ctl0 | ctl1, packed from address 0.
    inline FlashLayout makeLayout( size_t blockBytes, size_t bootloaderBytes, size_t slotBytes )
    {
        FlashLayout l;
        l.blockBytes = blockBytes;
        l.slotBytes  = slotBytes;
        l.slotBase[0] = bootloaderBytes;
        l.slotBase[1] = bootloaderBytes + slotBytes;
        l.ctlBase[0]  = bootloaderBytes + 2 * slotBytes;
        l.ctlBase[1]  = l.ctlBase[0] + blockBytes;
        return l;
    }

} // namespace system
} // namespace kick_mcu
