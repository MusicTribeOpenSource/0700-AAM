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
#include "kick_camera/types.h"

#include <algorithm>
#include <cctype>
#include <chrono>

namespace kickcam
{
    uint32_t minStride(uint32_t fourcc, uint32_t width)
    {
        if (fourcc == kFourccYuyv) return width * 2;
        if (fourcc == kFourccNv12) return width;
        return 0;
    }

    size_t frameBytes(const FrameFormat& f)
    {
        const uint32_t minimum = minStride(f.fourcc, f.width);
        if (f.width == 0 || f.height == 0 || minimum == 0 || f.stride < minimum) return 0;
        if ((f.width & 1u) != 0) return 0; // both formats share chroma between pixel pairs
        if (f.fourcc == kFourccYuyv) return static_cast<size_t>(f.stride) * f.height;
        if ((f.height & 1u) != 0) return 0;
        return static_cast<size_t>(f.stride) * f.height * 3 / 2;
    }

    bool parseFormatName(const std::string& name, uint32_t& fourcc)
    {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        if (lower == "yuyv") { fourcc = kFourccYuyv; return true; }
        if (lower == "nv12") { fourcc = kFourccNv12; return true; }
        return false;
    }

    const char* formatName(uint32_t fourcc)
    {
        if (fourcc == kFourccYuyv) return "YUYV";
        if (fourcc == kFourccNv12) return "NV12";
        return "unknown";
    }

    uint64_t monotonicNs()
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }
}
