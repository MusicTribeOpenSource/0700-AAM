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

#include <vector>

namespace kickgest
{
    struct Anchor
    {
        float x; // centre, 0 to 1 across the 192 x 192 palm detector input
        float y;
    };

    // The 2016 SSD anchors of the MediaPipe palm detector: a 24 x 24 grid with 2 anchors per cell (stride 8), then a
    // 12 x 12 grid with 6 anchors per cell (the three stride 16 layers merged). Anchors have a fixed size, so only
    // the centres are kept.
    const std::vector<Anchor>& palmAnchors();
}
