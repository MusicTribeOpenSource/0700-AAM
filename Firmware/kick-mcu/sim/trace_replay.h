// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Plays the host-to-MCU half of an emulator trace (KICK_EMU_TRACE, see emu/README.md) into the
// firmware, and leaves the firmware's LED table for inspection.
#pragma once

#include <memory>
#include <string>

#include "kick_mcu/app/firmware.h"
#include "sim_board.h"
#include "sim_clock.h"
#include "sim_leds.h"
#include "sim_uart.h"

namespace kick_mcu
{
namespace sim
{
    /// Owns a firmware on the simulated board and a virtual clock. run() feeds it every `RX`
    /// line of the trace, in order, exactly as the bytes were read by the emulator Surface:
    /// the handshake, then whatever the host commanded. Nothing the firmware sends back is
    /// looked at. The firmware sees the trace without delays between lines; a line's bytes
    /// arrive in chunks the receive ring can hold, with a few main-loop passes between chunks.
    class TraceReplayer
    {
    public:
        TraceReplayer();

        /// Returns false and sets error() if the text has no RX lines or a line cannot be parsed.
        bool run( const std::string& traceText );

        const std::string& error() const { return error_; }
        size_t             rxLines() const { return rxLines_; }
        size_t             rxBytes() const { return rxBytes_; }

        app::Firmware&         firmware() { return *firmware_; }
        const SimLedDriver&    driver() const { return driver_; }
        const output::LedFrame& table() const { return firmware_->leds().front(); }

    private:
        void pump( int passes );

        SimClock                       clock_;
        SimUart                        uart_{ 8192 };
        SimBoard                       board_;
        SimLedDriver                   driver_;
        std::unique_ptr<app::Firmware> firmware_;
        std::string                    error_;
        size_t                         rxLines_{ 0 };
        size_t                         rxBytes_{ 0 };
    };

} // namespace sim
} // namespace kick_mcu
