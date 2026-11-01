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
 *   KickSurface [--root <dir>] [--tcp <port>] [--script <file>] [--no-window-focus]
 *   The tty path is printed as "KICK_SERIAL_PORT=<path>" and linked at <root>/dev/ttymxc0.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <juce_gui_extra/juce_gui_extra.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>

namespace
{
    //==============================================================================
    // Protocol constants, mirrored from Kick/modules/contracts/dcl/dcl_contracts.h and
    // ComProtocol/PktMgr/PktMgr.hpp. Kept local so the Surface builds without the Kick tree.

    enum PacketType
    {
        PktSystem = 0,
        PktButton,
        PktLed,
        PktPad,
        PktKnobs,
        PktMidi,
        PktCv,
        PktSlider
    };

    enum SysCmd
    {
        SysResetGd = 0,
        SysGetSwVer,
        SysCommInit,
        SysSwUpdate,
        SysCheckCommStatus,
        SysBpm,
        SysReconnect,
        SysShutdown
    };

    enum LedCmd
    {
        LedSingle = 0,
        LedBlink,
        LedAll
    };

    enum PadCmd
    {
        PadState = 0,
        PadSetSingle,
        PadSetAllSingleColour,
        PadSetAllMultiColour,
        PadSettings,
        PadColourMode,
        PadSetColourState,
        PadSetAfterTouch,
        PadSetBlinking,
        PadSetStepSingle,
        PadSetStepAll
    };

    enum SliderCmd
    {
        SliderTouch = 0,
        SliderBar,
        SliderLevel,
        SliderIndividual
    };

    enum PadButtonState
    {
        StateOff = 0,
        StateOn,
        StateSampleAssigned,
        StateSampleNotAssigned,
        StateOnBlink,
        StateOffBlink
    };

    constexpr int numPads    = 16;
    constexpr int numButtons = 53;
    constexpr int numKnobs   = 16;
    constexpr int numCv      = 8;

    constexpr int btnShift      = 31;
    constexpr int btnPlay       = 45;
    constexpr int btnStop       = 44;
    constexpr int btnEnter      = 26;
    constexpr int btnKnobCcw    = 28;
    constexpr int btnKnobCw     = 30;
    constexpr int btnBack       = 29;
    constexpr int colourTrans   = 15;
    constexpr int knobCenter    = 0;
    constexpr int knobCmdRotary = 0;
    constexpr int knobCmdQlink  = 1;

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
    /// The serial line to Kick: a pty by default, or a TCP listener for QEMU's serial chardev.
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

        bool listenTcp( int port )
        {
            listenFd = socket( AF_INET, SOCK_STREAM, 0 );
            int one  = 1;
            setsockopt( listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof( one ) );
            sockaddr_in addr{};
            addr.sin_family      = AF_INET;
            addr.sin_port        = htons( (uint16_t)port );
            addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
            if( bind( listenFd, (sockaddr*)&addr, sizeof( addr ) ) != 0 || listen( listenFd, 1 ) != 0 )
                return false;
            startThread();
            return true;
        }

        bool isUp() const { return dataFd.load() >= 0; }

        void send( int type, int cmd, const std::vector<int>& data )
        {
            std::vector<uint8_t> out{ 'M', 'T', (uint8_t)type, 0 /* request */, (uint8_t)cmd, (uint8_t)( data.size() + 1 ) };
            for( int v : data )
                out.push_back( (uint8_t)v );
            out.push_back( crc8( out ) );
            out.push_back( '\n' ); // trailing byte Kick's reader counts in its frame length

            std::lock_guard<std::mutex> g( writeLock );
            const int                   fd = dataFd.load();
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
        }

    private:
        void closeFds()
        {
            for( int* fd : { &masterFd, &slaveFd, &clientFd, &listenFd } )
                if( *fd >= 0 )
                {
                    ::close( *fd );
                    *fd = -1;
                }
            dataFd = -1;
        }

