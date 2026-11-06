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

#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace kickgest
{
    struct BenchOptions
    {
        std::string modelDir;
        std::vector<std::string> modelSets = {"int8", "fp32"};
        std::vector<std::pair<int, int>> sizes = {{320, 240}, {640, 480}};
        int frames = 100;
        int warmup = 10;
        int threads = 1;
        // A PPM with a hand in it. Without one the frames are noise, the palm detector finds nothing and the landmark
        // network never runs, so the numbers are a lower bound.
        std::string imagePath;
    };

    // Runs each model set at each frame size and writes a markdown table: milliseconds per frame (mean, p50, p95, max),
    // the share spent in each stage, and the peak resident memory of the process so far. Returns 0, or non-zero with a message
    // in `error`.
    int runBench(const BenchOptions& options, std::ostream& out, std::string& error);

    // "320x240,640x480" to sizes. False on malformed input.
    bool parseSizes(const std::string& text, std::vector<std::pair<int, int>>& out);
}
