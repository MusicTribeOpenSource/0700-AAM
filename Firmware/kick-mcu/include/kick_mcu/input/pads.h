// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pad strike, release and aftertouch.
#pragma once

#include <cstdint>

#include "kick_mcu/contract.h"
#include "kick_mcu/input/event_sink.h"
#include "kick_mcu/input/velocity_curve.h"

namespace kick_mcu
{
namespace input
{
    /// Thresholds are in raw sensor units, a 12-bit reading (0 to 4095). The values below are
    /// placeholders: the sensor type and its response are not known until the board exists.
    struct PadConfig
    {
        uint16_t onThreshold{ 150 };  ///< a reading at or above this starts a strike
        uint16_t offThreshold{ 70 };  ///< a reading below this releases the pad; below onThreshold so noise cannot chatter it
        uint16_t fullScale{ 3600 };   ///< the peak that maps to velocity 127

        /// How long the peak is tracked after a strike begins. The velocity is the highest
        /// reading in this window, and the pad-down frame cannot go out before it ends, so it
        /// is added to the latency of every strike. A strike that releases before the window ends
        /// is reported early, with the peak seen so far.
        uint32_t velocityWindowUs{ 1000 };

        /// Aftertouch is off until the host turns it on (PAD_SET_AFTER_TOUCH), as the host
        /// has an enable message for it.
        bool     aftertouchEnabled{ false };
        uint32_t aftertouchHoldoffUs{ 30000 };    ///< pressure in the first 30 ms is the strike settling, not aftertouch
        uint8_t  aftertouchDeadband{ 2 };         ///< a new value must differ from the last one sent by at least this
        uint32_t aftertouchMinIntervalUs{ 20000 }; ///< per pad, 50 frames a second at most
    };

    struct PadStats
    {
        uint32_t strikes{ 0 };          ///< pad-down frames delivered
        uint32_t releases{ 0 };         ///< pad-up frames delivered
        uint32_t aftertouch{ 0 };       ///< aftertouch frames queued
        uint32_t discarded{ 0 };        ///< pad-down events lost: the sink refused them and the pad was released first
        uint32_t retries{ 0 };          ///< ticks a down or up frame had to wait because the sink was full
    };

    /// Turns 16 pressure readings per tick into pad events:
    ///
    ///   down     [pad, 1, velocity, 0]    PadKnob lane, after the velocity window
    ///   up       [pad, 0, 0, 0]           PadKnob lane
    ///   pressure [pad, 2, 0, value]       Aftertouch lane, see contract::kPadStateAftertouch
    ///
    /// Velocity is the peak reading in the window, scaled between onThreshold and fullScale to
    /// 0 to 127 and passed through the velocity curve. Aftertouch is the current reading scaled
    /// between offThreshold and fullScale. It is sent only while the pad is down, after the
    /// holdoff, when the value moves by the deadband, and at most once per minimum interval.
    ///
    /// A down or up frame the sink refuses is retried on later ticks. It is never dropped
    /// silently, because a lost up leaves a note stuck on in the host. A pad does not start a new
    /// strike until its previous frames have been delivered, so down and up always alternate.
    class PadScanner
    {
    public:
        explicit PadScanner( const PadConfig& config = {} )
            : config_( config )
        {
        }

        /// Consumes one sample of all 16 pads taken at `nowUs`.
        void update( const uint16_t ( &pressure )[contract::kPadCount], uint32_t nowUs, IEventSink& sink );

        void setAftertouchEnabled( bool on ) { config_.aftertouchEnabled = on; }
        bool aftertouchEnabled() const { return config_.aftertouchEnabled; }

        VelocityCurve&       curve() { return curve_; }
        const VelocityCurve& curve() const { return curve_; }
        const PadConfig&     config() const { return config_; }
        const PadStats&      stats() const { return stats_; }

        /// True while the pad is held, from the first reading over the threshold until it is
        /// released (a held pad keeps this true even before its down frame has gone out).
        bool isDown( unsigned pad ) const { return pad < contract::kPadCount && pads_[pad].phase != Phase::Idle; }

    private:
        enum class Phase : uint8_t
        {
            Idle,
            Striking, ///< tracking the peak
            Down,     ///< down frame decided; held
        };

        struct Pad
        {
            Phase    phase{ Phase::Idle };
            bool     sendDown{ false };  ///< a down frame is owed to the host
            bool     sendUp{ false };    ///< an up frame is owed to the host
            bool     announced{ false }; ///< the host has seen this pad go down and not yet up
            uint8_t  velocity{ 0 };
            uint16_t peak{ 0 };
            uint32_t strikeUs{ 0 };
            uint32_t lastAftertouchUs{ 0 };
            uint8_t  lastAftertouch{ 0 };
            bool     sentAnyAftertouch{ false };
        };

        void    finishStrike( Pad& pad );
        void    flushOwed( unsigned index, Pad& pad, IEventSink& sink );
        uint8_t strength( uint16_t peak ) const;
        uint8_t pressureValue( uint16_t reading ) const;

        PadConfig     config_;
        VelocityCurve curve_;
        Pad           pads_[contract::kPadCount];
        PadStats      stats_{};
    };

} // namespace input
} // namespace kick_mcu
