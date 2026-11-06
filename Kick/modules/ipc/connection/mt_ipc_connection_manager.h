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
#include <deque>
#include <vector>

#include "mti_ipc_connection_callback.h"
#include "mti_ipc_server_callback.h"



namespace juce
{
	class var;
	class InterprocessConnection;
    class String;
}
namespace ipc
{
	class MTIPCConnectionManagerPData;
    class MTIPCConnection;

	class MTIPCConnectionManager
	{
	public:
		MTIPCConnectionManager(std::shared_ptr<juce::var> config, std::shared_ptr<ipc::MTIIPCConnectionCallback> callback);
		~MTIPCConnectionManager();

	public:
		void disconnect(const int connectionId);
		bool connect(const int connectionId);
		bool publish(const int connectionId, std::shared_ptr<ipc::MTIPCJson> message);
		void broadcast(std::shared_ptr<ipc::MTIPCJson> message);
		void multicast(const std::vector<int>& connectionIds, std::shared_ptr<ipc::MTIPCJson> message);
		bool isConnected(const int connectionId);
		bool startServer();
		void startClients();
		void stop();
		bool canAttemptReconnection(int connectionId);
        const juce::String& connectionName( const int connectionId );
        bool                areAllClientsConnected();
        void updateUnConnectedConnectionIds( std::vector<int>& ids );
	private:
		virtual void initialize(std::shared_ptr<juce::var> config);
		
		void addNewConnection(const int connectionId, std::shared_ptr<ipc::MTIPCConnection> newConn);
		void connectionRemoved(int connectionId);
		void clearData();
		void onDelayedStartClientTimeout();
		void startClientConnections();
        std::shared_ptr<ipc::MTIPCConnection> getConnection( const int connectionId );


	private:
		std::shared_ptr<ipc::MTIPCConnectionManagerPData>      data{ nullptr };
	};
}
