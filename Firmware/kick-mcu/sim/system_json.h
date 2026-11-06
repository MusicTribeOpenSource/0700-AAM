// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Boot and flash state as one line of JSON, for --boot-report and --inspect.
#pragma once

#include <string>

#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/system/boot_control.h"
#include "kick_mcu/system/self_check.h"

namespace kick_mcu
{
namespace sim
{
    /// {"boot":N,"bootable":true,"slot":"B","trial":true,"reverted":false,"fellBack":false,
    ///  "recordMissing":false,"state":"TrialBooted","status":"ok","version":"1.1.0"}
    /// `index` counts boots in this process, from 1.
    std::string bootReportJson( unsigned index, const system::BootDecision& decision );

    /// What is in the flash, without booting it or writing anything:
    /// {"record":{"seq":3,"state":"Confirmed","good":"B","trial":"B"}|null,
    ///  "A":{"status":"ok","version":"1.0.0","imageBytes":214},"B":{...}}
    std::string flashInspectJson( bsp::IFlash& flash, const system::FlashLayout& layout );

} // namespace sim
} // namespace kick_mcu
