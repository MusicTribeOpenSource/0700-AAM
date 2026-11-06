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
#include "kick_gesture/synth.h"

#include <cmath>

namespace kickgest
{
    namespace
    {
        struct V
        {
            float x, y;
        };

        constexpr float kPi = 3.14159265358979f;
    }

    SynthPose synthOpenPalm(float x, float y)
    {
        SynthPose p;
        p.x = x;
        p.y = y;
        return p;
    }

    SynthPose synthFist(float x, float y)
    {
        SynthPose p;
        p.x = x;
        p.y = y;
        for (float& c : p.curl) c = 1.f;
        p.thumbOut = false;
        return p;
    }

    SynthPose synthPinch(float x, float y, float ratio)
    {
        SynthPose p;
        p.x = x;
        p.y = y;
        p.pinch = ratio;
        return p;
    }

    Hand synthHand(const SynthPose& pose, float aspect, bool flipX)
    {
        // Local coordinates, palm size 1, x to the user's right, y down, wrist at the origin.
        V pts[kNumLandmarks];
        pts[WRIST] = {0.f, 0.f};

        const V mcp[4] = {{-0.45f, -0.95f}, {0.f, -1.f}, {0.40f, -0.92f}, {0.78f, -0.82f}};
        const float seg[4][3] = {{0.45f, 0.30f, 0.25f}, {0.50f, 0.33f, 0.27f}, {0.45f, 0.30f, 0.25f}, {0.35f, 0.22f, 0.20f}};
        for (int f = 0; f < 4; ++f)
        {
            const float c = pose.curl[f];
            // Cumulative bend at each joint, seen from the front: the projected segment length shrinks with the
            // cosine of the angle and goes negative once the finger folds past the palm.
            const float bend[3] = {80.f * c, 100.f * c, 70.f * c};
            V p = mcp[f];
            float cum = 0.f;
            const int base = INDEX_MCP + 4 * f;
            pts[base] = p;
            for (int s = 0; s < 3; ++s)
            {
                cum += bend[s] * kPi / 180.f;
                p.y -= seg[f][s] * std::cos(cum);
                pts[base + 1 + s] = p;
            }
        }

        pts[THUMB_CMC] = {-0.25f, -0.2f};
        if (pose.thumbOut)
        {
            pts[THUMB_MCP] = {-0.55f, -0.45f};
            pts[THUMB_IP] = {-0.85f, -0.65f};
            pts[THUMB_TIP] = {-1.15f, -0.8f};
        }
        else
        {
            pts[THUMB_MCP] = {-0.30f, -0.35f};
            pts[THUMB_IP] = {-0.33f, -0.45f};
            pts[THUMB_TIP] = {-0.35f, -0.55f};
        }
        if (pose.pinch >= 0.f)
        {
            const V tip = pts[INDEX_TIP];
            pts[THUMB_TIP] = {tip.x - pose.pinch, tip.y};
            pts[THUMB_IP] = {(pts[THUMB_MCP].x + pts[THUMB_TIP].x) * 0.5f, (pts[THUMB_MCP].y + pts[THUMB_TIP].y) * 0.5f};
        }

        // Centre the palm on (x, y): the engine's palm centre is the mean of the wrist and the four finger bases.
        V centre{0.f, 0.f};
        const int palmIdx[5] = {WRIST, INDEX_MCP, MIDDLE_MCP, RING_MCP, PINKY_MCP};
        for (int i : palmIdx)
        {
            centre.x += pts[i].x / 5.f;
            centre.y += pts[i].y / 5.f;
        }

        const float rot = pose.rotationDeg * kPi / 180.f;
        const float cs = std::cos(rot);
        const float sn = std::sin(rot);
        Hand h;
        h.score = pose.score;
        h.rightProb = pose.rightProb;
        for (int i = 0; i < kNumLandmarks; ++i)
        {
            const float lx = pts[i].x - centre.x;
            const float ly = pts[i].y - centre.y;
            const float dx = (lx * cs - ly * sn) * pose.size;
            const float dy = (lx * sn + ly * cs) * pose.size;
            const float ux = (pose.x * aspect + dx) / aspect;
            const float uy = pose.y + dy;
            Point3 q;
            q.x = flipX ? 1.f - ux : ux;
            q.y = uy;
            q.z = 0.f;
            h.lm[static_cast<size_t>(i)] = q;
        }
        return h;
    }

    LandmarkFrame synthFrame(uint64_t tMs, const SynthPose& pose, float aspect, bool flipX)
    {
        LandmarkFrame f;
        f.tMs = tMs;
        f.aspect = aspect;
        f.hands.push_back(synthHand(pose, aspect, flipX));
        return f;
    }

    LandmarkFrame synthEmptyFrame(uint64_t tMs, float aspect)
    {
        LandmarkFrame f;
        f.tMs = tMs;
        f.aspect = aspect;
        return f;
    }
}
