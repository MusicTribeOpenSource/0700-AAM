/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "juce_serialport.h"

#include <cerrno>
#include <cstring>

#if !JUCE_WINDOWS
    #include <fcntl.h>
    #include <poll.h>
    #include <termios.h>
    #include <unistd.h>
#endif

namespace
{
#if !JUCE_WINDOWS
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
#endif
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

    if( isTcp() )
    {
        // tcp://host:port
        const auto   hostPort = path.fromFirstOccurrenceOf( "tcp://", false, false );
        const auto   host     = hostPort.upToLastOccurrenceOf( ":", false, false );
        const int    port     = hostPort.fromLastOccurrenceOf( ":", false, false ).getIntValue();
        auto         sock     = std::make_shared<juce::StreamingSocket>();
        if( host.isEmpty() || port <= 0 || !sock->connect( host, port, 2000 ) )
        {
            if( debugFn )
                debugFn( "SerialPort: tcp connect failed " + path, {} );
            return false;
        }
        std::lock_guard<std::mutex> g( socketLock );
        socket = sock;
        return true;
    }

#if JUCE_WINDOWS
    if( debugFn )
        debugFn( "SerialPort: only tcp:// ports are supported on Windows: " + path, {} );
    return false;
#else
    fd = ::open( path.toRawUTF8(), O_RDWR | O_NOCTTY | O_NONBLOCK );
    if( fd < 0 )
    {
        if( debugFn )
            debugFn( "SerialPort: open failed " + path, juce::String( std::strerror( errno ) ) );
        return false;
    }
    return true;
#endif
}

void SerialPort::close()
{
    {
        std::lock_guard<std::mutex> g( socketLock );
        if( socket )
            socket->close();
        socket.reset();
    }
#if !JUCE_WINDOWS
    if( fd >= 0 )
        ::close( fd );
#endif
    fd = -1;
}

std::shared_ptr<juce::StreamingSocket> SerialPort::getSocket() const
{
    std::lock_guard<std::mutex> g( socketLock );
    return socket;
}

bool SerialPort::exists() const noexcept
{
    if( fd >= 0 )
        return true;
    auto s = getSocket();
    return s != nullptr && s->isConnected();
}

int SerialPort::waitReadable( int timeoutMs )
{
    if( auto s = getSocket() )
    {
        const int r = s->waitUntilReady( true, timeoutMs );
        return r < 0 || !s->isConnected() ? -1 : r;
    }
#if !JUCE_WINDOWS
    if( fd >= 0 )
    {
        pollfd pfd{ fd, POLLIN, 0 };
        const int r = ::poll( &pfd, 1, timeoutMs );
        if( r > 0 && ( pfd.revents & POLLIN ) == 0 )
            return 0; // POLLHUP on a pty means the other side closed. The caller backs off and retries.
        return r < 0 ? 0 : r;
    }
#endif
    return -1;
}

int SerialPort::readSome( void* dst, int maxBytes )
{
    if( auto s = getSocket() )
    {
        const int n = s->read( dst, maxBytes, false );
        return n > 0 ? n : -1; // waitReadable said data was ready, so 0 or less is a hang-up
    }
#if !JUCE_WINDOWS
    if( fd >= 0 )
    {
        const auto n = ::read( fd, dst, (size_t)maxBytes );
        return n > 0 ? (int)n : 0;
    }
#endif
    return -1;
}

