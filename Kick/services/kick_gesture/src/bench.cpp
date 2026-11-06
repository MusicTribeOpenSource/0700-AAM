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
#include "kick_gesture/bench.h"

#include "kick_gesture/image.h"
#include "kick_gesture/onnx_hand_model.h"

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <numeric>

namespace kickgest
{
    bool parseSizes(const std::string& text, std::vector<std::pair<int, int>>& out)
    {
        std::vector<std::pair<int, int>> v;
        size_t pos = 0;
        while (pos <= text.size())
        {
            const size_t comma = text.find(',', pos);
            const std::string item = text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            int w = 0, h = 0;
            char tail = 0;
            if (std::sscanf(item.c_str(), "%dx%d%c", &w, &h, &tail) != 2 || w < 16 || h < 16 || w > 4096 || h > 4096) return false;
            v.emplace_back(w, h);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
        if (v.empty()) return false;
        out = v;
        return true;
    }

    namespace
    {
        double peakRssMb()
        {
            rusage ru{};
            if (::getrusage(RUSAGE_SELF, &ru) != 0) return 0.0;
#if defined(__APPLE__)
            return static_cast<double>(ru.ru_maxrss) / (1024.0 * 1024.0); // bytes
#else
            return static_cast<double>(ru.ru_maxrss) / 1024.0; // kilobytes
#endif
        }

        double percentile(std::vector<double> v, double p)
        {
            std::sort(v.begin(), v.end());
            const size_t i = static_cast<size_t>(p * static_cast<double>(v.size() - 1) + 0.5);
            return v[i];
        }
    }

    int runBench(const BenchOptions& o, std::ostream& out, std::string& error)
    {
        RgbImage photo;
        const bool havePhoto = !o.imagePath.empty();
        if (havePhoto && !loadPpmFile(o.imagePath, photo))
        {
            error = "cannot read PPM image " + o.imagePath;
            return 1;
        }
        char line[256];
        out << "| model set | frame | ms/frame mean | p50 | p95 | max | preprocess | palm | landmark | hands found | peak RSS MB |\n";
        out << "|---|---|---|---|---|---|---|---|---|---|---|\n";
        for (const std::string& set : o.modelSets)
        {
            const std::string suffix = set == "int8" ? "_int8.onnx" : ".onnx";
            OnnxModelOptions mo;
            mo.palmModelPath = o.modelDir + "/palm_detection_mediapipe_2023feb" + suffix;
            mo.landmarkModelPath = o.modelDir + "/handpose_estimation_mediapipe_2023feb" + suffix;
            mo.intraOpThreads = o.threads;
            std::string err;
            auto model = OnnxHandModel::create(mo, err);
            if (!model)
            {
                error = "model set " + set + ": " + err;
                return 1;
            }
            for (const auto& size : o.sizes)
            {
                RgbImage frame;
                if (havePhoto)
                    frame = fitInto(photo, size.first, size.second);
                else
                {
                    frame.resize(size.first, size.second);
                    uint32_t s = 7;
                    for (size_t i = 0; i < frame.px.size(); ++i)
                    {
                        s = s * 1664525u + 1013904223u;
                        frame.px[i] = static_cast<uint8_t>(96 + ((s >> 24) & 63));
                    }
                }
                std::vector<Hand> hands;
                std::vector<double> total;
                double pre = 0, palm = 0, lm = 0;
                int found = 0;
                for (int i = 0; i < o.warmup + o.frames; ++i)
                {
                    const auto t0 = std::chrono::steady_clock::now();
                    if (!model->detect(frame, hands, err))
                    {
                        error = "detect failed: " + err;
                        return 1;
                    }
                    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                    if (i < o.warmup) continue;
                    total.push_back(ms);
                    pre += model->lastTimes().preprocessMs;
                    palm += model->lastTimes().palmMs;
                    lm += model->lastTimes().landmarkMs;
                    found += hands.empty() ? 0 : 1;
                }
                const double n = static_cast<double>(total.size());
                const double mean = std::accumulate(total.begin(), total.end(), 0.0) / n;
                std::snprintf(line, sizeof(line), "| %s | %dx%d | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | %.2f | %d/%d | %.0f |\n", set.c_str(), size.first, size.second, mean, percentile(total, 0.5),
                              percentile(total, 0.95), *std::max_element(total.begin(), total.end()), pre / n, palm / n, lm / n, found, static_cast<int>(total.size()), peakRssMb());
                out << line;
            }
        }
        return 0;
    }
}
