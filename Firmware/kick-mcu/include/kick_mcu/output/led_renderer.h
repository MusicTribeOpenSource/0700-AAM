// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The refresh task: turns the LED table into driver levels and owns the ILedDriver.
#pragma once

#include <cstdint>
#include <cstring>

#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/output/blink.h"
#include "kick_mcu/output/colours.h"
#include "kick_mcu/output/current_budget.h"
#include "kick_mcu/output/led_state.h"

namespace kick_mcu
{
namespace output
{
    struct RenderConfig
    {
        float gamma{ 2.2f };

        /// How often service() draws, in ms. Blink edges land on a draw, so this is also the
        /// blink timing resolution.
        uint16_t refreshIntervalMs{ 10 };

        /// Multiplies every colour before gamma, in 1/256ths (256 = unchanged). Meant for a
        /// brightness setting; nothing sets it yet.
        uint16_t masterQ8{ 256 };

        /// Pad brightness for SAMPLE_ASSIGNED (0.75) and SAMPLE_NOT_ASSIGNED (0.35), in 1/256ths.
        /// The Surface dims those states by the same factors; the real feel is a hardware call.
        uint16_t sampleAssignedQ8{ 192 };
        uint16_t sampleNotAssignedQ8{ 90 };

        /// Duty of a lit button LED and of a fully lit strip LED.
        uint8_t buttonDuty{ 255 };
        uint8_t stripDuty{ 255 };

        CurrentModel current{};
    };

    /// What goes to the driver: PWM duty, 0 to 255, after gamma and the current cap.
    struct RenderedFrame
    {
        uint8_t buttons[kButtonLeds]{};
        uint8_t pads[kPadLeds][3]{}; ///< red, green, blue
        uint8_t strip[kStripLedCount]{};

        bool operator==( const RenderedFrame& o ) const { return std::memcmp( this, &o, sizeof( *this ) ) == 0; }
        bool operator!=( const RenderedFrame& o ) const { return !( *this == o ); }
    };

    /// What the current cap did to one render.
    struct RenderInfo
    {
        uint32_t demandMa{ 0 };  ///< estimated draw before the cap
        uint32_t outputMa{ 0 };  ///< estimated draw of the frame that goes out
        uint16_t scaleQ8{ 256 }; ///< factor the cap applied, 256 if it did nothing
    };

    struct RenderStats
    {
        uint32_t draws{ 0 };       ///< frames rendered
        uint32_t pushes{ 0 };      ///< frames that differed from the last and went to the driver
        uint32_t limited{ 0 };     ///< draws where the current cap had to scale the output down
        uint32_t demandMa{ 0 };    ///< estimated draw of the last draw before the cap
        uint32_t outputMa{ 0 };    ///< estimated draw after the cap
        uint16_t scaleQ8{ 256 };   ///< factor the cap applied to the last draw
    };

    /// Reads the front LedFrame at a fixed rate and drives the LEDs. Packets never touch the
    /// driver; they change the table and this class draws it.
    ///
    /// One pass of the pipeline, in this order:
    ///  1. Blink: a blinking state shows one colour in the clock's "on" half and the other in the
    ///     "off" half. ON_BLINK is colour then offColour, OFF_BLINK is the reverse. Button LEDs
    ///     are lit in the half where they would show their colour (ON_BLINK on, OFF_BLINK off).
    ///  2. Colour: the pad's colour id looks up an RGB value. PadColourTrans is dark. Unknown
    ///     states are dark.
    ///  3. Perceptual scaling, before gamma: state dimming, velocity (velocity / 127), master.
    ///  4. Gamma, per channel.
    ///  5. Current cap: if the estimated draw of the whole frame is over the budget, every duty is
    ///     scaled down by the same factor.
    ///
    /// The frame is pushed to the driver, and committed, only when it differs from the previous
    /// one, so an idle panel causes no bus traffic.
    class LedRefresh
    {
    public:
        LedRefresh( bsp::ILedDriver& driver, const LedState& state, BlinkClock& blink, const bsp::IClock& clock, const RenderConfig& config = {} );

        /// Call once per main-loop pass. Draws when refreshIntervalMs has passed since the last
        /// draw (the first call draws at once).
        void service();

        /// Draws now, whatever the interval, updating the blink clock first.
        void drawNow();

        /// Renders a table without touching the driver or the blink clock. For tests, and for
        /// service() itself. `info`, if given, reports what the current cap did.
        void render( const LedFrame& frame, bool blinkOn, RenderedFrame& out, RenderInfo* info = nullptr ) const;

        ColourTable&         colours() { return colours_; }
        GammaTable&          gamma() { return gamma_; }
        RenderConfig&        config() { return config_; }
        const RenderedFrame& last() const { return last_; }
        const RenderStats&   stats() const { return stats_; }

    private:
        void renderPad( const PadLed& pad, bool blinkOn, uint8_t out[3] ) const;

        bsp::ILedDriver&   driver_;
        const LedState&    state_;
        BlinkClock&        blink_;
        const bsp::IClock& clock_;
        RenderConfig       config_;
        ColourTable        colours_;
        GammaTable         gamma_;
        RenderedFrame      last_{};
        RenderStats        stats_{};
        uint32_t           lastDrawMs_{ 0 };
        bool               everDrawn_{ false };
    };

} // namespace output
} // namespace kick_mcu
