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

#include "kick_camera/controls.h"
#include "kick_camera/types.h"

#include <string>

namespace kickcam
{
    struct Config
    {
        // v4l2 (a capture device, including UVC webcams), file, pattern, pipe.
        std::string source = "v4l2";
        std::string device = "/dev/video0";
        std::string file;
        // pipe source: a shell command that writes raw frames to stdout. Empty reads this process's stdin.
        std::string pipeCommand;

        // 320x240 at 15 fps keeps capture cheap next to the audio engine. Raise them in the config file if a
        // consumer needs more, up to maxWidth x maxHeight, which also fixes the size of the shared-memory ring.
        uint32_t width = 320;
        uint32_t height = 240;
        uint32_t fps = 15;
        uint32_t fourcc = 0; // 0 lets the backend choose (YUYV first, then NV12)
        uint32_t maxWidth = 640;
        uint32_t maxHeight = 480;
        bool loop = true; // file source only

        std::string shmName = "/kick-camera";
        std::string socketPath = "/run/kick-camera.sock";

        Preset preset = Preset::LitRoom;

        // Frames are never written to disk unless this names a directory. At most debugDumpMax frames are written.
        std::string debugDumpDir;
        uint32_t debugDumpMax = 100;

        // Open the camera at start-up. Off by default: the camera stays closed until someone sends "enable".
        bool enableOnStart = false;
    };

    // Sets one setting by its config-file name. False with error set for an unknown key or a bad value.
    //   source, device, file, pipe_command, size (WxH), fps, format, max_size (WxH), loop, shm, socket, preset,
    //   debug_dump, debug_dump_max, enable
    bool applySetting(Config& cfg, const std::string& key, const std::string& value, std::string& error);

    // key=value lines, '#' starts a comment. A missing file is an error.
    bool loadConfigFile(const std::string& path, Config& cfg, std::string& error);

    // Checks the combination of settings. Called before the service starts.
    bool validate(const Config& cfg, std::string& error);

    // Bytes the largest frame can take (the ring's slot capacity).
    uint32_t ringCapacity(const Config& cfg);
}
