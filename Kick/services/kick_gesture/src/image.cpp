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
#include "kick_gesture/image.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace kickgest
{
    namespace
    {
        inline uint8_t clamp8(int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }

        inline void yuvToRgb(int y, int u, int v, uint8_t* rgb)
        {
            const int c = y - 16;
            const int d = u - 128;
            const int e = v - 128;
            // 1.164, 1.596, 0.392, 0.813, 2.018 in 8 bit fixed point
            const int base = 298 * c + 128;
            rgb[0] = clamp8((base + 409 * e) >> 8);
            rgb[1] = clamp8((base - 100 * d - 208 * e) >> 8);
            rgb[2] = clamp8((base + 516 * d) >> 8);
        }
    }

    bool convertToRgb(const uint8_t* data, size_t size, const kickcam::FrameFormat& f, RgbImage& out)
    {
        const size_t need = kickcam::frameBytes(f);
        if (need == 0 || size < need || data == nullptr) return false;
        out.resize(static_cast<int>(f.width), static_cast<int>(f.height));
        uint8_t* dst = out.px.data();
        if (f.fourcc == kickcam::kFourccYuyv)
        {
            for (uint32_t y = 0; y < f.height; ++y)
            {
                const uint8_t* row = data + static_cast<size_t>(y) * f.stride;
                for (uint32_t x = 0; x < f.width; x += 2)
                {
                    const int y0 = row[2 * x];
                    const int u = row[2 * x + 1];
                    const int y1 = row[2 * x + 2];
                    const int v = row[2 * x + 3];
                    yuvToRgb(y0, u, v, dst);
                    yuvToRgb(y1, u, v, dst + 3);
                    dst += 6;
                }
            }
            return true;
        }
        if (f.fourcc == kickcam::kFourccNv12)
        {
            const uint8_t* uvPlane = data + static_cast<size_t>(f.height) * f.stride;
            for (uint32_t y = 0; y < f.height; ++y)
            {
                const uint8_t* row = data + static_cast<size_t>(y) * f.stride;
                const uint8_t* uv = uvPlane + static_cast<size_t>(y / 2) * f.stride;
                for (uint32_t x = 0; x < f.width; ++x)
                {
                    yuvToRgb(row[x], uv[(x & ~1u)], uv[(x & ~1u) + 1], dst);
                    dst += 3;
                }
            }
            return true;
        }
        return false;
    }

    void warpToFloat(const RgbImage& src, const Affine& m, int dstW, int dstH, float* out)
    {
        const float inv255 = 1.f / 255.f;
        for (int v = 0; v < dstH; ++v)
        {
            for (int u = 0; u < dstW; ++u)
            {
                const float fu = static_cast<float>(u);
                const float fv = static_cast<float>(v);
                const float x = m.a * fu + m.b * fv + m.c;
                const float y = m.d * fu + m.e * fv + m.f;
                float r = 0.f, g = 0.f, b = 0.f;
                if (std::isfinite(x) && std::isfinite(y) && x > -1.f && y > -1.f && x < static_cast<float>(src.w) && y < static_cast<float>(src.h))
                {
                    const int x0 = static_cast<int>(std::floor(x));
                    const int y0 = static_cast<int>(std::floor(y));
                    const float tx = x - static_cast<float>(x0);
                    const float ty = y - static_cast<float>(y0);
                    for (int j = 0; j < 2; ++j)
                    {
                        for (int i = 0; i < 2; ++i)
                        {
                            const int xi = x0 + i;
                            const int yi = y0 + j;
                            if (xi < 0 || yi < 0 || xi >= src.w || yi >= src.h) continue;
                            const float w = (i ? tx : 1.f - tx) * (j ? ty : 1.f - ty);
                            const uint8_t* p = &src.px[(static_cast<size_t>(yi) * static_cast<size_t>(src.w) + static_cast<size_t>(xi)) * 3u];
                            r += w * static_cast<float>(p[0]);
                            g += w * static_cast<float>(p[1]);
                            b += w * static_cast<float>(p[2]);
                        }
                    }
                }
                float* o = out + (static_cast<size_t>(v) * static_cast<size_t>(dstW) + static_cast<size_t>(u)) * 3u;
                o[0] = r * inv255;
                o[1] = g * inv255;
                o[2] = b * inv255;
            }
        }
    }
}

namespace kickgest
{
    bool loadPpmFile(const std::string& path, RgbImage& out)
    {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (f == nullptr) return false;
        int w = 0, h = 0, maxv = 0;
        const int n = std::fscanf(f, "P6 %d %d %d", &w, &h, &maxv);
        bool ok = n == 3 && w > 0 && h > 0 && w <= 16384 && h <= 16384 && maxv == 255 && std::fgetc(f) != EOF;
        if (ok)
        {
            out.resize(w, h);
            ok = std::fread(out.px.data(), 1, out.px.size(), f) == out.px.size();
        }
        std::fclose(f);
        return ok;
    }

    RgbImage fitInto(const RgbImage& src, int w, int h)
    {
        RgbImage out;
        out.resize(w, h);
        if (src.w <= 0 || src.h <= 0) return out;
        const float scale = std::min(static_cast<float>(w) / static_cast<float>(src.w), static_cast<float>(h) / static_cast<float>(src.h));
        const float padX = (static_cast<float>(w) - static_cast<float>(src.w) * scale) / 2.f;
        const float padY = (static_cast<float>(h) - static_cast<float>(src.h) * scale) / 2.f;
        Affine m;
        m.a = 1.f / scale;
        m.e = 1.f / scale;
        m.c = (0.5f - padX) / scale - 0.5f;
        m.f = (0.5f - padY) / scale - 0.5f;
        std::vector<float> tmp(static_cast<size_t>(w) * static_cast<size_t>(h) * 3u);
        warpToFloat(src, m, w, h, tmp.data());
        for (size_t i = 0; i < tmp.size(); ++i) out.px[i] = clamp8(static_cast<int>(tmp[i] * 255.f + 0.5f));
        return out;
    }
}
