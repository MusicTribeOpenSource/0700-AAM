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

#include "camera_status.h"

#include <cstring>

#if !defined( _WIN32 )
    #include <cerrno>
    #include <fcntl.h>
    #include <poll.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
#endif

namespace kick_gesture
{
    namespace
    {
        // The value text after "key": in a flat JSON object that kick-camera wrote. No nesting, no escapes except \" in strings.
        bool findValue( const std::string& json, const char* key, std::string& value, bool& isString )
        {
            const std::string needle = std::string( "\"" ) + key + "\"";
            // A match only counts as a key when a colon follows. Otherwise "state":"error" would match key "error" on its value.
            size_t at = json.find( needle );
            while( at != std::string::npos )
            {
                size_t after = at + needle.size();
                while( after < json.size() && json[after] == ' ' )
                    ++after;
                if( after < json.size() && json[after] == ':' )
                    break;
                at = json.find( needle, at + 1 );
            }
            if( at == std::string::npos )
                return false;
            at += needle.size();
            while( at < json.size() && ( json[at] == ' ' || json[at] == ':' ) )
                ++at;
            if( at >= json.size() )
                return false;
            value.clear();
            if( json[at] == '"' )
            {
                isString = true;
                for( ++at; at < json.size() && json[at] != '"'; ++at )
                {
                    if( json[at] == '\\' && at + 1 < json.size() )
                        ++at;
                    value += json[at];
                }
                return at < json.size();
            }
            isString = false;
            while( at < json.size() && json[at] != ',' && json[at] != '}' && json[at] != ' ' )
                value += json[at++];
            return !value.empty();
        }
    }

    contracts::CameraStatus cameraServiceMissing( const std::string& why )
    {
        contracts::CameraStatus s;
        s.state     = contracts::CameraState::ServiceMissing;
        s.capturing = false;
        s.detail    = why;
        return s;
    }

    contracts::CameraStatus parseCameraStatus( const std::string& reply )
    {
        std::string state, capturing, error;
        bool        isString = false;
        if( reply.find( '{' ) == std::string::npos || !findValue( reply, "state", state, isString ) || !isString )
            return cameraServiceMissing( "the camera service sent a reply I could not read" );
        findValue( reply, "capturing", capturing, isString );
        findValue( reply, "error", error, isString );

        contracts::CameraStatus s;
        if( state == "streaming" )
        {
            s.state     = contracts::CameraState::Streaming;
            s.capturing = ( capturing == "true" );
            return s;
        }
        if( state == "off" )
        {
            s.state = contracts::CameraState::Off;
            return s;
        }
        if( state == "error" || state == "ended" )
        {
            s.state  = contracts::CameraState::Error;
            s.detail = error.empty() ? state : error;
            // A device that failed to open can still have the flag raised for a moment. Follow the flag, as the indicator does.
            s.capturing = ( capturing == "true" );
            return s;
        }
        return cameraServiceMissing( "the camera service reported an unknown state: " + state );
    }

    std::string describeCameraStatus( const contracts::CameraStatus& s )
    {
        switch( s.state )
        {
            case contracts::CameraState::ServiceMissing: return "Camera service not running";
            case contracts::CameraState::Off: return "Camera off";
            case contracts::CameraState::Streaming: return "Camera on";
            case contracts::CameraState::Error: return s.detail.empty() ? "Camera error" : "Camera error: " + s.detail;
        }
        return "Camera off";
    }

#if defined( _WIN32 )
    bool cameraRequest( const std::string&, const std::string&, std::string&, std::string& error, int )
    {
        error = "the camera service is not available on this platform";
        return false;
    }
#else
    bool cameraRequest( const std::string& socketPath, const std::string& command, std::string& reply, std::string& error, int timeoutMs )
    {
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if( socketPath.empty() || socketPath.size() >= sizeof( addr.sun_path ) )
        {
            error = "bad camera socket path";
            return false;
        }
        std::memcpy( addr.sun_path, socketPath.c_str(), socketPath.size() + 1 );

        const int fd = ::socket( AF_UNIX, SOCK_STREAM, 0 );
        if( fd < 0 )
        {
            error = std::string( "socket: " ) + std::strerror( errno );
            return false;
        }
        ::fcntl( fd, F_SETFD, FD_CLOEXEC );
    #ifdef SO_NOSIGPIPE
        const int one = 1;
        ::setsockopt( fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof( one ) );
    #endif
        // A Unix connect either completes at once or fails at once (no listener), so a blocking connect is fine.
        if( ::connect( fd, reinterpret_cast<sockaddr*>( &addr ), sizeof( addr ) ) != 0 )
        {
            error = std::string( "connect: " ) + std::strerror( errno );
            ::close( fd );
            return false;
        }

        const std::string line = command + "\n";
        size_t            sent = 0;
        while( sent < line.size() )
        {
    #ifdef MSG_NOSIGNAL
            const int flags = MSG_NOSIGNAL;
    #else
            const int flags = 0;
    #endif
            const ssize_t n = ::send( fd, line.data() + sent, line.size() - sent, flags );
            if( n < 0 )
            {
                if( errno == EINTR )
                    continue;
                error = std::string( "send: " ) + std::strerror( errno );
                ::close( fd );
                return false;
            }
            sent += static_cast<size_t>( n );
        }

        reply.clear();
        for( ;; )
        {
            pollfd p{ fd, POLLIN, 0 };
            const int ready = ::poll( &p, 1, timeoutMs );
            if( ready == 0 )
            {
                error = "the camera service did not answer";
                ::close( fd );
                return false;
            }
            if( ready < 0 )
            {
                if( errno == EINTR )
                    continue;
                error = std::string( "poll: " ) + std::strerror( errno );
                ::close( fd );
                return false;
            }
            char          buf[512];
            const ssize_t n = ::recv( fd, buf, sizeof( buf ), 0 );
            if( n <= 0 )
                break;
            reply.append( buf, static_cast<size_t>( n ) );
            const auto nl = reply.find( '\n' );
            if( nl != std::string::npos )
            {
                reply.resize( nl );
                ::close( fd );
                return true;
            }
            if( reply.size() > 4096 )
            {
                error = "the camera reply was too long";
                ::close( fd );
                return false;
            }
        }
        ::close( fd );
        if( reply.empty() )
        {
            error = "the camera service closed the connection without a reply";
            return false;
        }
        return true;
    }
#endif
}
