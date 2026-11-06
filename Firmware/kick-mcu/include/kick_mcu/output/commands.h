// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Host LED, PAD and SLIDER commands, decoded into the LED table.
#pragma once

#include <cstdint>

#include "kick_mcu/link/frame_codec.h"
#include "kick_mcu/output/led_state.h"

namespace kick_mcu
{
namespace output
{
    enum class CommandResult : uint8_t
    {
        Applied,    ///< the table changed (or was set to what it already was)
        Malformed,  ///< payload too short, or a length that does not fit the command
        OutOfRange, ///< a well-formed command naming a button, pad or strip LED that does not exist
        Unknown,    ///< a LED, PAD or SLIDER cmd the host should not send to the MCU
        NotMine,    ///< not an output command: another packet type, or PAD_SET_AFTER_TOUCH (the input layer's)
    };

    /// The debug counters. Nothing is logged, there is nowhere to log to yet; a debugger or the
    /// simulator reads these. last* describe the most recent rejected frame.
    struct CommandStats
    {
        uint32_t applied{ 0 };
        uint32_t malformed{ 0 };
        uint32_t outOfRange{ 0 };
        uint32_t unknown{ 0 };
        uint8_t  lastRejectedType{ 0 };
        uint8_t  lastRejectedCmd{ 0 };
        uint8_t  lastRejectedLength{ 0 };
    };

    /// PAD_SETTINGS payload: [count, threshold, sensitivity, curve]. The host sends count = 16
    /// and three values from its hardware settings page. They describe the pad sensors, not LEDs.
    /// They are kept here and not applied: what the numbers mean on the real sensors is unknown.
    struct PadSettings
    {
        uint8_t count{ 0 };
        uint8_t threshold{ 0 };
        uint8_t sensitivity{ 0 };
        uint8_t curve{ 0 };
        bool    received{ false };
    };

    /// A set of buttons of which one is lit at a time.
    struct RadioGroup
    {
        uint8_t first;
        uint8_t last; ///< inclusive
    };

    /// INFERRED, not specified. The host switches these LEDs on and never off: setBankMessage
    /// (banks A to H, codes 0 to 7), setMediaControlMessage (Play 45 and Stop 44) and
    /// setLEDONMessage (Full Level 48 and Half Level 49, kick_AppHome.cpp) all send state ON, and
    /// the only setLEDOFFMessage callers I found are for Erase and for screen buttons, none
    /// for these groups. For the panel to show one lit at a time, the MCU must be doing the
    /// clearing, so it treats each set as a radio group: lighting one member turns the others
    /// off. If the real MCU does not, the panel would show several banks lit at once; confirm
    /// on hardware. Returns null for a button outside every group.
    const RadioGroup* radioGroupOf( uint8_t buttonCode );

    /// Decodes host commands into a LedState. One method per command, each with its payload
    /// layout in the comment, taken from the host's senders in
    /// Kick/modules/contracts/dcl/dcl_contracts_helper.cpp unless noted. A payload may be longer
    /// than the layout needs: the extra bytes are ignored, so the host can add a field later.
    /// A payload that is shorter, or whose length does not fit the command, is Malformed and
    /// changes nothing; every check happens before the first write, and the table is only
    /// committed on success.
    class CommandDecoder
    {
    public:
        explicit CommandDecoder( LedState& state )
            : state_( state )
        {
        }

        CommandResult handle( const link::Frame& frame );

        const CommandStats& stats() const { return stats_; }
        const PadSettings&  padSettings() const { return padSettings_; }
        /// The last PAD_COLOR_MODE value (a mt::PadColorMode), or -1 before one arrives. Kept, not applied.
        int padColourMode() const { return padColourMode_; }

    private:
        CommandResult led( const link::Frame& f );
        CommandResult pad( const link::Frame& f );
        CommandResult slider( const link::Frame& f );

        // LED
        CommandResult setSingleLed( const uint8_t* p, size_t n );
        CommandResult blinkSingleLed( const uint8_t* p, size_t n );
        CommandResult setAllLeds( const uint8_t* p, size_t n );

        // PAD
        CommandResult setSinglePad( const uint8_t* p, size_t n );
        CommandResult setAllPadsSingleColour( const uint8_t* p, size_t n );
        CommandResult setAllPadsMultiColour( const uint8_t* p, size_t n );
        CommandResult setPadSettings( const uint8_t* p, size_t n );
        CommandResult setPadColourMode( const uint8_t* p, size_t n );
        CommandResult setPadColourState( const uint8_t* p, size_t n );
        CommandResult setPadBlinking( const uint8_t* p, size_t n );
        CommandResult setStepSinglePad( const uint8_t* p, size_t n );
        CommandResult setStepAllPads( const uint8_t* p, size_t n );

        // SLIDER
        CommandResult setStripBar( const uint8_t* p, size_t n, StripMode mode );
        CommandResult setStripIndividual( const uint8_t* p, size_t n );

        LedState&    state_;
        CommandStats stats_{};
        PadSettings  padSettings_{};
        int          padColourMode_{ -1 };
    };

} // namespace output
} // namespace kick_mcu
