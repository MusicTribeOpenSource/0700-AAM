/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "juce_serialport.h"

#if JUCE_WINDOWS
    #error "juce_serialport stand-in is POSIX only"
#endif

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace
{
    speed_t toSpeed( int bps )
    {
        switch( bps )
        {
            case 9600: return B9600;
            case 19200: return B19200;
            case 38400: return B38400;
            case 57600: return B57600;
            case 115200: return B115200;
            case 230400: return B230400;
            default: return B230400;
        }
    }
}

//==============================================================================
SerialPort::SerialPort( const juce::String& path, const SerialPortConfig& config, DebugFn debug )
    : debugFn( std::move( debug ) )
{
    if( open( path ) )
        setConfig( config );
}

SerialPort::~SerialPort()
{
    close();
}

bool SerialPort::open( const juce::String& path )
{
    close();
    portPath = path;
    fd       = ::open( path.toRawUTF8(), O_RDWR | O_NOCTTY | O_NONBLOCK );
    if( fd < 0 )
    {
        if( debugFn )
            debugFn( "SerialPort: open failed " + path, juce::String( std::strerror( errno ) ) );
        return false;
    }
    return true;
}

void SerialPort::close()
{
    if( fd >= 0 )
        ::close( fd );
    fd = -1;
}

bool SerialPort::setConfig( const SerialPortConfig& config )
{
    if( fd < 0 )
        return false;

    termios tio{};
    if( tcgetattr( fd, &tio ) != 0 )
        return false;

    cfmakeraw( &tio );
    cfsetispeed( &tio, toSpeed( config.bps ) );
    cfsetospeed( &tio, toSpeed( config.bps ) );

    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= ( config.databits == 7 ? CS7 : CS8 );
    tio.c_cflag |= ( CLOCAL | CREAD );

    tio.c_cflag &= ~( PARENB | PARODD );
    if( config.parity == SerialPortConfig::SERIALPORT_PARITY_ODD )
        tio.c_cflag |= ( PARENB | PARODD );
    else if( config.parity == SerialPortConfig::SERIALPORT_PARITY_EVEN )
        tio.c_cflag |= PARENB;

    if( config.stopbits == SerialPortConfig::STOPBITS_2 )
        tio.c_cflag |= CSTOPB;
    else
        tio.c_cflag &= ~CSTOPB;

#ifdef CRTSCTS
    if( config.flowcontrol == SerialPortConfig::FLOWCONTROL_HARDWARE )
        tio.c_cflag |= CRTSCTS;
    else
        tio.c_cflag &= ~CRTSCTS;
#endif

    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 0;

    // A pseudo-terminal ignores the speed, and tcsetattr can fail on some pty
    // flavours. That is not fatal for the emulator, so only log it.
    if( tcsetattr( fd, TCSANOW, &tio ) != 0 && debugFn )
        debugFn( "SerialPort: tcsetattr failed " + portPath, juce::String( std::strerror( errno ) ) );
    return true;
}

juce::StringPairArray SerialPort::getSerialPortPaths()
{
    juce::StringPairArray result;
    juce::Array<juce::File> files;
    juce::File( "/dev" ).findChildFiles( files, juce::File::findFiles, false, "tty*" );
    juce::File( "/dev" ).findChildFiles( files, juce::File::findFiles, false, "cu.*" );
    for( auto& f : files )
        result.set( f.getFileName(), f.getFullPathName() );

    // An emulator pty usually lives under /dev/ttys* (macOS) or /dev/pts/* (Linux).
    auto emuPort = juce::SystemStats::getEnvironmentVariable( "KICK_SERIAL_PORT", {} );
    if( emuPort.isNotEmpty() )
        result.set( "KICK_SERIAL_PORT", emuPort );
    return result;
}

//==============================================================================
SerialPortInputStream::SerialPortInputStream( SerialPort* p )
    : juce::Thread( "SerialPortInputStream" )
    , port( p )
{
    startThread();
}

SerialPortInputStream::~SerialPortInputStream()
{
    signalThreadShouldExit();
    stopThread( 1000 );
}

juce::int64 SerialPortInputStream::getTotalLength()
{
    std::lock_guard<std::mutex> g( lock );
    return (juce::int64)used;
}

bool SerialPortInputStream::isExhausted()
{
    return getTotalLength() == 0;
}

int SerialPortInputStream::read( void* destBuffer, int maxBytesToRead )
{
    std::lock_guard<std::mutex> g( lock );
    const auto n = (size_t)juce::jmin( (size_t)juce::jmax( 0, maxBytesToRead ), used );
    if( n == 0 )
        return 0;

    std::memcpy( destBuffer, buffer.getData(), n );
    std::memmove( buffer.getData(), static_cast<char*>( buffer.getData() ) + n, used - n );
    used -= n;
    return (int)n;
}

void SerialPortInputStream::run()
{
    char chunk[512];

    while( !threadShouldExit() )
    {
        const int fd = port != nullptr ? port->getFd() : -1;
        if( fd < 0 )
        {
            wait( 100 );
            continue;
        }

        pollfd pfd{ fd, POLLIN, 0 };
        const int r = ::poll( &pfd, 1, 50 );
        if( r <= 0 || ( pfd.revents & POLLIN ) == 0 )
        {
            // POLLHUP on a pty means the other side closed. Back off and retry.
            if( r > 0 && ( pfd.revents & ( POLLHUP | POLLERR ) ) )
                wait( 100 );
            continue;
        }

        const auto got = ::read( fd, chunk, sizeof( chunk ) );
        if( got <= 0 )
            continue;

        bool notify = false;
        {
            std::lock_guard<std::mutex> g( lock );
            if( buffer.getSize() < used + (size_t)got )
                buffer.setSize( ( used + (size_t)got ) * 2, false );
            std::memcpy( static_cast<char*>( buffer.getData() ) + used, chunk, (size_t)got );
            used += (size_t)got;

            if( notifyType == NOTIFY_ALWAYS )
                notify = true;
            else if( notifyType == NOTIFY_ON_CHAR )
                notify = std::memchr( chunk, notifyChar, (size_t)got ) != nullptr;
        }

        if( notify )
            sendChangeMessage();
    }
}

//==============================================================================
bool SerialPortOutputStream::write( const void* data, size_t numBytes )
{
    if( port == nullptr || port->getFd() < 0 )
        return false;

    auto*  p    = static_cast<const char*>( data );
    size_t left = numBytes;
    int    spins = 0;
    while( left > 0 )
    {
        const auto n = ::write( port->getFd(), p, left );
        if( n > 0 )
        {
            p += n;
            left -= (size_t)n;
            continue;
        }
        if( n < 0 && ( errno == EAGAIN || errno == EWOULDBLOCK ) && ++spins < 100 )
        {
            juce::Thread::sleep( 1 );
            continue;
        }
        return false;
    }
    return true;
}

void SerialPortOutputStream::flush()
{
    if( port != nullptr && port->getFd() >= 0 )
        tcdrain( port->getFd() );
}
