/*
 * Copyright (c) 2021 MUSIC Tribe
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

#pragma once

namespace kick_ui
{
    constexpr int numNotesInOctave{ 12 };
    constexpr int numNotesInSemiTone{ 1 };

    enum class GridToolType
    {
        GridPencilTool,
        GridSelectTool,
        GridEraseTool,
        GridCopyTool,
        GridMagnifyTool
    };

    enum class GridViewType
    {
        DrumView,
        MidiView
    };

    ///////////////////////////////////////////////////////////////////////
    // Grid Settings
    ///////////////////////////////////////////////////////////////////////
    enum AutoScrollMode
    {
        Off,
        Follow,
        Page
    };

    class GridSettings : public juce::Component, public juce::DeletedAtShutdown
    {
    public:
        std::function<void( bool )>                     onHittingPadModeChanged;
        std::function<void( int, const juce::String& )> onAutoScrollChanged;
        std::function<void( int, const juce::String& )> onSnapModeChanged;
        std::function<void( bool )>                     onAutoAdvanceChanged;

        static GridSettings& Instance()
        {
            static auto* gridSettings = new GridSettings;
            return *gridSettings;
        }

        enum ScreenType
        {
            Grid,
            ListEdit
        };

        GridSettings();
        ~GridSettings() = default;

        bool           getHittingPadMode();
        bool           getAutoAdvance();
        int            getSnapMode();
        AutoScrollMode getAutoScrollType();
        void           setScreen( ScreenType Screen );

    private:
        void resized() override;

        ui::KickToggleOptions snapMode{ "SNAP MODE", { "Absolute", "Relative" } };
        ui::KickToggleOptions hittingPadsMode{ "HITTING PAD SELECTS ALL EVENTS", { "Off", "On" } };
        ui::KickToggleOptions autoScroll{ "AUTO SCROLL", { "Off", "Follow", "Page" } };
        ui::KickToggleOptions autoAdvance{ "AUTO ADVANCE ON STEP RECORD", { "Off", "On" } };

        ScreenType screenType{ Grid };
    };

    ///////////////////////////////////////////////////////////////////////
    // GridData
    ///////////////////////////////////////////////////////////////////////

    class GridData : public mt::MTAudioInterfaceListener, public juce::DeletedAtShutdown
    {
        SIGNALS : sigslot::signal<>    gridChanged;
        SIGNALS : sigslot::signal<>    gridPlayHeadChanged;
        SIGNALS : sigslot::signal<int> onNoteSelectionChanged;

    public:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        const mt::MTSelected& aeSel{ ae.getSelected() };
        const mt::MTPlayhead& playHead{ ae.getPlayhead() };
        const mt::MTPlayhead& trackPlayHead{ ae.getTrackPlayhead() };

        static GridData& Instance()
        {
            static auto* instance = new GridData;
            return *instance;
        }

        static int   getNotesInSemiTone() { return 1; }
        static int   getNotesInOctave() { return 12; }
        static float getNotesMax() { return (float)mt::ScaleAndNotes::maxNotes; }

        // Refresh all data.
        void refresh( bool Notify = true );

        // Measurements...
        inline float getHeight() const { return getNotesMax() * zoom.height; }
        inline float getWidth() const { return (float)getNumBars() * zoom.width; }
        inline float getNoteHeight() const { return zoom.height; }

        inline float getBarWidth() const { return getWidth() / (float)getNumBars(); }
        inline float getBeatWidth( const int Bar ) { return getBarWidth() / (float)getBeatsPerBar( Bar ); }
        inline float getQLWidth( const int Bar ) const { return getBarWidth() / (float)getNumQlPerBar( Bar ); }

        // Data...
        inline int getNumBars() const { return numBars; }
        inline int getNumQlPerBar( const int Bar ) const { return tempoMgr ? tempoMgr->getQuantizationPerBar( Bar ) : 16; }
        inline int getBeatsPerBar( const int Bar ) { return tempoMgr ? tempoMgr->getBeatsPerBar( Bar ) : 4; }
        inline int getTicksPerBar( const int Bar ) { return tempoMgr ? tempoMgr->getTicksPerBar( Bar ) : 0; }
        inline int getTicksPerQuantization( const int Bar ) { return tempoMgr ? tempoMgr->getTicksPerQuantization( Bar ) : 0; }

        // Zoom...
        void setZoomFactor( float horozontalFactor, float verticalFactor );
        void setHorizantalZoomFactor( float horozontalFactor, bool Notify = true );
        void setVerticalZoomFactor( float verticalFactor, bool Notify = true );
        void setDefaultZoom();

        // Positional
        float getYFromNote( int Note ) const;
        int   getNoteFromY( float Y ) const;

        int getXFromTime( int Time );
        int getTimeFromX( int X, bool Quantize = true );

        int getBarFromX( int X ) const;
        int getXFromBar( int Bar ) const;

        float getXPerTick( const int Bar ) { return getBarWidth() / (float)getTicksPerBar( Bar ); };
        float getTicksForX( const int Bar, const float X ) { return ( X * (float)getTicksPerBar( Bar ) ) / getBarWidth(); };

        int getQuantizedXFromX( int X ) const;

        // Grid Data API's
        unsigned int getTicksUptoBar( unsigned int Bar );
        unsigned int getTicksPerQuantizationAt( unsigned int Ticks );
        unsigned int getTicksPerTimebaseAt( unsigned int Ticks, const juce::String& Timebase );
        unsigned int getTicksPerTimebaseAt( unsigned int Ticks, mt::MTTimeBase Timebase );
        unsigned int getTicksPerBarAt( unsigned int Ticks );

        const mt::MTTempoMgr* getTempoMgr() const { return tempoMgr; }

        int            getTimeBase( mt::MTTimeBase Timebase );
        mt::MTTimeBase getTimeBase( const juce::String& Timebase );
        int            getTimeBaseIndex( int TimeDiv );

    private:
        GridData();
        ~GridData();

        void recalculate( bool Notify );

        void onTrackSelectionChange( const mt::MTSelected& /*Selected*/ ) override;
        void onPlayHeadChanged( [[maybe_unused]] mt::MTBarNBeats BnB ) override;
        void onTrackProgramChanged( int, int ) override;

    private:
        struct zoomFactor
        {
            float height{ 26.f }; // Minimum pixels per NOTE.
            float width{ 216.f }; // Minimum pixels per BAR.
        } zoom;

        static const float gridWidth;
        static const float gridHeight;

        mt::MTTempoMgr* tempoMgr{ nullptr };

        unsigned int numBars{ 1 };
        unsigned int totalTicks{ 0 };

        double seqLengthMs{ 0.0 };
    };
}
