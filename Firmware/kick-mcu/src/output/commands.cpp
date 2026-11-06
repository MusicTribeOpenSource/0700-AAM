// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/output/commands.h"

namespace kick_mcu
{
namespace output
{
    using namespace contract;

    namespace
    {
        constexpr RadioGroup kRadioGroups[] = {
            { 0, 7 },   // banks A to H
            { 44, 45 }, // Stop, Play
            { 48, 49 }, // Full Level, Half Level
        };

        void setPad( PadLed& pad, uint8_t state, uint8_t colour, uint8_t offColour, uint8_t velocity )
        {
            pad.state     = state;
            pad.colour    = colour;
            pad.offColour = offColour;
            pad.velocity  = velocity;
        }

        /// Lights a member of a radio group: the others go dark.
        void lightRadio( LedFrame& frame, uint8_t code )
        {
            if( const RadioGroup* group = radioGroupOf( code ) )
                for( unsigned i = group->first; i <= group->last; ++i )
                    if( i != code )
                        frame.buttons[i].state = STATE_OFF;
        }
    } // namespace

    const RadioGroup* radioGroupOf( uint8_t code )
    {
        for( const RadioGroup& group : kRadioGroups )
            if( code >= group.first && code <= group.last )
                return &group;
        return nullptr;
    }

    CommandResult CommandDecoder::handle( const link::Frame& f )
    {
        CommandResult result = CommandResult::NotMine;
        switch( f.type )
        {
            case PKT_LED: result = led( f ); break;
            case PKT_PAD: result = pad( f ); break;
            case PKT_SLIDER: result = slider( f ); break;
            default: return CommandResult::NotMine;
        }

        switch( result )
        {
            case CommandResult::Applied: ++stats_.applied; return result;
            case CommandResult::NotMine: return result;
            case CommandResult::Malformed: ++stats_.malformed; break;
            case CommandResult::OutOfRange: ++stats_.outOfRange; break;
            case CommandResult::Unknown: ++stats_.unknown; break;
        }
        stats_.lastRejectedType   = f.type;
        stats_.lastRejectedCmd    = f.cmd;
        stats_.lastRejectedLength = f.length;
        return result;
    }

    CommandResult CommandDecoder::led( const link::Frame& f )
    {
        switch( f.cmd )
        {
            case LED_SET_SINGLE_LED: return setSingleLed( f.payload, f.length );
            case LED_BLINK_SINGLE: return blinkSingleLed( f.payload, f.length );
            case LED_SET_ALL: return setAllLeds( f.payload, f.length );
            default: return CommandResult::Unknown;
        }
    }

    CommandResult CommandDecoder::pad( const link::Frame& f )
    {
        switch( f.cmd )
        {
            case PAD_SET_SINGLE_PAD: return setSinglePad( f.payload, f.length );
            case PAD_SET_ALL_SINGLE_COLOUR: return setAllPadsSingleColour( f.payload, f.length );
            case PAD_SET_ALL_MULTI_COLOUR: return setAllPadsMultiColour( f.payload, f.length );
            case PAD_SETTINGS: return setPadSettings( f.payload, f.length );
            case PAD_COLOR_MODE: return setPadColourMode( f.payload, f.length );
            case PAD_SET_COLOUR_STATE: return setPadColourState( f.payload, f.length );
            case PAD_SET_AFTER_TOUCH: return CommandResult::NotMine; // pads.setAftertouchEnabled(), in app/firmware.cpp
            case PAD_SET_BLINKING: return setPadBlinking( f.payload, f.length );
            case PAD_SET_STEP_SINGLE_PAD: return setStepSinglePad( f.payload, f.length );
            case PAD_SET_STEP_ALL_PAD: return setStepAllPads( f.payload, f.length );
            default: return CommandResult::Unknown; // PAD_STATE is an MCU-to-host event
        }
    }

    CommandResult CommandDecoder::slider( const link::Frame& f )
    {
        switch( f.cmd )
        {
            case SLIDER_BAR_LED: return setStripBar( f.payload, f.length, StripMode::Bar );
            case SLIDER_LEVEL_LED: return setStripBar( f.payload, f.length, StripMode::Level );
            case SLIDER_INDIVIDUAL_LED: return setStripIndividual( f.payload, f.length );
            default: return CommandResult::Unknown; // SLIDER_TOUCH_INPUT is an MCU-to-host event
        }
    }

    // ---- LED --------------------------------------------------------------------------------

