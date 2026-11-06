#pragma once
// Helpers for driving JUCE components without a window: synthesized mouse
// events and off-screen snapshots.

#include <juce_gui_basics/juce_gui_basics.h>

namespace kicktest
{
    // Run the message loop so async work (Button::triggerClick, async notifications) lands.
    inline void pump( int ms = 60 )
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil( ms );
    }

    // Run the message loop until pred() holds or timeoutMs passes. Prefer this over a
    // fixed pump: it returns as soon as the work lands and tolerates a loaded CI runner.
    template<typename Pred>
    bool pumpUntil( Pred pred, int timeoutMs = 2000 )
    {
        const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32)timeoutMs;
        while( !pred() && juce::Time::getMillisecondCounter() < deadline )
            pump( 10 );
        return pred();
    }

    inline juce::MouseEvent makeMouse( juce::Component& c, juce::Point<float> pos, bool down, int clicks = 1, bool dragged = false )
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        auto mods   = down ? juce::ModifierKeys::leftButtonModifier : juce::ModifierKeys::noModifiers;
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent( source, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, pos, now, clicks, dragged );
    }

    struct Mouse
    {
        explicit Mouse( juce::Component& comp ) : c( comp ) {}
        void down( juce::Point<float> p ) { start = p; c.mouseDown( makeMouse( c, p, true ) ); }
        void drag( juce::Point<float> p )
        {
            auto e = juce::MouseEvent( juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                       &c, &c, juce::Time::getCurrentTime(), start, juce::Time::getCurrentTime(), 1, true );
            c.mouseDrag( e );
        }
        void up( juce::Point<float> p ) { c.mouseUp( makeMouse( c, p, false ) ); }
        void click( juce::Point<float> p ) { down( p ); up( p ); }

        juce::Component&    c;
        juce::Point<float>  start;
    };

    inline juce::Image snapshot( juce::Component& c )
    {
        return c.createComponentSnapshot( c.getLocalBounds(), true, 1.0f );
    }

    // Number of pixels that differ between two same-sized snapshots.
    inline int differingPixels( const juce::Image& a, const juce::Image& b )
    {
        int diff = 0;
        for( int y = 0; y < a.getHeight(); ++y )
            for( int x = 0; x < a.getWidth(); ++x )
                if( a.getPixelAt( x, y ) != b.getPixelAt( x, y ) )
                    ++diff;
        return diff;
    }

    inline int countPixels( const juce::Image& img, juce::Colour target, int tolerance = 8 )
    {
        int n = 0;
        for( int y = 0; y < img.getHeight(); ++y )
            for( int x = 0; x < img.getWidth(); ++x )
            {
                auto p = img.getPixelAt( x, y );
                if( std::abs( p.getRed() - target.getRed() ) <= tolerance && std::abs( p.getGreen() - target.getGreen() ) <= tolerance &&
                    std::abs( p.getBlue() - target.getBlue() ) <= tolerance )
                    ++n;
            }
        return n;
    }
}
