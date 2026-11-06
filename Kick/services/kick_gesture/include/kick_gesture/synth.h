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

#include "kick_gesture/types.h"

// Synthetic hands for tests, the eval example and the canned replay stream. The skeleton is a flat 2D
// projection with plausible proportions (palm size 1 = wrist to middle finger base), not a model of anatomy:
// it only has to drive the feature extraction and the state machines the way a real hand would.
namespace kickgest
{
    struct SynthPose
    {
        // Palm centre in the user's frame: x as a fraction of the frame width (0 is the user's left), y as a
        // fraction of the frame height.
        float x = 0.5f;
        float y = 0.5f;
        // Palm size (wrist to middle finger base) as a fraction of the frame height.
        float size = 0.12f;
        float rotationDeg = 0.f;
        // 0 is a straight finger, 1 is fully curled. Index, middle, ring, pinky.
        float curl[4] = {0.f, 0.f, 0.f, 0.f};
        bool thumbOut = true;
        // When 0 or more, the thumb tip is put this far from the index tip, in palm sizes.
        float pinch = -1.f;
        float score = 0.95f;
        float rightProb = 0.8f;
    };

    SynthPose synthOpenPalm(float x, float y);
    SynthPose synthFist(float x, float y);
    // Index and thumb at `ratio` palm sizes apart, the other fingers extended.
    SynthPose synthPinch(float x, float y, float ratio);

    Hand synthHand(const SynthPose& pose, float aspect, bool flipX);
    LandmarkFrame synthFrame(uint64_t tMs, const SynthPose& pose, float aspect, bool flipX);
    LandmarkFrame synthEmptyFrame(uint64_t tMs, float aspect);
}
