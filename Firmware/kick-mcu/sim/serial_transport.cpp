// SPDX-License-Identifier: AGPL-3.0-or-later
#include "serial_transport.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>

namespace kick_mcu
{
namespace sim
{
    namespace
    {
        constexpr size_t kBacklogCap = 4096; // bytes waiting for a peer that is not reading

        bool setNonBlocking( int fd )
        {
            const int flags = fcntl( fd, F_GETFL );
            return flags >= 0 && fcntl( fd, F_SETFL, flags | O_NONBLOCK ) == 0;
        }

        class FdTransport : public SerialTransport
        {
        public:
            ~FdTransport() override
            {
                for( int fd : { dataFd_, listenFd_, slaveFd_ } )
                    if( fd >= 0 )
                        ::close( fd );
                if( !linkPath_.empty() )
                    ::unlink( linkPath_.c_str() );
                if( !portFile_.empty() )
                    ::unlink( portFile_.c_str() );
            }

            bool service() override
            {
                bool accepted = false;
                if( listenFd_ >= 0 && dataFd_ < 0 )
                {
                    const int client = ::accept( listenFd_, nullptr, nullptr );
                    if( client >= 0 )
                    {
                        setNonBlocking( client );
                        int one = 1;
                        setsockopt( client, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one );
                        dataFd_ = client;
                        backlog_.clear();
                        accepted = true;
                    }
                }
                flush();
                return accepted;
            }

            size_t read( uint8_t* buffer, size_t capacity ) override
            {
                if( dataFd_ < 0 )
                    return 0;
                const ssize_t n = ::read( dataFd_, buffer, capacity );
                if( n > 0 )
                    return (size_t)n;
                // Read returns 0 when a TCP peer closed. A pty master reports EIO when no slave is
                // open, which is the normal state before Kick opens it, so only TCP is closed here.
                if( n == 0 && listenFd_ >= 0 )
                    dropClient();
                else if( n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EIO && errno != EINTR && listenFd_ >= 0 )
                    dropClient();
                return 0;
            }

            bool write( const uint8_t* data, size_t length ) override
            {
                if( dataFd_ < 0 )
                    return false;
                if( backlog_.size() + length > kBacklogCap )
                    return false;
                backlog_.insert( backlog_.end(), data, data + length );
                flush();
                return true;
            }

            bool connected() const override { return dataFd_ >= 0; }

            int                dataFd_{ -1 };
            int                listenFd_{ -1 };
            int                slaveFd_{ -1 }; ///< kept open so the pty master never sees a hang-up when Kick closes its end
            std::string        linkPath_, portFile_;

        private:
            void dropClient()
            {
                ::close( dataFd_ );
                dataFd_ = -1;
                backlog_.clear();
            }

            void flush()
            {
                while( dataFd_ >= 0 && !backlog_.empty() )
                {
                    uint8_t chunk[512];
                    size_t  n = 0;
                    for( auto it = backlog_.begin(); it != backlog_.end() && n < sizeof chunk; ++it )
                        chunk[n++] = *it;
                    const ssize_t w = ::write( dataFd_, chunk, n );
                    if( w > 0 )
                    {
                        backlog_.erase( backlog_.begin(), backlog_.begin() + w );
                        continue;
                    }
                    if( w < 0 && ( errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ) )
                        return;
                    if( listenFd_ >= 0 )
                        dropClient();
                    else
                        backlog_.clear();
                    return;
                }
            }

            std::deque<uint8_t> backlog_;
        };
    } // namespace

    std::unique_ptr<SerialTransport> SerialTransport::openPty( const std::string& root, std::string& error )
    {
        auto t = std::make_unique<FdTransport>();

        const int master = ::posix_openpt( O_RDWR | O_NOCTTY );
        if( master < 0 || ::grantpt( master ) != 0 || ::unlockpt( master ) != 0 )
        {
            error = "cannot open a pty";
            if( master >= 0 )
                ::close( master );
            return nullptr;
        }
        t->dataFd_ = master;

        const char* name = ::ptsname( master );
        if( name == nullptr )
        {
            error = "cannot name the pty";
            return nullptr;
        }
        const std::string slave( name );
        t->slaveFd_ = ::open( slave.c_str(), O_RDWR | O_NOCTTY );
        if( t->slaveFd_ < 0 )
        {
            error = "cannot open " + slave;
            return nullptr;
        }

        termios tio{};
        tcgetattr( t->slaveFd_, &tio );
        cfmakeraw( &tio );
        cfsetspeed( &tio, B230400 );
        tcsetattr( t->slaveFd_, TCSANOW, &tio );
        setNonBlocking( master );

        const std::string dev = root + "/dev";
        ::mkdir( root.c_str(), 0755 );
        ::mkdir( dev.c_str(), 0755 );
        t->linkPath_ = dev + "/ttymxc0";
        ::unlink( t->linkPath_.c_str() );
        if( ::symlink( slave.c_str(), t->linkPath_.c_str() ) != 0 )
        {
            error = "cannot link " + t->linkPath_;
            t->linkPath_.clear();
            return nullptr;
        }
        std::printf( "KICK_SERIAL_PORT=%s\nKICK_SERIAL_PTY=%s\n", t->linkPath_.c_str(), slave.c_str() );
        std::fflush( stdout );
        return t;
    }

    std::unique_ptr<SerialTransport> SerialTransport::listenTcp( const std::string& root, int port, std::string& error )
    {
        auto t = std::make_unique<FdTransport>();

        t->listenFd_ = ::socket( AF_INET, SOCK_STREAM, 0 );
        if( t->listenFd_ < 0 )
        {
            error = "cannot create a socket";
            return nullptr;
        }
        int one = 1;
        setsockopt( t->listenFd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one );

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons( (uint16_t)port );
        addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
        if( ::bind( t->listenFd_, (sockaddr*)&addr, sizeof addr ) != 0 || ::listen( t->listenFd_, 1 ) != 0 || !setNonBlocking( t->listenFd_ ) )
        {
            error = "cannot listen on TCP port " + std::to_string( port );
            return nullptr;
        }

        ::mkdir( root.c_str(), 0755 );
        t->portFile_ = root + "/surface.port";
        std::ofstream( t->portFile_ ) << "tcp://127.0.0.1:" << port;
        std::printf( "KICK_SERIAL_TCP=%d\n", port );
        std::fflush( stdout );
        return t;
    }

} // namespace sim
} // namespace kick_mcu
