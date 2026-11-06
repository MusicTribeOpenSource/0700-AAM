// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A fake LED driver for tests and the simulator. Values are staged by the set calls and become
// the visible ones on commit(), the way the ILedDriver contract says.
#pragma once

#include <cstdint>

#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/output/led_state.h"

namespace kick_mcu
{
namespace sim
{
    class SimLedDriver : public bsp::ILedDriver
    {
    public:
        struct Levels
        {
            uint8_t buttons[output::kButtonLeds]{};
            uint8_t pads[output::kPadLeds][3]{};
            uint8_t strip[output::kStripLedCount]{};
        };

        void setButton( unsigned index, uint8_t brightness ) override
        {
            if( index < output::kButtonLeds )
                staged_.buttons[index] = brightness;
            else
                ++badIndex_;
        }

        void setPad( unsigned index, uint8_t red, uint8_t green, uint8_t blue ) override
        {
            if( index < output::kPadLeds )
            {
                staged_.pads[index][0] = red;
                staged_.pads[index][1] = green;
                staged_.pads[index][2] = blue;
            }
            else
                ++badIndex_;
        }

        void setStrip( unsigned index, uint8_t brightness ) override
        {
            if( index < output::kStripLedCount )
                staged_.strip[index] = brightness;
            else
                ++badIndex_;
        }

        void commit() override
        {
            visible_ = staged_;
            ++commits_;
        }

        /// What the LEDs are showing: the state at the last commit().
        const Levels& visible() const { return visible_; }
        uint32_t      commits() const { return commits_; }
        uint32_t      badIndexCalls() const { return badIndex_; }

    private:
        Levels   staged_{};
        Levels   visible_{};
        uint32_t commits_{ 0 };
        uint32_t badIndex_{ 0 };
    };

} // namespace sim
} // namespace kick_mcu
