/*
 * Pointer input for Kick's window in the emulator: touches and drags go in through the window peer, the way input
 * reaches Kick on the unit. Header-only and part of the kick_emu module, which compiles it through kick_emu.cpp.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <typeinfo>
#include <utility>

namespace kick_emu::input
{
    /// Sends one touch through Kick's window peer, the path input takes on the unit, so mouse listeners,
    /// click-to-focus, modal blocking and double taps behave as they do there. Calling mouseDown() directly skips
    /// all of that: tables that listen to their rows ignore taps and nothing ever gets focus.
    /// The kiosk JUCE patch stops Component::contains() asking the OS what is on top, which would drop every event
    /// because the window is minimised behind the Surface. Coordinates are in the root component's space, which is
    /// the peer's space at Kick's 1.0 scale. It goes in as a mouse event: JUCE on macOS has no touch sources
    /// (canUseTouch() is false) and drops touch events silently.
    /// TimeMs is the event time JUCE sees. It decides click versus drag (a press over 300 ms is a drag), so scripted
    /// taps pass their intended times, not the time the message thread got round to them.
    inline void injectTouch( juce::Component& root, juce::Point<float> at, bool pressed, juce::int64 timeMs = juce::Time::currentTimeMillis() )
    {
        if( auto* peer = root.getPeer() )
            peer->handleMouseEvent( juce::MouseInputSource::InputSourceType::mouse, at,
                                    pressed ? juce::ModifierKeys( juce::ModifierKeys::leftButtonModifier ) : juce::ModifierKeys(),
                                    pressed ? 1.0f : juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                                    timeMs, {}, 0 );
    }

    /// Presses at From, drags to To in Steps moves, holds for HoldMs, then releases.
    /// Returns the type name of the component under From, for the reply.
    inline juce::String touch( juce::Component& root, juce::Point<float> from, juce::Point<float> to, int steps, int holdMs )
    {
        auto*      hit  = root.getComponentAt( from.roundToInt() );
        const auto name = juce::String( typeid( hit != nullptr ? *hit : root ).name() );

        holdMs        = juce::jmax( 0, holdMs );
        const auto t0 = juce::Time::currentTimeMillis();
        injectTouch( root, from, true, t0 );
        for( int i = 1; i <= steps; ++i )
            injectTouch( root, from + ( to - from ) * ( (float)i / (float)steps ), true, t0 + holdMs * i / ( steps + 1 ) );

        juce::Component::SafePointer<juce::Component> safeRoot( &root );
        juce::Timer::callAfterDelay( holdMs, [safeRoot, to, t0, holdMs] {
            if( auto* r = safeRoot.getComponent() )
                injectTouch( *r, to, false, t0 + holdMs );
        } );

        return name;
    }

    /// A touch that stays down between commands, so the Surface can forward a live drag
    /// (down, many moves, up) from its screen bezel.
    inline bool& pointerIsDown()
    {
        static bool down = false;
        return down;
    }

    inline void pointerUp( juce::Component& root, juce::Point<float> at )
    {
        if( std::exchange( pointerIsDown(), false ) )
            injectTouch( root, at, false );
    }

    inline void pointerDown( juce::Component& root, juce::Point<float> at )
    {
        pointerUp( root, at );
        pointerIsDown() = true;
        injectTouch( root, at, true );
    }

    inline void pointerMove( juce::Component& root, juce::Point<float> at )
    {
        if( pointerIsDown() )
            injectTouch( root, at, true );
    }
}
