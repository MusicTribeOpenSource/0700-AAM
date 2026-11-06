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
#include "kick_gesture/features.h"

#include <cmath>

namespace kickgest
{
    namespace
    {
        struct P
        {
            float x, y;
        };

        float dist(P a, P b) { return std::hypot(a.x - b.x, a.y - b.y); }
    }

    bool computeFeatures(const Hand& hand, float aspect, const GestureConfig& cfg, HandFeatures& out)
    {
        if (!std::isfinite(aspect) || aspect <= 0.f) return false;
        P p[kNumLandmarks];
        for (int i = 0; i < kNumLandmarks; ++i)
        {
            const Point3& q = hand.lm[static_cast<size_t>(i)];
            if (!std::isfinite(q.x) || !std::isfinite(q.y)) return false;
            const float ux = cfg.flipX ? 1.f - q.x : q.x;
            p[i] = {ux * aspect, q.y};
        }
        const float palm = dist(p[WRIST], p[MIDDLE_MCP]);
        if (!(palm >= 0.001f)) return false;

        HandFeatures f;
        f.palm = palm;
        f.cx = (p[WRIST].x + p[INDEX_MCP].x + p[MIDDLE_MCP].x + p[RING_MCP].x + p[PINKY_MCP].x) / 5.f;
        f.cy = (p[WRIST].y + p[INDEX_MCP].y + p[MIDDLE_MCP].y + p[RING_MCP].y + p[PINKY_MCP].y) / 5.f;

        const int mcp[4] = {INDEX_MCP, MIDDLE_MCP, RING_MCP, PINKY_MCP};
        for (int i = 0; i < 4; ++i)
        {
            const float toPip = dist(p[WRIST], p[mcp[i] + 1]);
            const float toTip = dist(p[WRIST], p[mcp[i] + 3]);
            f.ext[i] = toPip > 1e-6f && toTip / toPip > cfg.fingerExtendedRatio;
            if (f.ext[i]) ++f.extCount;
        }
        f.thumbExt = dist(p[THUMB_TIP], p[INDEX_MCP]) / palm > cfg.thumbExtendedRatio;
        f.pinchRatio = dist(p[THUMB_TIP], p[INDEX_TIP]) / palm;
        f.indexReach = dist(p[WRIST], p[INDEX_TIP]) / palm;
        out = f;
        return true;
    }
}
