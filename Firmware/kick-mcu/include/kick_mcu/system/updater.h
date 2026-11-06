// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The MCU side of EN_SYS_SW_UPDATE: receives a new firmware image from the host into the idle
// slot, verifies it, and arms a trial boot. The running image is never written.
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
    enum class UpdateState : uint8_t
    {
        Idle,
        Erasing,   ///< INITIATE accepted; the idle slot is being erased a block per poll()
        Receiving, ///< BEGIN sent; DATA and DATABASE records are being written
        Committed, ///< END verified and the trial armed; waiting out the reset delay
    };

    enum class UpdateFailure : uint8_t
    {
        None = 0,
        NoFlash,       ///< this build has no flash to update
        NotConfirmed,  ///< the running image is not the confirmed one, so there is no safe fallback
        FlashError,    ///< an erase, write or boot-control write failed
        OutOfSlot,     ///< a record addressed bytes outside the slot
        WriteConflict, ///< a record would need to set a bit that is already cleared: the slot was not erased
        BadFrame,      ///< wrong length for the subcommand
        BadImage,      ///< END arrived and the image in flash did not verify
        NotStarted,    ///< DATA, DATABASE or END without a transfer in progress
        Busy,          ///< INITIATE after a committed update, before the reset
        Timeout,       ///< the host went quiet for idleTimeoutMs
    };

    const char* updateFailureName( UpdateFailure failure );

    struct UpdaterConfig
    {
        /// No frame from the host for this long ends the transfer. The host has no timeout of its
        /// own and waits forever for an ACK, so this is also what ends a stuck exchange: the MCU
        /// sends FAILURE, which unblocks the host.
        uint32_t idleTimeoutMs{ 5000 };

        /// After END is acknowledged the MCU waits this long before resetting into the trial, so
        /// the ACK has left the UART. 500 ms is far longer than the few milliseconds a frame takes.
        uint32_t resetDelayMs{ 500 };
    };

    struct UpdaterStats
    {
        uint32_t started{ 0 };        ///< INITIATE accepted
        uint32_t completed{ 0 };      ///< END verified and committed
        uint32_t failed{ 0 };         ///< transfers ended with FAILURE
        uint32_t timeouts{ 0 };       ///< of those, ended by the idle timeout
        uint32_t records{ 0 };        ///< DATA records written
        uint32_t duplicates{ 0 };     ///< DATA records that matched what was already in flash and were not rewritten
        uint32_t ignored{ 0 };        ///< frames dropped without a reply (empty, or a subcommand the host never sends)
        UpdateFailure lastFailure{ UpdateFailure::None };
    };

    /// Update state machine. The wire protocol, as the unmodified host uses it
    /// (kick_FirmwareUpdate.cpp, kick_SWUpdateScreen.cpp), with payloads after the SYS_SW_UPDATE cmd:
    ///
    ///   host -> [0]                          INITIATE
    ///   MCU  -> [1]                          BEGIN_TRANSFER, once the idle slot is erased
    ///   host -> [3, ulbaHi, ulbaLo]          DATABASE: set the upper 16 address bits (Intel HEX type 04)
    ///   MCU  -> [5]                          ACK
    ///   host -> [2, addrHi, addrLo, d0..dn-1, stale]   DATA: one Intel HEX type 00 record
    ///   MCU  -> [5]                          ACK
    ///   ...
    ///   host -> [4]                          END
    ///   MCU  -> [5] if the image verified and the trial is armed, [6] if not
    ///   MCU  -> [6]                          FAILURE, at any point; the transfer is over
    ///
    /// The host treats any reply that is not [1] or [6] as an ACK. It claims success after sending
    /// END without waiting for the MCU's answer, so the END reply is for the log and the tests; no
    /// current host code acts on it.
    ///
    /// The DATA payload carries one byte more than the record: the host sends binaryData[0..n+2]
    /// for an n-byte record, so the last byte is stale data left over from an earlier record. The
    /// MCU drops it, which means a record's data length is payload length minus 4. If a future host
    /// stops sending the extra byte this would drop a real data byte; the CRC check at END then
    /// fails the transfer instead of installing a damaged image.
    ///
    /// Addresses are offsets from the start of the image, not flash addresses. The updater adds the
    /// base of the idle slot. The image header is at offset 0.
    ///
    /// Safety, in order:
    ///  1. Only the slot that is not running is ever erased or written.
    ///  2. Nothing is armed until END, and END arms nothing unless the whole image reads back from
    ///     flash with a good header, length and CRC-32.
    ///  3. Arming writes a boot-control record, which is atomic (see BootControl). Until that write
    ///     lands, a reset boots the old image; after it, the bootloader tries the new one once.
    ///  4. The new image has to confirm itself (BootControl::confirm) or the next reset returns to
    ///     the old one.
    ///
    /// Not thread-safe: onFrame() and poll() belong to the main loop.
    class Updater
    {
    public:
        /// Sends one SYS_SW_UPDATE payload to the host. Returns false if it could not be queued.
        using ReplyFn = bool ( * )( void* context, const uint8_t* payload, size_t length );

        /// `running` is the slot the bootloader started. `power` may be null (no reset after commit).
        Updater( bsp::IFlash& flash, const FlashLayout& layout, Slot running, bsp::IPower* power, const bsp::IClock& clock, ReplyFn reply, void* replyContext, const UpdaterConfig& config = {} );

        /// One SYS_SW_UPDATE frame from the host. `payload[0]` is the subcommand.
        void onFrame( const uint8_t* payload, size_t length );

        /// Erases one block if an erase is due, enforces the idle timeout, and resets after a commit.
        void poll();

        UpdateState         state() const { return state_; }
        const UpdaterStats& stats() const { return stats_; }
        Slot                target() const { return target_; }

    private:
        void begin();
        void data( const uint8_t* payload, size_t length );
        void dataBase( const uint8_t* payload, size_t length );
        void end();
        void reject( UpdateFailure why ); ///< sends FAILURE and counts it, leaves the state alone
        void fail( UpdateFailure why );   ///< reject() and back to Idle
        void ack();
        void send( const uint8_t* payload, size_t length );
        void touch() { lastActivityMs_ = clock_.nowMs(); }

        bsp::IFlash&       flash_;
        FlashLayout        layout_;
        BootControl        control_;
        Slot               running_;
        Slot               target_;
        bsp::IPower*       power_;
        const bsp::IClock& clock_;
        ReplyFn            reply_;
        void*              replyContext_;
        UpdaterConfig      config_;

        UpdateState  state_{ UpdateState::Idle };
        size_t       nextEraseBlock_{ 0 };
        uint32_t     ulba_{ 0 };
        uint32_t     lastActivityMs_{ 0 };
        uint32_t     resetAtMs_{ 0 };
        bool         resetRequested_{ false };
        UpdaterStats stats_{};
    };

} // namespace system
} // namespace kick_mcu
