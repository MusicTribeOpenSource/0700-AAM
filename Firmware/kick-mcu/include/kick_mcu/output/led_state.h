// SPDX-License-Identifier: AGPL-3.0-or-later
//
// What the host has told the panel to show, as a double-buffered table.
#pragma once

#include <atomic>
#include <cstdint>

#include "kick_mcu/contract.h"

namespace kick_mcu
{
namespace output
{
    constexpr unsigned kButtonLeds = contract::kBtnSize; ///< button codes 0 to 52
    constexpr unsigned kPadLeds    = contract::kPadCount;

    /// The touch strip's LED column. The count comes from the emulator Surface, which draws 12;
    /// the real strip is not documented. Changing it changes only how a percentage turns into LEDs.
    constexpr unsigned kStripLedCount = 12;

    struct ButtonLed
    {
        uint8_t state{ contract::STATE_OFF }; ///< a PadButtonState value, kept as the host sent it
        uint8_t colour{ 0 };                  ///< from SET_SINGLE_LED and BLINK_SINGLE; the button driver ignores it
    };

    struct PadLed
    {
        uint8_t state{ contract::STATE_OFF };
        uint8_t colour{ contract::kPadColourTrans };
        uint8_t offColour{ contract::kPadColourTrans }; ///< the other half of a blink
        uint8_t velocity{ 127 };                        ///< brightness hint from the step-sequencer commands, 0 to 127
    };

    enum class StripMode : uint8_t
    {
        Bar,        ///< SLIDER_BAR_LED: percent fills from the bottom
        Level,      ///< SLIDER_LEVEL_LED: shown the same way as Bar
        Individual, ///< SLIDER_INDIVIDUAL_LED: each LED has its own level
    };

    struct StripLeds
    {
        uint8_t   percent{ 0 }; ///< 0 to 100, for Bar and Level
        StripMode mode{ StripMode::Bar };
        uint8_t   individual[kStripLedCount]{}; ///< 0 to 100 each, for Individual
    };

    struct LedFrame
    {
        ButtonLed buttons[kButtonLeds]{};
        PadLed    pads[kPadLeds]{};
        StripLeds strip{};
    };

    /// Two LedFrames. The command decoder owns the back one: beginUpdate() copies the front into
    /// it, the decoder changes it, and commit() makes it the front in one store. The refresh task
    /// reads only front(). Whatever a command does, including the 16 pads of a MULTI_COLOUR or a
    /// STEP_ALL, becomes visible together or not at all, so a refresh never draws half a command.
    ///
    /// This is safe for one writer and one reader provided the writer does not call
    /// beginUpdate() while the reader is still drawing the frame it fetched before the previous
    /// commit(). The firmware runs the decoder and the refresh from the same main loop, so that
    /// holds trivially. Moving the refresh to an interrupt would need a third buffer.
    class LedState
    {
    public:
        /// The frame to draw.
        const LedFrame& front() const { return frames_[front_.load( std::memory_order_acquire )]; }

        /// Returns the back buffer, holding a copy of the front. Change it and call commit(), or
        /// leave it: the next beginUpdate() overwrites it.
        LedFrame& beginUpdate()
        {
            LedFrame& back = frames_[1u - front_.load( std::memory_order_relaxed )];
            back           = front();
            return back;
        }

        /// Publishes the back buffer.
        void commit()
        {
            front_.store( 1u - front_.load( std::memory_order_relaxed ), std::memory_order_release );
            ++commits_;
        }

        /// Everything off, as at power-up.
        void reset()
        {
            frames_[0] = LedFrame{};
            frames_[1] = LedFrame{};
            front_.store( 0, std::memory_order_release );
        }

        /// How many times commit() has run.
        uint32_t commits() const { return commits_; }

    private:
        LedFrame              frames_[2]{};
        std::atomic<uint8_t>  front_{ 0 };
        uint32_t              commits_{ 0 };
    };

} // namespace output
} // namespace kick_mcu
