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
#include "kick_gesture/types.h"

#include <cmath>
#include <cstring>

namespace kickgest
{
    namespace
    {
        float round6(float v) { return static_cast<float>(std::round(static_cast<double>(v) * 1e6) / 1e6); }
    }

    void LandmarkFrame::quantize()
    {
        aspect = round6(aspect);
        for (Hand& h : hands)
        {
            for (Point3& p : h.lm)
            {
                p.x = round6(p.x);
                p.y = round6(p.y);
                p.z = round6(p.z);
            }
            h.score = round6(h.score);
            h.rightProb = round6(h.rightProb);
        }
    }

    namespace
    {
        const char* const kNames[kGestureTypeCount] = {"swipe_left", "swipe_right", "palm_hold",   "fist",
                                                       "pinch_start", "pinch_change", "pinch_end"};
    }

    const char* gestureName(GestureType t)
    {
        const int i = static_cast<int>(t);
        return (i >= 0 && i < kGestureTypeCount) ? kNames[i] : "unknown";
    }

    bool parseGestureName(const char* name, GestureType& out)
    {
        if (name == nullptr) return false;
        for (int i = 0; i < kGestureTypeCount; ++i)
        {
            if (std::strcmp(name, kNames[i]) == 0)
            {
                out = static_cast<GestureType>(i);
                return true;
            }
        }
        return false;
    }

    const char* handednessName(Handedness h)
    {
        switch (h)
        {
            case Handedness::Left: return "left";
            case Handedness::Right: return "right";
            default: return "unknown";
        }
    }
}
