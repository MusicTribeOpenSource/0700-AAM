// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/output/current_budget.h"

namespace kick_mcu
{
namespace output
{
    namespace
    {
        // duty * mA, summed. 255 duty * 1 mA = 1 mA at full duty, so divide by 255 for mA. The
        // largest term is 2448 * 255 duty * 65535 mA, which does not fit in 32 bits, hence 64.
        uint64_t dutyMilliamps( const CurrentModel& m, const LedLoad& l )
        {
            return (uint64_t)l.padDuty * m.padChannelMa + (uint64_t)l.buttonDuty * m.buttonMa + (uint64_t)l.stripDuty * m.stripMa;
        }
    } // namespace

    uint32_t estimateMa( const CurrentModel& model, const LedLoad& load )
    {
        return (uint32_t)( ( dutyMilliamps( model, load ) + 254 ) / 255 );
    }

    uint16_t scaleQ8( const CurrentModel& model, const LedLoad& load )
    {
        const uint64_t demand = dutyMilliamps( model, load );
        const uint64_t limit  = (uint64_t)model.capMa * 255;
        if( demand <= limit )
            return 256;
        return (uint16_t)( limit * 256 / demand );
    }

} // namespace output
} // namespace kick_mcu
