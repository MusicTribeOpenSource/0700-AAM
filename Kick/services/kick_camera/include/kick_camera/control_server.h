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

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace kickcam
{
    // A Unix stream socket that takes one text command per line and answers each with one line.
    // No network socket is involved. The socket file is mode 0600, so only the owner (root on the unit) can drive it.
    //
    // start() sets SIGPIPE to ignored for the process, so a client that hangs up mid-reply cannot kill the service.
    class ControlServer
    {
    public:
        using Handler = std::function<std::string(const std::string& line)>;

        ControlServer() = default;
        ~ControlServer();
        ControlServer(const ControlServer&) = delete;
        ControlServer& operator=(const ControlServer&) = delete;

        bool start(const std::string& path, Handler handler, std::string& error);
        void stop();

        // Connects to a control socket, sends one command and returns the reply line (without its newline).
        static bool request(const std::string& path, const std::string& command, std::string& reply, std::string& error, int timeoutMs = 3000);

    private:
        void run();
        std::string path_;
        Handler handler_;
        int listenFd_ = -1;
        int wakeRead_ = -1;
        int wakeWrite_ = -1;
        std::atomic<bool> stopping_{false};
        std::thread thread_;
    };
}
