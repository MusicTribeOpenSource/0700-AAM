/*
 * Copyright (c) 2026 Dawid Janas
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
    // Drum grid: all 16 pads of a bank as rows, the steps of the current page as columns.
    // No tool modes: in the NOTES layer a tap sets or clears a step and a swipe paints the row;
    // in the value layers every cell is a small fader and a swipe draws a curve along the row.
    class DrumGrid : public juce::Component
        , private juce::Timer
    {
    public:
        enum class Layer
        {
            Notes,
            Velocity,
            Probability,
            Ratchet
        };

        DrumGrid( StepSequencerData& Data, StepSequencerTopControl& Control );
        ~DrumGrid() override = default;

        void  refresh();
        void  setLayer( Layer L );
        Layer getLayer() const { return layer; }

        // Dial / +- on the selected cell in a value layer. Returns false if nothing is selected.
        bool adjustSelected( int Delta );

        std::function<void( Layer )> onLayerChanged;

    private:
        static constexpr int numRows = 16;

        struct Cell
        {
            bool on{ false };
            int  start{ 0 };
            int  length{ 0 };
            int  velocity{ 0 };
            int  probability{ 100 };
            int  ratchet{ 1 };
        };

        enum class Paint
        {
            None,
            Set,
            Clear,
            Value
        };

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void mouseDown( const juce::MouseEvent& e ) override;
        void mouseDrag( const juce::MouseEvent& e ) override;
        void mouseUp( const juce::MouseEvent& e ) override;
        void visibilityChanged() override;
        void timerCallback() override;

        void readNotes();
        void readPadNames();
        int  bankBase() const;
        int  numColumns() const;
        int  firstStep() const;
        int  stepStartTick( int Column ) const;

        juce::Rectangle<int> cellBounds( int Row, int Column ) const;
        int                  columnAt( int X ) const;
        int                  rowAt( int Y ) const;

        void applyAt( int Row, int Column, int Y );
        void setStep( int Row, int Column, bool On );
        void setValue( int Row, int Column, float Normalized );

        int   valueOf( const Cell& C ) const;
        float normalizedOf( const Cell& C ) const;
        void  writeCell( int Row, int Column );
        bool  hasSelection() const;

    private:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        const mt::MTSelected& aeSel{ mt::getAudioInterface().getSelected() };
        GridData&             gridData{ GridData::Instance() };

        StepSequencerData&       data;
        StepSequencerTopControl& control;

        Layer layer{ Layer::Notes };

        std::array<std::array<Cell, 16>, numRows> cells{};
        std::array<juce::String, numRows>         padNames{};
        std::vector<mt::MTNote>                   notes;
        std::vector<mt::MTISample*>               samples;

        juce::Rectangle<int> tabArea, headerArea, gridArea;

        Paint paintMode{ Paint::None };
        int   dragRow{ -1 };
        int   lastColumn{ -1 };
        int   playColumn{ -1 };
        int   pollCounter{ 0 };
        int   selRow{ -1 };
        int   selColumn{ -1 };
        bool  dragging{ false };
        juce::Point<int> downPos;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( DrumGrid )
    };
}
