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
#include <vector>

namespace juce
{
    class var;
}

namespace ipc
{
    class MTIPCConnection;
    class MTIPCSocketServerImpl;
    class MTIIPCServerCallback;
    class MTIPCSocketServer
    {
    public:
        MTIPCSocketServer( const juce::var& config, std::shared_ptr<ipc::MTIIPCServerCallback> callback = nullptr );
        ~MTIPCSocketServer();

    public:
        // Returns false if the listening socket could not be opened.
        bool start();
        void stop();
        bool areAllValidClientsConnected();
        void updateUnConnectedConnectionIds( std::vector<int> & ids);
    private:
        std::unique_ptr<ipc::MTIPCSocketServerImpl> impl;
    };
}
