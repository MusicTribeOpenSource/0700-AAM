// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Where the input layer sends events.
#pragma once

#include <cstddef>
#include <cstdint>

#include "kick_mcu/link/tx_queue.h"

namespace kick_mcu
{
namespace input
{
    /// What the input layer needs from the transport. The real implementation (app/firmware.h)
    /// puts events in the transmit queue; tests use a recorder. Nothing here blocks.
    class IEventSink
    {
    public:
        virtual ~IEventSink() = default;

        /// False until the host handshake is done. The input layer keeps tracking input while this
        /// is false but discards the events, so a burst of stale events does not arrive the
        /// moment the link comes up, and the host never sees a release for a press it missed.
        virtual bool accepting() const = 0;

        /// Queues one event. False means it was refused (the lane is full); the caller keeps the
        /// event and tries again on the next scan tick when it must not be lost.
        virtual bool send( link::Lane lane, uint8_t type, uint8_t cmd, const uint8_t* payload, size_t length ) = 0;

        /// Removes queued frames of `type` whose first payload byte is `firstByte` from a lane.
        /// A pad release uses it to pull that pad's stale aftertouch out of the queue, so the
        /// host never sees pressure for a pad it already saw go up.
        virtual void discardQueued( link::Lane lane, uint8_t type, uint8_t firstByte ) = 0;
    };

} // namespace input
} // namespace kick_mcu
