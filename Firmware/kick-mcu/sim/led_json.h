// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The LED table as one line of JSON, the format the emulator Surface writes with --dump-leds.
// The two have to agree byte for byte, because the replay comparison diffs the files.
#pragma once

#include <string>

#include "kick_mcu/output/led_state.h"

namespace kick_mcu
{
namespace sim
{
    /// {"buttons":[<53 states>],"pads":[[state,colour,offColour,velocity] * 16],"slider":<percent>}
    /// followed by a newline, no spaces. The firmware keeps more than this (button colours, the
    /// strip mode, individual strip levels) because the Surface does not model it; those are not
    /// part of the comparison.
    std::string ledTableJson( const output::LedFrame& frame );

} // namespace sim
} // namespace kick_mcu
