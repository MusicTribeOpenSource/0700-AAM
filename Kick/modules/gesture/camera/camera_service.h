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

#include <memory>
#include <string>

namespace kick_gesture
{
    // Follows kick-camera over its control socket and switches the camera on and off for the settings page.
    //
    // A background thread asks `status` about once a second and fires contracts::GestureSignals::cameraStatusChanged
    // on the message thread when the answer changes. While the service does not answer, the polling backs off from
    // 1 s up to 30 s, so a box without the camera service does not spin. Enable and disable requests wake the thread
    // at once and reset the backoff.
    class CameraService
    {
    public:
        // socketPath empty means KICK_CAMERA_SOCKET, else /run/kick-camera.sock.
        explicit CameraService( std::string socketPath = {} );
        ~CameraService();

        void start();
        void stop();

        // Asks the camera service to open or close the camera. Returns at once; the result shows up as a status change.
        void requestEnabled( bool on );

        static std::string defaultSocketPath();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    // The one camera service object of the process. The settings page and the adapter share it.
    CameraService& sharedCameraService();
}
