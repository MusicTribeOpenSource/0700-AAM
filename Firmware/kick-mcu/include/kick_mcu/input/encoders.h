// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Quadrature decoding for the jog wheel and the 16 Q-Link knobs.
#pragma once

#include <cstdint>

#include "kick_mcu/contract.h"
#include "kick_mcu/input/event_sink.h"

namespace kick_mcu
{
namespace input
{
    struct EncoderConfig
    {
        /// Quadrature transitions the encoder makes per detent. Four is a full-cycle encoder
        /// (a click per A/B cycle), two is a half-cycle one. The encoder part is not known yet.
        uint8_t transitionsPerStep{ 4 };
    };

    struct EncoderStats
    {
        uint32_t frames{ 0 };         ///< [knob, dir, steps] frames queued
        uint32_t steps{ 0 };          ///< steps delivered in those frames
        uint32_t skipped{ 0 };        ///< two-bit jumps (a transition was missed); counted as two steps in the last direction
        uint32_t reversalNets{ 0 };   ///< reversals that could not flush the pending steps first and were netted instead
        uint32_t held{ 0 };           ///< ticks that ended with steps still owed because the sink was full
    };

    /// Reads all 17 encoders each tick and sends at most one frame per encoder and direction.
    ///
    /// The BSP supplies two bits per encoder (A in bit 2n, B in bit 2n+1). The decoder turns
    /// transitions into detent steps with a table, so contact bounce that goes forward and back
    /// cancels instead of counting. Frames are `[knob, direction, steps]`: knob 0 is the jog
    /// wheel (cmd KNOB_ROTARY), 1 to 16 the Q-Links (cmd KNOB_QLINK), direction 1 is clockwise.
    ///
    /// Coalescing: steps from one tick are summed into a single frame, so a fast spin is a few
    /// frames with a large step count, not one frame per detent. Steps the sink refuses stay
    /// owed and join the next tick's, so a backed-up queue also produces larger frames. A frame
    /// carries at most 127 steps; more than that goes out as consecutive frames.
    ///
    /// A reversal flushes the pending steps first, so the host sees the two directions in
    /// order. If the sink refuses that flush, the two are netted (a clockwise 3 followed by an
    /// anticlockwise 2 becomes a clockwise 1), which keeps the position right and gives up only
    /// the path.
    ///
    /// The BSP maps A and B so that clockwise runs 00, 01, 11, 10. If the board wires them the
    /// other way, swap the bits in the BSP rather than here.
    class EncoderBank
    {
    public:
        explicit EncoderBank( const EncoderConfig& config = {} )
            : config_( config )
        {
        }

        /// Decodes one reading of all encoders and sends the resulting frames. The first call
        /// only records the starting positions.
        void update( uint64_t phases, IEventSink& sink );

        /// Adds detent steps to an encoder directly (positive is clockwise) and flushes the
        /// pending steps at the end of the tick. `update` uses these; they are public so the
        /// coalescing rules can be tested without synthesising quadrature.
        void addSteps( unsigned encoder, int steps, IEventSink& sink );
        void flush( IEventSink& sink );

        /// Steps still owed to the host for an encoder (positive is clockwise).
        int pending( unsigned encoder ) const { return encoder < contract::kEncoderCount ? pending_[encoder] : 0; }

        const EncoderStats& stats() const { return stats_; }

    private:
        bool flushOne( unsigned encoder, IEventSink& sink );

        EncoderConfig config_;
        uint8_t       previous_[contract::kEncoderCount]{};
        int8_t        lastDirection_[contract::kEncoderCount]{};
        int8_t        accumulated_[contract::kEncoderCount]{};
        int           pending_[contract::kEncoderCount]{};
        bool          primed_{ false };
        EncoderStats  stats_{};
    };

} // namespace input
} // namespace kick_mcu
