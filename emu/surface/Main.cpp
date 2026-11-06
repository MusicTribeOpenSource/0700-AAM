/*
 * KICK Surface: a virtual control surface (the MCU side of the unit) for the Kick emulator.
 *
 * Kick talks to a microcontroller over a serial line (/dev/ttymxc0, 230400 8N1) using the
 * PktMgr framing in ComProtocol/PktMgr. This app plays the MCU: it opens a pty (or listens on
 * TCP for QEMU), turns mouse and keyboard input into the same packets the real pads, buttons,
 * encoders and touch strip send, and renders the LEDs the app commands back.
 *
 * Frame, both directions:  'M' 'T' type dir cmd size data[size-1] crc8
 *   - size is data length + 1; crc8 is poly 0x31, init 0xFF, over every byte before it.
 *   - Kick expects one extra byte after the CRC on frames it receives (input_events.cpp reads
 *     size + 7 bytes), so frames sent to the app end with '\n'. Frames from the app do not.
 *
 * Usage:
 *   KickSurface [--root <dir>] [--tcp <port>] [--script <file>] [--no-window-focus] [--dump-leds <file>]
 *   KickSurface --replay <trace> [--dump-leds <file>]
 *   The tty path is printed as "KICK_SERIAL_PORT=<path>" and linked at <root>/dev/ttymxc0.
 *
 * --dump-leds writes the LED model (Surface::ledModelJson) to a file as one line of JSON when the app quits.
 * --replay plays the RX lines of a KICK_EMU_TRACE file into the frame parser with no window and no serial line,
 * writes the dump and exits. Firmware/kick-mcu does the same (kick-mcu-sim --replay) and the two files are
 * compared, see Firmware/kick-mcu/README.md.
 *
 * Environment:
 *   KICK_EMU_TRACE=<file>  write every byte on the serial line to <file> (see the Trace class).
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <juce_gui_extra/juce_gui_extra.h>

#if !JUCE_WINDOWS
    #include <fcntl.h>
    #include <poll.h>
    #include <stdlib.h>
    #include <sys/stat.h>
    #include <termios.h>
    #include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>

#include "panel_paint.h"
#include "protocol.h"

namespace
{
    using namespace surface_protocol;
    using surface_paint::drawPot;

    const char* const buttonNames[numButtons] = {
        "Bank A", "Bank B", "Bank C", "Bank D", "Bank E", "Bank F", "Bank G", "Bank H",
        "Main", "Grid", "Browse", "Save", "Track Mix", "Pad Mix", "Track Mute", "Pad Mute",
        "Next Seq", "FX", "Tap Tempo", "Master", "Copy", "+", "Menu", "Pref",
        "Note Repeat", "Latch", "Enter", "-", "Knob CCW", "Back", "Knob CW", "Shift",
        "Step Seq", "Automation", "TC", "On/Off", "Sampler", "Looper", "Sample Edit", "Link Edit",
        "Prog Edit", "MIDI Ctrl", "Record", "Overdub", "Stop", "Play", "Play Start", "Link",
        "Full Level", "Half Level", "16 Levels", "Notes", "Erase"
    };

    juce::Colour padColour( int id )
    {
        static const juce::uint32 table[] = {
            0xff8b0000, 0xff9f00ff, 0xffb8a000, 0xff90ee90, 0xff4cd964, 0xff2e8b57,
            0xff87cefa, 0xff00ffff, 0xff1e5bff, 0xffff69b4, 0xffff4fa3, 0xffff1493,
            0xffff2020, 0xffffa040, 0xffff8000, 0x00000000, 0xffffffff, 0xff808080
        };
        return juce::Colour( table[juce::jlimit( 0, 17, id )] );
    }

    juce::String normalise( const juce::String& s )
    {
        return s.toLowerCase().retainCharacters( "abcdefghijklmnopqrstuvwxyz0123456789+-" );
    }

    int buttonIdFromName( const juce::String& name )
    {
        if( name.containsOnly( "0123456789" ) )
            return juce::jlimit( 0, numButtons - 1, name.getIntValue() );
        const auto n = normalise( name );
        for( int i = 0; i < numButtons; ++i )
            if( normalise( buttonNames[i] ) == n )
                return i;
        if( n == "plus" ) return 21;
        if( n == "minus" ) return 27;
        return -1;
    }

    //==============================================================================
    struct Frame
    {
        int              type{ 0 }, dir{ 0 }, cmd{ 0 };
        std::vector<int> data;
    };

    uint8_t crc8( const std::vector<uint8_t>& bytes )
    {
        uint8_t crc = 0xff;
        for( auto b : bytes )
        {
            crc ^= b;
            for( int j = 0; j < 8; ++j )
                crc = ( crc & 0x80 ) ? (uint8_t)( ( crc << 1 ) ^ 0x31 ) : (uint8_t)( crc << 1 );
        }
        return crc;
    }

    //==============================================================================
    /// Optional record of the serial line, enabled by KICK_EMU_TRACE=<file>. Firmware tests replay
    /// it (Firmware/kick-mcu/tests), so the format is a contract; change it there too.
    ///
    ///   # kick-emu-trace v1
    ///   <ms> TX <hex>      one frame this app sent to Kick, the exact bytes written (trailer included)
    ///   <ms> RX <hex>      one read() from Kick, the bytes as they arrived (reads can hold several frames)
    ///   <ms> DEC t<type> d<dir> c<cmd> [<byte> ...]
    ///                      a frame this app decoded from RX with a good CRC, in the order it decoded it
    ///
    /// <ms> is milliseconds since the trace was opened, three decimals. Hex is lower-case, two digits
    /// per byte, separated by single spaces. Each line is flushed as it is written.
    class Trace
    {
    public:
        ~Trace() { close(); }

        bool open( const juce::String& path )
        {
            std::lock_guard<std::mutex> g( lock );
            if( file != nullptr )
                return true;
            file = std::fopen( path.toRawUTF8(), "w" );
            if( file == nullptr )
                return false;
            startMs = juce::Time::getMillisecondCounterHiRes();
            std::fputs( "# kick-emu-trace v1\n", file );
            std::fflush( file );
            return true;
        }

        void close()
        {
            std::lock_guard<std::mutex> g( lock );
            if( file != nullptr )
                std::fclose( file );
            file = nullptr;
        }

        void tx( const std::vector<uint8_t>& bytes ) { line( "TX", bytes.data(), bytes.size() ); }
        void rx( const uint8_t* bytes, size_t length ) { line( "RX", bytes, length ); }

        void decoded( int type, int dir, int cmd, const std::vector<int>& data )
        {
            std::lock_guard<std::mutex> g( lock );
            if( file == nullptr )
                return;
            std::fprintf( file, "%.3f DEC t%d d%d c%d [", now(), type, dir, cmd );
            for( size_t i = 0; i < data.size(); ++i )
                std::fprintf( file, i == 0 ? "%d" : " %d", data[i] );
            std::fputs( "]\n", file );
            std::fflush( file );
        }

    private:
        double now() const { return juce::Time::getMillisecondCounterHiRes() - startMs; }

        void line( const char* tag, const uint8_t* bytes, size_t length )
        {
            std::lock_guard<std::mutex> g( lock );
            if( file == nullptr )
                return;
            std::fprintf( file, "%.3f %s", now(), tag );
            for( size_t i = 0; i < length; ++i )
                std::fprintf( file, " %02x", bytes[i] );
            std::fputc( '\n', file );
            std::fflush( file );
        }

        std::mutex lock;
        FILE*      file{ nullptr };
        double     startMs{ 0 };
    };

    //==============================================================================
    /// The serial line to Kick: a pty by default (not on Windows), or a TCP listener. Kick
    /// connects to the listener through a tcp:// KICK_SERIAL_PORT, and QEMU's serial chardev does too.
    class Link : private juce::Thread
    {
    public:
        Link()
            : juce::Thread( "surface link" )
        {
        }

        ~Link() override
        {
            signalThreadShouldExit();
            stopThread( 1000 );
            closeFds();
        }

        std::function<void( const Frame& )> onFrame;
        Trace                               trace;

#if !JUCE_WINDOWS
        /// Opens a pty and returns the slave path. The slave stays open here so the
        /// master never sees a hang-up when Kick opens and closes its end.
        juce::String openPty()
        {
            masterFd = posix_openpt( O_RDWR | O_NOCTTY );
            if( masterFd < 0 || grantpt( masterFd ) != 0 || unlockpt( masterFd ) != 0 )
                return {};

            const juce::String path( ptsname( masterFd ) );
            slaveFd = ::open( path.toRawUTF8(), O_RDWR | O_NOCTTY );
            if( slaveFd < 0 )
                return {};

            termios t{};
            tcgetattr( slaveFd, &t );
            cfmakeraw( &t );
            cfsetspeed( &t, B230400 );
            tcsetattr( slaveFd, TCSANOW, &t );

            fcntl( masterFd, F_SETFL, fcntl( masterFd, F_GETFL ) | O_NONBLOCK );
            dataFd = masterFd;
            startThread();
            return path;
        }
#endif

        bool listenTcp( int port )
        {
            if( !listener.createListener( port, "127.0.0.1" ) )
                return false;
            startThread();
            return true;
        }

        bool isUp() const { return dataFd.load() >= 0 || tcpUp.load(); }

        /// Feeds bytes to the frame parser as if they had arrived on the line. Used by --replay.
        void feedBytes( const uint8_t* bytes, size_t length )
        {
            for( size_t i = 0; i < length; ++i )
                feed( bytes[i] );
        }
        void send( const Packet& p ) { send( p.type, p.cmd, p.data ); }

        void send( int type, int cmd, const std::vector<int>& data )
        {
            std::vector<uint8_t> out{ 'M', 'T', (uint8_t)type, 0 /* request */, (uint8_t)cmd, (uint8_t)( data.size() + 1 ) };
            for( int v : data )
                out.push_back( (uint8_t)v );
            out.push_back( crc8( out ) );
            out.push_back( '\n' ); // trailing byte Kick's reader counts in its frame length
            trace.tx( out );

            std::lock_guard<std::mutex> g( writeLock );
            if( tcpUp.load() )
            {
                std::lock_guard<std::mutex> cg( clientLock );
                if( client != nullptr )
                {
                    size_t sent = 0;
                    while( sent < out.size() )
                    {
                        const int n = client->write( out.data() + sent, (int)( out.size() - sent ) );
                        if( n <= 0 )
                            return;
                        sent += (size_t)n;
                    }
                }
                return;
            }
#if !JUCE_WINDOWS
            const int fd = dataFd.load();
            if( fd < 0 )
                return;
            size_t sent = 0;
            while( sent < out.size() )
            {
                const auto n = ::write( fd, out.data() + sent, out.size() - sent );
                if( n > 0 )
                {
                    sent += (size_t)n;
                    continue;
                }
                pollfd p{ fd, POLLOUT, 0 };
                if( poll( &p, 1, 200 ) <= 0 )
                    return; // Kick is not reading; drop the frame instead of blocking the UI
            }
