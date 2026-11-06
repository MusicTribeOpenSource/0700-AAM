// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/input/encoders.h"

namespace kick_mcu
{
namespace input
{
    namespace
    {
        // Gray sequence for clockwise is 00, 01, 11, 10. Indexed [previous][next]; 2 marks a jump
        // across two positions (a missed transition) whose direction the table cannot tell, and
        // 0 on the diagonal is no change.
        constexpr int8_t kJump = 2;
        constexpr int8_t kDelta[4][4] = {
            /* from 00 */ { 0, +1, -1, kJump },
            /* from 01 */ { -1, 0, kJump, +1 },
            /* from 10 */ { +1, kJump, 0, -1 },
            /* from 11 */ { kJump, -1, +1, 0 },
        };
        constexpr int kMaxStepsPerFrame = 127;
    } // namespace

    void EncoderBank::update( uint64_t phases, IEventSink& sink )
    {
        for( unsigned e = 0; e < contract::kEncoderCount; ++e )
        {
            const uint8_t now = (uint8_t)( ( phases >> ( 2 * e ) ) & 3u );
            if( !primed_ )
            {
                previous_[e]      = now;
                lastDirection_[e] = +1;
                continue;
            }

            int8_t delta = kDelta[previous_[e]][now];
            previous_[e] = now;
            if( delta == kJump )
            {
                ++stats_.skipped;
                delta = (int8_t)( 2 * lastDirection_[e] );
            }
            if( delta == 0 )
                continue;
            lastDirection_[e] = delta > 0 ? +1 : -1;

            int total = accumulated_[e] + delta;
            const int per = config_.transitionsPerStep == 0 ? 1 : config_.transitionsPerStep;
            const int steps = total / per; // truncates toward zero
            accumulated_[e] = (int8_t)( total - steps * per );
            if( steps != 0 )
                addSteps( e, steps, sink );
        }
        primed_ = true;
        flush( sink );
    }

    void EncoderBank::addSteps( unsigned encoder, int steps, IEventSink& sink )
    {
        if( encoder >= contract::kEncoderCount || steps == 0 )
            return;

        int& owed = pending_[encoder];
        if( owed != 0 && ( ( owed > 0 ) != ( steps > 0 ) ) )
        {
            if( !flushOne( encoder, sink ) )
                ++stats_.reversalNets;
        }
        owed += steps;
    }

    bool EncoderBank::flushOne( unsigned encoder, IEventSink& sink )
    {
        int& owed = pending_[encoder];
        while( owed != 0 )
        {
            const bool    clockwise = owed > 0;
            const int     magnitude = clockwise ? owed : -owed;
            const uint8_t steps     = (uint8_t)( magnitude > kMaxStepsPerFrame ? kMaxStepsPerFrame : magnitude );
            const uint8_t payload[3] = { (uint8_t)encoder, clockwise ? contract::KNOB_CLOCKWISE : contract::KNOB_ANTICLOCKWISE, steps };
            const uint8_t cmd        = encoder == contract::kKnobCenter ? contract::KNOB_ROTARY : contract::KNOB_QLINK;

            if( !sink.accepting() )
            {
                owed = 0; // nobody is listening; do not build up a backlog for the moment the link comes up
                return true;
            }
            if( !sink.send( link::Lane::PadKnob, contract::PKT_KNOBS, cmd, payload, 3 ) )
                return false;
            ++stats_.frames;
            stats_.steps += steps;
            owed += clockwise ? -(int)steps : (int)steps;
        }
        return true;
    }

    void EncoderBank::flush( IEventSink& sink )
    {
        bool left = false;
        for( unsigned e = 0; e < contract::kEncoderCount; ++e )
            if( pending_[e] != 0 && !flushOne( e, sink ) )
                left = true;
        if( left )
            ++stats_.held;
    }

} // namespace input
} // namespace kick_mcu
