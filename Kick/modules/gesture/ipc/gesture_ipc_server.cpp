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

#include "gesture_ipc_server.h"

#include "gesture_decode.h"

#include "contracts/gesture/gesture_ipc_keys.h"
#include "contracts/gesture/gesture_signals.h"
#include "ipc/ipc.h"

#include <cstdlib>
#include <set>

#include <juce_events/juce_events.h>

namespace kick_gesture
{
    namespace
    {
        // Runs on the IPC thread for the connection callbacks, and on the message thread for everything after callAsync.
        // `connections` and `lastStatus` belong to the message thread alone.
        class Hub : public ipc::MTIIPCServerCallback,
                    public ipc::MTIIPCConnectionCallback,
                    public std::enable_shared_from_this<Hub>
        {
        public:
            void newConnectionRegistered( const int connectionId, std::shared_ptr<ipc::MTIPCConnection> conn ) override
            {
                // After registration the server clears the connection's callback. Set it so later messages reach us.
                if( conn )
                    conn->setCallback( shared_from_this() );
                std::weak_ptr<Hub> weak = shared_from_this();
                juce::MessageManager::callAsync( [weak, connectionId] {
                    if( auto self = weak.lock() )
                        self->onRegistered( connectionId );
                } );
            }

            void connectionRemoved( const int connectionId ) override
            {
                std::weak_ptr<Hub> weak = shared_from_this();
                juce::MessageManager::callAsync( [weak, connectionId] {
                    if( auto self = weak.lock() )
                        self->onRemoved( connectionId );
                } );
            }

            void onConnected( const int, const juce::String& ) override {}
            void onDisconnected( const int, const juce::String& ) override {}

            void onMessage( const int, std::shared_ptr<ipc::MTIPCJson> message ) override
            {
                if( !message )
                    return;
                DecodedGestureMessage decoded;
                // getRoot() is a plain var. MTIPCJson::getProperty throws on a missing key, and a payload that is not an
                // object can crash it, so the decoder only ever sees the root.
                if( !decodeGestureMessage( message->getRoot(), decoded ) )
                    return;
                std::weak_ptr<Hub> weak = shared_from_this();
                juce::MessageManager::callAsync( [weak, decoded] {
                    if( auto self = weak.lock() )
                        self->deliver( decoded );
                } );
            }

        private:
            void onRegistered( int id )
            {
                connections.insert( id );
                lastStatus           = contracts::GestureServiceStatus{};
                lastStatus.connected = true;
                contracts::GestureSignals::serviceStatusChanged( lastStatus );
            }

            void onRemoved( int id )
            {
                // The server also reports clients that never finished registering. Only a registered one counts.
                if( connections.erase( id ) == 0 )
                    return;
                if( !connections.empty() )
                    return;
                lastStatus           = contracts::GestureServiceStatus{};
                lastStatus.connected = false;
                contracts::GestureSignals::serviceStatusChanged( lastStatus );
            }

            void deliver( const DecodedGestureMessage& m )
            {
                if( m.kind == DecodedGestureMessage::Kind::Event )
                    contracts::GestureSignals::gestureReceived( m.event );
                else if( m.kind == DecodedGestureMessage::Kind::Status )
                {
                    lastStatus           = m.status;
                    lastStatus.connected = true;
                    contracts::GestureSignals::serviceStatusChanged( lastStatus );
                }
            }

            std::set<int>                   connections;
            contracts::GestureServiceStatus lastStatus;
        };
    }

    struct GestureIpcServer::Impl
    {
        int                                     port{ 0 };
        std::shared_ptr<Hub>                    hub{ std::make_shared<Hub>() };
        std::unique_ptr<ipc::MTIPCSocketServer> server;
    };

    int GestureIpcServer::defaultPort()
    {
        if( const char* env = std::getenv( "KICK_GESTURE_PORT" ) )
        {
            const int p = std::atoi( env );
            if( p > 0 && p < 65536 )
                return p;
        }
        return contracts::GESTURE_IPC_DEFAULT_PORT;
    }

    GestureIpcServer::GestureIpcServer( int port ) : impl( new Impl )
    {
        impl->port = port > 0 ? port : defaultPort();
    }

    GestureIpcServer::~GestureIpcServer() { stop(); }

    int GestureIpcServer::port() const { return impl->port; }

    void GestureIpcServer::start()
    {
        if( impl->server )
            return;

        juce::DynamicObject::Ptr cfg = new juce::DynamicObject();
        cfg->setProperty( ipc::IPC_KEY_PORT, impl->port );
        cfg->setProperty( ipc::IPC_KEY_BIND_ADDRESS, contracts::GESTURE_IPC_DEFAULT_ADDRESS );
        cfg->setProperty( ipc::IPC_KEY_CLIENT_ENABLE_VALIDATION, false );
        impl->server.reset( new ipc::MTIPCSocketServer( juce::var( cfg.get() ), impl->hub ) );
        impl->server->start();
    }

    void GestureIpcServer::stop()
    {
        if( impl && impl->server )
        {
            impl->server->stop();
            impl->server.reset();
        }
    }
}