#endif
        }

    private:
        void closeFds()
        {
#if !JUCE_WINDOWS
            for( int* fd : { &masterFd, &slaveFd } )
                if( *fd >= 0 )
                {
                    ::close( *fd );
                    *fd = -1;
                }
#endif
            dataFd = -1;
            tcpUp  = false;
            {
                std::lock_guard<std::mutex> cg( clientLock );
                client.reset();
            }
            listener.close();
        }

        void run() override
        {
            while( !threadShouldExit() )
            {
                if( listener.isConnected() )
                {
                    if( !tcpUp.load() )
                    {
                        if( listener.waitUntilReady( true, 200 ) > 0 )
                        {
                            std::unique_ptr<juce::StreamingSocket> accepted( listener.waitForNextConnection() );
                            if( accepted != nullptr )
                            {
                                std::lock_guard<std::mutex> cg( clientLock );
                                client = std::move( accepted );
                                tcpUp  = true;
                                reset();
                            }
                        }
                        continue;
                    }

                    if( client->waitUntilReady( true, 100 ) <= 0 )
                        continue;
                    uint8_t   buf[512];
                    const int n = client->read( buf, (int)sizeof( buf ), false );
                    if( n > 0 )
                    {
                        trace.rx( buf, (size_t)n );
                        for( int i = 0; i < n; ++i )
                            feed( buf[i] );
                    }
                    else
                    {
                        std::lock_guard<std::mutex> cg( clientLock );
                        client.reset();
                        tcpUp = false;
                    }
                    continue;
                }

#if !JUCE_WINDOWS
                const int fd = dataFd.load();
                pollfd    p{ fd, POLLIN, 0 };
                if( poll( &p, 1, 100 ) <= 0 )
                    continue;

                uint8_t buf[512];
                const auto n = ::read( fd, buf, sizeof( buf ) );
                if( n > 0 )
                {
                    trace.rx( buf, (size_t)n );
                    for( ssize_t i = 0; i < n; ++i )
                        feed( buf[i] );
                }
                else
                    juce::Thread::sleep( 10 );
#else
                juce::Thread::sleep( 50 );
#endif
            }
        }

        enum State
        {
            Hdr1,
            Hdr2,
            Type,
            Dir,
            Cmd,
            Size,
            Data,
            Crc
        };

        void reset()
        {
            state = Hdr1;
            raw.clear();
            frame = {};
        }

        void feed( uint8_t b )
        {
            switch( state )
            {
                case Hdr1:
                    if( b == 'M' )
                    {
                        raw   = { b };
                        state = Hdr2;
                    }
                    break;
                case Hdr2:
                    if( b == 'T' )
                    {
                        raw.push_back( b );
                        state = Type;
                    }
                    else
                        state = ( b == 'M' ) ? Hdr2 : Hdr1;
                    break;
                case Type: frame = {}; frame.type = b; raw.push_back( b ); state = Dir; break;
                case Dir: frame.dir = b; raw.push_back( b ); state = Cmd; break;
                case Cmd: frame.cmd = b; raw.push_back( b ); state = Size; break;
                case Size:
                    raw.push_back( b );
                    expected = b > 0 ? b - 1 : 0;
                    state    = expected > 0 ? Data : Crc;
                    break;
                case Data:
                    raw.push_back( b );
                    frame.data.push_back( b );
                    if( (int)frame.data.size() == expected )
                        state = Crc;
                    break;
                case Crc:
                    if( crc8( raw ) == b )
                    {
                        trace.decoded( frame.type, frame.dir, frame.cmd, frame.data );
                        if( onFrame )
                            onFrame( frame );
                    }
                    state = Hdr1;
                    break;
            }
        }

#if !JUCE_WINDOWS
        int masterFd{ -1 }, slaveFd{ -1 };
