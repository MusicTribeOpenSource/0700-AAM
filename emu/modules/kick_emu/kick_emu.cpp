/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "kick_emu.h"

#include <map>
#include <mutex>

namespace kick_emu
{
    namespace
    {
        juce::String env( const char* name, const juce::String& fallback )
        {
            auto v = juce::SystemStats::getEnvironmentVariable( name, {} );
            return v.isNotEmpty() ? v : fallback;
        }

        const juce::File& rootDir()
        {
            static const juce::File dir = [] {
                auto r = env( "KICK_EMU_ROOT", juce::File::getSpecialLocation( juce::File::userHomeDirectory ).getChildFile( "KickEmu" ).getFullPathName() );
                juce::File f( juce::File::isAbsolutePath( r ) ? r : juce::File::getCurrentWorkingDirectory().getChildFile( r ).getFullPathName() );
                f.createDirectory();
                return f;
            }();
            return dir;
        }

        void log( const juce::String& s )
        {
            juce::Logger::writeToLog( "[emu] " + s );
        }

        //==============================================================================
        /// UDP event bus to the Surface.
        struct EventBus
        {
            EventBus()
                : socket( false )
                , port( env( "KICK_EMU_EVENT_PORT", "50902" ).getIntValue() )
            {
                socket.bindToPort( 0, "127.0.0.1" );
            }

            void send( const juce::String& line )
            {
                const auto msg = line.trimEnd() + "\n";
                std::lock_guard<std::mutex> g( lock );
                socket.write( "127.0.0.1", port, msg.toRawUTF8(), (int)msg.getNumBytesAsUTF8() );
            }

            juce::DatagramSocket socket;
            int                  port;
            std::mutex           lock;
        };

        EventBus& events()
        {
            static EventBus bus;
            return bus;
        }

        //==============================================================================
        /// 8-channel 12-bit DAC at I2C address 0x0C, as driven by mt::CV
        /// (Kick/modules/hardware/source/kick_I2c.cpp):
        ///   write [0x3n, hi, lo]  -> channel n = 12-bit value, left-justified in hi:lo
        ///   write [0x9n, 0, 0]    -> select channel n for read-back
        ///   read 2 bytes          -> selected channel, left-justified
        ///   read 16 bytes         -> all eight channels, left-justified
        /// Full scale (4095) is 5 V.
        struct VirtualDac
        {
            static constexpr int kAddress = 0x0C;

            bool write( const uint8_t* d, int len )
            {
                if( len < 1 )
                    return false;
                const int cmd = d[0] & 0xF0;
                const int ch  = d[0] & 0x0F;
                std::lock_guard<std::mutex> g( lock );
                if( cmd == 0x30 && ch < 8 && len >= 3 )
                {
                    regs[ch] = (uint16_t)( ( ( d[1] << 8 ) | d[2] ) >> 4 );
                    events().send( "cv " + juce::String( ch ) + " " + juce::String( regs[ch] ) + " " + juce::String( regs[ch] * 5.0f / 4095.0f, 3 ) );
                    return true;
                }
                if( cmd == 0x90 && ch < 8 )
                {
                    readSel = ch;
                    return true;
                }
                return true; // Unknown commands are accepted, like a real bus write.
            }

            bool read( uint8_t* d, int len )
            {
                std::lock_guard<std::mutex> g( lock );
                if( len == 2 )
                {
                    const uint16_t v = (uint16_t)( regs[readSel] << 4 );
                    d[0]             = (uint8_t)( v >> 8 );
                    d[1]             = (uint8_t)( v & 0xFF );
                    return true;
                }
                for( int i = 0; i + 1 < len && i / 2 < 8; i += 2 )
                {
                    const uint16_t v = (uint16_t)( regs[i / 2] << 4 );
                    d[i]             = (uint8_t)( v >> 8 );
                    d[i + 1]         = (uint8_t)( v & 0xFF );
                }
                return true;
            }

            uint16_t   regs[8]{};
            int        readSel{ 0 };
            std::mutex lock;
        };

        VirtualDac& dac()
        {
            static VirtualDac d;
            return d;
        }
    }

    //==============================================================================
    // HAL

    const char* root()
    {
        static const std::string r = rootDir().getFullPathName().toStdString();
        return r.c_str();
    }

    const char* devicePath( const char* linuxPath )
    {
        static std::mutex                                          lock;
        static std::map<std::string, std::unique_ptr<std::string>> cache;

        std::lock_guard<std::mutex> g( lock );
        auto&                       slot = cache[linuxPath != nullptr ? linuxPath : ""];
        if( !slot )
        {
            std::string p = linuxPath != nullptr ? linuxPath : "";
            // Collapse the "//" Kick uses in "/mnt//sdcard".
            for( size_t i; ( i = p.find( "//" ) ) != std::string::npos; )
                p.erase( i, 1 );
            slot = std::make_unique<std::string>( std::string( root() ) + ( p.empty() || p[0] != '/' ? "/" : "" ) + p );
        }
        return slot->c_str();
    }

