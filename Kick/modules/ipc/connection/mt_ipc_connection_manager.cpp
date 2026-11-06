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

#include <functional>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "mt_ipc_connection.h"
#include "mt_ipc_connection_manager.h"
#include "mt_ipc_server_connection.h"
#include "mt_ipc_socket_server.h"
#include "../utils/mt_ipc_constants.h"
#include "../utils/mt_ipc_keys.h"
#include "../utils/mt_ipc_message_queue.h"
#include "../utils/mt_ipc_json.h"
#include "../utils/mt_ipc_message_constructor.h"
#include "../utils/mt_ipc_timer.h"

namespace ipc
{
    class MTIPCConnectionData
    {
    public:
        MTIPCConnectionData( bool connectsToServer, std::shared_ptr<ipc::MTIPCConnection> conn, std::shared_ptr<juce::var> config )
            : connectsToServer( connectsToServer ), connection( conn ), config( config )
        {}

        ~MTIPCConnectionData()
        {
            connection = nullptr;
            config     = nullptr;
        }

    public:
        //This indicates that this connection is a client to a server in another process.
        bool                                  connectsToServer{ false };
        std::shared_ptr<ipc::MTIPCConnection> connection{ nullptr };
        std::shared_ptr<juce::var>            config{ nullptr };
    };

    // connection manager  private data
    class MTIPCConnectionManagerPData
    {
    public:
        MTIPCConnectionManagerPData()
        {
        }

        ~MTIPCConnectionManagerPData()
        {
            connections.clear();
            server                = nullptr;
            serverCallbackPtr     = nullptr;
            connectionCallbackPtr = nullptr;
        }

    public:
        juce::HashMap<int, std::shared_ptr<MTIPCConnectionData>> connections;
        std::unique_ptr<ipc::MTIPCSocketServer>                  server{ nullptr };
        std::shared_ptr<ipc::MTIIPCConnectionCallback>           connectionCallbackPtr{ nullptr };
        std::shared_ptr<ipc::MTIIPCServerCallback>               serverCallbackPtr{ nullptr };
        juce::String                                             uuid;
        bool                                                     delayedClientStart{ false };
        std::unique_ptr<ipc::MTIPCTimer>                         delayedStartTimer;
        juce::String                                             defaultConnectionName{ ipc::IPC_CLIENT_NAME_NOT_DEFINED };
    };

    //server callback
    class MTIPCServerCallback : public ipc::MTIIPCServerCallback
    {
    public:
        MTIPCServerCallback( std::function<void( const int, std::shared_ptr<ipc::MTIPCConnection> )> onNewConnection, std::function<void( const int )> onConnectionRemoved )
            : onNewConnection( onNewConnection ), onConnectionRemoved( onConnectionRemoved ) {}

        inline void newConnectionRegistered( const int connectionId, std::shared_ptr<ipc::MTIPCConnection> newConn )
        {
            onNewConnection( connectionId, newConn );
        }

        inline void connectionRemoved( const int connectionId )
        {
            onConnectionRemoved( connectionId );
        }

    private:
        std::function<void( const int, std::shared_ptr<ipc::MTIPCConnection> )> onNewConnection;
        std::function<void( const int )>                                       onConnectionRemoved;
    };

    // connection manager implementation.

    MTIPCConnectionManager::MTIPCConnectionManager( std::shared_ptr<juce::var> config, std::shared_ptr<MTIIPCConnectionCallback> callback )
        : data( new ipc::MTIPCConnectionManagerPData() )
    {
        data->connectionCallbackPtr = callback;
        data->serverCallbackPtr.reset(
            new MTIPCServerCallback(
                std::bind( &MTIPCConnectionManager::addNewConnection, this, std::placeholders::_1, std::placeholders::_2 ),
                std::bind( &MTIPCConnectionManager::connectionRemoved, this, std::placeholders::_1 ) ) );
        initialize( config );
    }

    MTIPCConnectionManager::~MTIPCConnectionManager()
    {
        
    }

    void MTIPCConnectionManager::clearData()
    {
        if( data )
        {
            data->connections.clear();
        }
    }

    bool MTIPCConnectionManager::areAllClientsConnected()
    {
        bool connected = true;
        for( auto conn : data->connections )
        {
            if( conn->connectsToServer )
            {
                connected = conn->connection ? conn->connection->isConnected() : false;
                if( connected )
                    continue;
                break;
            }
        }
        return connected;
    }

    void MTIPCConnectionManager::updateUnConnectedConnectionIds( std::vector<int> & ids)
    {
        if( data->server )
            data->server->updateUnConnectedConnectionIds( ids );
        for( auto itr = data->connections.begin(); itr != data->connections.end(); itr.next() )
        {
            auto conn = itr.getValue();
            if( conn->connectsToServer )
            {
                if(conn->connection)                
                    continue;
                ids.push_back( itr.getKey() );
            }
        }
    }

    void MTIPCConnectionManager::onDelayedStartClientTimeout()
    {
        startClientConnections();
    }

