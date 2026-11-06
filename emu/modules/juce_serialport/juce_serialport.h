/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 juce_serialport
  vendor:             Bombest
  version:            0.1.0
  name:               Serial port (clean-room stand-in)
  description:        Serial port classes with the API Kick's input_events module expects
  website:
  license:            AGPLv3

  dependencies:       juce_core, juce_events

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

/*
 * The original juce_serialport module was removed from the public release
 * because it had no license. This is a clean-room replacement written from the
 * calls Kick makes (Kick/modules/input_events/input_events.cpp), not from the
 * original source.
 *
 * Two backends. A POSIX tty (Linux and macOS) opens real UARTs such as /dev/ttymxc0 and
 * pseudo-terminals such as the one the KICK Surface emulator creates. A path of the form
 * tcp://host:port connects to a TCP server instead, on every OS, and is how the emulator
 * reaches the Surface on Windows, which has no pty.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#ifndef JUCE_SERIALPORT_H_INCLUDE
    #define JUCE_SERIALPORT_H_INCLUDE

    #include <juce_core/juce_core.h>
    #include <juce_events/juce_events.h>

    #include <functional>
    #include <memory>
    #include <mutex>

class SerialPortConfig
{
public:
    enum SerialPortParity
    {
        SERIALPORT_PARITY_NONE = 0,
        SERIALPORT_PARITY_ODD,
        SERIALPORT_PARITY_EVEN,
    };

    enum SerialPortStopBits
    {
        STOPBITS_1 = 0,
        STOPBITS_2,
    };

    enum SerialPortFlowControl
    {
        FLOWCONTROL_NONE = 0,
        FLOWCONTROL_HARDWARE,
    };

    SerialPortConfig() = default;
    SerialPortConfig( int baud, int dataBits, SerialPortParity p, SerialPortStopBits s, SerialPortFlowControl f )
        : bps( baud ), databits( dataBits ), parity( p ), stopbits( s ), flowcontrol( f ) {}

    int                   bps         = 230400;
    int                   databits    = 8;
    SerialPortParity      parity      = SERIALPORT_PARITY_NONE;
    SerialPortStopBits    stopbits    = STOPBITS_1;
    SerialPortFlowControl flowcontrol = FLOWCONTROL_NONE;
};

class SerialPort
{
public:
    using DebugFn = std::function<void( juce::String, juce::String )>;

    SerialPort( const juce::String& path, const SerialPortConfig& config, DebugFn debug = {} );
    ~SerialPort();

    bool open( const juce::String& path );
    void close();
    bool exists() const noexcept;
    bool setConfig( const SerialPortConfig& config );

    const juce::String& getPortPath() const noexcept { return portPath; }

    /// 1 when bytes can be read, 0 on timeout, -1 when the port is closed or the peer hung up.
    int waitReadable( int timeoutMs );
    /// Bytes read (> 0), 0 when nothing is available, -1 on a closed port.
    int readSome( void* dst, int maxBytes );
    bool writeAll( const void* data, size_t numBytes );
    void drain();
    /// True for a tcp:// port, which the input thread reconnects when the peer goes away.
    bool isTcp() const noexcept { return portPath.startsWith( "tcp://" ); }

    /// Lists tty devices: key is the friendly name, value the path.
    static juce::StringPairArray getSerialPortPaths();

private:
    std::shared_ptr<juce::StreamingSocket> getSocket() const;

    int          fd = -1;
    juce::String portPath;
    DebugFn      debugFn;
    mutable std::mutex                     socketLock;
    std::shared_ptr<juce::StreamingSocket> socket;

    JUCE_DECLARE_NON_COPYABLE( SerialPort )
};

class SerialPortInputStream
    : public juce::InputStream
    , public juce::ChangeBroadcaster
    , private juce::Thread
{
public:
    enum NotifyType
    {
        NOTIFY_OFF = 0,
        NOTIFY_ON_CHAR,
        NOTIFY_ALWAYS,
    };

    explicit SerialPortInputStream( SerialPort* port );
    ~SerialPortInputStream() override;

    void setNotify( NotifyType type, char c = 0 ) noexcept
    {
        notifyType = type;
        notifyChar = c;
    }

    // InputStream. getTotalLength() is the number of bytes buffered and not yet read.
    juce::int64 getTotalLength() override;
    bool        isExhausted() override;
    int         read( void* destBuffer, int maxBytesToRead ) override;
    juce::int64 getPosition() override { return 0; }
    bool        setPosition( juce::int64 ) override { return false; }

private:
    void run() override;

    SerialPort*            port;
    std::mutex             lock;
    juce::MemoryBlock      buffer;
    size_t                 used       = 0;
    std::atomic<int>       notifyType { NOTIFY_OFF };
    char                   notifyChar = 0;
};

class SerialPortOutputStream : public juce::OutputStream
{
public:
    explicit SerialPortOutputStream( SerialPort* port ) : port( port ) {}

    bool        write( const void* data, size_t numBytes ) override;
    void        flush() override;
    juce::int64 getPosition() override { return 0; }
    bool        setPosition( juce::int64 ) override { return false; }

private:
    SerialPort* port;
};

#endif // JUCE_SERIALPORT_H_INCLUDE