        void run() override
        {
            while( !threadShouldExit() )
            {
                if( listenFd >= 0 && clientFd < 0 )
                {
                    pollfd p{ listenFd, POLLIN, 0 };
                    if( poll( &p, 1, 200 ) > 0 )
                    {
                        clientFd = accept( listenFd, nullptr, nullptr );
                        if( clientFd >= 0 )
                        {
                            fcntl( clientFd, F_SETFL, fcntl( clientFd, F_GETFL ) | O_NONBLOCK );
                            int one = 1;
                            setsockopt( clientFd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof( one ) );
                            dataFd = clientFd;
                            reset();
                        }
                    }
                    continue;
                }

                const int fd = dataFd.load();
                pollfd    p{ fd, POLLIN, 0 };
                if( poll( &p, 1, 100 ) <= 0 )
                    continue;

                uint8_t buf[512];
                const auto n = ::read( fd, buf, sizeof( buf ) );
                if( n > 0 )
                {
                    for( ssize_t i = 0; i < n; ++i )
                        feed( buf[i] );
                }
                else if( clientFd >= 0 && n == 0 )
                {
                    ::close( clientFd );
                    clientFd = -1;
                    dataFd   = -1;
                }
                else
                    juce::Thread::sleep( 10 );
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
                    if( crc8( raw ) == b && onFrame )
                        onFrame( frame );
                    state = Hdr1;
                    break;
            }
        }

        int              masterFd{ -1 }, slaveFd{ -1 }, listenFd{ -1 }, clientFd{ -1 };
        std::atomic<int> dataFd{ -1 };
        std::mutex       writeLock;

