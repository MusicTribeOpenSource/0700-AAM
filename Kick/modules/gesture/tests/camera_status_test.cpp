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

#include <gtest/gtest.h>

#if !defined( _WIN32 )
    #include <atomic>
    #include <chrono>
    #include <cstdio>
    #include <cstdlib>
    #include <poll.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <thread>
    #include <unistd.h>
#endif

using contracts::CameraState;
using namespace kick_gesture;

TEST( CameraStatus, Streaming )
{
    const auto s = parseCameraStatus( R"({"state":"streaming","capturing":true,"source":"pattern","width":320,"height":240,"fps":15.0,"frames":42})" );
    EXPECT_EQ( CameraState::Streaming, s.state );
    EXPECT_TRUE( s.capturing );
    EXPECT_EQ( "Camera on", describeCameraStatus( s ) );
}

TEST( CameraStatus, Off )
{
    const auto s = parseCameraStatus( R"({"state":"off","capturing":false,"source":"v4l2","preset":"lit_room"})" );
    EXPECT_EQ( CameraState::Off, s.state );
    EXPECT_FALSE( s.capturing );
    EXPECT_EQ( "Camera off", describeCameraStatus( s ) );
}

TEST( CameraStatus, ErrorCarriesTheReason )
{
    const auto s = parseCameraStatus( R"({"state":"error","capturing":false,"error":"cannot open /dev/video0: No such file or directory"})" );
    EXPECT_EQ( CameraState::Error, s.state );
    EXPECT_EQ( "cannot open /dev/video0: No such file or directory", s.detail );
    EXPECT_NE( std::string::npos, describeCameraStatus( s ).find( "/dev/video0" ) );
}

TEST( CameraStatus, EndedCountsAsAnErrorWithoutAReason )
{
    const auto s = parseCameraStatus( R"({"state":"ended","capturing":false})" );
    EXPECT_EQ( CameraState::Error, s.state );
    EXPECT_EQ( "ended", s.detail );
}

TEST( CameraStatus, TheIndicatorFollowsTheFlagEvenWhenTheOpenFailed )
{
    const auto s = parseCameraStatus( R"({"state":"error","capturing":true,"error":"x"})" );
    EXPECT_TRUE( s.capturing );
}

TEST( CameraStatus, StreamingWithoutTheFlagIsNotCapturing )
{
    EXPECT_FALSE( parseCameraStatus( R"({"state":"streaming","capturing":false})" ).capturing );
    EXPECT_FALSE( parseCameraStatus( R"({"state":"streaming"})" ).capturing );
}

TEST( CameraStatus, GarbageMeansTheServiceIsNotUsable )
{
    for( const char* text : { "", "ok", "err no such command", "{}", R"({"state":5})", R"({"state":"weird"})", "<html>" } )
    {
        const auto s = parseCameraStatus( text );
        EXPECT_EQ( CameraState::ServiceMissing, s.state ) << text;
        EXPECT_FALSE( s.capturing ) << text;
        EXPECT_FALSE( s.detail.empty() ) << text;
    }
}

TEST( CameraStatus, MissingServiceText )
{
    EXPECT_EQ( "Camera service not running", describeCameraStatus( cameraServiceMissing( "connect failed" ) ) );
}

#if !defined( _WIN32 )
namespace
{
    // A stand-in for kick-camera's control socket: answers each line with `reply`, or never answers.
    class FakeCameraSocket
    {
    public:
        FakeCameraSocket( std::string reply, bool answer ) : reply_( std::move( reply ) ), answer_( answer )
        {
            char tmpl[] = "/tmp/kgtestXXXXXX";
            dir_        = ::mkdtemp( tmpl );
            path_       = dir_ + "/cam.sock";
            fd_         = ::socket( AF_UNIX, SOCK_STREAM, 0 );
            sockaddr_un a{};
            a.sun_family = AF_UNIX;
            std::snprintf( a.sun_path, sizeof( a.sun_path ), "%s", path_.c_str() );
            ::bind( fd_, reinterpret_cast<sockaddr*>( &a ), sizeof( a ) );
            ::listen( fd_, 4 );
            thread_ = std::thread( [this] { run(); } );
        }
        ~FakeCameraSocket()
        {
            stop_ = true;
            thread_.join();
            ::close( fd_ );
            ::unlink( path_.c_str() );
            ::rmdir( dir_.c_str() );
        }
        const std::string& path() const { return path_; }
        std::string        lastCommand() const { return last_; }

    private:
        void run()
        {
            while( !stop_ )
            {
                pollfd p{ fd_, POLLIN, 0 };
                if( ::poll( &p, 1, 20 ) <= 0 )
                    continue;
                const int c = ::accept( fd_, nullptr, nullptr );
                if( c < 0 )
                    continue;
                char    buf[128] = {};
                ssize_t n        = ::recv( c, buf, sizeof( buf ) - 1, 0 );
                if( n > 0 )
                {
                    last_ = std::string( buf, static_cast<size_t>( n ) );
                    if( answer_ )
                    {
                        const std::string out = reply_ + "\n";
                        ::send( c, out.data(), out.size(), 0 );
                    }
                    else
                    {
                        // Hold the connection open and say nothing, like a hung service.
                        for( int i = 0; i < 50 && !stop_; ++i )
                            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
                    }
                }
                ::close( c );
            }
        }

        std::string       reply_, dir_, path_;
        bool              answer_;
        int               fd_{ -1 };
        std::atomic<bool> stop_{ false };
        std::thread       thread_;
        std::string       last_;
    };
}

TEST( CameraRequest, ReadsOneReplyLine )
{
    FakeCameraSocket srv( R"({"state":"off","capturing":false})", true );
    std::string      reply, error;
    ASSERT_TRUE( cameraRequest( srv.path(), "status", reply, error ) ) << error;
    EXPECT_EQ( R"({"state":"off","capturing":false})", reply );
    EXPECT_EQ( "status\n", srv.lastCommand() );
    EXPECT_EQ( CameraState::Off, parseCameraStatus( reply ).state );
}

TEST( CameraRequest, MissingSocketFailsAtOnce )
{
    std::string reply, error;
    const auto  t0 = std::chrono::steady_clock::now();
    EXPECT_FALSE( cameraRequest( "/tmp/kick-gesture-no-such-socket", "status", reply, error ) );
    EXPECT_FALSE( error.empty() );
    EXPECT_LT( std::chrono::steady_clock::now() - t0, std::chrono::milliseconds( 200 ) );
}

TEST( CameraRequest, AHungServiceTimesOut )
{
    FakeCameraSocket srv( "", false );
    std::string      reply, error;
    const auto       t0 = std::chrono::steady_clock::now();
    EXPECT_FALSE( cameraRequest( srv.path(), "status", reply, error, 100 ) );
    const auto took = std::chrono::steady_clock::now() - t0;
    EXPECT_GE( took, std::chrono::milliseconds( 90 ) );
    EXPECT_LT( took, std::chrono::milliseconds( 450 ) );
    EXPECT_EQ( "the camera service did not answer", error );
}

TEST( CameraRequest, PathThatIsTooLongIsRejected )
{
    std::string reply, error;
    EXPECT_FALSE( cameraRequest( std::string( 300, 'x' ), "status", reply, error ) );
    EXPECT_FALSE( error.empty() );
}
#endif
