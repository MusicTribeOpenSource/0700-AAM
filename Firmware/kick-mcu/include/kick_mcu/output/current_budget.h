// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Caps the total LED current. The arithmetic is written out in docs/mcu-led-current-budget.md.
#pragma once

#include <cstdint>

namespace kick_mcu
{
namespace output
{
    /// What each output draws, and how much the LEDs may draw together.
    ///
    /// ALL OF THESE ARE ASSUMPTIONS. The LED parts, the driver chips and the board's supply are
    /// not documented. A PWM-dimmed LED behind a constant-current driver draws a current in
    /// proportion to its duty, which is the only physics this model uses.
    struct CurrentModel
    {
        /// One colour channel of one pad at 100% duty. A pad has three channels.
        uint16_t padChannelMa{ 20 };
        /// One button LED at 100% duty.
        uint16_t buttonMa{ 5 };
        /// One strip LED at 100% duty.
        uint16_t stripMa{ 5 };
        /// The most all the LEDs together may draw.
        uint16_t capMa{ 600 };
    };

    /// Each output's total duty, summed over all of its channels or LEDs, 0 to 255 per channel.
    struct LedLoad
    {
        uint32_t padDuty{ 0 };    ///< 16 pads * 3 channels * 255 at most
        uint32_t buttonDuty{ 0 }; ///< 53 * 255 at most
        uint32_t stripDuty{ 0 };  ///< 12 * 255 at most
    };

    /// Estimated draw in mA, rounded up.
    uint32_t estimateMa( const CurrentModel& model, const LedLoad& load );

    /// The factor, in 1/256ths, to multiply every duty by so the estimate stays at or under
    /// capMa: 256 when the load is already within the cap, less when it is not. Rounded down, so
    /// the result never exceeds the cap. Duties scaled as (duty * scale) >> 8 round down too.
    uint16_t scaleQ8( const CurrentModel& model, const LedLoad& load );

} // namespace output
} // namespace kick_mcu