    void MTIPCConnectionManager::initialize( std::shared_ptr<juce::var> config )
    {
        data->uuid     = config->getProperty( ipc::IPC_KEY_UUID, "" );
        bool hasServer = config->hasProperty( ipc::IPC_KEY_SERVER );
        if( hasServer )
        {
            data->server.reset(
                new ipc::MTIPCSocketServer( config->getProperty( ipc::IPC_KEY_SERVER, juce::var() ), data->serverCallbackPtr ) );
        }
        if( config->hasProperty( ipc::IPC_KEY_CLIENTS ) )
        {
            juce::var               clientConfig = config->getProperty( ipc::IPC_KEY_CLIENTS, juce::var() );
            juce::Array<juce::var>* clients      = clientConfig.getArray();

            data->delayedClientStart = config->getProperty( ipc::IPC_KEY_DELAY_STARTING_CLIENT, false );
            if( data->delayedClientStart )
            {
                int delay = config->getProperty( ipc::IPC_KEY_CLIENT_START_DELAY, ipc::IPC_CLIENT_START_DEFAULT_DELAY );
                if( delay > 0 )
                    data->delayedStartTimer.reset( new ipc::MTIPCTimer(
                        delay, std::bind( &MTIPCConnectionManager::onDelayedStartClientTimeout, this ) ) );
                else
                    data->delayedClientStart = false;
            }

            for( const juce::var& client : *clients )
            {
                if( client.hasProperty( ipc::IPC_KEY_CONNECTION_ID ) )
                    data->connections.set( client.getProperty( ipc::IPC_KEY_CONNECTION_ID, -1 ),
                                       std::shared_ptr<ipc::MTIPCConnectionData>( new ipc::MTIPCConnectionData( true, nullptr, std::make_shared<juce::var>( client ) ) ) );
            }
        }
    }

    bool MTIPCConnectionManager::startServer()
    {
        if( !data->server )
            return true;

        const bool listening = data->server->start();
        if( !listening )
            DBG( "IPC server could not open its listening socket" );
        return listening;
    }

    void MTIPCConnectionManager::startClients()
    {
        if( data->delayedClientStart )
            data->delayedStartTimer->start();
        else
            startClientConnections();
    }

    void MTIPCConnectionManager::startClientConnections()
    {
        for( auto itr = data->connections.begin(); itr != data->connections.end(); itr.next() )
        {
            auto conn = itr.getValue();
            auto id   = itr.getKey();
            if( conn->connectsToServer )
            {
                if( !conn->connection )
                    conn->connection.reset( new ipc::MTIPCServerConnection( data->uuid, id, conn->config, data->connectionCallbackPtr ) );
                conn->connection->connect();
            }
        }
    }

    bool MTIPCConnectionManager::canAttemptReconnection( int connectionId )
    {
        if( !data->connections.contains( connectionId ) )
            return false;

        auto ptr = data->connections[connectionId];
        if( ptr)
        {
            return ptr->connectsToServer;
        }
            
        return false;
    }

    void MTIPCConnectionManager::stop()
    {
        for( auto conn : data->connections )
        {
            if( conn->connection && conn->connectsToServer )
            {
                conn->connection->disconnect();
                data->connections[conn->connection->connectionIdentifier()]->connection = nullptr;
            }
        }

        if( data->server )
            data->server->stop();
    }
    std::shared_ptr<ipc::MTIPCConnection> MTIPCConnectionManager::getConnection( const int connectionId )
    {
        if( data->connections.contains( connectionId ) )
            return data->connections[connectionId]->connection;
        return nullptr;
    }

    bool MTIPCConnectionManager::connect( const int connectionId )
    {
        std::shared_ptr<ipc::MTIPCConnection> conn = getConnection( connectionId );
        return conn ? conn->connect() : false;
    }

    void MTIPCConnectionManager::addNewConnection( const int connectionId, std::shared_ptr<ipc::MTIPCConnection> newConn )
    {
        newConn->setCallback( data->connectionCallbackPtr );
        data->connections.set( connectionId, std::shared_ptr<ipc::MTIPCConnectionData>( new MTIPCConnectionData( false, newConn, nullptr ) ) );
    }

    void MTIPCConnectionManager::connectionRemoved( int connectionId )
    {
        if( data->connections.contains( connectionId ) )
            data->connections.remove( connectionId );
    }

    void MTIPCConnectionManager::disconnect( const int connectionId )
    {
        std::shared_ptr<ipc::MTIPCConnection> conn = getConnection( connectionId );
        if( conn )
            conn->disconnect();
        data->connections.remove( connectionId );
    }

    bool MTIPCConnectionManager::publish( const int connectionId, std::shared_ptr<ipc::MTIPCJson> message )
    {
        std::shared_ptr<ipc::MTIPCConnection> conn = getConnection( connectionId );

        return conn ? conn->publish( message ) : false;
    }

    void MTIPCConnectionManager::broadcast( std::shared_ptr<ipc::MTIPCJson> message )
    {
        for( auto conn : data->connections )
        {
            if( conn->connection )
            {
                conn->connection->publish( message );
            }
        }
    }

    void MTIPCConnectionManager::multicast( const std::vector<int>& connectionIds, std::shared_ptr<ipc::MTIPCJson> message )
    {
        for( int id : connectionIds )
        {
            publish( id, message );
        }
    }

    bool MTIPCConnectionManager::isConnected( const int connectionId )
    {
        std::shared_ptr<ipc::MTIPCConnection> conn = getConnection( connectionId );
        return conn ? conn->isConnected() : false;
    }

    const juce::String& MTIPCConnectionManager::connectionName( const int connectionId )
    {
        std::shared_ptr<ipc::MTIPCConnection> conn = getConnection( connectionId );
        return conn ? conn->connectionName() : data->defaultConnectionName;
    }
}
