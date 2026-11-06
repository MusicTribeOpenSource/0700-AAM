// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Touch strip position and lift.
#pragma once

#include <cstdint>

#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/input/event_sink.h"

namespace kick_mcu
{
namespace input
{
    struct StripConfig
    {
        /// A new position is sent only when it differs from the last one sent by at least this
        /// many percent. 1 sends every change.
        uint8_t deadband{ 1 };

        /// The strip must read untouched for this long before the lift is reported, so one
        /// dropped reading in the middle of a drag does not end the touch.
        uint32_t liftDebounceUs{ 3000 };
    };

    struct StripStats
    {
        uint32_t positions{ 0 }; ///< position frames queued
        uint32_t lifts{ 0 };     ///< lift frames queued
        uint32_t retries{ 0 };   ///< ticks a frame had to wait because the sink was full
    };

    /// Reads the strip each tick. Frames are `[value]` on the PadKnob lane: 1 to 100 while
    /// touched, from the bottom of the strip, and 0 once on lift. Position 0 of the sensor maps
    /// to 1, never 0, because 0 is how the host recognises a lift (input_events and the emulator's
    /// releaseStrip do the same). A frame the sink refuses is retried on the next tick; a lift
    /// is never dropped, since the host would think the finger is still down.
    class StripScanner
    {
    public:
        explicit StripScanner( const StripConfig& config = {} )
            : config_( config )
        {
        }

        void update( bsp::IStrip& strip, uint32_t nowUs, IEventSink& sink );

        /// Maps a 0 to 1023 sensor position to 1 to 100.
        static uint8_t toPercent( uint16_t position );

        bool               touched() const { return active_; }
        const StripStats&  stats() const { return stats_; }

    private:
        StripConfig config_;
        bool        active_{ false };  ///< a touch is in progress as far as the host knows (or is about to be)
        uint8_t     lastSent_{ 0 };
        bool        sentFirst_{ false };
        uint32_t    lastTouchUs_{ 0 };
        StripStats  stats_{};
    };

} // namespace input
} // namespace kick_mcu