    bool deviceExists( const char* linuxPath )
    {
        return juce::File( devicePath( linuxPath ) ).exists();
    }

    bool isMounted( const char* id )
    {
        juce::StringArray lines;
        juce::File( devicePath( "/proc/mounts" ) ).readLines( lines );
        for( auto& l : lines )
            if( l.contains( id ) )
                return true;
        return false;
    }

    int shell( const char* command )
    {
        const juce::String cmd( command != nullptr ? command : "" );
        log( "shell (not run): " + cmd );

        if( cmd.contains( "reboot" ) )
            events().send( "power reboot" );
        else if( cmd.contains( "poweroff" ) )
            events().send( "power off" );
        return 0;
    }

    bool i2cWrite( int bus, int address, const uint8_t* data, int length )
    {
        if( bus == 3 && address == VirtualDac::kAddress )
            return dac().write( data, length );
        log( "i2c write to unknown device " + juce::String( bus ) + ":" + juce::String::toHexString( address ) );
        return false;
    }

    bool i2cRead( int bus, int address, uint8_t* data, int length )
    {
        if( bus == 3 && address == VirtualDac::kAddress )
            return dac().read( data, length );
        return false;
    }

    void notify( const char* line )
    {
        events().send( line );
    }

    //==============================================================================
    // Control socket

    namespace
    {
        /// Presses at From, drags to To in Steps moves, holds for HoldMs, then releases.
        /// The events go straight to the component under From instead of through
        /// ComponentPeer::handleMouseEvent. JUCE's macOS peer drops injected events whenever
        /// another window covers Kick, because NSViewComponentPeer::contains() asks the window
        /// server what is on top. That would make headless runs and scripted tests flaky.
        /// Coordinates are in the root component's space.
        juce::String touch( juce::Component& root, juce::Point<float> from, juce::Point<float> to, int steps, int holdMs )
        {
            auto* hit = root.getComponentAt( from.roundToInt() );
            if( hit == nullptr )
                hit = &root;

            juce::Component::SafePointer<juce::Component> target( hit );
            const auto                                    name = juce::String( typeid( *hit ).name() );

            const auto source   = juce::Desktop::getInstance().getMainMouseSource();
            const auto buttons  = juce::ModifierKeys( juce::ModifierKeys::leftButtonModifier );
            const auto downTime = juce::Time::getCurrentTime();

            auto makeEvent = [target, &root, source, from, downTime]( juce::Point<float> rootPos, juce::ModifierKeys mods, bool dragged ) {
                auto* c = target.getComponent();
                return juce::MouseEvent( source, c->getLocalPoint( &root, rootPos ), mods, 1.0f, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation, 0.0f, 0.0f, c, c, juce::Time::getCurrentTime(), c->getLocalPoint( &root, from ), downTime, 1, dragged );
            };

            hit->mouseEnter( makeEvent( from, {}, false ) );
            hit->mouseDown( makeEvent( from, buttons, false ) );

            for( int i = 1; i <= steps && target != nullptr; ++i )
                target->mouseDrag( makeEvent( from + ( to - from ) * ( (float)i / (float)steps ), buttons, true ) );

            const bool dragged = steps > 0;
            juce::Timer::callAfterDelay( juce::jmax( 0, holdMs ), [target, makeEvent, to, buttons, dragged] {
                if( auto* c = target.getComponent() )
                {
                    c->mouseUp( makeEvent( to, buttons, dragged ) );
                    c->mouseExit( makeEvent( to, {}, false ) );
                }
            } );

            return name;
        }
    }

    class Host::ControlServer : private juce::Thread
    {
    public:
        ControlServer()
            : juce::Thread( "kick_emu control" )
        {
            port = env( "KICK_EMU_CONTROL_PORT", "50901" ).getIntValue();
            if( listener.createListener( port, "127.0.0.1" ) )
            {
                log( "control socket on 127.0.0.1:" + juce::String( port ) );
                startThread();
            }
            else
                log( "control socket: could not listen on port " + juce::String( port ) );
        }

        ~ControlServer() override
        {
            signalThreadShouldExit();
            listener.close();
            stopThread( 2000 ); // serve() polls threadShouldExit() every 200 ms
        }

    private:
        void run() override
        {
            while( !threadShouldExit() )
            {
                std::unique_ptr<juce::StreamingSocket> c( listener.waitForNextConnection() );
                if( c == nullptr )
                    continue;
                serve( *c );
            }
        }

