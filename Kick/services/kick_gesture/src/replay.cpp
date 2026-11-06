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
#include "kick_gesture/replay.h"

#include "kick_gesture/record.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace kickgest
{
    ReplayResult replayStream(std::istream& in, GestureEngine& engine, const std::function<void(const GestureEvent&)>& onEvent, bool realtime)
    {
        ReplayResult r;
        LandmarkReader reader(in);
        LandmarkFrame f;
        std::vector<GestureEvent> events;
        uint64_t prev = 0;
        while (reader.next(f))
        {
            if (r.frames == 0)
                r.firstMs = f.tMs;
            else if (realtime && f.tMs > prev)
                std::this_thread::sleep_for(std::chrono::milliseconds(std::min<uint64_t>(f.tMs - prev, 1000)));
            prev = f.tMs;
            r.lastMs = f.tMs;
            ++r.frames;
            events.clear();
            engine.process(f, events);
            for (const GestureEvent& e : events) onEvent(e);
            r.events += events.size();
        }
        if (!reader.error().empty())
        {
            r.ok = false;
            r.error = reader.error();
            return r;
        }
        events.clear();
        engine.endSession(r.lastMs, events);
        for (const GestureEvent& e : events) onEvent(e);
        r.events += events.size();
        return r;
    }
}
