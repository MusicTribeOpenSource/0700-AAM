// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The SYSTEM commands the session leaves alone: version query, tempo, shutdown and software update.
#pragma once

#include <cstdint>

#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/link/frame_codec.h"
#include "kick_mcu/link/tx_queue.h"
#include "kick_mcu/system/boot_control.h"
#include "kick_mcu/system/bpm.h"
#include "kick_mcu/system/flash_layout.h"
#include "kick_mcu/system/self_check.h"
#include "kick_mcu/system/shutdown.h"
#include "kick_mcu/system/updater.h"
#include "kick_mcu/system/version.h"

namespace kick_mcu
{
namespace system
{
    /// What the system layer needs from the board and from the bootloader. All pointers are
    /// borrowed and may be null: with no flash the update commands answer FAILURE and the version
    /// is the build version, with no power the shutdown and reset requests have nowhere to go.
    struct SystemConfig
    {
        bsp::IFlash*  flash{ nullptr };
        FlashLayout   layout{};
        bsp::IPower*  power{ nullptr };
        BootDecision  boot{};                  ///< what selectBoot() decided for this run
        VersionNo     buildVersion{ kBuildVersion };
        UpdaterConfig updater{};

        /// How long to wait after a failed confirm before trying again. A failed confirm is a
        /// flash write error, and hammering a failing flash helps nobody.
        uint32_t confirmRetryMs{ 1000 };
    };

    /// Dispatches SYSTEM frames the session passes through:
    ///
    ///   GET_SW_VER (host request)  answered with the extended report (self_check.h)
    ///   BPM                        stored (bpm.h)
    ///   SHUTDOWN                   ack, quiesce, power off (shutdown.h)
    ///   SW_UPDATE                  the Updater
    ///
    /// RESET_GD (cmd 0) is not handled: nothing in the host sends it and its meaning is not
    /// documented. It falls through to the caller's ignored-frame count.
    ///
    /// After a trial boot it confirms the new image once the handshake has completed. That is the
    /// only evidence this layer has that the image can talk to the host, which is what makes it
    /// worth keeping.
    class System
    {
    public:
        using QuiesceFn = void ( * )( void* context );

        System( const SystemConfig& config, const SelfCheckResult& selfCheck, link::TxQueue& tx, const bsp::IClock& clock, QuiesceFn onQuiesce, void* quiesceContext );

        /// True if the frame was a SYSTEM command this layer owns.
        bool handle( const link::Frame& frame );

        /// Main-loop service. `streaming` is the session's handshake state.
        void poll( bool streaming );

        const SelfCheckResult& selfCheck() const { return selfCheck_; }
        const Bpm&             bpm() const { return bpm_; }
        const Shutdown&        shutdown() const { return shutdown_; }
        const Updater&         updater() const { return updater_; }
        bool                   quiesced() const { return shutdown_.quiesced(); }
        const bool&            quiescedFlag() const { return shutdown_.quiescedFlag(); }

        /// True once a trial image has confirmed itself (or when there was no trial to confirm).
        bool     confirmed() const { return confirmed_; }
        uint32_t confirmFailures() const { return confirmFailures_; }

    private:
        static bool replyUpdate( void* context, const uint8_t* payload, size_t length );

        SystemConfig       config_;
        SelfCheckResult    selfCheck_;
        link::TxQueue&     tx_;
        const bsp::IClock& clock_;
        QuiesceFn          onQuiesce_;
        void*              quiesceContext_;

        Bpm      bpm_;
        Shutdown shutdown_;
        Updater  updater_;
        bool     confirmed_;
        uint32_t confirmFailures_{ 0 };
        uint32_t lastConfirmTryMs_{ 0 };
        bool     triedConfirm_{ false };
    };

} // namespace system
} // namespace kick_mcu
