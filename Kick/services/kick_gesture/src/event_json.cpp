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
#include "kick_gesture/event_json.h"

#include "kick_gesture/json.h"

#include <cmath>
#include <cstdio>

namespace kickgest
{
    namespace
    {
        // Three decimals, clamped to the contract range.
        std::string unit(float v)
        {
            const float c = std::isfinite(v) ? (v < 0.f ? 0.f : (v > 1.f ? 1.f : v)) : 0.f;
            char buf[24];
            std::snprintf(buf, sizeof(buf), "%.3f", static_cast<double>(c));
            return buf;
        }
    }

    std::string eventToJson(const GestureEvent& e)
    {
        std::string s = std::string("{\"type\":\"") + gestureName(e.type) + "\"";
        if (e.hasValue) s += ",\"value\":" + unit(e.value);
        s += std::string(",\"hand\":\"") + handednessName(e.hand) + "\",\"confidence\":" + unit(e.confidence);
        s += ",\"t_ms\":" + std::to_string(e.tMs) + "}";
        return s;
    }

    std::string statusToJson(const StatusMessage& m)
    {
        char num[48];
        std::string s = "{\"type\":\"status\",\"state\":\"" + jsonEscape(m.state) + "\"";
        if (!m.reason.empty()) s += ",\"reason\":\"" + jsonEscape(m.reason) + "\"";
        std::snprintf(num, sizeof(num), "%.1f", m.rateHz);
        s += std::string(",\"rate_hz\":") + num;
        std::snprintf(num, sizeof(num), "%.2f", m.cpu);
        s += std::string(",\"cpu\":") + num;
        if (m.tempC)
        {
            std::snprintf(num, sizeof(num), "%.1f", *m.tempC);
            s += std::string(",\"temp_c\":") + num;
        }
        s += ",\"t_ms\":" + std::to_string(m.tMs) + "}";
        return s;
    }
}
