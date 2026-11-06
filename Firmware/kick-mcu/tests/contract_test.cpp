// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Every value restated in kick_mcu/contract.h is checked against the host's own headers. If the
// host changes an enum, this file stops compiling, which is the point.
#include <gtest/gtest.h>

#include <string>

#include "button_names.h"
#include "kick_mcu/contract.h"

// Host headers. dcl_contracts.h includes juce_core.h; tests/stubs provides an empty one.
#include "Defines.hpp"
#include "PktMgr.hpp"
#include "dcl_contracts.h"
#include "ComConfig.hpp"

namespace c  = kick_mcu::contract;
using Host   = contracts::DeviceControlContracts;
using HostPkt = CPktMgr;

#define SAME( mine, theirs ) static_assert( (int)( mine ) == (int)( theirs ), #mine " differs from " #theirs )

// Packet types and directions, from PktMgr.hpp.
SAME( c::PKT_SYSTEM, HostPkt::EN_PKT_TYPE_SYSTEM );
SAME( c::PKT_BUTTON, HostPkt::EN_PKT_TYPE_BUTTON );
SAME( c::PKT_LED, HostPkt::EN_PKT_TYPE_LED );
SAME( c::PKT_PAD, HostPkt::EN_PKT_TYPE_PAD );
SAME( c::PKT_KNOBS, HostPkt::EN_PKT_TYPE_KNOBS );
SAME( c::PKT_MIDI, HostPkt::EN_PKT_TYPE_MIDI );
SAME( c::PKT_CV, HostPkt::EN_PKT_TYPE_CV );
SAME( c::PKT_SLIDER, HostPkt::EN_PKT_TYPE_SLIDER );
SAME( c::DIR_REQUEST, HostPkt::EN_PKT_REQUEST );
SAME( c::DIR_RESPONSE, HostPkt::EN_PKT_RESPONSE );

// PKTMGR_START_BYTE is 5: 'M' 'T' type dir cmd. The size byte follows, which makes the firmware's header 6.
static_assert( c::kHeaderBytes == PKTMGR_START_BYTE + 1, "header is 'M' 'T' type dir cmd size" );
static_assert( c::kCrcBytes == PKTMGR_CRC_BYTE, "one CRC byte" );

// System commands.
SAME( c::SYS_RESET_GD, Host::EN_SYS_RESET_GD );
SAME( c::SYS_GET_SW_VER, Host::EN_SYS_GET_SW_VER );
SAME( c::SYS_COMM_INIT, Host::EN_SYS_COMM_INIT );
SAME( c::SYS_SW_UPDATE, Host::EN_SYS_SW_UPDATE );
SAME( c::SYS_CHECK_COMM_STATUS, Host::EN_SYS_CHECK_COMM_STATUS );
SAME( c::SYS_BPM, Host::EN_SYS_BPM );
SAME( c::SYS_RECONNECT, Host::EN_SYS_RECONNECT );
SAME( c::SYS_SHUTDOWN, Host::EN_SYS_SHUTDOWN );
SAME( c::COMM_NOT_CONNECTED, Host::EN_SYS_COMM_NOT_CONNECTED );
SAME( c::COMM_CONNECTED, Host::EN_SYS_COMM_CONNECTED );

// Buttons.
SAME( c::BTN_STATUS, Host::EN_BTN_STATUS );
SAME( c::BTN_RELEASED, Host::BTN_RELEASED );
SAME( c::BTN_PRESSED, Host::BTN_PRESSED );
SAME( c::BTN_LONGPRESSED, Host::BTN_LONGPRESSED );
SAME( c::BTN_DOUBLECLICK, Host::BTN_DOUBLECLICK );

// LEDs.
SAME( c::LED_SET_SINGLE_LED, Host::EN_LED_SET_SINGLE_LED );
SAME( c::LED_BLINK_SINGLE, Host::EN_LED_BLINK_SINGLE );
SAME( c::LED_SET_ALL, Host::EN_LED_SET_ALL );

// Pads.
SAME( c::PAD_STATE, Host::EN_PAD_STATE );
SAME( c::PAD_SET_SINGLE_PAD, Host::EN_PAD_SET_SINGLE_PAD );
SAME( c::PAD_SET_ALL_SINGLE_COLOUR, Host::EN_PAD_SET_ALL_SINGLE_COLOUR );
SAME( c::PAD_SET_ALL_MULTI_COLOUR, Host::EN_PAD_SET_ALL_MULTI_COLOUR );
SAME( c::PAD_SETTINGS, Host::EN_PAD_SETTINGS );
SAME( c::PAD_COLOR_MODE, Host::EN_PAD_COLOR_MODE );
SAME( c::PAD_SET_COLOUR_STATE, Host::EN_PAD_SET_COLOUR_STATE );
SAME( c::PAD_SET_AFTER_TOUCH, Host::EN_PAD_SET_AFTER_TOUCH );
SAME( c::PAD_SET_BLINKING, Host::EN_PAD_SET_BLINKING );
SAME( c::PAD_SET_STEP_SINGLE_PAD, Host::EN_PAD_SET_STEP_SINGLE_PAD );
SAME( c::PAD_SET_STEP_ALL_PAD, Host::EN_PAD_SET_STEP_ALL_PAD );

