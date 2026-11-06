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
// kick-camera-peek: reads the newest frame from the shared-memory ring in another process and writes its
// luma plane as a binary PGM. A way to see what a consumer sees, and a check that the ring works across processes.
//
//   kick-camera-peek [--shm /kick-camera] [--out frame.pgm] [--wait-ms 3000]
//
// Prints one line of metadata to stdout. Exit 0 on success, 1 when no frame arrived in time.
#include "kick_camera/shm_region.h"
#include "kick_camera/shm_ring.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

int main(int argc, char** argv)
{
    std::string shm = "/kick-camera", out;
    int waitMs = 3000;
    for (int i = 1; i + 1 < argc; i += 2)
    {
        if (std::strcmp(argv[i], "--shm") == 0) shm = argv[i + 1];
        else if (std::strcmp(argv[i], "--out") == 0) out = argv[i + 1];
        else if (std::strcmp(argv[i], "--wait-ms") == 0) waitMs = std::atoi(argv[i + 1]);
        else
        {
            std::fprintf(stderr, "usage: kick-camera-peek [--shm /name] [--out file.pgm] [--wait-ms N]\n");
            return 2;
        }
    }

    kickcam::ShmRegion region;
    std::string error;
    if (!region.openReadOnly(shm, error))
    {
        std::fprintf(stderr, "kick-camera-peek: %s\n", error.c_str());
        return 1;
    }
    kickcam::RingReader reader;
    if (!reader.attach(region.data(), region.size()))
    {
        std::fprintf(stderr, "kick-camera-peek: %s is not a kick-camera ring\n", shm.c_str());
        return 1;
    }

    std::vector<uint8_t> pixels;
    kickcam::FrameMeta meta;
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
    while (reader.readLatest(0, pixels, meta) != kickcam::RingReader::Result::Ok)
    {
        if (std::chrono::steady_clock::now() > end)
        {
            std::printf("no frame (capturing=%d)\n", reader.capturing() ? 1 : 0);
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const double ageMs = static_cast<double>(kickcam::monotonicNs() - meta.timestampNs) / 1e6;
    std::printf("frame %llu %ux%u %s stride %u age %.1f ms\n", static_cast<unsigned long long>(meta.frameNo), meta.format.width,
                meta.format.height, kickcam::formatName(meta.format.fourcc), meta.format.stride, ageMs);

    if (!out.empty())
    {
        std::FILE* f = std::fopen(out.c_str(), "wb");
        if (f == nullptr)
        {
            std::fprintf(stderr, "kick-camera-peek: cannot write %s\n", out.c_str());
            return 1;
        }
        std::fprintf(f, "P5\n%u %u\n255\n", meta.format.width, meta.format.height);
        const unsigned step = meta.format.fourcc == kickcam::kFourccYuyv ? 2 : 1; // luma is every second byte in YUYV
        for (uint32_t y = 0; y < meta.format.height; ++y)
            for (uint32_t x = 0; x < meta.format.width; ++x)
                std::fputc(pixels[static_cast<size_t>(y) * meta.format.stride + static_cast<size_t>(x) * step], f);
        std::fclose(f);
    }
    return 0;
}
