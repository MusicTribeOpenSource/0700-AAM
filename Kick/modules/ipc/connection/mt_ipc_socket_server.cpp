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

#include "juce_core/juce_core.h"
#include "juce_events/juce_events.h"
#include "mt_ipc_socket_server.h"
#include "mt_ipc_connection.h"
#include "mti_ipc_server_callback.h"
#include "mti_ipc_connection_callback.h"
#include "../utils/mt_ipc_constants.h"
#include "../utils/mt_ipc_keys.h"
#include "../utils/mt_ipc_json.h"
#include "../utils/mt_ipc_message_constructor.h"
#include "../utils/mt_ipc_message_types.h"
#include "../utils/mt_ipc_enums.h"

namespace ipc
{
    class MTIPCClientConnection : public ipc::MTIPCConnection
    {
    public:
        MTIPCClientConnection( int connectionId, std::shared_ptr<ipc::MTIIPCConnectionCallback> pCallback )
            : ipc::MTIPCConnection( connectionId, pCallback, false )

        {
        }

        virtual void disconnect() override
        {
            if( disconnectNotifier )
            {
                juce::MessageManager::callAsync( [&]() { disconnectNotifier( connectionIdentifier() ); } );
            }
            ipc::MTIPCConnection::disconnect();
        }

        void setDisconnectionNotifier( std::function<void( int connectionId )> notifier )
        {
            disconnectNotifier = notifier;
        }

    protected:
        virtual void onDisconnected()
        {
            if( disconnectNotifier )
            {
                juce::MessageManager::callAsync( [&]() { disconnectNotifier( connectionIdentifier() ); } );
            }

            ipc::MTIPCConnection::onDisconnected();
        }

    private:
        std::function<void( int connectionId )> disconnectNotifier{ nullptr };
    };

    // connection callback
    class MTIPCSeverConnectionCallback : public MTIIPCConnectionCallback
    {
    public:
        MTIPCSeverConnectionCallback( std::function<void( int )>                             disconnectedCB,
                                      std::function<void( int, std::shared_ptr<MTIPCJson> )> messageCB )
            : disconnectedCB( disconnectedCB ), messageCB( messageCB )
        {}
        void onConnected( [[maybe_unused]] int connectionId, const juce::String& connectionName ) override
        {

        }

        void onDisconnected( const int connectionId, const juce::String& connectionName ) override
        {
            disconnectedCB( connectionId );
        }

        void onMessage( const int connectionId, std::shared_ptr<MTIPCJson> message) override
        {
            messageCB( connectionId, message );
        }

    private:
        std::function<void( int )>                             connectedCB;
        std::function<void( int )>                             disconnectedCB;
        std::function<void( int, std::shared_ptr<MTIPCJson> )> messageCB;
    };

    // server impl
    class MTIPCSocketServerImpl : public juce::InterprocessConnectionServer
    {
    public:
        MTIPCSocketServerImpl( const juce::var& config, std::shared_ptr<ipc::MTIIPCServerCallback> serverCallback )
            : serverCallback( serverCallback ), connectionCallBackPtr( new MTIPCSeverConnectionCallback( std::bind( &MTIPCSocketServerImpl::onClientDisconnected, this, std::placeholders::_1 ), std::bind( &MTIPCSocketServerImpl::processMessage, this, std::placeholders::_1, std::placeholders::_2 ) ) )
        {
            serverPort = config.getProperty( ipc::IPC_KEY_PORT, -1 );
            if( serverPort <= 0 ) return;

            bindAddress = config.getProperty( IPC_KEY_BIND_ADDRESS, "" );

            enableValidation = config.getProperty( ipc::IPC_KEY_CLIENT_ENABLE_VALIDATION, true );

            juce::var connections = config.getProperty( ipc::IPC_KEY_CONNECTIONS, juce::Array<juce::var>() );

            for( const juce::var& connection : *connections.getArray() )
            {
                int connId = connection.getProperty( ipc::IPC_KEY_CONNECTION_ID, -1 );
                validationData.set( connId, connection.getProperty( ipc::IPC_KEY_UUID, "" ) );
            }
        }

        ~MTIPCSocketServerImpl()
        {
            
        }

        void clearData()
        {
            validationData.clear();
            registered.clear();
            unregistered.clear();
        }

        // if the all the clients are connected then start will not start the server
        // notify server about some clients being disconnected and then start.
        // Returns false when the port could not be bound (already in use, bad address, no port configured).
        bool start()
        {
            if( serverPort <= 0 )
                return false;

            return beginWaitingForSocket( serverPort, bindAddress );
        }

        bool areAllValidClientsConnected()
        {
            for( auto itr = validationData.begin(); itr != validationData.end(); itr.next() )
            {
                int key = itr.getKey();

                if( registered.contains( key ) && registered[key] )
                    continue;
                return false;
            }
            return true;
        }