bool SerialPort::writeAll( const void* data, size_t numBytes )
{
    if( auto s = getSocket() )
    {
        auto*  p    = static_cast<const char*>( data );
        size_t left = numBytes;
        while( left > 0 )
        {
            const int n = s->write( p, (int)left );
            if( n <= 0 )
                return false;
            p += n;
            left -= (size_t)n;
        }
        return true;
    }
#if !JUCE_WINDOWS
    if( fd >= 0 )
    {
        auto*  p     = static_cast<const char*>( data );
        size_t left  = numBytes;
        int    spins = 0;
        while( left > 0 )
        {
            const auto n = ::write( fd, p, left );
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
#endif
    return false;
}

void SerialPort::drain()
{
#if !JUCE_WINDOWS
    if( fd >= 0 )
        tcdrain( fd );
#endif
}

bool SerialPort::setConfig( const SerialPortConfig& config )
{
#if JUCE_WINDOWS
    juce::ignoreUnused( config );
    return exists(); // baud, parity and flow control mean nothing to a TCP stream
#else
    if( fd < 0 )
        return exists(); // a tcp:// port has no line settings

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
    if( tcsetattr( fd, TCSANOW, &tio ) != 0 )
    {
        if( debugFn )
            debugFn( "SerialPort: tcsetattr failed " + portPath, juce::String( std::strerror( errno ) ) );
        // A real UART with unknown line settings corrupts packets, so that is a failure. Only
        // pseudo-terminals (the emulator's link) may carry on.
        const bool isPty = portPath.contains( "/pts/" ) || portPath.contains( "/ttys" )
                           || juce::File( portPath ).isSymbolicLink();
        if( !isPty )
            return false;
    }
    return true;
#endif
}

juce::StringPairArray SerialPort::getSerialPortPaths()
{
    juce::StringPairArray result;
#if !JUCE_WINDOWS
    juce::Array<juce::File> files;
    juce::File( "/dev" ).findChildFiles( files, juce::File::findFiles, false, "tty*" );
    juce::File( "/dev" ).findChildFiles( files, juce::File::findFiles, false, "cu.*" );
    for( auto& f : files )
        result.set( f.getFileName(), f.getFullPathName() );
#endif

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

namespace
{
    /// True when buf is a run of complete frames, or starts with something that is not a frame
    /// (so Kick can discard it). False while the last frame is still arriving.
    bool holdsWholeFrames( const unsigned char* buf, size_t n )
    {
        size_t i = 0;
        while( i < n )
        {
            if( n - i < 6 )
                return buf[i] != 'M';
            if( buf[i] != 'M' || buf[i + 1] != 'T' )
                return true;
            i += (size_t)buf[i + 5] + 7;
            if( i > n )
                return false;
        }
        return true;
    }
}

void SerialPortInputStream::run()
{
    char chunk[512];

    while( !threadShouldExit() )
    {
        if( port == nullptr )
        {
            wait( 100 );
            continue;
        }

        const int ready = port->waitReadable( 50 );
        if( ready < 0 )
        {
            // Closed, or a TCP peer that went away. A TCP port is reopened so the Surface can restart.
            wait( 200 );
            if( port->isTcp() && !threadShouldExit() )
                port->open( port->getPortPath() );
            continue;
        }
        if( ready == 0 )
            continue;

        const int got = port->readSome( chunk, (int)sizeof( chunk ) );
        if( got < 0 )
        {
            port->close(); // next pass sees ready < 0 and reconnects when the port is TCP
            continue;
        }
        if( got == 0 )
        {
            wait( 20 );
            continue;
        }

        bool notify = false;
        {
            std::lock_guard<std::mutex> g( lock );
            if( buffer.getSize() < used + (size_t)got )
                buffer.setSize( ( used + (size_t)got ) * 2, false );
            std::memcpy( static_cast<char*>( buffer.getData() ) + used, chunk, (size_t)got );
            used += (size_t)got;

            if( port->isTcp() )
                // TCP splits frames anywhere and Kick drops what it cannot parse, so only wake
                // it once the buffer holds whole PktMgr frames ('M' 'T' type dir cmd size data crc '\n').
                notify = holdsWholeFrames( static_cast<const unsigned char*>( buffer.getData() ), used );
            else if( notifyType == NOTIFY_ALWAYS )
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
    return port != nullptr && port->writeAll( data, numBytes );
}

void SerialPortOutputStream::flush()
{
    if( port != nullptr )
        port->drain();
}
