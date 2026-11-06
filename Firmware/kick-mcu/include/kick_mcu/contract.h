// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The wire contract with the Kick host, restated for firmware that cannot include JUCE.
//
// Nothing here is authoritative. The authoritative definitions are
// Kick/modules/contracts/dcl/dcl_contracts.h and ComProtocol/PktMgr/PktMgr.hpp, and
// tests/contract_test.cpp compiles against both and static_asserts every value below equal to
// them, so a drift on either side fails the build instead of the link.
#pragma once

#include <cstddef>
#include <cstdint>

namespace kick_mcu
{
namespace contract
{
    // ---- framing -------------------------------------------------------------------------
    //
    //   'M' 'T' type dir cmd size payload[size-1] crc8 [trailer]
    //
    // size is the payload length plus one (the CRC byte). crc8 is polynomial 0x31, initial value
    // 0xFF, over every byte from 'M' through the last payload byte.

    constexpr uint8_t kStartByte1 = 'M';
    constexpr uint8_t kStartByte2 = 'T';
    constexpr uint8_t kCrcPoly    = 0x31;
    constexpr uint8_t kCrcInit    = 0xFF;

    /// Bytes before the payload: 'M' 'T' type dir cmd size.
    constexpr size_t kHeaderBytes = 6;
    constexpr size_t kCrcBytes    = 1;

    /// The byte the Surface appends after the CRC on every frame it sends to the host. The host
    /// reads size + 7 bytes per frame (input_events.cpp, `packetLength`) and hands the first
    /// size + 6 to PktMgr, so the last one is dropped unread. MCU-to-host frames need it;
    /// host-to-MCU frames do not carry one.
    constexpr uint8_t kMcuToHostTrailer = '\n';

    /// Largest MCU-to-host payload. The host's PktMgr RX buffer is BUFFER_RX_SIZE = 20 bytes
    /// (Kick/modules/input_events/ComConfig.hpp) and PushByteToRxFifo writes size - 1 bytes into
    /// it with no check against its length, so 21 bytes would overrun it. 20 would fit; this
    /// stops at 19 to keep a byte of margin, and nothing the MCU sends is longer than a few bytes.
    constexpr size_t kMaxPayloadToHost = 19;

    /// Largest host-to-MCU payload the host's own senders produce (EN_PAD_SET_STEP_ALL_PAD).
    constexpr size_t kLargestHostPayload = 33;

    /// MCU receive buffer, in bytes of a whole frame (header, payload and CRC).
    constexpr size_t kRxFrameBufferBytes = 64;
    constexpr size_t kMaxPayloadFromHost = kRxFrameBufferBytes - kHeaderBytes - kCrcBytes;

    static_assert(kLargestHostPayload <= kMaxPayloadFromHost, "receive buffer too small for the host's largest frame");

    // ---- enums, values from the host --------------------------------------------------------

    /// CPktMgr::enumPacketType
    enum PacketType : uint8_t
    {
        PKT_SYSTEM = 0,
        PKT_BUTTON,
        PKT_LED,
        PKT_PAD,
        PKT_KNOBS,
        PKT_MIDI,
        PKT_CV,
        PKT_SLIDER,
    };

    /// CPktMgr::enumPacketDir. The host only ever sends requests and ignores responses.
    enum PacketDir : uint8_t
    {
        DIR_REQUEST = 0,
        DIR_RESPONSE,
    };

    /// DeviceControlContracts::SysCmd
    enum SysCmd : uint8_t
    {
        SYS_RESET_GD = 0,
        SYS_GET_SW_VER,
        SYS_COMM_INIT,
        SYS_SW_UPDATE,
        SYS_CHECK_COMM_STATUS,
        SYS_BPM,
        SYS_RECONNECT,
        SYS_SHUTDOWN,
    };

    /// DeviceControlContracts::SysCommCmd
    enum SysCommCmd : uint8_t
    {
        COMM_NOT_CONNECTED = 0,
        COMM_CONNECTED,
    };

    /// The second handshake stage reuses COMM_INIT and CHECK_COMM_STATUS with payload [2]. The
    /// value is not in dcl_contracts.h; it is what the host sends and expects (see the handshake
    /// section of the README).
    constexpr uint8_t kCommStageLive = 2;

    // ---- panel layout, values from the host ---------------------------------------------------

    /// DeviceControlContracts::BtnLedSequence: SHIFT is tracked by the host (Controls::shiftBtnState),
    /// so its press and release must always reach it.
    constexpr uint8_t kBtnShift = 31;
    /// BtnLedSequence::SIZE. Codes 0 to 52 are the buttons the LED table covers.
    constexpr uint8_t kBtnSize = 53;
    /// BtnLedSequence::EN_COLOR_EDIT sits above SIZE. The input layer scans it too.
    constexpr uint8_t kBtnColorEdit = 54;
    /// One past the highest button code the input layer reports.
    constexpr uint8_t kBtnCodeCount = kBtnColorEdit + 1;

