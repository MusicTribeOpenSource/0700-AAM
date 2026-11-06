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

#include "kick_camera/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kickgest
{
    struct RgbImage
    {
        int w = 0;
        int h = 0;
        std::vector<uint8_t> px; // R, G, B per pixel, row after row, 3 * w * h bytes

        void resize(int width, int height)
        {
            w = width;
            h = height;
            px.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u, 0);
        }
    };

    // Camera frame (YUYV or NV12, BT.601 limited range as UVC cameras and the Kick camera service deliver it) to RGB.
    // False when the format is not supported or `size` is below what the format needs.
    bool convertToRgb(const uint8_t* data, size_t size, const kickcam::FrameFormat& format, RgbImage& out);

    // Maps a destination pixel to a source position: x = a*u + b*v + c, y = d*u + e*v + f. Coordinates are pixel
    // centres at integer positions, so the centre of the top left pixel is (0, 0).
    struct Affine
    {
        float a = 1.f, b = 0.f, c = 0.f;
        float d = 0.f, e = 1.f, f = 0.f;
    };

    // Binary PPM (P6, maxval 255), the format tools/prepare_test_images.py writes. False when the file is missing or malformed.
    bool loadPpmFile(const std::string& path, RgbImage& out);

    // `src` scaled to fit inside a w x h canvas, centred, black bars where it does not reach.
    RgbImage fitInto(const RgbImage& src, int w, int h);

    // Samples `src` bilinearly through `map` into dstW x dstH x 3 floats scaled to 0..1 (R, G, B, row major).
    // Anything outside the source reads as black.
    void warpToFloat(const RgbImage& src, const Affine& map, int dstW, int dstH, float* out);
}
