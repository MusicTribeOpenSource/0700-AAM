// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Integrating debouncer for the button matrix.
#pragma once

#include <cstdint>

namespace kick_mcu
{
namespace input
{
    struct DebounceConfig
    {
        /// How long a key must read closed, in net microseconds, before it counts as pressed.
        /// Short, because this is added to every press. The 1 ms default is a guess for a
        /// dome-switch matrix; the real contact bounce is not known until the board exists.
        uint32_t pressUs{ 1000 };

        /// How long a key must read open before it counts as released. Longer than pressUs
        /// because a late release costs nothing a player can feel.
        uint32_t releaseUs{ 4000 };
    };

    /// One integrator per key, up to 64 keys (one per bit of the scan word).
    ///
    /// A released key has a counter between 0 and pressUs. Each update adds the elapsed time while
    /// the raw bit is set and subtracts it while clear, never below 0. At pressUs the key becomes
    /// pressed and its counter jumps to releaseUs. A pressed key's counter runs the other way: it
    /// is topped up while the raw bit is set and drained while clear, and at 0 the key becomes
    /// released. A glitch shorter than the window moves the counter and is forgiven by the next
    /// good reading, which a "must read the same N times in a row" counter would not do.
    ///
    /// Time comes from the caller, so the result depends on when update() is called and not on
    /// how often. The step added per call is capped at half of the shorter window, so a scan
    /// loop that stalled does not turn one stale reading into a long run of identical ones: it
    /// takes at least two readings to flip a key. With the default windows the cap is 500 us, so
    /// a loop that runs slower than that debounces slower than the configured windows.
    class Debouncer
    {
    public:
        explicit Debouncer( const DebounceConfig& config = {} )
            : config_( config )
        {
        }

        /// Starts the timeline at `nowUs` with every key released.
        void reset( uint32_t nowUs );

        /// Feeds one raw scan (bit n set while key n reads closed). Returns the debounced state.
        uint64_t update( uint64_t raw, uint32_t nowUs );

        uint64_t state() const { return state_; }

    private:
        DebounceConfig config_;
        uint64_t       state_{ 0 };
        uint32_t       counter_[64]{};
        uint32_t       lastUs_{ 0 };
        bool           started_{ false };
    };

} // namespace input
} // namespace kick_mcu
