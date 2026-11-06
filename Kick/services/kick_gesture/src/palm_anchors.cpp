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
#include "kick_gesture/palm_anchors.h"

namespace kickgest
{
    const std::vector<Anchor>& palmAnchors()
    {
        static const std::vector<Anchor> anchors = [] {
            std::vector<Anchor> a;
            a.reserve(2016);
            const struct
            {
                int grid;
                int perCell;
            } layers[] = {{24, 2}, {12, 6}};
            for (const auto& layer : layers)
                for (int y = 0; y < layer.grid; ++y)
                    for (int x = 0; x < layer.grid; ++x)
                        for (int k = 0; k < layer.perCell; ++k)
                            a.push_back({(static_cast<float>(x) + 0.5f) / static_cast<float>(layer.grid), (static_cast<float>(y) + 0.5f) / static_cast<float>(layer.grid)});
            return a;
        }();
        return anchors;
    }
}
