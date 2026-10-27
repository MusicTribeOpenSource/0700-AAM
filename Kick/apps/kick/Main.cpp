/*
 * Copyright (c) 2022 MUSIC Tribe
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

#include "kick_ui/kick_ui.h"
#include "audio_engine/audio_engine.h"
#include "kick_device_adapter.h"
#include "kick_audio_engine_adapter.h"
#include "kick_audio_interface.h"
#if KICK_APP_SMOKE_TEST
 #include "../../tests/platform/AppSmoke.h"
#endif

#ifdef PERFETTO
    #include "melatonin_perfetto/melatonin_perfetto.h"
#endif

#define ENABLE_X11_FULLSCREEN ( 0 )

#if defined( ENABLE_X11_FULLSCREEN ) && ( 0 < ENABLE_X11_FULLSCREEN )
    #if( JUCE_LINUX || __linux__ )
namespace xlib
{
    extern "C"
    {
        #include <X11/Xlib.h>
        #include <X11/Xatom.h>

        void maximizeWindow( Window win )
        {
            auto display = XOpenDisplay( NULL );

            XEvent ev;
            ev.xclient.window       = win;
            ev.xclient.type         = ClientMessage;
            ev.xclient.format       = 32;
            ev.xclient.message_type = XInternAtom( display, "_NET_WM_STATE", False );
            ev.xclient.data.l[0]    = 1;
            ev.xclient.data.l[1]    = XInternAtom( display, "_NET_WM_STATE_MAXIMIZED_HORZ", False );
            ev.xclient.data.l[2]    = XInternAtom( display, "_NET_WM_STATE_MAXIMIZED_VERT", False );
            ev.xclient.data.l[3]    = 1;

            XSendEvent( display, DefaultRootWindow( display ), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev );
            XCloseDisplay( display );
        }

        void fullscreenWindow( Window win )
        {
            auto display = XOpenDisplay( NULL );

        #if 1
            XEvent ev;
            ev.xclient.window       = win;
            ev.xclient.type         = ClientMessage;
            ev.xclient.format       = 32;
            ev.xclient.message_type = XInternAtom( display, "_NET_WM_STATE", False );
            ev.xclient.data.l[0]    = 2; // _NET_WM_STATE_TOGGLE
            ev.xclient.data.l[1]    = XInternAtom( display, "_NET_WM_STATE_FULLSCREEN", True );
            ;
            ev.xclient.data.l[2] = 0; // No second property to toggle
            ev.xclient.data.l[3] = 1;

            XSendEvent( display, DefaultRootWindow( display ), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev );
        #else
            Atom wm_state      = XInternAtom( display, "_NET_WM_STATE", true );
            Atom wm_fullscreen = XInternAtom( display, "_NET_WM_STATE_FULLSCREEN", true );

            XChangeProperty( display, win, wm_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)&wm_fullscreen, 1 );
        #endif

            XCloseDisplay( display );
        }
    }
}
    #endif
#endif

class TestWindow : public juce::Component
{
public:
    TestWindow()
    {
        setSize( ui::layout::getAppWidth(), ui::layout::getAppHeight() );
    }
    ~TestWindow() {}

private:
    void paint( juce::Graphics& g )
    {
        g.fillAll( juce::Colours::coral );
    }
};

//==============================================================================
class AudioEngineApplication : public juce::JUCEApplication
{
public:
    //==============================================================================
    const juce::String getApplicationName() override
    {
        return "Kick";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.0.1";
    }

    bool moreThanOneInstanceAllowed() override
    {
        return false;
    }

    //==============================================================================
    void initialise( const juce::String& commandLine ) override
    {
        audioInterface.reset(new KickAudioInterface());
        engineAdapter.reset(new KickAudioEngineAdapter(audioInterface));
        
        adapter.reset( new KickDeviceAdapter() );
        // This method is where you should put your application's initialisation code ...
        const bool smoke = commandLine.contains("--smoke-test");
        if (!smoke) juce::Desktop::getInstance().getMainMouseSource().enableUnboundedMouseMovement(true, true);

        // Exit kiosk mode ...
        juce::Desktop::getInstance().setKioskModeComponent( nullptr );

        // Initialize Settings ...
        mt::getSettings();

        // Initialize Audio engine and interface ...
        mt::getAudioInterface();

        // Initialize User Interface...
        w = std::make_unique<HomeWindow>( getApplicationName() );

        // Title bar ...
        w->setUsingNativeTitleBar( false );
        w->setTitleBarHeight( 0 );

#if defined( ENABLE_X11_FULLSCREEN ) && ( 0 < ENABLE_X11_FULLSCREEN )
    #if( JUCE_LINUX || __linux__ )
        //xlib::maximizeWindow( (xlib::Window)getPeer()->getNativeHandle() );
        xlib::fullscreenWindow( (xlib::Window)w->getPeer()->getNativeHandle() );
    #endif
#endif

        // Dimension ...
        juce::Rectangle<int> bounds  = ui::layout::screen;
        auto&                display = juce::Desktop::getInstance().getDisplays();
        //bounds = display.findDisplayForRect( w->getBounds() ).totalArea;
        //bounds = display.getPrimaryDisplay()->totalArea;
        //bounds = display.logicalToPhysical( ui::layout::screen, nullptr );
        w->setBounds( bounds );

#if( JUCE_IOS || JUCE_ANDROID || JUCE_LINUX || __linux__ )
        if (!smoke) juce::Desktop::getInstance().setKioskModeComponent( w.get(), false );
#endif
        // Position ...
        w->setTopLeftPosition( 0, 0 );

        // Visibility...
        w->setVisible(!smoke);
        if (smoke) juce::Timer::callAfterDelay(1500, [this] {
#if KICK_APP_SMOKE_TEST
            const auto error = runKickAppSmoke();
            if (error.isNotEmpty()) {
                std::cerr << "FAIL: " << error << std::endl;
                setApplicationReturnValue(3);
            }
#endif
            const auto path = juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_SNAPSHOT", {});
            if (path.isNotEmpty() && w->getContentComponent()) {
                auto image = w->getContentComponent()->createComponentSnapshot(w->getContentComponent()->getLocalBounds());
                juce::FileOutputStream output{juce::File(path)};
                juce::PNGImageFormat png;
                if (!output.openedOk() || !png.writeImageToStream(image, output)) setApplicationReturnValue(2);
            }
            quit();
        });

#if PERFETTO
        MelatoninPerfetto::get().beginSession();
#endif
    }

    void shutdown() override
    {
#if PERFETTO
        MelatoninPerfetto::get().endSession();
#endif
        // Add your application's shutdown code here ...
        Controls::InputEvents::shutdown();
        mt::getAudioInterface().closeProject();
        w = nullptr;
        adapter = nullptr;
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        if (ui::RecentProject::isProjectLoading) {
            juce::Timer::callAfterDelay(100, [this] { systemRequestedQuit(); });
            return;
        }
        // Check while the UI and live programs still exist. A rejected save
        // leaves the project open and its global error visible for recovery.
        if (!mt::getAudioInterface().saveProject()) return;
#if PERFETTO
        MelatoninPerfetto::get().endSession();
#endif

        // This is called when the app is being asked to quit: you can ignore this request and let the app
        // carry on running, or call quit() to allow the app to close.
        auto mgr = juce::ModalComponentManager::getInstance();
        if( mgr->cancelAllModalComponents() )
            juce::Timer::callAfterDelay( 100, [this] { systemRequestedQuit(); } );
        else
            quit();
    }

    void anotherInstanceStarted( const juce::String& /*commandLine*/ ) override
    {
        // When another instance of the app is launched while this one is running, this method is invoked,
        // and the commandLine parameter tells you what the other instance's command-line arguments were.
    }

    //==============================================================================
    // This class implements the desktop window that contains an instance of our MainComponent class.
    class HomeWindow : public juce::DocumentWindow
    {
    public:
        explicit HomeWindow( const juce::String& name )
            : juce::DocumentWindow( name, juce::Colours::black, 0 )
        {
            setTitle( name );
            setContentOwned( &kick_ui::KickHome::Instance(), true );
        }

        ~HomeWindow() final = default;

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

        void inputAttemptWhenModal() override
        {
            auto numModal = getNumCurrentlyModalComponents();
            auto index    = juce::jmax( 0, numModal - 1 );
            if( auto p = getCurrentlyModalComponent( index ) )
                p->toFront( true );
        }
    };

private:
    std::unique_ptr<juce::DocumentWindow> w;
    std::unique_ptr<KickDeviceAdapter>    adapter;
    std::unique_ptr<KickAudioEngineAdapter>    engineAdapter;
    std::shared_ptr<contracts::AudioInterface> audioInterface;
#if PERFETTO
    std::unique_ptr<perfetto::TracingSession> tracingSession;
#endif
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION( AudioEngineApplication )
