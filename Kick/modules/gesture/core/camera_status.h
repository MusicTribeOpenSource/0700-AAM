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

#include "contracts/gesture/gesture_event.h"

#include <string>

namespace kick_gesture
{
    // Reads the one-line JSON reply of kick-camera's `status` command (docs/camera.md). Only the `state`, `capturing`
    // and `error` fields are used.
    //   state "streaming"                     -> Streaming (capturing follows the `capturing` field)
    //   state "off"                           -> Off
    //   state "error" or "ended"              -> Error, with the `error` text as detail
    //   anything else, or text that is not a status object -> ServiceMissing with a detail saying the reply was not understood
    contracts::CameraStatus parseCameraStatus( const std::string& reply );

    // The status shown when the control socket cannot be reached.
    contracts::CameraStatus cameraServiceMissing( const std::string& why );

    // The text for the settings page, a few words.
    std::string describeCameraStatus( const contracts::CameraStatus& s );

    // Talks to kick-camera's control socket (a Unix stream socket, one command per line, one reply line).
    // Returns false with `error` set when the socket cannot be reached or does not answer within timeoutMs.
    // Always false on Windows, where there is no kick-camera.
    bool cameraRequest( const std::string& socketPath, const std::string& command, std::string& reply, std::string& error, int timeoutMs = 500 );
}