    /// KnobSequence: 0 is the centre jog wheel (cmd KNOB_ROTARY), 1 to 16 are the Q-Link knobs
    /// (cmd KNOB_QLINK).
    constexpr uint8_t kKnobCenter = 0;
    constexpr uint8_t kQlinkCount = 16;
    constexpr uint8_t kEncoderCount = kQlinkCount + 1;

    /// PadSequence: EN_PAD1 to EN_PAD16.
    constexpr uint8_t kPadCount = 16;

    /// The pad state byte in a PAD_STATE frame. The host acts on 1 (note on) and 0 (note off) only.
    constexpr uint8_t kPadStateReleased = 0;
    constexpr uint8_t kPadStatePressed  = 1;
    /// Not in dcl_contracts.h: a pressure update on a held pad. State 1 would retrigger the note and
    /// 0 would end it, so this is a third value. The host handlers that test for 1 and 0 only
    /// (kick_PadPerform.cpp, kick_AppGrid.cpp) ignore it, and kick_MidiControlTabs.cpp skips the
    /// note-off for it (padStatePressureUpdate) and forwards only the aftertouch message.
    constexpr uint8_t kPadStateAftertouch = 2;

    /// Touch strip: 0 means the finger lifted, 1 to 100 is a position from the bottom.
    constexpr uint8_t kStripLift = 0;
    constexpr uint8_t kStripMax  = 100;

    /// DeviceControlContracts::PadButtonState: the state byte of every LED, pad and blink command.
    enum PadButtonState : uint8_t
    {
        STATE_OFF = 0,
        STATE_ON,
        STATE_SAMPLE_ASSIGNED,
        STATE_SAMPLE_NOT_ASSIGNED,
        STATE_ON_BLINK,
        STATE_OFF_BLINK,
    };

    /// DeviceControlContracts::PadColourIds has 18 entries. 15 is PadColourTrans: no colour.
    constexpr uint8_t kPadColourCount = 18;
    constexpr uint8_t kPadColourTrans = 15;

    /// DeviceControlContracts::BtnCmd
    enum BtnCmd : uint8_t
    {
        BTN_STATUS = 0,
    };

    /// DeviceControlContracts::BtnState
    enum BtnState : uint8_t
    {
        BTN_RELEASED = 0,
        BTN_PRESSED,
        BTN_LONGPRESSED,
        BTN_DOUBLECLICK,
    };

    /// DeviceControlContracts::LedCmd
    enum LedCmd : uint8_t
    {
        LED_SET_SINGLE_LED = 0,
        LED_BLINK_SINGLE,
        LED_SET_ALL,
    };

    /// DeviceControlContracts::PadCmd
    enum PadCmd : uint8_t
    {
        PAD_STATE = 0,
        PAD_SET_SINGLE_PAD,
        PAD_SET_ALL_SINGLE_COLOUR,
        PAD_SET_ALL_MULTI_COLOUR,
        PAD_SETTINGS,
        PAD_COLOR_MODE,
        PAD_SET_COLOUR_STATE,
        PAD_SET_AFTER_TOUCH,
        PAD_SET_BLINKING,
        PAD_SET_STEP_SINGLE_PAD,
        PAD_SET_STEP_ALL_PAD,
    };

    /// DeviceControlContracts::KnobCmd
    enum KnobCmd : uint8_t
    {
        KNOB_ROTARY = 0,
        KNOB_QLINK,
    };

    /// DeviceControlContracts::KnobDirection
    enum KnobDirection : uint8_t
    {
        KNOB_ANTICLOCKWISE = 0,
        KNOB_CLOCKWISE,
    };

    /// Subcommands of SYS_SW_UPDATE, the first payload byte. The host's copy is
    /// mt::MCUUpdateAction in Kick/modules/hardware/source/kick_Hwcommon.h (not in dcl_contracts.h);
    /// tests/system_contract_test.cpp checks them against it.
    enum SwUpdateCmd : uint8_t
    {
        SW_UPDATE_INITIATE = 0,      ///< host to MCU: start
        SW_UPDATE_BEGIN_TRANSFER,    ///< MCU to host: ready for data
        SW_UPDATE_DATA,              ///< host to MCU: one Intel HEX data record
        SW_UPDATE_DATABASE,          ///< host to MCU: Intel HEX extended linear address record
        SW_UPDATE_END,               ///< host to MCU: all records sent
        SW_UPDATE_ACK,               ///< MCU to host: record accepted
        SW_UPDATE_FAILURE,           ///< MCU to host: transfer abandoned
        SW_UPDATE_IDLE,              ///< host-internal; never on the wire
    };

    /// DeviceControlContracts::SliderCmd
    enum SliderCmd : uint8_t
    {
        SLIDER_TOUCH_INPUT = 0,
        SLIDER_BAR_LED,
        SLIDER_LEVEL_LED,
        SLIDER_INDIVIDUAL_LED,
    };

} // namespace contract
} // namespace kick_mcu
