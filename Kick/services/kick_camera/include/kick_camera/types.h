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

#include <cstddef>
#include <cstdint>
#include <string>

namespace kickcam
{
    constexpr uint32_t makeFourcc(char a, char b, char c, char d)
    {
        return static_cast<uint32_t>(static_cast<uint8_t>(a)) | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8) |
               (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16) | (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
    }

    // Same values as V4L2_PIX_FMT_YUYV and V4L2_PIX_FMT_NV12, so a V4L2 fourcc can be stored as is.
    constexpr uint32_t kFourccYuyv = makeFourcc('Y', 'U', 'Y', 'V');
    constexpr uint32_t kFourccNv12 = makeFourcc('N', 'V', '1', '2');

    struct FrameFormat
    {
        uint32_t width = 0;
        uint32_t height = 0;
        // Bytes per row of the first plane. For NV12 the interleaved UV plane uses the same stride.
        uint32_t stride = 0;
        uint32_t fourcc = 0;

        bool operator==(const FrameFormat& o) const
        {
            return width == o.width && height == o.height && stride == o.stride && fourcc == o.fourcc;
        }
        bool operator!=(const FrameFormat& o) const { return !(*this == o); }
    };

    // Smallest stride for a width, 0 for an unsupported fourcc.
    uint32_t minStride(uint32_t fourcc, uint32_t width);

    // Bytes in one frame, 0 when the format is unsupported or inconsistent
    // (zero size, stride below the minimum, odd width, odd height for NV12).
    size_t frameBytes(const FrameFormat& f);

    // "yuyv" or "nv12" (case-insensitive) to a fourcc. Returns false for anything else.
    bool parseFormatName(const std::string& name, uint32_t& fourcc);
    const char* formatName(uint32_t fourcc);

    // Pixels of one frame. The pointer is only valid until the next call on the source that returned it.
    struct FrameView
    {
        const uint8_t* data = nullptr;
        size_t size = 0;
        // CLOCK_MONOTONIC (std::chrono::steady_clock on Linux), nanoseconds.
        uint64_t timestampNs = 0;
    };

    // Monotonic nanoseconds, the clock every timestamp in this service uses.
    uint64_t monotonicNs();
}