        void updateUnConnectedConnectionIds( std::vector<int>& ids )
        {
            for( auto itr = validationData.begin(); itr != validationData.end(); itr.next() )
            {
                int key = itr.getKey();
                if( registered.contains( key ) )
                    continue;
                ids.push_back( key );
            }            
        }


    protected:
        juce::InterprocessConnection* createConnectionObject()
        {
            tempConnectionId++;
            std::shared_ptr<ipc::MTIPCClientConnection> conn( new ipc::MTIPCClientConnection( tempConnectionId, connectionCallBackPtr ) );
            if( !enableValidation )
                registered.set( tempConnectionId, conn );
            unregistered.set( tempConnectionId, conn );
            return conn->getConnectionObject();
        }

    private:
        void processMessage( const int connectionId, std::shared_ptr<MTIPCJson> message )
        {
            try
            {
                int type = message->getProperty( ipc::IPC_KEY_MESSAGE_TYPE );
                if( type != ipc::MTIPCMessageType::IPC_MESSAGE_TYPE_REGISTER ) return;

                int          clientConnectionId = message->getProperty( ipc::IPC_KEY_CONNECTION_ID );
                juce::String uuid               = message->getProperty( ipc::IPC_KEY_UUID );
                juce::String connectionName     = message->getProperty( ipc::IPC_KEY_CONNECTION_NAME );

                std::shared_ptr<ipc::MTIPCClientConnection> conn = unregistered[connectionId];

                bool validConnection = enableValidation ? ( validationData.contains( clientConnectionId ) && validationData[clientConnectionId] == uuid )
                                                              && ( registered[clientConnectionId] == nullptr ) :
                                                          true;

                std::shared_ptr<ipc::MTIPCJson>
                    handshakeMessage = ipc::MTIPCMessageConstructor::constructHeaderMessage( ipc::MTIPCMessageType::IPC_MESSAGE_TYPE_REGISTERATION_STATUS );

                if( validConnection )
                {
                    handshakeMessage->setProperty( ipc::IPC_KEY_REGISTERATION_STATUS, ipc::MTIPCRegistrationStatus::IPC_MESSAGE_REGISTERATION_SUCCESS );

                    conn->setConnectionIdentifier( clientConnectionId );
                    conn->setConnectionName( connectionName );
                    conn->setCallback( nullptr );
                    conn->setDisconnectionNotifier( std::bind( &MTIPCSocketServerImpl::onClientDisconnected, this, std::placeholders::_1 ) );
                    registered.set( clientConnectionId, conn );
                    conn->publish( handshakeMessage );

                    if( serverCallback )
                        serverCallback->newConnectionRegistered( clientConnectionId, std::static_pointer_cast<ipc::MTIPCConnection>( conn ) );
                }
                else
                {
                    handshakeMessage->setProperty( ipc::IPC_KEY_REGISTERATION_STATUS, ipc::MTIPCRegistrationStatus::IPC_MESSAGE_REGISTERATION_FAILED );
                    conn->publish( handshakeMessage );
                }
                unregistered.remove( connectionId );
                registered.remove( connectionId );

            } catch( ... )
            {
                // log exception.
            }
        }

        void onClientDisconnected( int connectionId )
        {
            if( serverCallback )
                serverCallback->connectionRemoved( connectionId );

            if( registered.contains( connectionId ) )
            {
                registered.remove( connectionId );
            }
            if( unregistered.contains( connectionId ) )
            {
                unregistered.remove( connectionId );
            }
        }

    private:
        juce::HashMap<int, std::shared_ptr<ipc::MTIPCClientConnection>> unregistered;
        juce::HashMap<int, std::shared_ptr<ipc::MTIPCClientConnection>> registered;
        juce::HashMap<int, juce::String>                               validationData;
        bool                                                           enableValidation{true};
        int                                                            serverPort{ -1 };
        juce::String                                                   bindAddress{ juce::String() };
        int                                                            tempConnectionId{ 0 };
        std::shared_ptr<ipc::MTIIPCServerCallback>                      serverCallback;
        std::shared_ptr<ipc::MTIIPCConnectionCallback>                  connectionCallBackPtr{ nullptr };
        std::function<void( int )>                                     disconnectionCallbackfPtr{ nullptr };
    };

    // facade class

    MTIPCSocketServer::MTIPCSocketServer( const juce::var& config, std::shared_ptr<ipc::MTIIPCServerCallback> serverCallback )
        : impl( new ipc::MTIPCSocketServerImpl( config, serverCallback ) )
    {
    }

    MTIPCSocketServer::~MTIPCSocketServer() { impl.reset(); };

    bool MTIPCSocketServer::start()
    {
        return impl->start();
    }

    void MTIPCSocketServer::stop()
    {
        impl->stop();
        
    }

    bool MTIPCSocketServer::areAllValidClientsConnected()
    {
        return impl->areAllValidClientsConnected();
    }

    void MTIPCSocketServer::updateUnConnectedConnectionIds( std::vector<int>& ids )
    {
        impl->updateUnConnectedConnectionIds( ids );
    }
}
