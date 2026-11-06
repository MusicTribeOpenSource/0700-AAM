// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Boot control: which slot runs, and whether a freshly written image has earned its place.
//
// This file holds both halves. selectBoot() is the bootloader's decision and runs before the
// application starts; BootControl is what the application uses to start a trial and to confirm
// it. On the MCU they live in different binaries, but they share the record format, so the
// format code is written once.
#pragma once

#include <cstddef>
#include <cstdint>

#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/system/flash_layout.h"
#include "kick_mcu/system/image.h"

namespace kick_mcu
{
namespace system
{
    enum class BootState : uint8_t
    {
        Confirmed    = 1, ///< `good` is the known-good slot and runs
        TrialPending = 2, ///< an update was verified into `trial`; the next boot should try it once
        TrialBooted  = 3, ///< the bootloader started `trial`; if it is not confirmed, the next boot reverts
    };

    /// One boot-control record, 16 bytes in flash, little endian:
    ///
    ///    0  u32  magic  'KBC1' (0x3143424B)
    ///    4  u32  seq    newest wins; compared with wraparound, so 0xFFFFFFFF is followed by 0
    ///    8  u8   state  BootState
    ///    9  u8   good   Slot of the last confirmed image
    ///   10  u8   trial  Slot of the image under trial (equals `good` when state is Confirmed)
    ///   11  u8   0
    ///   12  u32  crc32  CRC-32 over bytes 0 to 11
    struct BootRecord
    {
        uint32_t  seq{ 0 };
        BootState state{ BootState::Confirmed };
        Slot      good{ Slot::A };
        Slot      trial{ Slot::A };
    };

    constexpr size_t kBootRecordBytes = 16;

    /// Reads and writes the two boot-control copies.
    ///
    /// store() erases and rewrites the copy that does NOT hold the newest valid record, then the
    /// roles swap. At every point of that sequence the newest valid copy is untouched, so a power
    /// cut at any byte leaves either the old record or the new one, never neither. A copy that
    /// was cut mid-write fails its CRC and is ignored.
    class BootControl
    {
    public:
        BootControl( bsp::IFlash& flash, const FlashLayout& layout )
            : flash_( flash )
            , layout_( layout )
        {
        }

        /// The newest valid record. False when neither copy is valid (first boot, or both damaged).
        bool load( BootRecord& out ) const;

        /// Writes a record with seq = newest + 1 (or 1 when there is none). False when a flash
        /// operation failed; the previous record is still the newest valid one then.
        bool store( BootState state, Slot good, Slot trial );

        /// An update was verified into `trial`: boot it once. `good` is the slot now running.
        bool commitTrial( Slot good, Slot trial ) { return store( BootState::TrialPending, good, trial ); }

        /// The running slot passed its checks: it becomes the known-good one.
        bool confirm( Slot running ) { return store( BootState::Confirmed, running, running ); }

    private:
        bool readCopy( unsigned copy, BootRecord& out ) const;

        bsp::IFlash&      flash_;
        const FlashLayout layout_;
    };

    struct BootDecision
    {
        bool       bootable{ false };       ///< false: no valid image anywhere; the bootloader stays in recovery
        Slot       slot{ Slot::A };         ///< the slot to run
        bool       trial{ false };          ///< it is a trial boot: the application must confirm it
        bool       reverted{ false };       ///< this boot gave up on a trial (never confirmed, or failed its check)
        bool       fellBack{ false };       ///< the recorded good slot was damaged and the other slot runs
        bool       recordMissing{ false };  ///< no valid boot-control copy was found
        bool       controlWriteFailed{ false }; ///< a boot-control write failed; the decision still stands
        BootRecord record{};                ///< the record as it stands after this decision
        ImageCheck image{};                 ///< the check of the slot chosen
    };

    /// The bootloader's decision, made on every reset. It may write the boot-control record.
    ///
    ///   Confirmed     run `good`; if `good` is damaged run the other slot when that is valid
    ///   TrialPending  verify `trial`; if valid, mark TrialBooted and run it; otherwise stay on `good`
    ///   TrialBooted   the trial was started and never confirmed: mark Confirmed(good) and run `good`
    ///   no record     run the first valid slot, A before B, and write a Confirmed record
    ///
    /// A trial is marked TrialBooted BEFORE it runs. A power cut after the mark and before the
    /// first instruction costs that trial (the next boot reverts), never the device. If the mark
    /// cannot be written, the trial is not started.
    BootDecision selectBoot( bsp::IFlash& flash, const FlashLayout& layout );

} // namespace system
} // namespace kick_mcu
