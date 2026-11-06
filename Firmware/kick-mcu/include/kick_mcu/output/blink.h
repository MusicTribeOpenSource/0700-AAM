// SPDX-License-Identifier: AGPL-3.0-or-later
//
// One blink phase for every LED.
#pragma once

#include <cstdint>

#include "kick_mcu/bsp/bsp.h"

namespace kick_mcu
{
namespace output
{
    /// A square wave that every blinking LED reads, so they all turn on and off together no
    /// matter when each was told to blink. The default is 2 Hz: on for 250 ms, off for 250 ms.
    ///
    /// The phase moves only when update() is called, and the refresh task calls it once per
    /// pass before drawing, so every LED in a pass sees the same phase. update() counts whole
    /// half-periods since the last edge with unsigned subtraction, which stays correct when the
    /// millisecond clock wraps. A main loop that stalls for several half-periods lands on the
    /// right phase in one step.
    ///
    /// A BPM-synced period is not wired up. setPeriodMs() is the hook: whatever tracks the tempo
    /// can set the period from it.
    class BlinkClock
    {
    public:
        explicit BlinkClock( const bsp::IClock& clock, uint32_t periodMs = 500 )
            : clock_( clock )
            , lastEdgeMs_( clock.nowMs() )
        {
            setPeriodMs( periodMs );
        }

        /// A full on-off cycle. Values under 2 ms are raised to 2, so each half is at least 1 ms.
        /// Changing the period restarts the cycle in its "on" half.
        void setPeriodMs( uint32_t periodMs )
        {
            halfPeriodMs_ = ( periodMs < 2 ? 2 : periodMs ) / 2;
            restart();
        }

        uint32_t periodMs() const { return halfPeriodMs_ * 2; }

        /// Starts a new cycle now, in the "on" half.
        void restart()
        {
            lastEdgeMs_ = clock_.nowMs();
            on_         = true;
        }

        /// Moves the phase to the present.
        void update()
        {
            const uint32_t now   = clock_.nowMs();
            const uint32_t edges = (uint32_t)( now - lastEdgeMs_ ) / halfPeriodMs_;
            if( edges == 0 )
                return;
            lastEdgeMs_ += edges * halfPeriodMs_;
            if( edges & 1u )
                on_ = !on_;
        }

        /// True in the "on" half of the cycle, as of the last update().
        bool on() const { return on_; }

    private:
        const bsp::IClock& clock_;
        uint32_t           halfPeriodMs_{ 250 };
        uint32_t           lastEdgeMs_{ 0 };
        bool               on_{ true };
    };

} // namespace output
} // namespace kick_mcu
