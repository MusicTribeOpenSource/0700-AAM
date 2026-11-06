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

#include <functional>
#include <istream>
#include <string>

namespace kickgest
{
    struct ReplayResult
    {
        bool ok = true;
        std::string error; // set when the stream is malformed
        size_t frames = 0;
        size_t events = 0;
        uint64_t firstMs = 0; // timestamps of the first and last frame
        uint64_t lastMs = 0;
    };

    // Feeds a landmark record to a fresh pass of `engine` and ends the session at the last frame, so an open pinch is
    // closed. `onEvent` gets each event in order. With `realtime` the call sleeps between frames by the recorded gaps
    // (capped at one second), so the service can be watched at the speed it was recorded.
    ReplayResult replayStream(std::istream& in, GestureEngine& engine, const std::function<void(const GestureEvent&)>& onEvent, bool realtime = false);
}
