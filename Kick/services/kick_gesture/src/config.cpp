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
#include "kick_gesture/config.h"

#include "kick_gesture/json.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

namespace kickgest
{
    uint32_t GestureConfig::minIntervalMs(GestureType t) const
    {
        switch (t)
        {
            case GestureType::SwipeLeft:
            case GestureType::SwipeRight: return rateSwipeMs;
            case GestureType::PalmHold: return ratePalmHoldMs;
            case GestureType::Fist: return rateFistMs;
            case GestureType::PinchStart: return ratePinchStartMs;
            case GestureType::PinchChange: return ratePinchChangeMs;
            default: return 0;
        }
    }

    namespace
    {
        enum class Kind
        {
            F32,
            U32,
            Bool
        };

        struct Field
        {
            const char* section;
            const char* key;
            Kind kind;
            size_t offset;
            double lo;
            double hi;
        };

#define KG_F(sec, key, member, lo, hi) {sec, key, Kind::F32, offsetof(GestureConfig, member), lo, hi}
#define KG_U(sec, key, member, lo, hi) {sec, key, Kind::U32, offsetof(GestureConfig, member), lo, hi}
#define KG_B(sec, key, member) {sec, key, Kind::Bool, offsetof(GestureConfig, member), 0, 1}

        // GestureConfig only holds scalars with default member initializers, so it stays standard-layout and offsetof is valid.
        const std::vector<Field>& fields()
        {
            static const std::vector<Field> f = {
                KG_F("tracking", "min_hand_score", minHandScore, 0.0, 1.0),
                KG_F("tracking", "min_event_confidence", minEventConfidence, 0.0, 1.0),
                KG_U("tracking", "lost_ms", lostMs, 50, 5000),
                KG_B("tracking", "flip_x", flipX),
                KG_B("tracking", "swap_handedness", swapHandedness),

                KG_F("pose", "finger_extended_ratio", fingerExtendedRatio, 0.5, 3.0),
                KG_F("pose", "thumb_extended_ratio", thumbExtendedRatio, 0.1, 3.0),
                KG_F("pose", "pinch_min_reach", pinchMinReach, 0.0, 4.0),

                KG_U("palm_hold", "hold_ms", palmHoldMs, 100, 10000),
                KG_F("palm_hold", "max_speed", palmHoldMaxSpeed, 0.0, 20.0),
                KG_U("palm_hold", "exit_ms", palmHoldExitMs, 0, 5000),

                KG_U("fist", "hold_ms", fistMs, 50, 10000),
                KG_F("fist", "max_speed", fistMaxSpeed, 0.0, 20.0),
                KG_U("fist", "exit_ms", fistExitMs, 0, 5000),

                KG_U("swipe", "window_ms", swipeWindowMs, 100, 2000),
                KG_F("swipe", "min_travel", swipeMinTravel, 0.2, 20.0),
                KG_F("swipe", "min_speed", swipeMinSpeed, 0.1, 100.0),
                KG_F("swipe", "min_dominance", swipeMinDominance, 1.0, 20.0),
                KG_F("swipe", "min_monotonic", swipeMinMonotonic, 0.5, 1.0),
                KG_U("swipe", "min_extended", swipeMinExtended, 0, 4),
                KG_U("swipe", "rearm_ms", swipeRearmMs, 0, 5000),
                KG_F("swipe", "rest_speed", swipeRestSpeed, 0.0, 20.0),

                KG_F("pinch", "on", pinchOn, 0.01, 3.0),
                KG_F("pinch", "off", pinchOff, 0.01, 3.0),
                KG_F("pinch", "min", pinchMin, 0.0, 3.0),
                KG_U("pinch", "start_ms", pinchStartMs, 0, 2000),
                KG_U("pinch", "end_ms", pinchEndMs, 0, 2000),
                KG_F("pinch", "deadband", pinchDeadband, 0.0, 1.0),

                KG_U("rate_limit_ms", "swipe", rateSwipeMs, 0, 60000),
                KG_U("rate_limit_ms", "palm_hold", ratePalmHoldMs, 0, 60000),
                KG_U("rate_limit_ms", "fist", rateFistMs, 0, 60000),
                KG_U("rate_limit_ms", "pinch_start", ratePinchStartMs, 0, 60000),
                // 50 ms is the contract ceiling of 20 Hz for pinch_change.
                KG_U("rate_limit_ms", "pinch_change", ratePinchChangeMs, 50, 60000),
            };
            return f;
        }
#undef KG_F
#undef KG_U
#undef KG_B

