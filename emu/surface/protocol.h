/*
 * Wire protocol the Surface speaks as the unit's MCU. These values are mirrored from
 * Kick/modules/contracts/dcl/dcl_contracts.h and ComProtocol/PktMgr/PktMgr.hpp, and kept local so the Surface
 * builds without the Kick tree. emu/tests/protocol_drift.py fails when they drift apart.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace surface_protocol
{
    enum PacketType
    {
        PktSystem = 0,
        PktButton,
        PktLed,
        PktPad,
        PktKnobs,
        PktMidi,
        PktCv,
        PktSlider
    };

    enum SysCmd
    {
        SysResetGd = 0,
        SysGetSwVer,
        SysCommInit,
        SysSwUpdate,
        SysCheckCommStatus,
        SysBpm,
        SysReconnect,
        SysShutdown
    };

    enum LedCmd
    {
        LedSingle = 0,
        LedBlink,
        LedAll
    };

    enum PadCmd
    {
        PadState = 0,
        PadSetSingle,
        PadSetAllSingleColour,
        PadSetAllMultiColour,
        PadSettings,
        PadColourMode,
        PadSetColourState,
        PadSetAfterTouch,
        PadSetBlinking,
        PadSetStepSingle,
        PadSetStepAll
    };

    enum SliderCmd
    {
        SliderTouch = 0,
        SliderBar,
        SliderLevel,
        SliderIndividual
    };

    enum PadButtonState
    {
        StateOff = 0,
        StateOn,
        StateSampleAssigned,
        StateSampleNotAssigned,
        StateOnBlink,
        StateOffBlink
    };

    constexpr int numPads    = 16;
    constexpr int numButtons = 53;
    constexpr int numKnobs   = 16;
    constexpr int numCv      = 8;

    constexpr int btnShift      = 31;
    constexpr int btnPlay       = 45;
    constexpr int btnStop       = 44;
    constexpr int btnEnter      = 26;
    constexpr int btnKnobCcw    = 28;
    constexpr int btnKnobCw     = 30;
    constexpr int btnBack       = 29;
    constexpr int colourTrans   = 15;

    /// Buttons whose LEDs form a radio group: lighting one turns the others off. Inferred, see handleFrame.
    /// Banks A to H, Stop and Play, Full Level and Half Level.
    constexpr int radioGroups[][2] = { { 0, 7 }, { 44, 45 }, { 48, 49 } };
    constexpr int knobCenter    = 0;
    constexpr int knobCmdRotary = 0;
    constexpr int knobCmdQlink  = 1;

    /// A frame's type, command and data bytes, before framing and checksum.
    struct Packet
    {
        int              type;
        int              cmd;
        std::vector<int> data;
    };

    /// What the Surface sends as the unit's MCU, one function per message, so the encoding lives next to the constants.
    namespace packets
    {
        inline Packet pad( int pad, bool pressed, int velocity, int aftertouch )
        {
            return { PktPad, PadState, { pad, pressed ? 1 : 0, pressed ? velocity : 0, pressed ? aftertouch : 0 } };
        }

        inline Packet button( int id, bool pressed ) { return { PktButton, 0, { id, pressed ? 1 : 0 } }; }

        /// Steps > 0 turn clockwise. One packet carries the whole burst, because some screens act once per
        /// packet (the step sequencer flips a step's selection on every one), and the count is a signed byte in
        /// the unit's simulator, so it stops at 127.
        inline Packet knob( int knob, int steps )
        {
            return { PktKnobs, knob == knobCenter ? knobCmdRotary : knobCmdQlink, { knob, steps > 0 ? 1 : 0, std::min( 127, std::abs( steps ) ) } };
        }

        /// 0 to 100 while a finger is on the strip, and 0 when it lifts, which is how the app ends a touch.
        inline Packet strip( int percent ) { return { PktSlider, SliderTouch, { std::clamp( percent, 0, 100 ) } }; }

        inline Packet commStatus( int state ) { return { PktSystem, SysCheckCommStatus, { state } }; }
        inline Packet softwareVersion() { return { PktSystem, SysGetSwVer, { 1, 0 } }; }
    }
}