#endif
        std::atomic<int>                       dataFd{ -1 };
        std::atomic<bool>                      tcpUp{ false };
        std::mutex                             writeLock, clientLock;
        juce::StreamingSocket                  listener;
        std::unique_ptr<juce::StreamingSocket> client;

        State                state{ Hdr1 };
        std::vector<uint8_t> raw;
        Frame                frame;
        int                  expected{ 0 };
    };


    //==============================================================================
    // Panel geometry. Everything on the unit's face is placed in a 1000 x 1000 logical space
    // measured from a photo of the real device, and scaled to the window when painted.

    struct Key
    {
        int          id;       // button id sent on press
        int          shiftId;  // id sent while Shift is held, -1 when the key has no shifted function
        const char*  label;
        const char*  legend;   // the shifted function, printed under the key
        float        cx, cy, w, h;
        juce::uint32 colour;
    };

    const Key panelKeys[] = {
        { 0, 4, "A", "E", 667, 516, 56, 36, 0xff3aa0ff },
        { 1, 5, "B", "F", 761, 516, 56, 36, 0xffff3ad0 },
        { 2, 6, "C", "G", 856, 516, 56, 36, 0xff3aa0ff },
        { 3, 7, "D", "H", 951, 516, 56, 36, 0xffff3ad0 },
        { 8, 9, "MAIN", "GRID", 668, 601, 56, 38, 0xff3aa0ff },
        { 10, 11, "BROWSE", "SAVE", 739, 601, 56, 38, 0xffff3ad0 },
        { 12, 13, "TRACK\nMIX", "PAD MIX", 810, 601, 56, 38, 0xffff5a5a },
        { 14, 15, "TRACK\nMUTE", "PAD MUTE", 881, 601, 56, 38, 0xff30e0ff },
        { 16, 17, "NEXT\nSEC", "FX", 951, 601, 56, 38, 0xff30e0ff },
        { 18, 19, "TAP\nTEMPO", "MASTER", 668, 688, 56, 38, 0xffff3ad0 },
        { 22, 23, "MENU", "PREFS", 951, 686, 56, 38, 0xff40ff90 },
        { 24, 25, "NOTE\nREPEAT", "LATCH", 668, 772, 56, 38, 0xff3a80ff },
        { 31, -1, "SHIFT", "", 952, 772, 56, 38, 0xff40ff90 },
        { 32, 33, "STEP\nSEQ", "AUTO", 668, 858, 56, 38, 0xffff3ad0 },
        { 34, 35, "TC", "ON/OFF", 741, 858, 56, 38, 0xffffffff },
        { 36, 37, "SAMPLER", "LOOP", 811, 858, 56, 38, 0xff3a60ff },
        { 38, 39, "SAMPLE\nEDIT", "LINK EDIT", 882, 858, 56, 38, 0xffff3ad0 },
        { 40, 41, "PROG\nEDIT", "MIDI CTRL", 953, 858, 56, 38, 0xff30e0ff },
        { 47, -1, "LINK", "", 82, 408, 70, 28, 0xff8a8f9c },
        { 48, 49, "FULL\nLEVEL", "HALF LVL", 62, 778, 56, 38, 0xffffffff },
        { 50, 51, "16\nLEVELS", "NOTES", 62, 863, 56, 38, 0xffc060ff },
        { 52, -1, "ERASE", "", 62, 950, 56, 30, 0xffff3ad0 },
        // Around the jog wheel
        { 29, -1, "BACK", "", 739, 669, 38, 26, 0xffa0a6b4 },
        { 20, -1, "COPY", "", 881, 669, 38, 26, 0xffa0a6b4 },
        { 27, -1, "-", "", 742, 792, 38, 26, 0xffa0a6b4 },
        { 21, -1, "+", "", 880, 792, 38, 26, 0xffa0a6b4 },
    };

    struct Transport
    {
        int          id;
        float        cx;
        juce::uint32 colour;
    };

    const Transport panelTransport[] = {
        { 42, 669, 0xffd0ffff }, { 43, 740, 0xff9aff6a }, { 44, 812, 0xff30e0ff }, { 45, 884, 0xff60ff90 }, { 46, 954, 0xff40ff90 }
    };
    constexpr float transportY = 942.0f, transportR = 24.0f;

    constexpr int potMaster = 100, potRec = 101;
    struct Pot
    {
        int         knob; // 1..4 are P1..P4 and resolve through the page, 100 and 101 are cosmetic
        const char* label;
        float       cx, cy, r;
    };
    const Pot panelPots[] = {
        { potMaster, "MASTER", 67, 152, 32 }, { potRec, "REC", 67, 276, 32 },
        { 1, "P1", 920, 78, 27 }, { 2, "P2", 920, 177, 27 }, { 3, "P3", 920, 276, 27 }, { 4, "P4", 920, 376, 27 },
    };

    constexpr float padX0 = 140.6f, padY0 = 502.4f, padW = 99.0f, padH = 95.0f, padDx = 121.7f, padDy = 118.4f;
    const juce::Rectangle<float> screenBezel{ 165, 26, 666, 416 };
    const juce::Rectangle<float> screenArea{ 218, 70, 560, 328 };
    const juce::Rectangle<float> stripBox{ 20, 504, 84, 212 };
    const juce::Rectangle<float> jogBox{ 728, 644, 163, 170 };
    const juce::Point<float>     jogCentre{ 810, 730 };
    constexpr float              jogR = 52.0f;
    constexpr int                kickScreenW = 1024, kickScreenH = 600;

    juce::Rectangle<float> keyRect( const Key& k ) { return { k.cx - k.w * 0.5f, k.cy - k.h * 0.5f, k.w, k.h }; }

    //==============================================================================
    /// Receives Kick's window image (see kick_emu FrameStreamer) and keeps the latest frame.
    class ScreenLink : private juce::Thread
    {
    public:
        ScreenLink()
            : juce::Thread( "surface screen" )
        {
            port = juce::SystemStats::getEnvironmentVariable( "KICK_EMU_FRAME_PORT", "50903" ).getIntValue();
            startThread();
        }

        ~ScreenLink() override
        {
            signalThreadShouldExit();
            stopThread( 2000 );
        }

        juce::Image latest() const
        {
            const juce::ScopedLock sl( lock );
            return frame;
        }

        bool isConnected() const { return connected.load(); }
        int  framesReceived() const { return count.load(); }

    private:
        bool readExact( juce::StreamingSocket& s, char* dst, int n )
        {
            int done = 0;
            while( done < n )
            {
                if( threadShouldExit() )
                    return false;
                const int ready = s.waitUntilReady( true, 200 );
                if( ready < 0 )
                    return false;
                if( ready == 0 )
                    continue;
                const int got = s.read( dst + done, n - done, false );
                if( got <= 0 )
                    return false;
                done += got;
            }
            return true;
        }

        void run() override
        {
            while( !threadShouldExit() )
            {
                juce::StreamingSocket s;
                if( !s.connect( "127.0.0.1", port, 500 ) )
                {
                    connected = false;
                    wait( 500 );
                    continue;
                }
                connected = true;
                while( !threadShouldExit() && s.isConnected() )
                {
                    char header[16];
                    if( !readExact( s, header, 16 ) || std::memcmp( header, "KFRM", 4 ) != 0 )
                        break;
                    auto field = [&header]( int i ) {
                        return (int)( (unsigned char)header[4 + i * 4] | ( (unsigned char)header[5 + i * 4] << 8 ) | ( (unsigned char)header[6 + i * 4] << 16 ) | ( (unsigned char)header[7 + i * 4] << 24 ) );
                    };
                    const int w = field( 0 ), h = field( 1 ), stride = field( 2 );
                    if( w < 1 || h < 1 || w > 4096 || h > 4096 || stride != w * 4 )
                        break;
                    juce::Image img( juce::Image::ARGB, w, h, false );
                    {
                        juce::Image::BitmapData bits( img, juce::Image::BitmapData::writeOnly );
                        bool                    ok = true;
                        for( int y = 0; y < h && ok; ++y )
                            ok = readExact( s, (char*)bits.getLinePointer( y ), stride );
                        if( !ok )
                            break;
                    }
                    {
                        const juce::ScopedLock sl( lock );
                        frame = img;
                    }
                    ++count;
                }
                connected = false;
            }
        }

        int                     port{ 50903 };
        mutable juce::CriticalSection lock;
        juce::Image             frame;
        std::atomic<bool>       connected{ false };
        std::atomic<int>        count{ 0 };
    };

    /// Sends touch events to Kick's control socket. "!" lines get no reply, so nothing to drain.
    class KickPointer
    {
    public:
        KickPointer()
            : port( juce::SystemStats::getEnvironmentVariable( "KICK_EMU_CONTROL_PORT", "50901" ).getIntValue() )
        {
        }

        void send( const juce::String& line )
        {
            if( !socket.isConnected() && !socket.connect( "127.0.0.1", port, 150 ) )
                return;
            const auto text = "!" + line + "\n";
            if( socket.write( text.toRawUTF8(), (int)text.getNumBytesAsUTF8() ) < 0 )
                socket.close();
        }

    private:
        int                   port;
        juce::StreamingSocket socket;
    };

    //==============================================================================
    /// Everything the app has told the surface to show. Written on the link's callbacks
    /// (via the message thread) and read by the painter and the script runner.
    struct Model
    {
        struct Pad
        {
            int state{ StateOff }, colour{ colourTrans }, offColour{ colourTrans }, velocity{ 127 };
        };

        mutable juce::CriticalSection lock;
        std::array<Pad, numPads>      pads;
        std::array<int, numButtons>   leds{};
        std::array<float, numCv>      cvVolts{};
        std::array<int, numCv>        cvRaw{};
        int                           bpm{ 0 };
        int                           sliderPercent{ 0 };
        int                           backlight{ 100 };
        bool                          appConnected{ false };
        bool                          appLive{ false };
        juce::String                  power;
    };

    //==============================================================================
    //==============================================================================
    /// Sends the events kick-gesture would send to the app's gesture server (127.0.0.1:50501, or KICK_GESTURE_PORT),
    /// so gestures can be tried with the keyboard or a script and no camera. It speaks the same JUCE
    /// InterprocessConnection framing: a REGISTER message first, then message_type 500 with the event nested under "event".
    class GestureInjector : private juce::InterprocessConnection
    {
    public:
        GestureInjector() : juce::InterprocessConnection( true ) {}
        ~GestureInjector() override { disconnect(); }

        static int port()
        {
            const int p = juce::SystemStats::getEnvironmentVariable( "KICK_GESTURE_PORT", "" ).getIntValue();
            return p > 0 && p < 65536 ? p : 50501;
        }

        /// Names are the event contract's: swipe_left, swipe_right, palm_hold, fist, pinch_start, pinch_change, pinch_end.
        /// `value` is the pinch aperture, 0 to 1, and is sent for the pinch events only.
        bool send( const juce::String& type, float value, float confidence = 0.97f )
        {
            if( !ensureConnected() )
                return false;
            auto* ev = new juce::DynamicObject();
            ev->setProperty( "type", type );
            if( type.startsWith( "pinch_" ) )
                ev->setProperty( "value", (double)value );
            ev->setProperty( "hand", "right" );
            ev->setProperty( "confidence", (double)confidence );
            ev->setProperty( "t_ms", (juce::int64)juce::Time::getMillisecondCounter() );
            auto* msg = new juce::DynamicObject();
            msg->setProperty( "message_id", ++messageId );
            msg->setProperty( "originator", 500 );
            msg->setProperty( "message_type", 500 );
            msg->setProperty( "transaction_type", 30 );
            msg->setProperty( "event", juce::var( ev ) );
            return sendJson( juce::var( msg ) );
        }

    private:
        bool ensureConnected()
        {
            if( isConnected() )
                return true;
            if( !connectToSocket( "127.0.0.1", port(), 500 ) )
                return false;
            auto* reg = new juce::DynamicObject();
            reg->setProperty( "message_type", 1 );
            reg->setProperty( "connection_id", 500 );
            reg->setProperty( "uuid", "" );
            reg->setProperty( "connection_name", "kick-surface-gesture" );
            return sendJson( juce::var( reg ) );
        }

        bool sendJson( const juce::var& v )
        {
            const auto json = juce::JSON::toString( v, true );
            return sendMessage( juce::MemoryBlock( json.toRawUTF8(), json.getNumBytesAsUTF8() ) );
        }

        void connectionMade() override {}
        void connectionLost() override {}
        void messageReceived( const juce::MemoryBlock& ) override {} // the registration reply, nothing to do

        int messageId{ 0 };
    };

    class Surface : public juce::Component
        , private juce::Timer
    {
    public:
        Surface( Link& l, juce::File deviceRoot )
            : link( l )
            , root( deviceRoot )
        {
            setWantsKeyboardFocus( true );
            // The pointer is made here, not inside the lambda: MSVC reads `this` in a nested lambda as const.
            const juce::Component::SafePointer<Surface> self( this );
            link.onFrame = [self]( const Frame& f ) {
                juce::MessageManager::callAsync( [self, f] {
                    if( self != nullptr )
                        self->handleFrame( f );
                } );
            };

            for( auto* s : { &velocity, &pressure } )
            {
                s->setSliderStyle( juce::Slider::LinearHorizontal );
                s->setTextBoxStyle( juce::Slider::TextBoxRight, false, 40, 20 );
                s->setRange( 1, 127, 1 );
                addAndMakeVisible( *s );
            }
            velocity.setValue( 100, juce::dontSendNotification );
            pressure.setValue( 1, juce::dontSendNotification );

            for( auto* t : { &sdToggle, &usbToggle } )
                addAndMakeVisible( *t );
            sdToggle.setButtonText( "SD card inserted" );
            usbToggle.setButtonText( "USB stick inserted" );
            sdToggle.onClick  = [this] { setMedia(); };
            usbToggle.onClick = [this] { setMedia(); };
            setMedia();

            addAndMakeVisible( devButton );
            devButton.onClick = [this] { setDrawerOpen( !drawerOpen ); };
            for( int i = 0; i < 4; ++i )
            {
                auto& b = pageButtons[(size_t)i];
                b.setButtonText( "P" + juce::String( i * 4 + 1 ) + "-" + juce::String( i * 4 + 4 ) );
                b.setRadioGroupId( 7 );
                b.setClickingTogglesState( true );
                b.onClick = [this, i] { pageBase = i * 4; };
                addChildComponent( b );
            }
            pageButtons[0].setToggleState( true, juce::dontSendNotification );
            for( auto* c : drawerComponents() )
                addChildComponent( *c );

            startTimerHz( 30 );
            setSize( 880 + drawerWidth, 880 );
            setDrawerOpen( true );
        }

        ~Surface() override { link.onFrame = nullptr; }

        void setTtyLabel( const juce::String& s ) { ttyLabel = s; }

        /// The LED model as one line of JSON, byte for byte what kick-mcu-sim --dump-leds writes
        /// (Firmware/kick-mcu/sim/led_json.cpp):
        ///   {"buttons":[<53 states>],"pads":[[state,colour,offColour,velocity] * 16],"slider":<percent>}
        juce::String ledModelJson() const
        {
            const juce::ScopedLock sl( model.lock );
            juce::String           out = "{\"buttons\":[";
            for( int i = 0; i < numButtons; ++i )
                out << ( i == 0 ? "" : "," ) << model.leds[(size_t)i];
            out << "],\"pads\":[";
            for( int i = 0; i < numPads; ++i )
            {
                const auto& p = model.pads[(size_t)i];
                out << ( i == 0 ? "" : "," ) << "[" << p.state << "," << p.colour << "," << p.offColour << "," << p.velocity << "]";
            }
            out << "],\"slider\":" << model.sliderPercent << "}\n";
            return out;
        }
        const juce::File& deviceRoot() const { return root; }

        //==============================================================================
        // Input, shared by the mouse, the keyboard and the script runner (message thread).

        void padDown( int pad, int vel )
        {
            if( !juce::isPositiveAndBelow( pad, numPads ) )
                return;
            pressedPads[(size_t)pad] = true;
            link.send( packets::pad( pad, true, vel, (int)pressure.getValue() - 1 ) );
            logLine( "TX pad " + juce::String( pad ) + " down vel " + juce::String( vel ) );
        }

        void padUp( int pad )
        {
            if( !juce::isPositiveAndBelow( pad, numPads ) )
                return;
            pressedPads[(size_t)pad] = false;
            link.send( packets::pad( pad, false, 0, 0 ) );
            logLine( "TX pad " + juce::String( pad ) + " up" );
        }

        void buttonDown( int id )
        {
            if( !juce::isPositiveAndBelow( id, numButtons ) )
                return;
            pressedButtons[(size_t)id] = true;
            link.send( packets::button( id, true ) );
            logLine( "TX button " + juce::String( buttonNames[id] ) + " down" );
        }

        void buttonUp( int id )
        {
            if( !juce::isPositiveAndBelow( id, numButtons ) )
                return;
            pressedButtons[(size_t)id] = false;
            link.send( packets::button( id, false ) );
            logLine( "TX button " + juce::String( buttonNames[id] ) + " up" );
        }

        /// Steps > 0 turn clockwise. knob 0 is the centre encoder, 1..16 the Q-Link knobs.
        void turnKnob( int knob, int steps )
        {
            if( steps == 0 || !juce::isPositiveAndBelow( knob, numKnobs + 1 ) )
                return;
            link.send( packets::knob( knob, steps ) );
            logLine( "TX knob " + juce::String( knob ) + " " + juce::String( steps ) );
        }

        void setStrip( int percent )
        {
            stripValue = juce::jlimit( 0, 100, percent );
            link.send( packets::strip( stripValue ) );
            logLine( "TX strip " + juce::String( stripValue ) );
        }

        void releaseStrip()
        {
            link.send( packets::strip( 0 ) );
        }

        void setMedia()
        {
            auto write = [this]( const juce::String& dev, const juce::String& mountDir, bool present ) {
                const auto devFile = root.getChildFile( "dev/" + dev );
                if( present )
                {
                    root.getChildFile( "dev" ).createDirectory();
                    devFile.create();
                    root.getChildFile( mountDir ).createDirectory();
                }
                else
                    devFile.deleteFile();
            };
            write( "mmcblk1", "mnt/sdcard", sdToggle.getToggleState() );
            write( "sda", "mnt/flash", usbToggle.getToggleState() );

            juce::String mounts;
            if( sdToggle.getToggleState() )
                mounts << "/dev/mmcblk1p1 /mnt/sdcard vfat rw 0 0\n";
            if( usbToggle.getToggleState() )
                mounts << "/dev/sda1 /mnt/flash vfat rw 0 0\n";
            root.getChildFile( "proc" ).createDirectory();
            root.getChildFile( "proc/mounts" ).replaceWithText( mounts );
            logLine( juce::String( "media: SD " ) + ( sdToggle.getToggleState() ? "in" : "out" ) + ", USB " + ( usbToggle.getToggleState() ? "in" : "out" ) );
        }

        void setSd( bool in ) { sdToggle.setToggleState( in, juce::dontSendNotification ); setMedia(); }
        void setUsb( bool in ) { usbToggle.setToggleState( in, juce::dontSendNotification ); setMedia(); }

        void handleEventLine( const juce::String& line )
        {
            auto t = juce::StringArray::fromTokens( line.trim(), " ", "" );
            if( t.isEmpty() )
                return;
            const juce::ScopedLock sl( model.lock );
            if( t[0] == "cv" && t.size() >= 4 )
            {
                const int ch = t[1].getIntValue();
                if( juce::isPositiveAndBelow( ch, numCv ) )
                {
                    model.cvRaw[(size_t)ch]   = t[2].getIntValue();
                    model.cvVolts[(size_t)ch] = t[3].getFloatValue();
                }
            }
            else if( t[0] == "backlight" && t.size() >= 2 )
                model.backlight = t[1].getIntValue();
            else if( t[0] == "power" && t.size() >= 2 )
                model.power = t[1];
            else if( t[0] == "app" && t.size() >= 2 )
            {
                if( t[1] == "stopped" )
                    model.appConnected = model.appLive = false;
            }
        }

        Model model;

        //==============================================================================
        float panelScale() const { return (float)getHeight() / 1000.0f; }
        int   panelSide() const { return getHeight(); }

        //==============================================================================
        bool ledLit( int id, bool phase ) const
        {
            if( !juce::isPositiveAndBelow( id, numButtons ) )
                return false;
            const int state = model.leds[(size_t)id];
            return state == StateOn || state == StateSampleAssigned || ( state == StateOnBlink && phase ) || ( state == StateOffBlink && !phase );
        }

        void paintPanel( juce::Graphics& g, bool phase )
        {
            using juce::Colour;
            // Chassis
            g.setColour( Colour( 0xff1a1b1f ) );
            g.fillRoundedRectangle( 0, 0, 1000, 1000, 14 );
            g.setColour( Colour( 0xff2a2c33 ) );
            g.drawRoundedRectangle( 1, 1, 998, 998, 14, 2.0f );

            // Camera module
            g.setColour( Colour( 0xff0d0e11 ) );
            g.fillRoundedRectangle( 28, 28, 130, 46, 6 );
            g.setColour( Colour( 0xffc9a64a ) );
            g.fillRect( 62.0f, 47.0f, 9.0f, 9.0f );
            g.setColour( Colour( 0xff1c2030 ) );
            g.fillEllipse( 84, 41, 22, 22 );
            g.setColour( Colour( 0xff4a5cc0 ) );
            g.fillEllipse( 91, 48, 8, 8 );

            // Screen bezel and screen
            g.setColour( Colour( 0xff08090b ) );
            g.fillRoundedRectangle( screenBezel, 10 );
            g.setColour( Colour( 0xff25272e ) );
            g.drawRoundedRectangle( screenBezel, 10, 2.0f );
            g.setColour( Colour( 0xffdedee2 ) );
            g.setFont( juce::Font( 24.0f, juce::Font::bold ) );
            g.drawText( "KICK", 400, 402, 200, 34, juce::Justification::centred );

            {
                g.setColour( Colour( 0xff000000 ) );
                g.fillRect( screenArea );
                const auto img = screens.latest();
                if( img.isValid() )
                    g.drawImage( img, screenArea, juce::RectanglePlacement::stretchToFit );
                else
                {
                    g.setColour( Colour( 0xff5a5f6c ) );
                    g.setFont( 15.0f );
                    g.drawFittedText( screens.isConnected() ? "waiting for the first frame" : "waiting for Kick\nrun emu/run.sh", screenArea.toNearestInt(), juce::Justification::centred, 2 );
                }
                int backlight;
                {
                    backlight = model.backlight;
                }
                g.setColour( juce::Colours::black.withAlpha( 0.8f * ( 1.0f - (float)juce::jlimit( 0, 100, backlight ) / 100.0f ) ) );
                g.fillRect( screenArea );
            }

            // Pots
            g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
            for( auto& pot : panelPots )
            {
                const int knob = pot.knob < 100 ? pageBase + pot.knob : pot.knob;
                const int idx  = pot.knob == potMaster ? numKnobs + 1 : pot.knob == potRec ? numKnobs + 2 : knob;
                drawPot( g, pot.cx, pot.cy, pot.r, knobAngle[(size_t)idx] );
                g.setColour( Colour( 0xffdadce2 ) );
                if( pot.knob >= 100 )
                {
                    g.drawText( pot.label, (int)pot.cx - 40, (int)( pot.cy - pot.r - 28 ), 80, 14, juce::Justification::centred );
                    // End-stop dots at 7 and 5 o'clock, as printed on the unit
                    for( const float a : { -2.36f, 2.36f } )
                        g.fillEllipse( pot.cx + std::sin( a ) * ( pot.r + 7 ) - 1.5f, pot.cy - std::cos( a ) * ( pot.r + 7 ) - 1.5f, 3.0f, 3.0f );
                    g.setFont( 9.0f );
                    g.drawText( "MIN", (int)pot.cx - 44, (int)( pot.cy + pot.r + 12 ), 40, 12, juce::Justification::centred );
                    g.drawText( "MAX", (int)pot.cx + 4, (int)( pot.cy + pot.r + 12 ), 40, 12, juce::Justification::centred );
                    g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
                }
                else
                    g.drawText( pot.label, (int)pot.cx - 20, (int)( pot.cy + pot.r + 12 ), 40, 14, juce::Justification::centred );
            }

            // Touch strip with its LED column
            {
                g.setColour( Colour( 0xff0b0c0e ) );
                g.fillRoundedRectangle( stripBox, 6 );
                g.setColour( Colour( 0xff2a2c33 ) );
                g.drawRoundedRectangle( stripBox, 6, 1.5f );
                const int level = model.sliderPercent;
                const int lit   = (int)std::round( (float)level / 100.0f * 12.0f );
                for( int i = 0; i < 12; ++i )
                {
                    const float y   = stripBox.getBottom() - 16.0f - (float)i * ( ( stripBox.getHeight() - 32.0f ) / 11.0f );
                    const bool  on  = i < lit;
                    const float x   = stripBox.getCentreX();
                    if( on )
                    {
                        g.setColour( Colour( 0xffff2a2a ).withAlpha( 0.35f ) );
                        g.fillEllipse( x - 7, y - 7, 14, 14 );
                    }
                    g.setColour( on ? Colour( 0xffff4040 ) : Colour( 0xff2a2c33 ) );
                    g.fillEllipse( x - 3.5f, y - 3.5f, 7, 7 );
                }
                if( stripDrag )
                {
                    const float y = stripBox.getBottom() - stripBox.getHeight() * (float)stripValue / 100.0f;
                    g.setColour( juce::Colours::white.withAlpha( 0.7f ) );
                    g.fillRect( stripBox.getX() + 4, y - 1, stripBox.getWidth() - 8, 2.0f );
                }
            }

            // Pads: dark pad, rim in the colour the app set
            for( int i = 0; i < numPads; ++i )
            {
                const auto& pd = model.pads[(size_t)i];
                const auto  r  = padRect( i );
                Colour      rim;
                bool        lit = true;
                switch( pd.state )
                {
                    case StateOn: rim = padColour( pd.colour ); break;
                    case StateSampleAssigned: rim = padColour( pd.colour ).withMultipliedBrightness( 0.75f ); break;
                    case StateSampleNotAssigned: rim = padColour( pd.colour ).withMultipliedBrightness( 0.35f ); break;
                    case StateOnBlink: rim = padColour( phase ? pd.colour : pd.offColour ); break;
                    case StateOffBlink: rim = padColour( phase ? pd.offColour : pd.colour ); break;
                    default: lit = false; break;
                }
                if( pd.state != StateOff && pd.colour == colourTrans )
                    lit = false;
                if( rim.isTransparent() )
                    lit = false;

                g.setColour( Colour( 0xff08090b ) );
                g.fillRoundedRectangle( r.expanded( 3 ), 6 );
                g.setColour( pressedPads[(size_t)i] ? Colour( 0xff2b2e37 ) : Colour( 0xff131418 ) );
                g.fillRoundedRectangle( r, 4 );
                if( lit )
                {
                    g.setColour( rim.withAlpha( 0.25f ) );
                    g.drawRoundedRectangle( r.reduced( 1 ), 4, 7.0f );
                    g.setColour( rim );
                    g.drawRoundedRectangle( r.reduced( 1.5f ), 4, 3.0f );
                }
                else
                {
                    g.setColour( Colour( 0xff2a2c33 ) );
                    g.drawRoundedRectangle( r.reduced( 1 ), 4, 1.5f );
                }
                g.setColour( Colour( 0xffd8dae0 ) );
                g.setFont( 11.0f );
                g.drawText( juce::String( i + 1 ), (int)r.getX() + 2, (int)r.getBottom() + 5, 30, 13, juce::Justification::left );
            }

            // BANK rule
            g.setColour( Colour( 0xffb0b4c0 ) );
            g.setFont( juce::Font( 12.0f, juce::Font::bold ) );
            g.drawText( "BANK", 770, 478, 70, 16, juce::Justification::centred );
            g.fillRect( 638.0f, 485.0f, 128.0f, 1.0f );
            g.fillRect( 844.0f, 485.0f, 134.0f, 1.0f );

            // Jog wheel
            {
                g.setColour( Colour( 0xff0b0c0e ) );
                g.fillRoundedRectangle( jogBox, 22 );
                g.setColour( Colour( 0xff2a2c33 ) );
                g.drawRoundedRectangle( jogBox, 22, 1.5f );
                const bool live = model.appLive;
                const auto disc = [&]( float rad ) { return juce::Rectangle<float>( rad * 2, rad * 2 ).withCentre( jogCentre ); };

                // Glossy collar the knob sits in
                g.setGradientFill( juce::ColourGradient( Colour( 0xff3a3d44 ), jogCentre.x - jogR, jogCentre.y - jogR, Colour( 0xff050506 ),
                                                         jogCentre.x + jogR * 0.6f, jogCentre.y + jogR, false ) );
                g.fillEllipse( disc( jogR + 12 ) );
                g.setColour( Colour( 0xff050506 ) );
                g.fillEllipse( disc( jogR + 2 ) );

                // Solid LED ring, with a glow while the app is live
                const Colour ring = live ? Colour( 0xffff3344 ) : Colour( 0xff3a1c1f );
                if( live )
                    for( const float w : { 9.0f, 6.0f } )
                    {
                        g.setColour( ring.withAlpha( w > 8 ? 0.12f : 0.25f ) );
                        g.drawEllipse( disc( jogR - 3 ), w );
                    }
                g.setColour( ring );
                g.drawEllipse( disc( jogR - 3 ), 3.0f );

                drawPot( g, jogCentre.x, jogCentre.y, jogR - 9, knobAngle[0], 8, false );
                if( pressedButtons[btnEnter] )
                {
                    g.setColour( juce::Colours::white.withAlpha( 0.2f ) );
                    g.fillEllipse( disc( jogR - 9 ) );
                }
            }

            // Keys
            const bool shiftOn = shiftHeld();
            for( auto& k : panelKeys )
            {
                const auto r      = keyRect( k );
                const bool litP   = ledLit( k.id, phase );
                const bool litS   = k.shiftId >= 0 && ledLit( k.shiftId, phase );
                const bool down   = pressedButtons[(size_t)k.id] || ( k.shiftId >= 0 && pressedButtons[(size_t)k.shiftId] );
                const Colour col( k.colour );
                g.setColour( Colour( 0xff08090b ) );
                g.fillRoundedRectangle( r.expanded( 2 ), 5 );
                g.setColour( down ? Colour( 0xff2f323c ) : Colour( 0xff121317 ) );
                g.fillRoundedRectangle( r, 4 );
                if( litP )
                {
                    g.setColour( col.withAlpha( 0.18f ) );
                    g.fillRoundedRectangle( r, 4 );
                }
                g.setColour( ( k.id == btnShift && shiftOn ) ? col : Colour( 0xff2a2c33 ) );
                g.drawRoundedRectangle( r, 4, k.id == btnShift && shiftOn ? 2.5f : 1.2f );
                g.setColour( litP ? col : col.withMultipliedBrightness( shiftOn && k.shiftId >= 0 ? 0.35f : 0.7f ) );
                g.setFont( juce::Font( k.h < 32 ? 9.5f : 10.5f, juce::Font::bold ) );
                g.drawFittedText( k.label, r.toNearestInt().reduced( 2, 1 ), juce::Justification::centred, 2 );
                if( k.legend[0] != 0 )
                {
                    g.setColour( litS ? col : ( shiftOn ? Colour( 0xffe8eaf0 ) : Colour( 0xffb4b8c4 ) ) );
                    g.setFont( juce::Font( 10.0f, juce::Font::bold ) );
                    g.drawText( k.legend, (int)( k.cx - 40 ), (int)r.getBottom() + 3, 80, 12, juce::Justification::centred );
                }
            }

            // Transport
            for( auto& t : panelTransport )
            {
                const float cx = t.cx, cy = transportY;
                const bool  lit = ledLit( t.id, phase );
                const Colour col( t.colour );
                g.setColour( Colour( 0xff08090b ) );
                g.fillEllipse( cx - transportR - 3, cy - transportR - 3, ( transportR + 3 ) * 2, ( transportR + 3 ) * 2 );
                g.setColour( pressedButtons[(size_t)t.id] ? Colour( 0xff2f323c ) : Colour( 0xff121317 ) );
                g.fillEllipse( cx - transportR, cy - transportR, transportR * 2, transportR * 2 );
                g.setColour( lit ? col.withAlpha( 0.6f ) : Colour( 0xff2a2c33 ) );
                g.drawEllipse( cx - transportR, cy - transportR, transportR * 2, transportR * 2, 1.6f );
                g.setColour( lit ? col : col.withMultipliedBrightness( 0.7f ) );
                switch( t.id )
                {
                    case 42: g.fillEllipse( cx - 9, cy - 9, 18, 18 ); break;
                    case 43: g.fillRoundedRectangle( cx - 4, cy - 11, 8, 15, 4 ); g.drawEllipse( cx - 9, cy - 5, 18, 16, 2.0f ); break;
                    case 44: g.fillRoundedRectangle( cx - 9, cy - 9, 18, 18, 3 ); break;
                    case 45: { juce::Path tri; tri.addTriangle( cx - 7, cy - 10, cx - 7, cy + 10, cx + 11, cy ); g.fillPath( tri ); break; }
                    default: { juce::Path tri; tri.addTriangle( cx - 11, cy - 10, cx - 11, cy + 10, cx + 4, cy ); g.fillPath( tri ); g.fillRect( cx + 6, cy - 10, 4.0f, 20.0f ); break; }
                }
            }

            // LINK button LED strip (cosmetic: the four lamps follow the LINK LED)
            for( int i = 0; i < 4; ++i )
            {
                g.setColour( ledLit( 47, phase ) ? juce::Colour( 0xff3aa0ff ) : juce::Colour( 0xff25272e ) );
                g.fillRect( 38.0f + (float)i * 24.0f, 362.0f, 12.0f, 6.0f );
            }
        }

        void paintDrawer( juce::Graphics& g )
        {
            const int x0 = panelSide() + 12, w = getWidth() - panelSide() - 24;
            g.setColour( juce::Colour( 0xff1f2128 ) );
            g.fillRect( panelSide(), 0, getWidth() - panelSide(), getHeight() );

            int          bpm;
            bool         connected, live;
            juce::String power;
            {
                bpm       = model.bpm;
                connected = model.appConnected;
                live      = model.appLive;
                power     = model.power;
            }
            g.setFont( juce::Font( 16.0f, juce::Font::bold ) );
            g.setColour( juce::Colours::white );
            g.drawText( "KICK Surface", x0, 8, 150, 22, juce::Justification::centredLeft );
            g.setFont( 12.0f );
            g.setColour( connected ? juce::Colours::lightgreen : juce::Colours::orange );
            g.drawText( juce::String( "app: " ) + ( live ? "live" : connected ? "linked" : link.isUp() ? "waiting" : "no link" ), x0, 32, 120, 16, juce::Justification::centredLeft );
            g.setColour( juce::Colours::lightgrey );
            g.drawText( "BPM " + juce::String( bpm ) + ( power.isNotEmpty() ? "   power: " + power : juce::String() ), x0 + 120, 32, w - 120, 16, juce::Justification::centredLeft );
            g.setColour( juce::Colours::grey );
            g.setFont( 10.5f );
            g.drawText( ttyLabel, x0, 50, w, 14, juce::Justification::centredLeft );
            g.drawText( juce::String( "screen: " ) + ( screens.isConnected() ? "streaming" : "no frames" ), x0, 64, w, 14, juce::Justification::centredLeft );

            g.setColour( juce::Colours::lightgrey );
            g.setFont( 11.0f );
            g.drawText( "P1-P4 send knobs. Hold G + 1-7: gestures", x0, 84, w, 14, juce::Justification::centredLeft );
            g.setColour( juce::Colours::grey );
            g.drawText( "velocity", velocity.getX(), velocity.getY() - 14, 80, 14, juce::Justification::left );
            g.drawText( "pad pressure byte", pressure.getX(), pressure.getY() - 14, 140, 14, juce::Justification::left );

            g.setColour( juce::Colours::lightgrey );
            g.setFont( 12.0f );
            g.drawText( "CV outputs (DAC at I2C 0x0C)", x0, 306, w, 16, juce::Justification::left );
            for( int i = 0; i < numCv; ++i )
            {
                const float cw = (float)w / (float)numCv;
                auto        r  = juce::Rectangle<float>( (float)x0 + (float)i * cw, 326.0f, cw - 4.0f, 90.0f );
                g.setColour( juce::Colour( 0xff0c0d10 ) );
                g.fillRoundedRectangle( r, 4 );
                const float frac = juce::jlimit( 0.0f, 1.0f, model.cvVolts[(size_t)i] / 5.0f );
                g.setColour( juce::Colours::deepskyblue );
                g.fillRect( r.reduced( 3 ).removeFromBottom( ( r.getHeight() - 6 ) * frac ) );
                g.setColour( juce::Colours::white );
                g.setFont( 9.5f );
                g.drawText( juce::String( model.cvVolts[(size_t)i], 1 ) + "V", r.toNearestInt().removeFromTop( 13 ), juce::Justification::centred );
                g.setColour( juce::Colours::grey );
                g.drawText( "CV" + juce::String( i + 1 ), r.toNearestInt().removeFromBottom( 13 ), juce::Justification::centred );
            }

            g.setColour( juce::Colours::grey );
            g.setFont( 10.5f );
            g.drawFittedText( "Camera: the module on the panel is a stand-in. The app follows kick-camera over its control socket.", x0, 422, w, 28, juce::Justification::centredLeft, 2 );

            auto r = juce::Rectangle<int>( x0, 456, w, getHeight() - 466 );
            g.setColour( juce::Colour( 0xff0c0d10 ) );
            g.fillRoundedRectangle( r.toFloat(), 6 );
            g.setColour( juce::Colours::lightgrey );
            g.setFont( juce::Font( juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain ) );
            const int fit   = juce::jmax( 1, ( r.getHeight() - 8 ) / 13 );
            const int first = juce::jmax( 0, (int)logLines.size() - fit );
            int       y     = r.getY() + 4;
            for( int i = first; i < (int)logLines.size(); ++i, y += 13 )
                g.drawText( logLines[(size_t)i], r.getX() + 6, y, r.getWidth() - 12, 13, juce::Justification::left );
        }

        void paint( juce::Graphics& g ) override
        {
            g.fillAll( juce::Colour( 0xff101114 ) );
            const auto phase = ( juce::Time::getMillisecondCounter() / 400 ) % 2 == 0;
            {
                const juce::ScopedLock sl( model.lock );
                juce::Graphics::ScopedSaveState save( g );
                g.addTransform( juce::AffineTransform::scale( panelScale() ) );
                paintPanel( g, phase );
            }
            if( drawerOpen )
            {
                const juce::ScopedLock sl( model.lock );
                paintDrawer( g );
            }
        }

        void resized() override
        {
            const int x0 = panelSide() + 12, w = getWidth() - panelSide() - 24;
            const bool show = drawerOpen;
            for( auto* c : drawerComponents() )
                c->setVisible( show );
            devButton.setBounds( getWidth() - 50, 4, 46, 20 );
            for( int i = 0; i < 4; ++i )
                pageButtons[(size_t)i].setBounds( x0 + i * ( w / 4 ), 100, w / 4 - 4, 22 );
            velocity.setBounds( x0, 160, w, 22 );
            pressure.setBounds( x0, 210, w, 22 );
            sdToggle.setBounds( x0, 250, w, 22 );
            usbToggle.setBounds( x0, 276, w, 22 );
        }

        /// Collapses or opens the developer drawer and resizes the window to match.
        void setDrawerOpen( bool open )
        {
            drawerOpen = open;
            devButton.setButtonText( open ? "hide" : "dev" );
            setSize( widthForHeight( getHeight() ), getHeight() );
            resized();
        }

        /// The panel is square and scales with the height; the drawer beside it keeps a fixed width.
        int widthForHeight( int h ) const { return h + ( drawerOpen ? drawerWidth : 0 ); }

        //==============================================================================
        bool shiftHeld() const { return pressedButtons[btnShift]; }

        juce::Point<float> toLogical( juce::Point<float> p ) const { return p / panelScale(); }

        /// Returns the id to send for a key press, switching to the shifted function while Shift is down.
        static int effectiveId( const Key& k, bool shift ) { return shift && k.shiftId >= 0 ? k.shiftId : k.id; }

        void screenPointer( const char* what, juce::Point<float> lp )
        {
            const float x = juce::jlimit( 0.0f, 1.0f, ( lp.x - screenArea.getX() ) / screenArea.getWidth() ) * (float)( kickScreenW - 1 );
            const float y = juce::jlimit( 0.0f, 1.0f, ( lp.y - screenArea.getY() ) / screenArea.getHeight() ) * (float)( kickScreenH - 1 );
            kick.send( juce::String( what ) + " " + juce::String( (int)x ) + " " + juce::String( (int)y ) );
        }

        /// Touch at Kick's own screen coordinates (1024 x 600), used by scripts.
        void touchKick( int x, int y, int holdMs )
        {
            kick.send( "down " + juce::String( x ) + " " + juce::String( y ) );
            juce::Timer::callAfterDelay( juce::jmax( 0, holdMs ), [safe = juce::Component::SafePointer<Surface>( this ), x, y] {
                if( safe != nullptr )
                    safe->kick.send( "up " + juce::String( x ) + " " + juce::String( y ) );
            } );
        }

        void mouseDown( const juce::MouseEvent& e ) override
        {
            grabKeyboardFocus();
            if( e.position.x >= (float)panelSide() )
                return;
            const auto p = toLogical( e.position );

            if( screenArea.contains( p ) )
            {
                screenDrag = true;
                screenPointer( "down", p );
                return;
            }
            for( int i = 0; i < numPads; ++i )
                if( padRect( i ).contains( p ) )
                {
                    activePad = i;
                    padDown( i, (int)velocity.getValue() );
                    return;
                }
            for( auto& k : panelKeys )
                if( keyRect( k ).contains( p ) )
                {
                    if( k.id == btnShift )
                    {
                        // The mouse cannot hold Shift and click a second key, so a click latches it.
                        if( pressedButtons[btnShift] ) buttonUp( btnShift ); else buttonDown( btnShift );
                        shiftLatched = pressedButtons[btnShift];
                        return;
                    }
                    activeButton = effectiveId( k, shiftHeld() );
                    buttonDown( activeButton );
                    return;
                }
            for( auto& t : panelTransport )
                if( p.getDistanceFrom( { t.cx, transportY } ) <= transportR )
                {
                    activeButton = t.id;
                    buttonDown( t.id );
                    return;
                }
            for( auto& pot : panelPots )
                if( p.getDistanceFrom( { pot.cx, pot.cy } ) <= pot.r + 6 )
                {
                    activeKnob = pot.knob < 100 ? pageBase + pot.knob : pot.knob;
                    lastDragY  = p.y;
                    dragAccum  = 0.0f;
                    return;
                }
            if( p.getDistanceFrom( jogCentre ) <= jogR + 12 )
            {
                activeKnob = knobCenter;
                jogMoved   = false;
                lastDragY  = p.y;
                dragAccum  = 0.0f;
                return;
            }
            if( stripBox.contains( p ) )
            {
                stripDrag = true;
                setStrip( stripFromY( p.y ) );
            }
        }

        void mouseDrag( const juce::MouseEvent& e ) override
        {
            const auto p = toLogical( e.position );
            if( screenDrag )
            {
                screenPointer( "move", p );
                return;
            }
            if( activeKnob >= 0 )
            {
                dragAccum += lastDragY - p.y; // up is clockwise
                lastDragY = p.y;
                const int steps = (int)( dragAccum / 4.0f );
                if( steps != 0 )
                {
                    dragAccum -= (float)steps * 4.0f;
                    jogMoved = true;
                    nudgeKnob( activeKnob, steps );
                }
            }
            else if( stripDrag )
                setStrip( stripFromY( p.y ) );
        }

        void mouseUp( const juce::MouseEvent& e ) override
        {
            if( screenDrag )
            {
                screenDrag = false;
                screenPointer( "up", toLogical( e.position ) );
                return;
            }
            if( activePad >= 0 ) padUp( activePad );
            if( activeButton >= 0 ) buttonUp( activeButton );
            if( activeKnob == knobCenter && !jogMoved )
            {
                // A press on the wheel without turning it is the wheel's push switch.
                buttonDown( btnEnter );
                juce::Timer::callAfterDelay( 60, [safe = juce::Component::SafePointer<Surface>( this )] {
                    if( safe != nullptr )
                        safe->buttonUp( btnEnter );
                } );
            }
            if( stripDrag ) releaseStrip();
            activePad = activeButton = -1;
            activeKnob               = -1;
            stripDrag                = false;
        }

        void mouseWheelMove( const juce::MouseEvent& e, const juce::MouseWheelDetails& w ) override
        {
            if( e.position.x >= (float)panelSide() )
                return;
            const auto p    = toLogical( e.position );
            int        knob = knobCenter;
            for( auto& pot : panelPots )
                if( p.getDistanceFrom( { pot.cx, pot.cy } ) <= pot.r + 6 )
                    knob = pot.knob < 100 ? pageBase + pot.knob : pot.knob;
            // Trackpad momentum keeps scrolling after the fingers lift, so a flick would keep turning the knob
            // by an amount that depends on how fast it was. Only the fingers' own movement counts.
            if( w.isInertial )
                return;
            wheelAccum += w.deltaY * ( w.isReversed ? -1.0f : 1.0f ) * 8.0f;
            const int steps = (int)wheelAccum;
            if( steps != 0 )
            {
                wheelAccum -= (float)steps;
                nudgeKnob( knob, steps );
            }
        }

        bool keyPressed( const juce::KeyPress& ) override { return true; }

        bool keyStateChanged( bool ) override
        {
            static const juce::String padKeys = "zxcvasdfqwer1234"; // pad 1 is bottom left, like the hardware

            // Hold G for gesture keys: 1 swipe left, 2 swipe right, 3 open palm, 4 fist, 5 pinch start, 6 pinch change,
            // 7 pinch end. Left and right then move the pinch aperture instead of turning the encoder. The digits
            // 1 to 4 are pad keys, so they are not pads while G is held.
            const bool gestureMode = juce::KeyPress::isKeyCurrentlyDown( 'G' );
            {
                static const char* const gestureNames[7] = { "swipe_left", "swipe_right", "palm_hold", "fist", "pinch_start", "pinch_change", "pinch_end" };
                for( int i = 0; i < 7; ++i )
                {
                    const bool down = gestureMode && juce::KeyPress::isKeyCurrentlyDown( (int)'1' + i );
                    if( down && !gestureKeyHeld[(size_t)i] )
                    {
                        if( i == 4 )
                        {
                            pinching = true;
                            pinchValue = 0.5f;
                        }
                        sendGesture( gestureNames[i], pinchValue );
                        if( i == 6 )
                            pinching = false;
                    }
                    gestureKeyHeld[(size_t)i] = down;
                }
                const bool left  = gestureMode && juce::KeyPress::isKeyCurrentlyDown( juce::KeyPress::leftKey );
                const bool right = gestureMode && juce::KeyPress::isKeyCurrentlyDown( juce::KeyPress::rightKey );
                if( ( left && !gestureLeftHeld ) || ( right && !gestureRightHeld ) )
                {
                    pinchValue = juce::jlimit( 0.f, 1.f, pinchValue + ( right ? 0.1f : -0.1f ) );
                    if( pinching )
                        sendGesture( "pinch_change", pinchValue );
                }
                gestureLeftHeld  = left;
                gestureRightHeld = right;
            }

            for( int i = 0; i < numPads; ++i )
            {
                const bool isDigit = padKeys[i] >= '1' && padKeys[i] <= '4';
                const bool down = !( gestureMode && isDigit ) && juce::KeyPress::isKeyCurrentlyDown( padKeys[i] );
                if( down && !pressedPads[(size_t)i] ) padDown( i, (int)velocity.getValue() );
                if( !down && pressedPads[(size_t)i] && activePad != i ) padUp( i );
            }
            struct Map { int key, button; };
            static const Map maps[] = {
                { juce::KeyPress::spaceKey, btnPlay }, { juce::KeyPress::returnKey, btnEnter },
                { juce::KeyPress::leftKey, btnKnobCcw }, { juce::KeyPress::rightKey, btnKnobCw },
                { juce::KeyPress::escapeKey, btnBack }, { juce::KeyPress::tabKey, btnStop } };
            for( auto& m : maps )
            {
                const bool arrow = m.key == juce::KeyPress::leftKey || m.key == juce::KeyPress::rightKey;
                const bool down = !( gestureMode && arrow ) && juce::KeyPress::isKeyCurrentlyDown( m.key );
                if( down && !pressedButtons[(size_t)m.button] ) buttonDown( m.button );
                if( !down && pressedButtons[(size_t)m.button] && activeButton != m.button ) buttonUp( m.button );
            }
            const bool shift = juce::ModifierKeys::currentModifiers.isShiftDown();
            if( shift && !pressedButtons[btnShift] ) buttonDown( btnShift );
            if( !shift && pressedButtons[btnShift] && activeButton != btnShift ) buttonUp( btnShift );

            static bool upHeld = false, downHeld = false;
            const bool  up = juce::KeyPress::isKeyCurrentlyDown( juce::KeyPress::upKey ), dn = juce::KeyPress::isKeyCurrentlyDown( juce::KeyPress::downKey );
            if( up && !upHeld ) nudgeKnob( knobCenter, 1 );
            if( dn && !downHeld ) nudgeKnob( knobCenter, -1 );
            upHeld = up;
            downHeld = dn;
            return true;
        }

        //==============================================================================
        /// Sends one gesture to the app. `value` is the pinch aperture for the pinch events.
        bool sendGesture( const juce::String& type, float value )
        {
            const bool ok = gestures.send( type, value );
            logLine( juce::String( ok ? "TX gesture " : "gesture not sent (no app gesture server) " ) + type );
            return ok;
        }

        float pinchAperture() const { return pinchValue; }

        juce::String lastLog() const { return logLines.empty() ? juce::String() : logLines.back(); }

        void nudgeKnob( int knob, int steps )
        {
            if( knob == potMaster || knob == potRec )
            {
                // Analog on the device and not reported to the app, so the knob only turns on screen.
                auto& angle = knobAngle[(size_t)( knob == potMaster ? numKnobs + 1 : numKnobs + 2 )];
                angle       = juce::jlimit( -2.356f, 2.356f, angle + (float)steps * 0.1f );
                return;
            }
            if( !juce::isPositiveAndBelow( knob, numKnobs + 1 ) )
                return;
            auto& angle = knobAngle[(size_t)knob];
            // The Q-Link knobs and the centre encoder are endless: the unit sends relative steps and the app owns
            // the value (a velocity of 0-127, a ratchet of 1-8 and so on, depending on the screen), so there is no
            // stop to draw. Only the analog MASTER and REC pots above have one.
            angle = std::fmod( angle + (float)steps * ( knob == knobCenter ? 0.25f : 0.1f ), juce::MathConstants<float>::twoPi );
            turnKnob( knob, steps );
        }

    private:
        void timerCallback() override
        {
            if( link.isUp() )
            {
                // Once the app has opened the line the heartbeat keeps its comm watchdog fed.
                bool live;
                {
                    const juce::ScopedLock sl( model.lock );
                    live = model.appConnected;
                }
                if( live && ++heartbeatTicks >= 90 )
                {
                    heartbeatTicks = 0;
                    link.send( packets::commStatus( 2 ) );
                }
            }
            repaint();
        }

        void logLine( const juce::String& s )
        {
            logLines.push_back( s );
            while( logLines.size() > 40 )
                logLines.pop_front();
        }

        void handleFrame( const Frame& f )
        {
            auto d = [&f]( size_t i ) { return i < f.data.size() ? f.data[i] : 0; };
            juce::StringArray values;
            for( int v : f.data )
                values.add( juce::String( v ) );
            logLine( "RX t" + juce::String( f.type ) + " c" + juce::String( f.cmd ) + " [" + values.joinIntoString( " " ) + "]" );

            const juce::ScopedLock sl( model.lock );
            switch( f.type )
            {
                case PktSystem:
                    if( f.cmd == SysCommInit )
                    {
                        // The app opens with COMM_INIT [CONNECTED = 1]. Our status [1] makes it answer
                        // COMM_INIT [2], and our status [2] makes it answer status [2], which marks the
                        // link live. Answering every COMM_INIT with status [1] would loop forever.
                        model.appConnected = true;
                        heartbeatTicks     = 0;
                        if( d( 0 ) == 1 )
                        {
                            model.appLive = false;
                            link.send( packets::softwareVersion() );
                            link.send( packets::commStatus( 1 ) );
                        }
                        else
                            link.send( packets::commStatus( 2 ) );
                    }
                    else if( f.cmd == SysCheckCommStatus && d( 0 ) == 2 )
                        model.appLive = true;
                    else if( f.cmd == SysBpm )
                        model.bpm = ( d( 0 ) << 8 ) | d( 1 );
                    break;

                case PktLed:
                    if( f.cmd == LedSingle || f.cmd == LedBlink )
                    {
                        if( juce::isPositiveAndBelow( d( 0 ), numButtons ) )
                        {
                            const int state = f.cmd == LedBlink ? StateOnBlink : d( 1 );
                            // INFERRED: the app switches banks, Play/Stop and Full/Half Level on and never
                            // off, so the unit must be clearing the rest of the group. Lighting one member
                            // turns the others off. Firmware/kick-mcu does the same (output/commands.cpp).
                            if( state != StateOff )
                                for( const auto& group : radioGroups )
                                    if( d( 0 ) >= group[0] && d( 0 ) <= group[1] )
                                        for( int other = group[0]; other <= group[1]; ++other )
                                            if( other != d( 0 ) )
                                                model.leds[(size_t)other] = StateOff;
                            model.leds[(size_t)d( 0 )] = state;
                        }
                    }
                    else if( f.cmd == LedAll )
                        model.leds.fill( d( 0 ) );
                    break;

                case PktPad:
                    handlePadFrame( f );
                    break;

                case PktSlider:
                    if( f.cmd == SliderBar || f.cmd == SliderLevel )
                        model.sliderPercent = juce::jlimit( 0, 100, d( 0 ) );
                    break;

                default: break;
            }
        }

        // Caller holds model.lock.
        void handlePadFrame( const Frame& f )
        {
            auto d = [&f]( size_t i ) { return i < f.data.size() ? f.data[i] : 0; };
            auto set = [this]( int pad, int state, int colour, int offColour, int vel ) {
                if( !juce::isPositiveAndBelow( pad, numPads ) )
                    return;
                auto& p     = model.pads[(size_t)pad];
                p.state     = state;
                p.colour    = colour;
                p.offColour = offColour;
                p.velocity  = vel;
            };

            switch( f.cmd )
            {
                case PadSetSingle: set( d( 0 ), d( 1 ), d( 2 ), colourTrans, 127 ); break;
                case PadSetAllSingleColour:
                    for( int i = 0; i < numPads; ++i )
                        set( i, d( 0 ), d( 1 ), colourTrans, 127 );
                    break;
                case PadSetAllMultiColour: // state, then one colour per pad; as many pads as the frame has colours
                    for( int i = 0; i < std::min( numPads, (int)f.data.size() - 1 ); ++i )
                        set( i, d( 0 ), d( 1 + (size_t)i ), colourTrans, 127 );
                    break;
                case PadSetColourState: // N colours, then N states
                    for( int i = 0; i < std::min( numPads, (int)f.data.size() / 2 ); ++i )
                        set( i, d( f.data.size() / 2 + (size_t)i ), d( (size_t)i ), colourTrans, 127 );
                    break;
                case PadSetBlinking: set( d( 0 ), d( 1 ), d( 2 ), d( 3 ), 127 ); break;
                case PadSetStepSingle: set( d( 0 ), d( 1 ), d( 2 ), colourTrans, d( 3 ) ); break;
                case PadSetStepAll: // state, N colours, N velocities
                {
                    const int n = ( (int)f.data.size() - 1 ) / 2;
                    for( int i = 0; i < std::min( numPads, n ); ++i )
                        set( i, d( 0 ), d( 1 + (size_t)i ), colourTrans, d( 1 + (size_t)n + (size_t)i ) );
                    break;
                }
                default: break; // settings, colour mode and aftertouch enable change no LEDs
            }
        }

        //==============================================================================
        /// Pad 1 is bottom left, like the hardware.
        static juce::Rectangle<float> padRect( int i )
        {
            const int col = i % 4, rowFromBottom = i / 4;
            return { padX0 + (float)col * padDx, padY0 + (float)( 3 - rowFromBottom ) * padDy, padW, padH };
        }
        static int stripFromY( float y ) { return (int)std::round( juce::jlimit( 0.0f, 1.0f, ( stripBox.getBottom() - 12.0f - y ) / ( stripBox.getHeight() - 24.0f ) ) * 100.0f ); }

        std::vector<juce::Component*> drawerComponents() { return { &pageButtons[0], &pageButtons[1], &pageButtons[2], &pageButtons[3], &velocity, &pressure, &sdToggle, &usbToggle }; }

        Link&        link;
        juce::File   root;
        juce::String ttyLabel;

        juce::Slider       velocity, pressure;
        juce::ToggleButton sdToggle, usbToggle;

        std::array<bool, numPads>    pressedPads{};
        std::array<bool, numButtons> pressedButtons{};
        std::array<float, numKnobs + 3> knobAngle{}; // 0 jog, 1..16 Q-Link, then MASTER and REC
        int                          activePad{ -1 }, activeButton{ -1 }, activeKnob{ -1 };
        bool                         stripDrag{ false }, screenDrag{ false }, jogMoved{ false }, shiftLatched{ false }, drawerOpen{ true };
        int                          pageBase{ 0 };
        static constexpr int         drawerWidth = 360;
        ScreenLink                   screens;
        KickPointer                  kick;
        juce::TextButton             devButton{ "dev" };
        std::array<juce::TextButton, 4> pageButtons;
        int                          stripValue{ 0 };
        float                        lastDragY{ 0 }, dragAccum{ 0 }, wheelAccum{ 0 };
        int                          heartbeatTicks{ 0 };
        std::deque<juce::String>     logLines;
        GestureInjector              gestures;
        std::array<bool, 7>          gestureKeyHeld{};
        bool                         gestureLeftHeld{ false }, gestureRightHeld{ false };
        bool                         pinching{ false };
        float                        pinchValue{ 0.5f };

    };

    //==============================================================================
    /// Receives the app's UDP events (CV writes, backlight, power) and hands them to the Surface.
    class EventListener : private juce::Thread
    {
    public:
        EventListener( int port, std::function<void( const juce::String& )> handler )
            : juce::Thread( "surface events" )
            , socket( true )
            , onLine( std::move( handler ) )
        {
            socket.setEnablePortReuse( true );
            if( socket.bindToPort( port, "127.0.0.1" ) )
                startThread();
            else
                juce::Logger::writeToLog( "Surface: could not bind event port " + juce::String( port ) );
        }

        ~EventListener() override
        {
            signalThreadShouldExit();
            socket.shutdown();
            stopThread( 1000 );
        }

    private:
        void run() override
        {
            char buf[512];
            while( !threadShouldExit() )
            {
                if( socket.waitUntilReady( true, 200 ) != 1 )
                    continue;
                const int n = socket.read( buf, sizeof( buf ) - 1, false );
                if( n > 0 )
                    onLine( juce::String::fromUTF8( buf, n ) );
            }
        }

        juce::DatagramSocket                     socket;
        std::function<void( const juce::String& )> onLine;
    };

    //==============================================================================
    /// Runs a --script file: one command per line, '#' for comments.
    ///   wait <ms>                      pad <1-16> [velocity] [holdMs]
    ///   button <name|id> [holdMs]      knob <0|1-16> <steps>        slider <0-100>
    ///   sd in|out    usb in|out        snapshot <file.png>
    ///   expect connected               expect pad <1-16> <colourId|off>
    ///   expect led <name> on|off       expect cv <1-8> <minVolts>   quit [code]
    ///   gesture <name> [value]         send a kick-gesture event to the app's gesture server
    ///                                  (swipe_left swipe_right palm_hold fist pinch_start pinch_change pinch_end;
    ///                                  value is the pinch aperture 0-1, default 0.5)
    class ScriptRunner : private juce::Thread
    {
    public:
        ScriptRunner( Surface& s, juce::StringArray scriptLines )
            : juce::Thread( "surface script" )
            , surface( s )
            , lines( std::move( scriptLines ) )
        {
            startThread();
        }

        ~ScriptRunner() override
        {
            *alive = false; // queued callbacks check this on the message thread and become no-ops
            signalThreadShouldExit();
            stopThread( 2000 );
        }

    private:
        /// Runs fn on the message thread and waits. The callback owns its state through a shared
        /// pointer, so a timeout cannot leave it touching destroyed locals. A timeout fails the script.
        void onMain( std::function<void()> fn )
        {
            struct Job
            {
                std::function<void()> fn;
                juce::WaitableEvent   done;
            };
            auto job = std::make_shared<Job>();
            job->fn  = std::move( fn );
            juce::MessageManager::callAsync( [job, alive = alive] {
                if( *alive )
                    job->fn();
                job->done.signal();
            } );
            for( int t = 0; t < 10000; t += 50 )
            {
                if( job->done.wait( 50 ) )
                    return;
                if( threadShouldExit() )
                    return;
            }
            timedOut = true;
        }

        bool waitFor( std::function<bool()> cond, int timeoutMs )
        {
            for( int t = 0; t <= timeoutMs; t += 20 )
            {
                if( cond() )
                    return true;
                juce::Thread::sleep( 20 );
            }
            return false;
        }

        void finish( int code )
        {
            juce::MessageManager::callAsync( [code] {
                if( auto* app = juce::JUCEApplicationBase::getInstance() )
                    app->setApplicationReturnValue( code );
                juce::JUCEApplicationBase::quit();
            } );
        }

        void run() override
        {
            int lineNo = 0;
            for( auto raw : lines )
            {
                ++lineNo;
                if( threadShouldExit() )
                    return;
                const auto line = raw.upToFirstOccurrenceOf( "#", false, false ).trim();
                if( line.isEmpty() )
                    continue;
                auto t = juce::StringArray::fromTokens( line, " ", "\"" );
                t.removeEmptyStrings();
                const auto cmd = t[0].toLowerCase();

                auto fail = [&]( const juce::String& why ) {
                    std::cerr << "script line " << lineNo << ": FAIL " << why << ": " << line << std::endl;
                    finish( 1 );
                };

                if( timedOut.load() )
                {
                    fail( "message thread did not respond within 10 s" );
                    return;
                }

                if( cmd == "wait" )
                    juce::Thread::sleep( t[1].getIntValue() );
                else if( cmd == "pad" )
                {
                    const int pad = t[1].getIntValue() - 1, vel = t.size() > 2 ? t[2].getIntValue() : 100, hold = t.size() > 3 ? t[3].getIntValue() : 80;
                    onMain( [=] { surface.padDown( pad, vel ); } );
                    juce::Thread::sleep( hold );
                    onMain( [=] { surface.padUp( pad ); } );
                }
                else if( cmd == "button" )
                {
                    // "button <name|id> [holdMs]"; names can hold spaces ("Track Mix").
                    const auto rest = line.fromFirstOccurrenceOf( " ", false, false ).trim();
                    int        id   = buttonIdFromName( rest );
                    int        hold = 60;
                    if( id < 0 && rest.contains( " " ) )
                    {
                        id   = buttonIdFromName( rest.upToLastOccurrenceOf( " ", false, false ).trim() );
                        hold = rest.fromLastOccurrenceOf( " ", false, false ).getIntValue();
                    }
                    if( id < 0 )
                        return fail( "unknown button" );
                    onMain( [=] { surface.buttonDown( id ); } );
                    juce::Thread::sleep( hold );
                    onMain( [=] { surface.buttonUp( id ); } );
                }
                else if( cmd == "gesture" )
                {
                    const juce::String name  = t[1];
                    const float        value = t.size() > 2 ? t[2].getFloatValue() : 0.5f;
                    auto               sent  = std::make_shared<std::atomic<bool>>( false );
                    onMain( [=] { *sent = surface.sendGesture( name, value ); } );
                    if( !*sent )
                        return fail( "could not reach the app's gesture server (is Kick running, and KICK_GESTURE_PORT right?)" );
                }
                else if( cmd == "knob" )
                    onMain( [=] { surface.nudgeKnob( t[1].getIntValue(), t[2].getIntValue() ); } );
                else if( cmd == "slider" )
                {
                    onMain( [=] { surface.setStrip( t[1].getIntValue() ); } );
                    juce::Thread::sleep( 60 );
                    onMain( [=] { surface.releaseStrip(); } );
                }
                else if( cmd == "sd" )
                    onMain( [=] { surface.setSd( t[1] == "in" ); } );
                else if( cmd == "usb" )
                    onMain( [=] { surface.setUsb( t[1] == "in" ); } );
                else if( cmd == "snapshot" )
                {
                    // A relative name lands in the device root, so a script does not depend on the cwd or the OS.
                    const auto file = juce::File::isAbsolutePath( t[1] ) ? juce::File( t[1] ) : surface.deviceRoot().getChildFile( t[1] );
                    onMain( [=] {
                        auto img = surface.createComponentSnapshot( surface.getLocalBounds() );
                        file.deleteFile();
                        juce::FileOutputStream out( file );
                        juce::PNGImageFormat().writeImageToStream( img, out );
                    } );
                }
                else if( cmd == "expect" )
                {
                    const auto what = t[1].toLowerCase();
                    if( what == "connected" )
                    {
                        if( !waitFor( [&] { const juce::ScopedLock sl( surface.model.lock ); return surface.model.appLive; }, 60000 ) )
                            return fail( "app never completed the comm handshake" );
                    }
                    else if( what == "pad" )
                    {
                        const int  pad   = t[2].getIntValue() - 1;
                        const bool wantOff = t[3] == "off";
                        const int  colour  = t[3].getIntValue();
                        const bool ok = waitFor( [&] {
                            const juce::ScopedLock sl( surface.model.lock );
                            const auto&            p = surface.model.pads[(size_t)juce::jlimit( 0, numPads - 1, pad )];
                            return wantOff ? ( p.state == StateOff || p.colour == colourTrans ) : ( p.state != StateOff && p.colour == colour );
                        }, 3000 );
                        if( !ok )
                            return fail( "pad LED did not reach the expected state" );
                    }
                    else if( what == "led" )
                    {
                        const auto name = line.fromFirstOccurrenceOf( "led", false, false ).trim();
                        const bool wantOn = name.endsWithIgnoreCase( " on" );
                        const int  id     = buttonIdFromName( name.upToLastOccurrenceOf( " ", false, false ).trim() );
                        if( id < 0 )
                            return fail( "unknown button" );
                        const bool ok = waitFor( [&] {
                            const juce::ScopedLock sl( surface.model.lock );
                            return ( surface.model.leds[(size_t)id] != StateOff ) == wantOn;
                        }, 3000 );
                        if( !ok )
                            return fail( "button LED did not reach the expected state" );
                    }
                    else if( what == "cv" )
                    {
                        const int   ch  = t[2].getIntValue() - 1;
                        const float min = t[3].getFloatValue();
                        const bool  ok  = waitFor( [&] {
                            const juce::ScopedLock sl( surface.model.lock );
                            return surface.model.cvVolts[(size_t)juce::jlimit( 0, numCv - 1, ch )] >= min;
                        }, 3000 );
                        if( !ok )
                            return fail( "CV output did not reach the minimum voltage" );
                    }
                    else
                        return fail( "unknown expect" );
                }
                else if( cmd == "quit" )
                {
                    finish( t.size() > 1 ? t[1].getIntValue() : 0 );
                    return;
                }
                else
                    return fail( "unknown command" );
            }
            finish( 0 );
        }

        Surface&          surface;
        juce::StringArray lines;
        std::atomic<bool> timedOut{ false };
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>( true );
    };

    //==============================================================================
    class SurfaceApplication : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override { return "KICK Surface"; }
        const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
        bool               moreThanOneInstanceAllowed() override { return true; }

        void initialise( const juce::String& commandLine ) override
        {
            auto args = juce::StringArray::fromTokens( commandLine, true );
            auto opt  = [&args]( const juce::String& name ) -> juce::String {
                const int i = args.indexOf( name );
                return i >= 0 && i + 1 < args.size() ? args[i + 1].unquoted() : juce::String();
            };

            auto rootPath = opt( "--root" );
            if( rootPath.isEmpty() )
                rootPath = juce::SystemStats::getEnvironmentVariable( "KICK_EMU_ROOT", {} );
            root = rootPath.isNotEmpty() ? juce::File( rootPath ) : juce::File::getSpecialLocation( juce::File::userHomeDirectory ).getChildFile( "KickEmu" );
            root.getChildFile( "dev" ).createDirectory();

            const auto tracePath = juce::SystemStats::getEnvironmentVariable( "KICK_EMU_TRACE", {} );
            if( tracePath.isNotEmpty() && !link.trace.open( tracePath ) )
                std::cerr << "KickSurface: cannot open KICK_EMU_TRACE file " << tracePath << std::endl;

            dumpLedsPath = opt( "--dump-leds" );
            if( opt( "--replay" ).isNotEmpty() )
            {
                replayTrace( opt( "--replay" ) );
                return;
            }

            juce::String ttyLabel;
            // Windows has no pty, so it always serves the line over TCP.
#if JUCE_WINDOWS
            const int tcpPort = opt( "--tcp" ).isNotEmpty() ? opt( "--tcp" ).getIntValue() : 50910;
#else
            const int tcpPort = opt( "--tcp" ).isNotEmpty() ? opt( "--tcp" ).getIntValue() : 0;
#endif
            if( tcpPort > 0 )
            {
                if( !link.listenTcp( tcpPort ) )
                {
                    std::cerr << "KickSurface: cannot listen on TCP port " << tcpPort << std::endl;
                    setApplicationReturnValue( 2 );
                    quit();
                    return;
                }
                ttyLabel = "serial: tcp 127.0.0.1:" + juce::String( tcpPort );
                std::cout << "KICK_SERIAL_TCP=" << tcpPort << std::endl;
                // A GUI app on Windows has no console, so the launcher reads the address from here.
                root.getChildFile( "surface.port" ).replaceWithText( "tcp://127.0.0.1:" + juce::String( tcpPort ) );
            }
            else
            {
#if !JUCE_WINDOWS
                const auto slave = link.openPty();
                if( slave.isEmpty() )
                {
                    std::cerr << "KickSurface: cannot open a pty" << std::endl;
                    setApplicationReturnValue( 2 );
                    quit();
                    return;
                }
                const auto linkFile = root.getChildFile( "dev/ttymxc0" );
                linkFile.deleteFile();
                juce::File( slave ).createSymbolicLink( linkFile, true ); // link at linkFile, pointing to the slave
                ttyLabel = "serial: " + linkFile.getFullPathName();
                std::cout << "KICK_SERIAL_PORT=" << linkFile.getFullPathName() << std::endl;
                std::cout << "KICK_SERIAL_PTY=" << slave << std::endl;
#endif
            }

            window = std::make_unique<Window>( root, link );
            window->surface().setTtyLabel( ttyLabel );

            auto* s  = &window->surface();
            events   = std::make_unique<EventListener>( juce::SystemStats::getEnvironmentVariable( "KICK_EMU_EVENT_PORT", "50902" ).getIntValue(), [s]( const juce::String& line ) {
                juce::MessageManager::callAsync( [safe = juce::Component::SafePointer<Surface>( s ), line] {
                    if( safe != nullptr )
                        safe->handleEventLine( line );
                } );
            } );

            if( opt( "--script" ).isNotEmpty() )
            {
                juce::File f( juce::File::getCurrentWorkingDirectory().getChildFile( opt( "--script" ) ) );
                if( !f.existsAsFile() )
                {
                    std::cerr << "KickSurface: script not found: " << f.getFullPathName() << std::endl;
                    setApplicationReturnValue( 2 );
                    quit();
                    return;
                }
                juce::StringArray lines;
                f.readLines( lines );
                script = std::make_unique<ScriptRunner>( window->surface(), lines );
            }
        }

        /// --replay: no window and no serial line. The RX lines of the trace go through the same frame
        /// parser and handleFrame() as live traffic, then the LED model is written out and the app quits.
        /// The frames are queued on the message thread by the parser's callback, so the finish is queued
        /// behind them.
        void replayTrace( const juce::String& path )
        {
            const juce::File file( juce::File::getCurrentWorkingDirectory().getChildFile( path ) );
            if( !file.existsAsFile() )
            {
                std::cerr << "KickSurface: trace not found: " << file.getFullPathName() << std::endl;
                setApplicationReturnValue( 2 );
                quit();
                return;
            }
            juce::StringArray lines;
            file.readLines( lines );

            replaySurface = std::make_unique<Surface>( link, root );
            int rxLines   = 0;
            for( const auto& line : lines )
            {
                auto tokens = juce::StringArray::fromTokens( line, " ", "" );
                tokens.removeEmptyStrings();
                if( tokens.size() < 2 || tokens[1] != "RX" )
                    continue;
                std::vector<uint8_t> bytes;
                for( int i = 2; i < tokens.size(); ++i )
                    bytes.push_back( (uint8_t)tokens[i].getHexValue32() );
                link.feedBytes( bytes.data(), bytes.size() );
                ++rxLines;
            }
            if( rxLines == 0 )
            {
                std::cerr << "KickSurface: the trace has no RX lines" << std::endl;
                setApplicationReturnValue( 2 );
                quit();
                return;
            }
            std::cerr << "KickSurface: replayed " << rxLines << " RX lines" << std::endl;
            juce::MessageManager::callAsync( [this] { quit(); });
        }

        bool writeLedDump( const Surface& surface )
        {
            if( dumpLedsPath.isEmpty() )
                return true;
            const auto json = surface.ledModelJson();
            if( dumpLedsPath == "-" )
            {
                std::cout << json << std::flush;
                return true;
            }
            const juce::File out( juce::File::getCurrentWorkingDirectory().getChildFile( dumpLedsPath ) );
            out.deleteFile();
            return out.replaceWithText( json, false, false, "\n" );
        }

        void shutdown() override
        {
            if( replaySurface != nullptr || window != nullptr )
            {
                if( !writeLedDump( replaySurface != nullptr ? *replaySurface : window->surface() ) )
                {
                    std::cerr << "KickSurface: cannot write " << dumpLedsPath << std::endl;
                    setApplicationReturnValue( 2 );
                }
            }
            replaySurface.reset();
            script.reset();
            events.reset();
            window.reset();
        }

        void systemRequestedQuit() override { quit(); }

    private:
        class Window : public juce::DocumentWindow
        {
        public:
            Window( juce::File root, Link& link )
                : juce::DocumentWindow( "KICK Surface", juce::Colour( 0xff15161a ), juce::DocumentWindow::allButtons )
                , content( new Surface( link, root ) )
            {
                setContentOwned( content, true );
                setUsingNativeTitleBar( true );

                // Start at the default size, or smaller when the screen's free area (menu bar and dock
                // excluded) cannot hold it. The title bar is outside the bounds with a native title bar,
                // so leave room for it.
                constrainer.surface = content;
                constrainer.setMinimumSize( content->widthForHeight( minHeight ), minHeight );
                setConstrainer( &constrainer );
                setResizable( true, false );
                int h = content->getHeight();
                if( auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() )
                    h = juce::jlimit( minHeight, h, display->userArea.getHeight() - titleBarAllowance );
                content->setSize( content->widthForHeight( h ), h );
                centreWithSize( content->getWidth(), content->getHeight() );
                setVisible( true );
            }

            ~Window() override { setConstrainer( nullptr ); }

            void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
            Surface& surface() { return *content; }

        private:
            /// Keeps the window at the Surface's shape while it is resized: dragging a side edge sets the
            /// height from the new width, anything else sets the width from the new height.
            struct Constrainer : juce::ComponentBoundsConstrainer
            {
                Surface* surface = nullptr;

                void checkBounds( juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previous, const juce::Rectangle<int>& limits,
                                  bool isStretchingTop, bool isStretchingLeft, bool isStretchingBottom, bool isStretchingRight ) override
                {
                    const int extra = surface->widthForHeight( 0 );
                    if( ( isStretchingLeft || isStretchingRight ) && !isStretchingTop && !isStretchingBottom )
                        bounds.setHeight( bounds.getWidth() - extra );
                    bounds.setHeight( juce::jmax( getMinimumHeight(), bounds.getHeight() ) );
                    if( !limits.isEmpty() )
                        bounds.setHeight( juce::jmin( bounds.getHeight(), limits.getHeight() ) );
                    bounds.setWidth( surface->widthForHeight( bounds.getHeight() ) );
                    if( isStretchingLeft )
                        bounds.setX( previous.getRight() - bounds.getWidth() );
                    if( isStretchingTop )
                        bounds.setY( previous.getBottom() - bounds.getHeight() );
                }
            };

            static constexpr int minHeight         = 560;
            static constexpr int titleBarAllowance = 32;

            Surface*    content;
            Constrainer constrainer;
        };

        juce::File                     root;
        Link                           link;
        std::unique_ptr<Window>        window;
        std::unique_ptr<EventListener> events;
        std::unique_ptr<ScriptRunner>  script;
        std::unique_ptr<Surface>       replaySurface;
        juce::String                   dumpLedsPath;
    };
}

START_JUCE_APPLICATION( SurfaceApplication )