    /// LED_SET_SINGLE_LED, payload [code, state, colour]. code is a BtnLedSequence value 0 to 52;
    /// 53 and up (the host sends 54 for Color Edit, which has no LED in the table) is OutOfRange.
    /// state is a PadButtonState. colour is stored and not used by the button driver, and is
    /// optional: setBankMessage sends only [code, 1], so two bytes are valid and colour is 0.
    /// Setting a radio-group member to anything but OFF turns the rest of its group off.
    CommandResult CommandDecoder::setSingleLed( const uint8_t* p, size_t n )
    {
        if( n < 2 )
            return CommandResult::Malformed;
        if( p[0] >= kButtonLeds )
            return CommandResult::OutOfRange;
        LedFrame& frame = state_.beginUpdate();
        if( p[1] != STATE_OFF )
            lightRadio( frame, p[0] );
        frame.buttons[p[0]] = { p[1], (uint8_t)( n >= 3 ? p[2] : 0 ) };
        state_.commit();
        return CommandResult::Applied;
    }

    /// LED_BLINK_SINGLE, payload [code, colour?]. No host code sends this command. The layout
    /// follows the emulator Surface, which reads only the code and sets ON_BLINK; the optional
    /// colour byte is a guess that mirrors SET_SINGLE_LED. Radio groups apply as for SET_SINGLE_LED.
    CommandResult CommandDecoder::blinkSingleLed( const uint8_t* p, size_t n )
    {
        if( n < 1 )
            return CommandResult::Malformed;
        if( p[0] >= kButtonLeds )
            return CommandResult::OutOfRange;
        LedFrame& frame = state_.beginUpdate();
        lightRadio( frame, p[0] );
        frame.buttons[p[0]] = { STATE_ON_BLINK, (uint8_t)( n >= 2 ? p[1] : 0 ) };
        state_.commit();
        return CommandResult::Applied;
    }

    /// LED_SET_ALL, payload [state, colour?]. No host code sends this command either; the layout
    /// is the Surface's (state only), with the same guessed colour byte. Every button LED gets
    /// the state, radio groups included.
    CommandResult CommandDecoder::setAllLeds( const uint8_t* p, size_t n )
    {
        if( n < 1 )
            return CommandResult::Malformed;
        LedFrame& frame = state_.beginUpdate();
        for( ButtonLed& b : frame.buttons )
            b = { p[0], (uint8_t)( n >= 2 ? p[1] : 0 ) };
        state_.commit();
        return CommandResult::Applied;
    }

    // ---- PAD --------------------------------------------------------------------------------
    //
    // Every command below that sets a pad writes all four fields. Fields the command does not
    // carry take their defaults: offColour = PadColourTrans, velocity = 127.

    /// PAD_SET_SINGLE_PAD, payload [pad, state, colour] (setPadDataMessage). pad is 0 to 15.
    CommandResult CommandDecoder::setSinglePad( const uint8_t* p, size_t n )
    {
        if( n < 3 )
            return CommandResult::Malformed;
        if( p[0] >= kPadLeds )
            return CommandResult::OutOfRange;
        LedFrame& frame = state_.beginUpdate();
        setPad( frame.pads[p[0]], p[1], p[2], kPadColourTrans, 127 );
        state_.commit();
        return CommandResult::Applied;
    }

    /// PAD_SET_ALL_SINGLE_COLOUR, payload [state, colour] (setAllPadDataColourInfoMessage,
    /// clearAllPadsMessage). All 16 pads.
    CommandResult CommandDecoder::setAllPadsSingleColour( const uint8_t* p, size_t n )
    {
        if( n < 2 )
            return CommandResult::Malformed;
        LedFrame& frame = state_.beginUpdate();
        for( PadLed& pad : frame.pads )
            setPad( pad, p[0], p[1], kPadColourTrans, 127 );
        state_.commit();
        return CommandResult::Applied;
    }

    /// PAD_SET_ALL_MULTI_COLOUR, payload [state, colour * N] (setAllPadDataColourInfoMessage with
    /// a vector). N is the payload length minus one and must be 1 to 16; it sets pads 0 to N-1
    /// and leaves the rest alone. The whole update is one commit.
    CommandResult CommandDecoder::setAllPadsMultiColour( const uint8_t* p, size_t n )
    {
        if( n < 2 || n - 1 > kPadLeds )
            return CommandResult::Malformed;
        const size_t count = n - 1;
        LedFrame&    frame = state_.beginUpdate();
        for( size_t i = 0; i < count; ++i )
            setPad( frame.pads[i], p[0], p[1 + i], kPadColourTrans, 127 );
        state_.commit();
        return CommandResult::Applied;
    }

    /// PAD_SETTINGS, payload [count, threshold, sensitivity, curve] (kick_leds.cpp sendPadSettings).
    /// Not an LED command; stored in padSettings().
    CommandResult CommandDecoder::setPadSettings( const uint8_t* p, size_t n )
    {
        if( n < 4 )
            return CommandResult::Malformed;
        padSettings_ = { p[0], p[1], p[2], p[3], true };
        return CommandResult::Applied;
    }

