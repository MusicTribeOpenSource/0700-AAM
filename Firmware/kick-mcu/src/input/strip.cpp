// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/input/strip.h"

#include "kick_mcu/contract.h"

namespace kick_mcu
{
namespace input
{
    uint8_t StripScanner::toPercent( uint16_t position )
    {
        const uint32_t p = position > 1023 ? 1023 : position;
        return (uint8_t)( 1 + p * ( contract::kStripMax - 1 ) / 1023 );
    }

    void StripScanner::update( bsp::IStrip& strip, uint32_t nowUs, IEventSink& sink )
    {
        uint16_t position = 0;
        const bool touched = strip.read( position );

        if( touched )
        {
            active_      = true;
            lastTouchUs_ = nowUs;
            const uint8_t value = toPercent( position );
            const int     delta = (int)value - (int)lastSent_;
            if( !sentFirst_ || ( delta < 0 ? -delta : delta ) >= (int)config_.deadband )
            {
                if( !sink.accepting() )
                {
                    // Nothing to tell yet, but remember the position so the first frame after the
                    // link comes up is not suppressed by the deadband.
                    sentFirst_ = false;
                    return;
                }
                if( sink.send( link::Lane::PadKnob, contract::PKT_SLIDER, contract::SLIDER_TOUCH_INPUT, &value, 1 ) )
                {
                    lastSent_  = value;
                    sentFirst_ = true;
                    ++stats_.positions;
                }
                else
                    ++stats_.retries;
            }
            return;
        }

        if( active_ && (uint32_t)( nowUs - lastTouchUs_ ) >= config_.liftDebounceUs )
        {
            // If no position ever reached the host, there is no touch to end.
            if( !sentFirst_ || !sink.accepting() )
            {
                active_ = false;
                return;
            }
            const uint8_t lift = contract::kStripLift;
            if( sink.send( link::Lane::PadKnob, contract::PKT_SLIDER, contract::SLIDER_TOUCH_INPUT, &lift, 1 ) )
            {
                active_    = false;
                sentFirst_ = false;
                lastSent_  = 0;
                ++stats_.lifts;
            }
            else
                ++stats_.retries;
        }
    }

} // namespace input
} // namespace kick_mcu
