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
    class GridPencilTool : public IGridTool
    {
    public:
        GridPencilTool( GridComponent& gridComponent, GridData& data, juce::Viewport& ViewPort );
        void paint( juce::Graphics& g );

        void mouseDown( const juce::MouseEvent& event ) override;
        void mouseDrag( const juce::MouseEvent& event );
        void mouseUp( const juce::MouseEvent& event ) override;
        void mouseDoubleClick( const juce::MouseEvent& event ) override;
        void mouseMove( const juce::MouseEvent& event ) override;
        bool keyPressed( const juce::KeyPress& key, juce::Component* originatingComponent ) override;

    private:
        void drawInfoBar( int noteNum, int startTime, juce::MouseEvent event );
        void validateSeqTrack();
        void selectInRectangle( const juce::MouseEvent& event );

        // SHIFT shortcuts: a tap on a note deletes it, a drag from an empty spot selects a rectangle.
        bool                   shiftAtDown{ false };
        juce::Rectangle<float> selectRect;

        bool        isLeftEdge{ false };
        juce::Label resizeLabel;
        int         prevDragValue = -1;
    };
}
