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

#include "kick_camera/frame_source.h"

#include <string>
#include <vector>

namespace kickcam
{
    // Exposure and white-balance behaviour. Auto exposure that keeps adjusting while a hand moves wrecks gesture
    // detection, so the service starts from a preset and can freeze the sensor on request.
    enum class Preset
    {
        LitRoom,  // sensor picks exposure and white balance, frame rate held fixed
        DimStage, // fixed long exposure (5/6 of the frame period), white balance auto, frame rate held fixed
    };

    bool parsePreset(const std::string& name, Preset& out);
    const char* presetName(Preset p);

    struct ControlState
    {
        Preset preset = Preset::LitRoom;
        bool exposureLocked = false;
        bool wbLocked = false;
    };

    struct ControlReport
    {
        std::vector<std::string> applied;
        std::vector<std::string> unsupported; // controls the source refused. Not an error: drivers differ.
    };

    // Exposure time of the DimStage preset in 100 microsecond units for a frame rate.
    int dimStageExposure(uint32_t fps);

    // Applies the preset to the source and records it in state. Clears both locks, since a new preset
    // is a new starting point.
    ControlReport applyPreset(FrameSource& src, Preset preset, uint32_t fps, ControlState& state);

    // Freezes exposure at the value the sensor is using now. False (with error set) when the source cannot go manual.
    bool lockExposure(FrameSource& src, ControlState& state, std::string& error);
    // Returns exposure to what the current preset specifies.
    bool unlockExposure(FrameSource& src, uint32_t fps, ControlState& state, std::string& error);

    bool lockWhiteBalance(FrameSource& src, ControlState& state, std::string& error);
    bool unlockWhiteBalance(FrameSource& src, ControlState& state, std::string& error);
}
