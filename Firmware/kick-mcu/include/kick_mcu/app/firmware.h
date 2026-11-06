// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The firmware's main loop body: transport plus input scanning.
#pragma once

#include "kick_mcu/app/scan_loop.h"
#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/link/link.h"
#include "kick_mcu/output/blink.h"
#include "kick_mcu/output/commands.h"
#include "kick_mcu/output/led_renderer.h"
#include "kick_mcu/output/led_state.h"
#include "kick_mcu/system/system.h"

namespace kick_mcu
{
namespace app
{
    struct FirmwareConfig
    {
        link::LinkConfig link{};
        ScanConfig       scan{};
        output::RenderConfig render{};
        uint32_t             blinkPeriodMs{ 500 }; ///< one on-off cycle of every blinking LED; 500 is 2 Hz
        system::SystemConfig system{};              ///< flash, power and the bootloader's decision; all optional
        bool                 blinkFollowsBpm{ false }; ///< blink once per beat once the host has sent a tempo (system/bpm.h)
    };

    /// The board's output drivers, borrowed.
    struct OutputHardware
    {
        bsp::ILedDriver& leds;
    };

    /// A driver that shows nothing, for a build with no LEDs attached.
    class NullLedDriver : public bsp::ILedDriver
    {
    public:
        void setButton( unsigned, uint8_t ) override {}
        void setPad( unsigned, uint8_t, uint8_t, uint8_t ) override {}
        void setStrip( unsigned, uint8_t ) override {}
        void commit() override {}
    };

    /// Connects the input layer's sink to a Link: events go into the transmit queue, and are
    /// refused until the host handshake is done.
    class LinkSink : public input::IEventSink
    {
    public:
        explicit LinkSink( link::Link& link )
            : link_( link )
        {
        }

        /// Once the unit has been told to shut down it stops reporting input.
        void setMute( const bool* muted ) { muted_ = muted; }

        bool accepting() const override { return link_.session().streaming() && !( muted_ != nullptr && *muted_ ); }
        bool send( link::Lane lane, uint8_t type, uint8_t cmd, const uint8_t* payload, size_t length ) override { return link_.send( lane, type, cmd, payload, length ); }
        void discardQueued( link::Lane lane, uint8_t type, uint8_t firstByte ) override { link_.tx().discard( lane, type, firstByte ); }

    private:
        link::Link& link_;
        const bool* muted_{ nullptr };
    };

    /// What runs on the MCU. step() is one pass of the main loop: scan, then let the transport
    /// receive, run the heartbeat and send, then refresh the LEDs. The caller decides how often
    /// to call it; the latency figures in the README assume a pass every 250 microseconds.
    ///
    /// Host commands the firmware acts on: PAD_SET_AFTER_TOUCH (turns pad aftertouch on or off),
    /// every LED, PAD and SLIDER command (output::CommandDecoder), and the SYSTEM commands the
    /// session leaves alone: GET_SW_VER, BPM, SHUTDOWN and SW_UPDATE (system::System). Anything
    /// else is counted in hostFramesIgnored(). LED state is not cleared when the host reconnects:
    /// the table keeps what it was told, like the emulator Surface. After SHUTDOWN the LEDs are
    /// off, further LED commands are dropped, and no more input events are sent.
    ///
    /// The version in the handshake comes from the running image's header when the system config
    /// supplies flash and the image verifies, and is system::kBuildVersion otherwise.
    class Firmware
    {
    public:
        /// With LEDs.
        Firmware( bsp::IUart& uart, const bsp::IClock& clock, InputHardware hardware, OutputHardware outputs, const FirmwareConfig& config = {} );

        /// Without: the LED table still tracks the host, and nothing is drawn.
        Firmware( bsp::IUart& uart, const bsp::IClock& clock, InputHardware hardware, const FirmwareConfig& config = {} );

        void step();

        link::Link&            link() { return link_; }
        ScanLoop&              scan() { return scan_; }
        uint32_t               hostFramesIgnored() const { return hostFramesIgnored_; }
        output::LedState&      leds() { return ledState_; }
        output::CommandDecoder& commands() { return commands_; }
        output::LedRefresh&    refresh() { return refresh_; }
        output::BlinkClock&    blink() { return blink_; }
        system::System&        system() { return system_; }

    private:
        Firmware( bsp::IUart& uart, const bsp::IClock& clock, InputHardware hardware, bsp::ILedDriver& leds, const FirmwareConfig& config );

        static void onHostFrame( void* context, const link::Frame& frame );
        static void quiesce( void* context );

        NullLedDriver          nullLeds_;
        system::SelfCheckResult selfCheck_; ///< before link_: the handshake reports its version
        link::Link             link_;
        LinkSink               sink_;
        ScanLoop               scan_;
        output::LedState       ledState_;
        output::CommandDecoder commands_;
        output::BlinkClock     blink_;
        output::LedRefresh     refresh_;
        system::System         system_;
        bool                   blinkFollowsBpm_;
        uint16_t               lastBpmApplied_{ system::Bpm::kUnset };
        uint32_t               hostFramesIgnored_{ 0 };
    };

} // namespace app
} // namespace kick_mcu