    /// PAD_COLOR_MODE, payload [mode] (setPadColorModeMessage, a mt::PadColorMode). Not applied;
    /// the colour a struck pad shows is the host's to command, and what the mode does to
    /// the MCU's own strike feedback, if anything, is unknown. Stored in padColourMode().
    CommandResult CommandDecoder::setPadColourMode( const uint8_t* p, size_t n )
    {
        if( n < 1 )
            return CommandResult::Malformed;
        padColourMode_ = p[0];
        return CommandResult::Applied;
    }

    /// PAD_SET_COLOUR_STATE, payload [colour * N, state * N] (setPadColourAndStateInfoMessage).
    /// The length must be even and N = length / 2 must be 1 to 16; it sets pads 0 to N-1.
    CommandResult CommandDecoder::setPadColourState( const uint8_t* p, size_t n )
    {
        if( n < 2 || ( n & 1u ) != 0 || n / 2 > kPadLeds )
            return CommandResult::Malformed;
        const size_t count = n / 2;
        LedFrame&    frame = state_.beginUpdate();
        for( size_t i = 0; i < count; ++i )
            setPad( frame.pads[i], p[count + i], p[i], kPadColourTrans, 127 );
        state_.commit();
        return CommandResult::Applied;
    }

    /// PAD_SET_BLINKING, payload [pad, state, onColour, offColour] (setPadBlinkingMessage).
    CommandResult CommandDecoder::setPadBlinking( const uint8_t* p, size_t n )
    {
        if( n < 4 )
            return CommandResult::Malformed;
        if( p[0] >= kPadLeds )
            return CommandResult::OutOfRange;
        LedFrame& frame = state_.beginUpdate();
        setPad( frame.pads[p[0]], p[1], p[2], p[3], 127 );
        state_.commit();
        return CommandResult::Applied;
    }

    /// PAD_SET_STEP_SINGLE_PAD, payload [pad, state, colour, velocity] (setPadColourByVelocityMessage).
    CommandResult CommandDecoder::setStepSinglePad( const uint8_t* p, size_t n )
    {
        if( n < 4 )
            return CommandResult::Malformed;
        if( p[0] >= kPadLeds )
            return CommandResult::OutOfRange;
        LedFrame& frame = state_.beginUpdate();
        setPad( frame.pads[p[0]], p[1], p[2], kPadColourTrans, p[3] );
        state_.commit();
        return CommandResult::Applied;
    }

    /// PAD_SET_STEP_ALL_PAD, payload [state, colour * N, velocity * N] (setPadColourByVelocityMessage
    /// and setAllPadColourByVelocityMessage). The length must be odd and N = (length - 1) / 2 must
    /// be 1 to 16; it sets pads 0 to N-1. The host sends N = 16, 33 bytes.
    CommandResult CommandDecoder::setStepAllPads( const uint8_t* p, size_t n )
    {
        if( n < 3 || ( n & 1u ) == 0 || ( n - 1 ) / 2 > kPadLeds )
            return CommandResult::Malformed;
        const size_t count = ( n - 1 ) / 2;
        LedFrame&    frame = state_.beginUpdate();
        for( size_t i = 0; i < count; ++i )
            setPad( frame.pads[i], p[0], p[1 + i], kPadColourTrans, p[1 + count + i] );
        state_.commit();
        return CommandResult::Applied;
    }

    // ---- SLIDER -----------------------------------------------------------------------------

    /// SLIDER_BAR_LED and SLIDER_LEVEL_LED, payload [percent] (sendSliderValueInPercentMessage;
    /// the host picks LEVEL for a centred slider). Values over 100 are clamped to 100. Switches
    /// the strip out of Individual mode and clears the individual levels.
    CommandResult CommandDecoder::setStripBar( const uint8_t* p, size_t n, StripMode mode )
    {
        if( n < 1 )
            return CommandResult::Malformed;
        LedFrame& frame = state_.beginUpdate();
        frame.strip     = StripLeds{};
        frame.strip.percent = p[0] > 100 ? 100 : p[0];
        frame.strip.mode    = mode;
        state_.commit();
        return CommandResult::Applied;
    }

    /// SLIDER_INDIVIDUAL_LED, payload [index, level]. No host code sends this command, so the
    /// layout is an assumption: index is the LED from the bottom (0 to 11), level is 0 to 100
    /// and is clamped. The first one after a Bar or Level command switches to Individual mode
    /// with every other LED dark; percent keeps its value.
    CommandResult CommandDecoder::setStripIndividual( const uint8_t* p, size_t n )
    {
        if( n < 2 )
            return CommandResult::Malformed;
        if( p[0] >= kStripLedCount )
            return CommandResult::OutOfRange;
        LedFrame& frame = state_.beginUpdate();
        if( frame.strip.mode != StripMode::Individual )
        {
            for( uint8_t& level : frame.strip.individual )
                level = 0;
            frame.strip.mode = StripMode::Individual;
        }
        frame.strip.individual[p[0]] = p[1] > 100 ? 100 : p[1];
        state_.commit();
        return CommandResult::Applied;
    }

} // namespace output
} // namespace kick_mcu
