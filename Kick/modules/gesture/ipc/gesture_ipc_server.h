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
    // The server kick-gesture connects to. It listens on loopback only, takes the service's REGISTER handshake,
    // decodes message types 500 and 501, and fires contracts::GestureSignals on the message thread.
    //
    // Nothing authenticates the sender: any local process can connect and inject gestures. That is why the port is
    // bound to 127.0.0.1 and why gestures stay off until the user turns them on.
    class GestureIpcServer
    {
    public:
        // port <= 0 means contracts::GESTURE_IPC_DEFAULT_PORT, or KICK_GESTURE_PORT when set.
        explicit GestureIpcServer( int port = 0 );
        ~GestureIpcServer();

        // Starts listening. ipc::MTIPCSocketServer::start() throws away the result of the bind, so a port that is
        // already taken goes unreported here; the app then just never sees a gesture.
        void start();
        void stop();
        int  port() const;

        // The port the process should use: KICK_GESTURE_PORT, else the default.
        static int defaultPort();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