// Pad and button LED states, colours.
SAME( c::STATE_OFF, Host::StateOff );
SAME( c::STATE_ON, Host::StateOn );
SAME( c::STATE_SAMPLE_ASSIGNED, Host::StateSampleAssigned );
SAME( c::STATE_SAMPLE_NOT_ASSIGNED, Host::StateSampleNotAssigned );
SAME( c::STATE_ON_BLINK, Host::StateOnBlink );
SAME( c::STATE_OFF_BLINK, Host::StateOffBlink );
SAME( c::kPadColourTrans, Host::PadColourTrans );
SAME( c::kPadColourCount, Host::PadColourNeutralGrey + 1 );

// Panel layout.
SAME( c::kBtnShift, Host::EN_SHIFT );
SAME( c::kBtnSize, Host::SIZE );
SAME( c::kBtnColorEdit, Host::EN_COLOR_EDIT );
SAME( c::kKnobCenter, Host::EN_KNOB_CENTER );
SAME( c::kQlinkCount, Host::EN_KNOB_16 );
SAME( c::kPadCount, Host::EN_PAD16 + 1 );
SAME( c::kPadStateReleased, Host::BTN_RELEASED );
SAME( c::kPadStatePressed, Host::BTN_PRESSED );
static_assert( c::kPadStateAftertouch != Host::BTN_RELEASED && c::kPadStateAftertouch != Host::BTN_PRESSED, "aftertouch must not look like a note on or off" );

// Knobs.
SAME( c::KNOB_ROTARY, Host::EN_KNOB_ROTARY );
SAME( c::KNOB_QLINK, Host::EN_KNOB_QLINK );
SAME( c::KNOB_ANTICLOCKWISE, Host::EN_KNOB_ANTICLOCKWISE );
SAME( c::KNOB_CLOCKWISE, Host::EN_KNOB_CLOCKWISE );

// Slider.
SAME( c::SLIDER_TOUCH_INPUT, Host::EN_SLIDER_TOUCH_INPUT );
SAME( c::SLIDER_BAR_LED, Host::EN_SLIDER_BAR_LED );
SAME( c::SLIDER_LEVEL_LED, Host::EN_SLIDER_LEVEL_LED );
SAME( c::SLIDER_INDIVIDUAL_LED, Host::EN_SLIDER_INDIVIDUAL_LED );

// The size limits the firmware chose have to fit what the host allocates.
static_assert( c::kMaxPayloadToHost < BUFFER_RX_SIZE, "a payload of BUFFER_RX_SIZE would overrun the host's RX fifo" );

TEST( Contract, EveryValueMatchesTheHostHeaders )
{
    // The checks are all static_asserts above. This test exists so a passing build shows up in
    // the test list.
    SUCCEED();
}

// The simulator's script button names are typed by hand, so check the ones that anchor the table
// against the host enum. A wrong anchor means every name after it is shifted.
TEST( Contract, ScriptButtonNamesLineUpWithTheHostEnum )
{
    const char* const* n = kick_mcu::sim::buttonNames();
    EXPECT_EQ( std::string( "Bank A" ), n[Host::EN_BANK_A] );
    EXPECT_EQ( std::string( "Bank H" ), n[Host::EN_BANK_H] );
    EXPECT_EQ( std::string( "Main" ), n[Host::EN_MAINMODE] );
    EXPECT_EQ( std::string( "Tap Tempo" ), n[Host::EN_TAP_TEMPO] );
    EXPECT_EQ( std::string( "+" ), n[Host::EN_PLUS] );
    EXPECT_EQ( std::string( "Enter" ), n[Host::EN_KNOB_ENTER] );
    EXPECT_EQ( std::string( "-" ), n[Host::EN_MINUS] );
    EXPECT_EQ( std::string( "Knob CCW" ), n[Host::EN_KNOB_CCW] );
    EXPECT_EQ( std::string( "Knob CW" ), n[Host::EN_KNOB_CW] );
    EXPECT_EQ( std::string( "Shift" ), n[Host::EN_SHIFT] );
    EXPECT_EQ( std::string( "Step Seq" ), n[Host::EN_STEP_SEQ] );
    EXPECT_EQ( std::string( "Record" ), n[Host::EN_RECORD] );
    EXPECT_EQ( std::string( "Stop" ), n[Host::EN_STOP] );
    EXPECT_EQ( std::string( "Play" ), n[Host::EN_PLAY] );
    EXPECT_EQ( std::string( "Full Level" ), n[Host::EN_FULL_LEVEL] );
    EXPECT_EQ( std::string( "Erase" ), n[Host::EN_ERASE] );
    EXPECT_EQ( std::string( "Color Edit" ), n[Host::EN_COLOR_EDIT] );
    EXPECT_EQ( Host::EN_COLOR_EDIT + 1, kick_mcu::sim::kButtonNameCount );
}
