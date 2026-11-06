// SPDX-License-Identifier: AGPL-3.0-or-later
//
// EN_SYS_SHUTDOWN: acknowledge, go dark, then ask for the power rail to drop.
#pragma once

#include <cstdint>

#include "kick_mcu/bsp/bsp.h"

namespace kick_mcu
{
namespace system
{
    /// The host does not send EN_SYS_SHUTDOWN over the wire today: input_events.cpp consumes it
    /// locally (it closes the serial port) before anything reaches PktMgr. The MCU handles it
    /// anyway, so a host that does send it gets a defined result.
    ///
    /// Sequence: request() marks the unit quiesced, which the firmware turns into LEDs off and
    /// no more input events. poll() asks for power off once the transmit queue has drained and
    /// the UART has had `drainMs` to push the last frame out.
    ///
    /// TODO(hardware): IPower::requestOff() is a stub. How the MCU drops the rail (a PMIC pin, a
    /// latching switch, a message to the i.MX8M's PMIC) is board design that does not exist yet,
    /// and nothing here has switched a real rail. The sim's SimPower only counts the call.
    class Shutdown
    {
    public:
        explicit Shutdown( uint32_t drainMs = 50 )
            : drainMs_( drainMs )
        {
        }

        void request( uint32_t nowMs )
        {
            if( quiesced_ )
                return;
            quiesced_    = true;
            requestedMs_ = nowMs;
        }

        void poll( uint32_t nowMs, bool txEmpty, bsp::IPower* power )
        {
            if( !quiesced_ || offRequested_ || !txEmpty || (uint32_t)( nowMs - requestedMs_ ) < drainMs_ )
                return;
            offRequested_ = true;
            if( power != nullptr )
                power->requestOff();
        }

        bool         quiesced() const { return quiesced_; }
        const bool&  quiescedFlag() const { return quiesced_; }
        bool         offRequested() const { return offRequested_; }

    private:
        uint32_t drainMs_;
        uint32_t requestedMs_{ 0 };
        bool     quiesced_{ false };
        bool     offRequested_{ false };
    };

} // namespace system
} // namespace kick_mcu
