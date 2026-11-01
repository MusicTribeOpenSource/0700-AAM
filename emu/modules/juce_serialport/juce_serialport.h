/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 juce_serialport
  vendor:             Bombest
  version:            0.1.0
  name:               POSIX serial port (clean-room stand-in)
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
 * POSIX only (Linux and macOS). It opens real UARTs such as /dev/ttymxc0 and
 * pseudo-terminals such as the one the KICK Surface emulator creates.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#ifndef JUCE_SERIALPORT_H_INCLUDE
    #define JUCE_SERIALPORT_H_INCLUDE

    #include <juce_core/juce_core.h>
    #include <juce_events/juce_events.h>

    #include <functional>
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
    bool exists() const noexcept { return fd >= 0; }
    bool setConfig( const SerialPortConfig& config );

    int  getFd() const noexcept { return fd; }
    const juce::String& getPortPath() const noexcept { return portPath; }

    /// Lists tty devices: key is the friendly name, value the path.
    static juce::StringPairArray getSerialPortPaths();

private:
    int          fd = -1;
    juce::String portPath;
    DebugFn      debugFn;

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
