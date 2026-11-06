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

#include "kick_gesture/guard.h"

#include <cstdint>
#include <string>

namespace kickgest
{
    // Service settings: one key = value per line in /etc/kick-gesture.conf ('#' starts a comment), or --key value on
    // the command line (dashes and underscores are the same). Gesture thresholds are not here, they live in gesture.json.
    struct ServiceConfig
    {
        std::string shm = "/kick-camera";    // shared memory object the camera service publishes frames in
        double rateHz = 12.0;                // inferences per second at full rate, 1 to 30
        std::string modelDir;                // directory with the .onnx files, empty for the built-in default
        std::string modelSet = "int8";       // int8 or fp32
        int threads = 1;                     // ONNX Runtime intra-op threads
        std::string gestureConfig;           // path of gesture.json, empty for the defaults
        bool eventsStdout = false;           // one JSON line per event on standard output
        std::string eventsSocket;            // Unix stream socket the app listens on
        std::string eventsIpc;               // host:port of the Kick app's IPC server on loopback, for example 127.0.0.1:50501
        std::string eventsFile;              // append events to this file
        std::string record;                  // write the landmark stream here (geometry only, no pixels)
        uint32_t statusIntervalMs = 5000;
        GuardLimits guard;
    };

    // Applies one setting. False with a message for an unknown key or a bad value.
    bool applyServiceSetting(const std::string& key, const std::string& value, ServiceConfig& cfg, std::string& error);

    // Applies every line of a config file on top of `cfg`.
    bool loadServiceConfigFile(const std::string& path, ServiceConfig& cfg, std::string& error);

    // True for the settings that are switches with no value on the command line.
    bool isServiceFlag(const std::string& key);
}
