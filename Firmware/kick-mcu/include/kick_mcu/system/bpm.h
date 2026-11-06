// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Tempo from the host, kept for whatever on the MCU wants it.
#pragma once

#include <cstddef>
#include <cstdint>

namespace kick_mcu
{
namespace system
{
    /// EN_SYS_BPM payload: [hi, lo], a big-endian 16-bit tempo in whole beats per minute. The
    /// sender is DclContractsHelper::setTempoMessage(int), fed from getMasterBPM() and
    /// getSelectedSeqBPM(), both ints. 0xFFFF (what -1 becomes) and 0 mean "no tempo".
    ///
    /// The one consumer today is the blink clock: with `blinkFollowsBpm` set in the firmware
    /// config, blinking LEDs flash once per beat. It is off by default, because the host's own
    /// blink behaviour is not known to follow tempo and the LED comparison against the Surface
    /// model assumes a fixed rate.
    class Bpm
    {
    public:
        static constexpr uint16_t kUnset = 0xFFFF;

        /// Returns false when the payload is shorter than two bytes (it is then ignored).
        bool onPayload( const uint8_t* payload, size_t length )
        {
            if( length < 2 )
            {
                ++malformed_;
                return false;
            }
            value_ = (uint16_t)( ( payload[0] << 8 ) | payload[1] );
            ++updates_;
            return true;
        }

        bool     valid() const { return value_ != kUnset && value_ != 0; }
        uint16_t raw() const { return value_; }
        uint32_t updates() const { return updates_; }
        uint32_t malformed() const { return malformed_; }

        /// One beat in milliseconds, 60000 / bpm, kept between 60 ms (1000 bpm) and 4000 ms (15 bpm)
        /// so a wild host value cannot make the blink clock spin or stall. 0 when there is no tempo.
        uint32_t beatPeriodMs() const
        {
            if( !valid() )
                return 0;
            const uint32_t p = 60000u / value_;
            return p < 60 ? 60 : p > 4000 ? 4000 : p;
        }

    private:
        uint16_t value_{ kUnset };
        uint32_t updates_{ 0 };
        uint32_t malformed_{ 0 };
    };

} // namespace system
} // namespace kick_mcu
