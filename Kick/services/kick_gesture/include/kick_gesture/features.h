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

#include "kick_gesture/config.h"
#include "kick_gesture/types.h"

namespace kickgest
{
    // What the state machines look at, computed from one hand. Positions are in user-frame, aspect-corrected
    // units (x to the user's right, y down, one unit is the frame height), so distances are isotropic.
    struct HandFeatures
    {
        float cx = 0.f; // palm centre: mean of the wrist and the four finger bases
        float cy = 0.f;
        float palm = 0.f;      // wrist to middle finger base
        bool thumbExt = false; // thumb extended away from the palm
        bool ext[4] = {false, false, false, false}; // index, middle, ring, pinky
        int extCount = 0;      // how many of ext[] are true
        float pinchRatio = 0.f; // dist(thumb tip, index tip) / palm
        float indexReach = 0.f; // dist(wrist, index tip) / palm
    };

    // False when a coordinate is not finite or the hand is degenerate (palm smaller than 0.001 of the frame height).
    bool computeFeatures(const Hand& hand, float aspect, const GestureConfig& cfg, HandFeatures& out);
}
