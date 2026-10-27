// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_core/juce_core.h>
namespace mt {
inline bool hardwareEnabled() noexcept {
#if JUCE_LINUX && defined(KICK_ENABLE_HARDWARE) && KICK_ENABLE_HARDWARE
    const auto* disabled = std::getenv("KICK_NO_HARDWARE");
    return !disabled || std::strcmp(disabled, "1") != 0;
#else
    return false;
#endif
}
}
