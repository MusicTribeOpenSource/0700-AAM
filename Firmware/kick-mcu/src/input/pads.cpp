// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/input/pads.h"

namespace kick_mcu
{
namespace input
{
    namespace
    {
        uint8_t scale127( uint32_t value, uint32_t low, uint32_t high )
        {
            if( value <= low || high <= low )
                return 0;
            const uint32_t v = ( value - low ) * 127u / ( high - low );
            return v > 127u ? 127 : (uint8_t)v;
        }
    } // namespace

    uint8_t PadScanner::strength( uint16_t peak ) const
    {
        return scale127( peak, config_.onThreshold, config_.fullScale );
    }

    uint8_t PadScanner::pressureValue( uint16_t reading ) const
    {
        return scale127( reading, config_.offThreshold, config_.fullScale );
    }

    void PadScanner::finishStrike( Pad& pad )
    {
        pad.velocity = curve_.map( strength( pad.peak ) );
        pad.phase    = Phase::Down;
        pad.sendDown = true;
    }

    void PadScanner::flushOwed( unsigned index, Pad& pad, IEventSink& sink )
    {
        if( pad.sendDown )
        {
            const uint8_t payload[4] = { (uint8_t)index, contract::kPadStatePressed, pad.velocity, 0 };
            if( !sink.accepting() )
            {
                // The link is not up: nothing was lost that the host could have used.
                pad.sendDown = false;
                pad.sendUp   = false;
            }
            else if( sink.send( link::Lane::PadKnob, contract::PKT_PAD, contract::PAD_STATE, payload, 4 ) )
            {
                pad.sendDown  = false;
                pad.announced = true;
                ++stats_.strikes;
            }
            else
            {
                ++stats_.retries;
                // Released before the frame could be queued: the host never saw the press, so it
                // must not see the release either.
                if( pad.sendUp )
                {
                    pad.sendDown = false;
                    pad.sendUp   = false;
                    ++stats_.discarded;
                }
                return;
            }
        }

        if( pad.sendUp && !pad.sendDown )
        {
            const uint8_t payload[4] = { (uint8_t)index, contract::kPadStateReleased, 0, 0 };
            if( !sink.accepting() )
                pad.sendUp = false;
            else if( sink.send( link::Lane::PadKnob, contract::PKT_PAD, contract::PAD_STATE, payload, 4 ) )
            {
                pad.sendUp    = false;
                pad.announced = false;
                ++stats_.releases;
            }
            else
                ++stats_.retries;
        }
    }

    void PadScanner::update( const uint16_t ( &pressure )[contract::kPadCount], uint32_t nowUs, IEventSink& sink )
    {
        for( unsigned i = 0; i < contract::kPadCount; ++i )
        {
            Pad&           pad     = pads_[i];
            const uint16_t reading = pressure[i];

            switch( pad.phase )
            {
                case Phase::Idle:
                    // A new strike waits until the last one's frames are out, so down and up alternate.
                    if( reading >= config_.onThreshold && !pad.sendDown && !pad.sendUp )
                    {
                        pad.phase             = Phase::Striking;
                        pad.peak              = reading;
                        pad.strikeUs          = nowUs;
                        pad.lastAftertouch    = 0;
                        pad.lastAftertouchUs  = nowUs;
                        pad.sentAnyAftertouch = false;
                    }
                    break;

                case Phase::Striking:
                    if( reading > pad.peak )
                        pad.peak = reading;
                    if( reading < config_.offThreshold )
                    {
                        // A tap shorter than the window: report it now with the peak seen so far.
                        finishStrike( pad );
                        pad.sendUp = true;
                        pad.phase  = Phase::Idle;
                    }
                    else if( (uint32_t)( nowUs - pad.strikeUs ) >= config_.velocityWindowUs )
                        finishStrike( pad );
                    break;

                case Phase::Down:
                    if( reading < config_.offThreshold )
                    {
                        // Only a pad the host was told about gets a release. One that went down
                        // while the link was not up has nothing to release.
                        pad.sendUp = pad.announced || pad.sendDown;
                        pad.phase  = Phase::Idle;
                        // Pressure queued for this pad would reach the host after its release.
                        sink.discardQueued( link::Lane::Aftertouch, contract::PKT_PAD, (uint8_t)i );
                    }
                    else if( config_.aftertouchEnabled && !pad.sendDown && (uint32_t)( nowUs - pad.strikeUs ) >= config_.aftertouchHoldoffUs
                             && (uint32_t)( nowUs - pad.lastAftertouchUs ) >= config_.aftertouchMinIntervalUs )
                    {
                        const uint8_t value = pressureValue( reading );
                        const int     delta = (int)value - (int)pad.lastAftertouch;
                        if( ( delta < 0 ? -delta : delta ) >= (int)config_.aftertouchDeadband && sink.accepting() )
                        {
                            const uint8_t payload[4] = { (uint8_t)i, contract::kPadStateAftertouch, 0, value };
                            // The aftertouch lane drops its oldest frame when full, so this does not fail.
                            if( sink.send( link::Lane::Aftertouch, contract::PKT_PAD, contract::PAD_STATE, payload, 4 ) )
                            {
                                pad.lastAftertouch   = value;
                                pad.lastAftertouchUs = nowUs;
                                ++stats_.aftertouch;
                            }
                        }
                    }
                    break;
            }

            if( pad.sendDown || pad.sendUp )
                flushOwed( i, pad, sink );
        }
    }

} // namespace input
} // namespace kick_mcu
