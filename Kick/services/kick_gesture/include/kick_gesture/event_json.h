/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "kick_gesture/guard.h"
#include "kick_gesture/types.h"

#include <cstdint>
#include <optional>
#include <string>

// The wire format of docs/gesture-events.md: one JSON object per line.
namespace kickgest
{
    // {"type":"pinch_change","value":0.512,"hand":"left","confidence":0.934,"t_ms":123456}
    // "value" is present only for the three pinch events. No trailing newline.
    std::string eventToJson(const GestureEvent& e);

    struct StatusMessage
    {
        // idle: camera off. no_camera: the camera service is not there. running, degraded, disabled: guard states.
        // error: the model failed. stopped: the service is shutting down.
        std::string state;
        std::string reason; // empty or a short machine-readable reason
        double rateHz = 0.0;
        double cpu = 0.0;
        std::optional<double> tempC;
        uint64_t tMs = 0;
    };

    // {"type":"status","state":"degraded","reason":"cpu_soft","rate_hz":6.0,"cpu":0.64,"temp_c":71.0,"t_ms":123456}
    // "reason" and "temp_c" are left out when empty or unknown.
    std::string statusToJson(const StatusMessage& s);
}
