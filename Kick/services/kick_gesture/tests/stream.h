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

#include "kick_gesture/engine.h"
#include "kick_gesture/synth.h"

#include <cstdint>
#include <vector>

namespace kicktest
{
    using namespace kickgest;

    // Feeds synthetic poses to an engine at a fixed frame rate and collects the events.
    struct Runner
    {
        explicit Runner(const GestureConfig& c = GestureConfig(), uint32_t frameMs = 67) : cfg(c), engine(c), step(frameMs) {}

        GestureConfig cfg;
        GestureEngine engine;
        std::vector<GestureEvent> events;
        uint64_t t = 1000;
        uint32_t step;
        float aspect = 4.f / 3.f;

        void frame(const SynthPose& p)
        {
            engine.process(synthFrame(t, p, aspect, cfg.flipX), events);
            t += step;
        }
        void none()
        {
            engine.process(synthEmptyFrame(t, aspect), events);
            t += step;
        }
        void hold(const SynthPose& p, uint32_t ms)
        {
            for (uint32_t e = 0; e < ms; e += step) frame(p);
        }
        void gap(uint32_t ms)
        {
            for (uint32_t e = 0; e < ms; e += step) none();
        }
        // The palm centre moves in a straight line from (x0, y0) to (x1, y1) over `ms`.
        void sweep(SynthPose p, float x0, float y0, float x1, float y1, uint32_t ms)
        {
            const uint32_t n = ms / step > 0 ? ms / step : 1;
            for (uint32_t i = 0; i <= n; ++i)
            {
                const float u = static_cast<float>(i) / static_cast<float>(n);
                p.x = x0 + (x1 - x0) * u;
                p.y = y0 + (y1 - y0) * u;
                frame(p);
            }
        }
        int count(GestureType type) const
        {
            int n = 0;
            for (const GestureEvent& e : events) n += e.type == type ? 1 : 0;
            return n;
        }
    };
}
