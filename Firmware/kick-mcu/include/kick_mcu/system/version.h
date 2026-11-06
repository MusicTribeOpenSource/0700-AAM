// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The firmware version, and how it reaches the host.
#pragma once

#include "kick_mcu/system/image.h"

namespace kick_mcu
{
namespace system
{
    /// Reported when the running image has no valid header to take a version from (the sim with
    /// no flash, or a build the stamping tool has not touched). It matches what the handshake
    /// reported before the update work, so the existing byte-for-byte handshake tests still hold.
    constexpr VersionNo kBuildVersion{ 1, 0, 0 };

    /// The version the host sees at handshake time is `GET_SW_VER [major, minor]`: two bytes,
    /// each printed in decimal and joined with a dot (kick_AppHome.cpp,
    /// `juce::String( data[0] ) + "." + juce::String( data[1] )`). The host reads nothing else
    /// from that frame, so `bug` never reaches it except in the extended report
    /// (encodeVersionReport) and the host's update gate compares major and minor only.
    /// 255.255 is the largest version the frame can carry.
    ///
    /// typeVersionNo in Defines.hpp is { ui8Main, ui8Sub, ui8Bug, ui8Dummy }, and VersionNo maps
    /// to it field for field: major to ui8Main, minor to ui8Sub, bug to ui8Bug. The dummy byte is
    /// alignment padding ("do not use") and is not carried.

} // namespace system
} // namespace kick_mcu