        char* base(GestureConfig& c, const Field& f) { return reinterpret_cast<char*>(&c) + f.offset; }
        const char* base(const GestureConfig& c, const Field& f) { return reinterpret_cast<const char*>(&c) + f.offset; }

        bool knownSection(const std::string& s)
        {
            for (const Field& f : fields())
                if (s == f.section) return true;
            return false;
        }
    }

    bool validateGestureConfig(const GestureConfig& c, std::string& error)
    {
        if (!(c.pinchMin < c.pinchOn))
        {
            error = "pinch.min must be below pinch.on";
            return false;
        }
        if (!(c.pinchOn < c.pinchOff))
        {
            error = "pinch.on must be below pinch.off (the gap is the hysteresis)";
            return false;
        }
        return true;
    }

    bool parseGestureConfig(const std::string& text, GestureConfig& cfg, std::string& error)
    {
        JsonValue root;
        if (!parseJson(text, root, error)) return false;
        if (!root.isObject())
        {
            error = "gesture config must be a JSON object";
            return false;
        }
        GestureConfig next = cfg;
        for (const auto& sec : root.object)
        {
            if (sec.first == "version")
            {
                if (!sec.second.isNumber() || sec.second.number != 1.0)
                {
                    error = "version must be 1";
                    return false;
                }
                continue;
            }
            if (!knownSection(sec.first))
            {
                error = "unknown section \"" + sec.first + "\"";
                return false;
            }
            if (!sec.second.isObject())
            {
                error = "section \"" + sec.first + "\" must be an object";
                return false;
            }
            for (const auto& kv : sec.second.object)
            {
                const Field* match = nullptr;
                for (const Field& f : fields())
                    if (sec.first == f.section && kv.first == f.key) match = &f;
                const std::string name = sec.first + "." + kv.first;
                if (match == nullptr)
                {
                    error = "unknown key \"" + name + "\"";
                    return false;
                }
                const JsonValue& v = kv.second;
                if (match->kind == Kind::Bool)
                {
                    if (v.type != JsonValue::Type::Bool)
                    {
                        error = name + " must be true or false";
                        return false;
                    }
                    *reinterpret_cast<bool*>(base(next, *match)) = v.boolean;
                    continue;
                }
                if (!v.isNumber())
                {
                    error = name + " must be a number";
                    return false;
                }
                if (v.number < match->lo || v.number > match->hi)
                {
                    char buf[160];
                    std::snprintf(buf, sizeof(buf), "%s must be between %g and %g", name.c_str(), match->lo, match->hi);
                    error = buf;
                    return false;
                }
                if (match->kind == Kind::U32)
                {
                    if (v.number != std::floor(v.number))
                    {
                        error = name + " must be a whole number";
                        return false;
                    }
                    *reinterpret_cast<uint32_t*>(base(next, *match)) = static_cast<uint32_t>(v.number);
                }
                else
                    *reinterpret_cast<float*>(base(next, *match)) = static_cast<float>(v.number);
            }
        }
        if (!validateGestureConfig(next, error)) return false;
        cfg = next;
        return true;
    }

    std::string gestureConfigToJson(const GestureConfig& cfg)
    {
        std::string out = "{\n  \"version\": 1";
        const char* section = nullptr;
        for (const Field& f : fields())
        {
            if (section == nullptr || std::strcmp(section, f.section) != 0)
            {
                if (section != nullptr) out += "\n  }";
                out += std::string(",\n  \"") + f.section + "\": {\n";
                section = f.section;
            }
            else
                out += ",\n";
            char val[48];
            switch (f.kind)
            {
                case Kind::Bool: std::snprintf(val, sizeof(val), "%s", *reinterpret_cast<const bool*>(base(cfg, f)) ? "true" : "false"); break;
                case Kind::U32: std::snprintf(val, sizeof(val), "%u", *reinterpret_cast<const uint32_t*>(base(cfg, f))); break;
                default: std::snprintf(val, sizeof(val), "%g", static_cast<double>(*reinterpret_cast<const float*>(base(cfg, f)))); break;
            }
            out += std::string("    \"") + f.key + "\": " + val;
        }
        out += "\n  }\n}\n";
        return out;
    }
}
