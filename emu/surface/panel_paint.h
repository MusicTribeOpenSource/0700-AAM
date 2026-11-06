/*
 * Drawing for the Surface's panel that needs no Surface state, so a change to how a part looks stays out of the
 * input and protocol code.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

namespace surface_paint
{
    /// A knurled knob like the unit's: a scalloped skirt, a faceted cone cap lit from the top left and a
    /// thin pointer. The skirt and facets turn with the knob; the lighting stays put.
    inline void drawPot( juce::Graphics& g, float cx, float cy, float r, float angle, int knurls = 12, bool pointer = true )
    {
        using juce::Colour;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        const float     light = -0.8f; // direction the light comes from, clockwise from 12 o'clock (top left)

        // Drop shadow
        g.setGradientFill( juce::ColourGradient( juce::Colours::black.withAlpha( 0.7f ), cx, cy + r * 0.15f, juce::Colours::transparentBlack,
                                                 cx, cy + r * 1.35f, true ) );
        g.fillEllipse( cx - r * 1.35f, cy - r * 1.2f, r * 2.7f, r * 2.7f );

        // Knurled skirt
        juce::Path skirt;
        for( int i = 0; i <= 96; ++i )
        {
            const float a  = (float)i / 96.0f * twoPi;
            const float rr = r * ( 0.94f + 0.06f * std::cos( a * (float)knurls - angle * (float)knurls ) );
            const juce::Point<float> p( cx + std::sin( a ) * rr, cy - std::cos( a ) * rr );
            i == 0 ? skirt.startNewSubPath( p ) : skirt.lineTo( p );
        }
        skirt.closeSubPath();
        g.setGradientFill( juce::ColourGradient( Colour( 0xff383b42 ), cx - r * 0.6f, cy - r * 0.7f, Colour( 0xff060709 ), cx + r * 0.5f, cy + r * 0.8f, false ) );
        g.fillPath( skirt );
        g.setColour( Colour( 0xff0a0b0d ) );
        g.strokePath( skirt, juce::PathStrokeType( 1.0f ) );

        // Faceted cone cap: each facet's shade comes from where it faces relative to the light
        const float capR   = r * 0.76f;
        const int   facets = 16;
        for( int i = 0; i < facets; ++i )
        {
            const float a0    = angle + (float)i / (float)facets * twoPi;
            const float a1    = a0 + twoPi / (float)facets;
            const float facing = 0.5f + 0.5f * std::cos( ( a0 + a1 ) * 0.5f - light );
            const float shade  = ( 0.15f + 0.85f * facing * facing ) * ( i % 2 == 0 ? 1.0f : 0.55f );
            juce::Path wedge;
            wedge.startNewSubPath( cx, cy );
            wedge.addCentredArc( cx, cy, capR, capR, 0.0f, a0, a1 );
            wedge.closeSubPath();
            g.setColour( Colour( 0xff111216 ).interpolatedWith( Colour( 0xff6a6e78 ), shade ) );
            g.fillPath( wedge );
        }
        g.setGradientFill( juce::ColourGradient( Colour( 0x60ffffff ), cx - capR * 0.7f, cy - capR * 0.7f, Colour( 0x00ffffff ), cx, cy, false ) );
        g.drawEllipse( cx - capR, cy - capR, capR * 2, capR * 2, 1.0f );
        g.setColour( Colour( 0xff08090b ) );
        g.drawEllipse( cx - capR - 0.6f, cy - capR - 0.6f, capR * 2 + 1.2f, capR * 2 + 1.2f, 0.8f );
        g.setGradientFill( juce::ColourGradient( Colour( 0xff2a2c32 ), cx - r * 0.1f, cy - r * 0.1f, Colour( 0xff0c0d10 ), cx + r * 0.15f, cy + r * 0.15f, true ) );
        g.fillEllipse( cx - r * 0.16f, cy - r * 0.16f, r * 0.32f, r * 0.32f );

        if( pointer )
        {
            const float s = std::sin( angle ), c = std::cos( angle );
            g.setColour( Colour( 0xffd8dbe2 ) );
            g.drawLine( cx + s * r * 0.24f, cy - c * r * 0.24f, cx + s * r * 0.9f, cy - c * r * 0.9f, juce::jmax( 1.4f, r * 0.06f ) );
            g.fillEllipse( cx + s * r * 0.9f - 1.6f, cy - c * r * 0.9f - 1.6f, 3.2f, 3.2f );
        }
    }
}