        void serve( juce::StreamingSocket& s )
        {
            juce::String pending;
            char         buf[512];
            while( !threadShouldExit() && s.isConnected() )
            {
                const int ready = s.waitUntilReady( true, 200 );
                if( ready < 0 )
                    return;
                if( ready == 0 )
                    continue;
                const int n = s.read( buf, sizeof( buf ), false );
                if( n <= 0 )
                    return;
                pending += juce::String::fromUTF8( buf, n );
                int nl;
                while( ( nl = pending.indexOfChar( '\n' ) ) >= 0 )
                {
                    const auto line = pending.substring( 0, nl ).trim();
                    pending         = pending.substring( nl + 1 );
                    if( line.isNotEmpty() )
                    {
                        const auto reply = handle( line );
                        const auto out   = reply + "\n";
                        s.write( out.toRawUTF8(), (int)out.getNumBytesAsUTF8() );
                        // A browser or other protocol hitting this port sends a non-command first line
                        // (for example "POST / HTTP/1.1"). Drop the connection so nothing after it runs.
                        if( reply.startsWith( "error unknown command" ) )
                            return;
                    }
                }
            }
        }

        /// Runs a command on the message thread and returns the reply.
        juce::String handle( const juce::String& line )
        {
            auto args = juce::StringArray::fromTokens( line, " ", "\"" );
            args.trim();
            args.removeEmptyStrings();
            if( args.isEmpty() )
                return "error empty";

            juce::String       result = "error timeout";
            juce::WaitableEvent done;
            juce::MessageManager::callAsync( [&] {
                result = execute( args );
                done.signal();
            } );
            done.wait( 5000 );
            return result;
        }

        static juce::String execute( const juce::StringArray& a )
        {
            auto* w   = Host::get().getWindow();
            auto  cmd = a[0].toLowerCase();

            if( cmd == "ping" )
                return "ok";

            if( cmd == "quit" )
            {
                juce::JUCEApplicationBase::getInstance()->systemRequestedQuit();
                return "ok";
            }

            if( w == nullptr )
                return "error no window";

            if( cmd == "snapshot" && a.size() >= 2 )
            {
                // Only .png files inside the device root or the system temp dir, so a stray local client
                // cannot overwrite arbitrary user files.
                const juce::File root( kick_emu::root() );
                const juce::File tmp = juce::File::getSpecialLocation( juce::File::tempDirectory );
                const juce::File f   = root.getChildFile( a[1] );
                const bool       ok  = f.hasFileExtension( "png" ) && ( f.isAChildOf( root ) || f.isAChildOf( tmp ) );
                if( !ok )
                    return "error snapshot path must be a .png inside " + root.getFullPathName() + " or " + tmp.getFullPathName();
                f.getParentDirectory().createDirectory();
                auto img = w->createComponentSnapshot( w->getLocalBounds(), true, 1.0f );
                f.deleteFile();
                juce::FileOutputStream out( f );
                juce::PNGImageFormat   png;
                if( !out.openedOk() || !png.writeImageToStream( img, out ) )
                    return "error cannot write " + f.getFullPathName();
                return "ok " + f.getFullPathName();
            }

            if( ( cmd == "tap" && a.size() >= 3 ) || ( cmd == "drag" && a.size() >= 5 ) )
            {
                const juce::Point<float> p1( a[1].getFloatValue(), a[2].getFloatValue() );
                if( cmd == "tap" )
                    return "ok " + touch( *w, p1, p1, 0, a.size() >= 4 ? a[3].getIntValue() : 40 );

                const juce::Point<float> p2( a[3].getFloatValue(), a[4].getFloatValue() );
                return "ok " + touch( *w, p1, p2, 12, a.size() >= 6 ? a[5].getIntValue() : 40 );
            }

            return "error unknown command " + cmd;
        }

        juce::StreamingSocket listener;
        int                   port{ 50901 };
    };

    //==============================================================================
    Host& Host::get()
    {
        static Host h;
        return h;
    }

    void Host::start( juce::Component* Window )
    {
        window = Window;
        log( "device root " + juce::String( root() ) );

        // Skeleton of the unit's filesystem. The SD card and USB stick are absent
        // until the Surface creates dev/mmcblk1, dev/sda and the matching mounts.
        for( auto* dir : { "/home/Kick/projects", "/home/Kick/logs", "/home/Kick/prefs", "/sys/devices/soc0", "/proc", "/dev", "/mnt" } )
            juce::File( devicePath( dir ) ).createDirectory();
        if( !server )
            server = std::make_unique<ControlServer>();
        notify( "app started" );
    }

    void Host::stop()
    {
        notify( "app stopped" );
        server.reset();
    }
}