        State                state{ Hdr1 };
        std::vector<uint8_t> raw;
        Frame                frame;
        int                  expected{ 0 };
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
    class Surface : public juce::Component
        , private juce::Timer
    {
    public:
        Surface( Link& l, juce::File deviceRoot )
            : link( l )
            , root( deviceRoot )
        {
            setWantsKeyboardFocus( true );
            link.onFrame = [this]( const Frame& f ) {
                juce::MessageManager::callAsync( [safe = juce::Component::SafePointer<Surface>( this ), f] {
                    if( safe != nullptr )
                        safe->handleFrame( f );
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

            startTimerHz( 30 );
            setSize( 1200, 820 );
        }

        ~Surface() override { link.onFrame = nullptr; }

        void setTtyLabel( const juce::String& s ) { ttyLabel = s; }

        //==============================================================================
        // Input, shared by the mouse, the keyboard and the script runner (message thread).

        void padDown( int pad, int vel )
        {
            if( !juce::isPositiveAndBelow( pad, numPads ) )
                return;
            pressedPads[(size_t)pad] = true;
            link.send( PktPad, PadState, { pad, 1, vel, (int)pressure.getValue() - 1 } );
            logLine( "TX pad " + juce::String( pad ) + " down vel " + juce::String( vel ) );
        }

        void padUp( int pad )
        {
            if( !juce::isPositiveAndBelow( pad, numPads ) )
                return;
            pressedPads[(size_t)pad] = false;
            link.send( PktPad, PadState, { pad, 0, 0, 0 } );
            logLine( "TX pad " + juce::String( pad ) + " up" );
        }

        void buttonDown( int id )
        {
            if( !juce::isPositiveAndBelow( id, numButtons ) )
                return;
            pressedButtons[(size_t)id] = true;
            link.send( PktButton, 0, { id, 1 } );
            logLine( "TX button " + juce::String( buttonNames[id] ) + " down" );
        }

        void buttonUp( int id )
        {
            if( !juce::isPositiveAndBelow( id, numButtons ) )
                return;
            pressedButtons[(size_t)id] = false;
            link.send( PktButton, 0, { id, 0 } );
            logLine( "TX button " + juce::String( buttonNames[id] ) + " up" );
        }

        /// Steps > 0 turn clockwise. knob 0 is the centre encoder, 1..16 the Q-Link knobs.
        void turnKnob( int knob, int steps )
        {
            if( steps == 0 || !juce::isPositiveAndBelow( knob, numKnobs + 1 ) )
                return;
            link.send( PktKnobs, knob == knobCenter ? knobCmdRotary : knobCmdQlink, { knob, steps > 0 ? 1 : 0, std::abs( steps ) } );
            logLine( "TX knob " + juce::String( knob ) + " " + juce::String( steps ) );
        }

        void setStrip( int percent )
        {
            stripValue = juce::jlimit( 0, 100, percent );
            link.send( PktSlider, SliderTouch, { stripValue } );
            logLine( "TX strip " + juce::String( stripValue ) );
        }

        void releaseStrip()
        {
            // The strip reports 0 when the finger lifts, which is how the app ends a touch.
            link.send( PktSlider, SliderTouch, { 0 } );
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
        void paint( juce::Graphics& g ) override
        {
            g.fillAll( juce::Colour( 0xff15161a ) );

            // Header
            g.setColour( juce::Colour( 0xff1f2128 ) );
            g.fillRect( 0, 0, getWidth(), 44 );
            g.setColour( juce::Colours::white );
            g.setFont( juce::Font( 18.0f, juce::Font::bold ) );
            g.drawText( "KICK Surface", 14, 0, 160, 44, juce::Justification::centredLeft );

            int          bpm, backlight;
            bool         connected, live;
            juce::String power;
            {
                const juce::ScopedLock sl( model.lock );
                bpm       = model.bpm;
                backlight = model.backlight;
                connected = model.appConnected;
                live      = model.appLive;
                power     = model.power;
            }
            g.setFont( 13.0f );
            g.setColour( connected ? juce::Colours::lightgreen : juce::Colours::orange );
            g.drawText( juce::String( "app: " ) + ( live ? "live" : connected ? "linked" : link.isUp() ? "waiting" : "no link" ), 180, 0, 110, 44, juce::Justification::centredLeft );
            g.setColour( juce::Colours::lightgrey );
            g.drawText( "BPM " + juce::String( bpm ) + "    backlight " + juce::String( backlight ) + "%" + ( power.isNotEmpty() ? "    power: " + power : juce::String() ), 300, 0, 400, 44, juce::Justification::centredLeft );
            g.setColour( juce::Colours::grey );
            g.drawText( ttyLabel, 700, 0, getWidth() - 710, 44, juce::Justification::centredRight );

            const auto phase = ( juce::Time::getMillisecondCounter() / 400 ) % 2 == 0;

            // Q-Link knobs
            for( int i = 0; i < numKnobs; ++i )
            {
                auto r = knobRect( i ).reduced( 6 );
                g.setColour( juce::Colour( 0xff2a2d36 ) );
                g.fillEllipse( r.withSizeKeepingCentre( 44, 44 ).translated( 0, -6 ) );
                auto c = r.getCentre().translated( 0, -6 );
                const float a = knobAngle[(size_t)( i + 1 )]; // index 0 is the centre encoder
                g.setColour( juce::Colours::orange );
                g.drawLine( c.x, c.y, c.x + std::sin( a ) * 18.0f, c.y - std::cos( a ) * 18.0f, 3.0f );
                g.setColour( juce::Colours::grey );
                g.setFont( 11.0f );
                g.drawText( "Q" + juce::String( i + 1 ), r.removeFromBottom( 14 ).toNearestInt(), juce::Justification::centred );
            }

            // Buttons
            {
                const juce::ScopedLock sl( model.lock );
                for( int i = 0; i < numButtons; ++i )
                {
                    auto       r     = buttonRect( i );
                    const int  state = model.leds[(size_t)i];
                    const bool lit   = state == StateOn || state == StateSampleAssigned || ( state == StateOnBlink && phase ) || ( state == StateOffBlink && !phase );
                    g.setColour( pressedButtons[(size_t)i] ? juce::Colour( 0xff5a5f70 ) : juce::Colour( 0xff2a2d36 ) );
                    g.fillRoundedRectangle( r, 5.0f );
                    g.setColour( lit ? juce::Colour( 0xffffb020 ) : juce::Colour( 0xff3a3e4a ) );
                    g.fillRoundedRectangle( r.removeFromTop( 5.0f ).reduced( 10, 0 ), 2.0f );
                    g.setColour( lit ? juce::Colours::white : juce::Colours::lightgrey );
                    g.setFont( 12.0f );
                    g.drawText( buttonNames[i], buttonRect( i ).toNearestInt().withTrimmedTop( 6 ), juce::Justification::centred );
                }
            }

            // Pads
            {
                const juce::ScopedLock sl( model.lock );
                for( int i = 0; i < numPads; ++i )
                {
                    const auto& p = model.pads[(size_t)i];
                    auto        r = padRect( i );
                    juce::Colour fill( 0xff23252d );
                    switch( p.state )
                    {
                        case StateOn: fill = padColour( p.colour ); break;
                        case StateSampleAssigned: fill = padColour( p.colour ).withMultipliedBrightness( 0.7f ); break;
                        case StateSampleNotAssigned: fill = padColour( p.colour ).withMultipliedBrightness( 0.3f ); break;
                        case StateOnBlink: fill = padColour( phase ? p.colour : p.offColour ); break;
                        case StateOffBlink: fill = padColour( phase ? p.offColour : p.colour ); break;
                        default: break;
                    }
                    if( p.state != StateOff && p.colour == colourTrans )
                        fill = juce::Colour( 0xff23252d );
                    g.setColour( juce::Colour( 0xff0c0d10 ) );
                    g.fillRoundedRectangle( r, 8.0f );
                    g.setColour( fill.isTransparent() ? juce::Colour( 0xff23252d ) : fill );
                    g.fillRoundedRectangle( r.reduced( 3 ), 6.0f );
                    if( pressedPads[(size_t)i] )
                    {
                        g.setColour( juce::Colours::white.withAlpha( 0.35f ) );
                        g.fillRoundedRectangle( r.reduced( 3 ), 6.0f );
                    }
                    g.setColour( juce::Colours::white.withAlpha( 0.5f ) );
                    g.setFont( 11.0f );
                    g.drawText( juce::String( i + 1 ), r.toNearestInt().reduced( 8, 5 ), juce::Justification::topLeft );
                }
            }

            // Touch strip
            {
                int level;
                {
                    const juce::ScopedLock sl( model.lock );
                    level = model.sliderPercent;
                }
                auto r = stripRect();
                g.setColour( juce::Colour( 0xff0c0d10 ) );
                g.fillRoundedRectangle( r, 6.0f );
                g.setColour( juce::Colours::orange.withAlpha( 0.8f ) );
                g.fillRoundedRectangle( r.reduced( 3 ).withWidth( ( r.getWidth() - 6 ) * (float)level / 100.0f ), 4.0f );
                g.setColour( juce::Colours::white );
                const float x = r.getX() + r.getWidth() * (float)stripValue / 100.0f;
                g.fillRect( x - 1.0f, r.getY() + 2, 3.0f, r.getHeight() - 4 );
                g.setColour( juce::Colours::grey );
                g.setFont( 11.0f );
                g.drawText( "touch strip", r.toNearestInt(), juce::Justification::centredRight );
            }

            // Centre encoder
            {
                auto r = encoderRect();
                g.setColour( juce::Colour( 0xff2a2d36 ) );
                g.fillEllipse( r );
                g.setColour( juce::Colour( 0xff3a3e4a ) );
                g.fillEllipse( r.reduced( 12 ) );
                auto  c = r.getCentre();
                const float a = knobAngle[0];
                g.setColour( juce::Colours::orange );
                g.drawLine( c.x, c.y, c.x + std::sin( a ) * r.getWidth() * 0.32f, c.y - std::cos( a ) * r.getWidth() * 0.32f, 4.0f );
                g.setColour( juce::Colours::grey );
                g.setFont( 11.0f );
                g.drawText( "encoder (drag or scroll)", (int)r.getX() - 20, (int)r.getBottom() + 2, 140, 14, juce::Justification::centred );
            }

            g.setColour( juce::Colours::grey );
            g.setFont( 11.0f );
            g.drawText( "velocity", velocity.getX(), velocity.getY() - 14, 80, 14, juce::Justification::left );
            g.drawText( "pad pressure byte", pressure.getX(), pressure.getY() - 14, 140, 14, juce::Justification::left );

            // CV outputs
            g.setColour( juce::Colours::lightgrey );
            g.setFont( 13.0f );
            g.drawText( "CV outputs (DAC at I2C 0x0C)", 10, 574, 300, 16, juce::Justification::left );
            {
                const juce::ScopedLock sl( model.lock );
                for( int i = 0; i < numCv; ++i )
                {
                    auto r = cvRect( i );
                    g.setColour( juce::Colour( 0xff0c0d10 ) );
                    g.fillRoundedRectangle( r, 4.0f );
                    const float frac = juce::jlimit( 0.0f, 1.0f, model.cvVolts[(size_t)i] / 5.0f );
                    g.setColour( juce::Colours::deepskyblue );
                    g.fillRect( r.reduced( 3 ).removeFromBottom( ( r.getHeight() - 6 ) * frac ) );
                    g.setColour( juce::Colours::white );
                    g.setFont( 11.0f );
                    g.drawText( juce::String( model.cvVolts[(size_t)i], 2 ) + " V", r.toNearestInt().removeFromTop( 14 ), juce::Justification::centred );
                    g.setColour( juce::Colours::grey );
                    g.drawText( "CV" + juce::String( i + 1 ), r.toNearestInt().removeFromBottom( 14 ), juce::Justification::centred );
                }
            }

            // Camera stand-in
            {
                auto r = cameraRect();
                juce::ColourGradient grad( juce::Colour( 0xff203050 ), r.getTopLeft(), juce::Colour( 0xff503020 ), r.getBottomRight(), false );
                g.setGradientFill( grad );
                g.fillRoundedRectangle( r, 6.0f );
                g.setColour( juce::Colours::white.withAlpha( 0.8f ) );
                g.setFont( 12.0f );
                g.drawFittedText( "Camera (OV5640)\nstand-in test pattern\nKick has no camera code yet", r.toNearestInt().reduced( 6 ), juce::Justification::centred, 3 );
            }

            // Packet log: newest line at the bottom, as many lines as fit
            {
                auto r = juce::Rectangle<int>( 790, 646, getWidth() - 800, getHeight() - 656 );
                g.setColour( juce::Colour( 0xff0c0d10 ) );
                g.fillRoundedRectangle( r.toFloat(), 6.0f );
                g.setColour( juce::Colours::lightgrey );
                g.setFont( juce::Font( juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain ) );
                const int fit   = juce::jmax( 1, ( r.getHeight() - 8 ) / 13 );
                const int first = juce::jmax( 0, (int)logLines.size() - fit );
                int       y     = r.getY() + 4;
                for( int i = first; i < (int)logLines.size(); ++i, y += 13 )
                    g.drawText( logLines[(size_t)i], r.getX() + 6, y, r.getWidth() - 12, 13, juce::Justification::left );
            }
        }

        void resized() override
        {
            velocity.setBounds( 930, 520, 260, 22 );
            pressure.setBounds( 930, 570, 260, 22 );
            sdToggle.setBounds( 10, 700, 200, 24 );
            usbToggle.setBounds( 10, 730, 200, 24 );
        }

        void mouseDown( const juce::MouseEvent& e ) override
        {
            grabKeyboardFocus();
            const auto p = e.position;
            for( int i = 0; i < numPads; ++i )
                if( padRect( i ).contains( p ) )
                {
                    activePad = i;
                    padDown( i, (int)velocity.getValue() );
                    return;
                }
            for( int i = 0; i < numButtons; ++i )
                if( buttonRect( i ).contains( p ) )
                {
                    activeButton = i;
                    buttonDown( i );
                    return;
                }
            for( int i = 0; i < numKnobs; ++i )
                if( knobRect( i ).contains( p ) )
                {
                    activeKnob = i + 1;
                    lastDragY  = p.y;
                    dragAccum  = 0.0f;
                    return;
                }
            if( encoderRect().contains( p ) )
            {
                activeKnob = knobCenter;
                lastDragY  = p.y;
                dragAccum  = 0.0f;
                return;
            }
            if( stripRect().contains( p ) )
            {
                stripDrag = true;
                setStrip( stripFromX( p.x ) );
            }
        }

        void mouseDrag( const juce::MouseEvent& e ) override
        {
            const auto p = e.position;
            if( activeKnob >= 0 )
            {
                dragAccum += lastDragY - p.y; // up is clockwise
                lastDragY = p.y;
                const int steps = (int)( dragAccum / 5.0f );
                if( steps != 0 )
                {
                    dragAccum -= (float)steps * 5.0f;
                    nudgeKnob( activeKnob, steps );
                }
            }
            else if( stripDrag )
                setStrip( stripFromX( p.x ) );
        }

        void mouseUp( const juce::MouseEvent& ) override
        {
            if( activePad >= 0 ) padUp( activePad );
            if( activeButton >= 0 ) buttonUp( activeButton );
            if( stripDrag ) releaseStrip();
            activePad = activeButton = -1;
            activeKnob               = -1;
            stripDrag                = false;
        }

        void mouseWheelMove( const juce::MouseEvent& e, const juce::MouseWheelDetails& w ) override
        {
            int knob = knobCenter;
            for( int i = 0; i < numKnobs; ++i )
                if( knobRect( i ).contains( e.position ) )
                    knob = i + 1;
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
            for( int i = 0; i < numPads; ++i )
            {
                const bool down = juce::KeyPress::isKeyCurrentlyDown( padKeys[i] );
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
                const bool down = juce::KeyPress::isKeyCurrentlyDown( m.key );
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
        juce::String lastLog() const { return logLines.empty() ? juce::String() : logLines.back(); }

        void nudgeKnob( int knob, int steps )
        {
            if( !juce::isPositiveAndBelow( knob, numKnobs + 1 ) )
                return;
            auto& angle = knobAngle[(size_t)knob];
            angle += (float)steps * ( knob == knobCenter ? 0.25f : 0.1f );
            // Q-Link knobs are endless encoders but read like pots on screen: stop at +-135 degrees.
            // The centre encoder is a free-spinning dial.
            if( knob != knobCenter )
                angle = juce::jlimit( -2.356f, 2.356f, angle );
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
                    link.send( PktSystem, SysCheckCommStatus, { 2 } );
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
                            link.send( PktSystem, SysGetSwVer, { 1, 0 } );
                            link.send( PktSystem, SysCheckCommStatus, { 1 } );
                        }
                        else
                            link.send( PktSystem, SysCheckCommStatus, { 2 } );
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
                            model.leds[(size_t)d( 0 )] = f.cmd == LedBlink ? StateOnBlink : d( 1 );
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
                case PadSetAllMultiColour:
                    for( int i = 0; i < numPads; ++i )
                        set( i, d( 0 ), d( 1 + (size_t)i ), colourTrans, 127 );
                    break;
                case PadSetColourState: // 16 colours, then 16 states
                    for( int i = 0; i < numPads; ++i )
                        set( i, d( numPads + (size_t)i ), d( (size_t)i ), colourTrans, 127 );
                    break;
                case PadSetBlinking: set( d( 0 ), d( 1 ), d( 2 ), d( 3 ), 127 ); break;
                case PadSetStepSingle: set( d( 0 ), d( 1 ), d( 2 ), colourTrans, d( 3 ) ); break;
                case PadSetStepAll: // state, 16 colours, 16 velocities
                    for( int i = 0; i < numPads; ++i )
                        set( i, d( 0 ), d( 1 + (size_t)i ), colourTrans, d( 1 + numPads + (size_t)i ) );
                    break;
                default: break; // settings, colour mode and aftertouch enable change no LEDs
            }
        }

        //==============================================================================
        juce::Rectangle<float> padRect( int i ) const
        {
            const int col = i % 4, rowFromBottom = i / 4;
            return { 790.0f + (float)col * 100.0f, 60.0f + (float)( 3 - rowFromBottom ) * 100.0f, 92.0f, 92.0f };
        }
        juce::Rectangle<float> buttonRect( int i ) const { return { 10.0f + (float)( i % 8 ) * 96.0f, 244.0f + (float)( i / 8 ) * 46.0f, 92.0f, 42.0f }; }
        juce::Rectangle<float> knobRect( int i ) const { return { 10.0f + (float)( i % 8 ) * 96.0f, 54.0f + (float)( i / 8 ) * 92.0f, 92.0f, 88.0f }; }
        juce::Rectangle<float> stripRect() const { return { 790.0f, 470.0f, 392.0f, 38.0f }; }
        juce::Rectangle<float> encoderRect() const { return { 800.0f, 524.0f, 100.0f, 100.0f }; }
        juce::Rectangle<float> cvRect( int i ) const { return { 10.0f + (float)i * 72.0f, 594.0f, 66.0f, 96.0f }; }
        juce::Rectangle<float> cameraRect() const { return { 612.0f, 594.0f, 166.0f, 96.0f }; }
        int                    stripFromX( float x ) const { return (int)std::round( juce::jlimit( 0.0f, 1.0f, ( x - stripRect().getX() ) / stripRect().getWidth() ) * 100.0f ); }

        Link&        link;
        juce::File   root;
        juce::String ttyLabel;

        juce::Slider       velocity, pressure;
        juce::ToggleButton sdToggle, usbToggle;

        std::array<bool, numPads>    pressedPads{};
        std::array<bool, numButtons> pressedButtons{};
        std::array<float, numKnobs + 1> knobAngle{};
        int                          activePad{ -1 }, activeButton{ -1 }, activeKnob{ -1 };
        bool                         stripDrag{ false };
        int                          stripValue{ 0 };
        float                        lastDragY{ 0 }, dragAccum{ 0 }, wheelAccum{ 0 };
        int                          heartbeatTicks{ 0 };
        std::deque<juce::String>     logLines;

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

        ~ScriptRunner() override { stopThread( 2000 ); }

    private:
        void onMain( std::function<void()> fn )
        {
            juce::WaitableEvent done;
            juce::MessageManager::callAsync( [&] {
                fn();
                done.signal();
            } );
            done.wait( 10000 );
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

                if( cmd == "wait" )
                    juce::Thread::sleep( t[1].getIntValue() );
                else if( cmd == "pad" )
                {
                    const int pad = t[1].getIntValue() - 1, vel = t.size() > 2 ? t[2].getIntValue() : 100, hold = t.size() > 3 ? t[3].getIntValue() : 80;
                    onMain( [&] { surface.padDown( pad, vel ); } );
                    juce::Thread::sleep( hold );
                    onMain( [&] { surface.padUp( pad ); } );
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
                    onMain( [&] { surface.buttonDown( id ); } );
                    juce::Thread::sleep( hold );
                    onMain( [&] { surface.buttonUp( id ); } );
                }
                else if( cmd == "knob" )
                    onMain( [&] { surface.nudgeKnob( t[1].getIntValue(), t[2].getIntValue() ); } );
                else if( cmd == "slider" )
                {
                    onMain( [&] { surface.setStrip( t[1].getIntValue() ); } );
                    juce::Thread::sleep( 60 );
                    onMain( [&] { surface.releaseStrip(); } );
                }
                else if( cmd == "sd" )
                    onMain( [&] { surface.setSd( t[1] == "in" ); } );
                else if( cmd == "usb" )
                    onMain( [&] { surface.setUsb( t[1] == "in" ); } );
                else if( cmd == "snapshot" )
                {
                    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile( t[1] );
                    onMain( [&] {
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
                        if( !waitFor( [&] { const juce::ScopedLock sl( surface.model.lock ); return surface.model.appLive; }, 15000 ) )
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

            juce::String ttyLabel;
            if( opt( "--tcp" ).isNotEmpty() )
            {
                const int port = opt( "--tcp" ).getIntValue();
                if( !link.listenTcp( port ) )
                {
                    std::cerr << "KickSurface: cannot listen on TCP port " << port << std::endl;
                    setApplicationReturnValue( 2 );
                    quit();
                    return;
                }
                ttyLabel = "serial: tcp 127.0.0.1:" + juce::String( port );
                std::cout << "KICK_SERIAL_TCP=" << port << std::endl;
            }
            else
            {
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

        void shutdown() override
        {
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
                centreWithSize( content->getWidth(), content->getHeight() );
                setVisible( true );
            }

            void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
            Surface& surface() { return *content; }

        private:
            Surface* content;
        };

        juce::File                     root;
        Link                           link;
        std::unique_ptr<Window>        window;
        std::unique_ptr<EventListener> events;
        std::unique_ptr<ScriptRunner>  script;
    };
}

START_JUCE_APPLICATION( SurfaceApplication )
