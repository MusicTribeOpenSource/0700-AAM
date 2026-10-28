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

namespace kick_ui
{
    namespace DrumGridConstants
    {
        constexpr int tabHeight     = 40;
        constexpr int headerWidth   = 150;
        constexpr int gap           = 3;
        constexpr int defaultVel    = 100;
        constexpr int timerHz       = 30;
        constexpr int pollEveryTick = 6; // re-read notes ~5x per second for hardware/other edits
        constexpr int dragThreshold = 4; // pixels before a tap in a value layer becomes a swipe

        const juce::StringArray tabNames{ "NOTES", "VELOCITY", "PROBABILITY", "RATCHET" };
        const juce::Colour      accent{ 0xffff6a00 };
        const juce::Colour      accentHi{ 0xffffb608 };
        const juce::Colour      valueBlue{ 0xff4fc3f7 };
    }

    DrumGrid::DrumGrid( StepSequencerData& Data, StepSequencerTopControl& Control )
        : data( Data ), control( Control )
    {
        setOpaque( false );
    }

    void DrumGrid::setLayer( Layer L )
    {
        if( layer == L )
            return;
        layer = L;
        if( layer == Layer::Notes )
            clearSelection();
        if( onLayerChanged )
            onLayerChanged( layer );
        repaint();
    }

    void DrumGrid::refresh()
    {
        readPadNames();
        readNotes();
        repaint();
    }

    int DrumGrid::bankBase() const
    {
        return ( juce::jmax( 0, control.padValueListBox.getSelectedIndex() ) / 16 ) * 16;
    }

    int DrumGrid::firstStep() const
    {
        return control.getPage() * juce::jmax( 1, data.numStepsPerPage );
    }

    int DrumGrid::numColumns() const
    {
        if( data.numSteps <= 0 || data.ticksPerQl <= 0 )
            return 0;
        return juce::jlimit( 0, 16, juce::jmin( data.numStepsPerPage, data.numSteps - firstStep() ) );
    }

    int DrumGrid::stepStartTick( int Column ) const
    {
        auto barStart = (int)GridData::Instance().getTicksUptoBar( (unsigned int)juce::jmax( 0, control.getBar() ) );
        return barStart + ( firstStep() + Column ) * data.ticksPerQl;
    }

    void DrumGrid::readNotes()
    {
        // The selection is per visible page; a different bank, bar, page or grid drops it.
        const std::array<int, 5> view{ bankBase(), control.getBar(), firstStep(), data.numStepsPerPage, data.ticksPerQl };
        if( view != selectionView )
        {
            selectionView = view;
            clearSelection();
        }

        for( auto& row : cells )
            for( auto& c : row )
                c = Cell{};

        const auto cols = numColumns();
        if( cols <= 0 || !ae.isProjectLoaded() )
            return;

        const auto base      = bankBase();
        const auto pageStart = stepStartTick( 0 );
        const auto pageEnd   = pageStart + cols * data.ticksPerQl;

        mt::NotesInRange range( base, base + numRows - 1, pageStart, pageEnd );
        if( ae.getNotesInRange( aeSel, notes, range ) < 0 )
            return;

        for( auto& n : notes )
        {
            const int start = n.start.get();
            if( start < pageStart || start >= pageEnd )
                continue; // overlaps from an earlier step, it does not belong to this page

            const int row = (int)n.number.get() - base;
            const int col = ( start - pageStart ) / data.ticksPerQl;
            if( !juce::isPositiveAndBelow( row, numRows ) || !juce::isPositiveAndBelow( col, cols ) )
                continue;

            auto& c = cells[(size_t)row][(size_t)col];
            if( c.on )
                continue; // keep the first note of a step

            c.on          = true;
            c.start       = start;
            c.length      = n.length.get();
            c.velocity    = (int)n.velocity.get();
            c.probability = (int)n.probability.get();
            c.ratchet     = (int)n.ratchet.get();
        }
    }

    void DrumGrid::readPadNames()
    {
        for( auto& n : padNames )
            n.clear();

        const auto prog = ae.isProjectLoaded() ? ae.getSelectedProgram() : -1;
        if( prog < 0 )
            return;

        ae.getProgramSamples( prog, samples );
        const auto base = bankBase();
        for( auto* s : samples )
        {
            if( s == nullptr )
                continue;
            const int row = (int)s->keyNote.get() - base;
            if( juce::isPositiveAndBelow( row, numRows ) && padNames[(size_t)row].isEmpty() )
            {
                auto src = s->getSource();
                if( src.isNotEmpty() )
                    padNames[(size_t)row] = juce::File( src ).getFileNameWithoutExtension();
            }
        }
    }

    void DrumGrid::resized()
    {
        auto b     = getLocalBounds().reduced( DrumGridConstants::gap );
        tabArea    = b.removeFromTop( DrumGridConstants::tabHeight );
        b.removeFromTop( DrumGridConstants::gap * 2 );
        headerArea = b.removeFromLeft( DrumGridConstants::headerWidth );
        b.removeFromLeft( DrumGridConstants::gap * 2 );
        gridArea = b;
    }

    juce::Rectangle<int> DrumGrid::cellBounds( int Row, int Column ) const
    {
        const auto cols = juce::jmax( 1, numColumns() );
        const auto w    = gridArea.getWidth() / (float)cols;
        const auto h    = gridArea.getHeight() / (float)numRows;
        auto       r    = juce::Rectangle<float>( gridArea.getX() + Column * w, gridArea.getY() + Row * h, w, h );
        return r.reduced( DrumGridConstants::gap * .5f ).toNearestInt();
    }

    int DrumGrid::columnAt( int X ) const
    {
        const auto cols = numColumns();
        if( cols <= 0 || gridArea.getWidth() <= 0 )
            return -1;
        const auto col = (int)std::floor( ( X - gridArea.getX() ) * cols / (float)gridArea.getWidth() );
        return juce::isPositiveAndBelow( col, cols ) ? col : -1;
    }

    int DrumGrid::rowAt( int Y ) const
    {
        if( gridArea.getHeight() <= 0 )
            return -1;
        const auto row = (int)std::floor( ( Y - gridArea.getY() ) * numRows / (float)gridArea.getHeight() );
        return juce::isPositiveAndBelow( row, numRows ) ? row : -1;
    }

    int DrumGrid::clampedColumnAt( int X ) const
    {
        const auto cols = numColumns();
        if( cols <= 0 || gridArea.getWidth() <= 0 )
            return -1;
        return juce::jlimit( 0, cols - 1, (int)std::floor( ( X - gridArea.getX() ) * cols / (float)gridArea.getWidth() ) );
    }

    int DrumGrid::clampedRowAt( int Y ) const
    {
        if( gridArea.getHeight() <= 0 )
            return -1;
        return juce::jlimit( 0, numRows - 1, (int)std::floor( ( Y - gridArea.getY() ) * numRows / (float)gridArea.getHeight() ) );
    }

    int DrumGrid::valueOf( const Cell& C ) const
    {
        switch( layer )
        {
            case Layer::Velocity: return C.velocity;
            case Layer::Probability: return C.probability;
            case Layer::Ratchet: return C.ratchet;
            default: return C.velocity;
        }
    }

    float DrumGrid::normalizedOf( const Cell& C ) const
    {
        switch( layer )
        {
            case Layer::Probability: return juce::jlimit( 0.f, 1.f, C.probability / 100.f );
            case Layer::Ratchet: return juce::jlimit( 0.f, 1.f, C.ratchet / 8.f );
            default: return juce::jlimit( 0.f, 1.f, C.velocity / 127.f );
        }
    }

    void DrumGrid::setStep( int Row, int Column, bool On )
    {
        auto& c    = cells[(size_t)Row][(size_t)Column];
        const auto note = bankBase() + Row;
        if( On && !c.on )
        {
            const auto st = stepStartTick( Column );
            ae.addOrModifyNote( note, st, data.ticksPerQl, DrumGridConstants::defaultVel, 100, 1 );
            c = Cell{ true, st, data.ticksPerQl, DrumGridConstants::defaultVel, 100, 1 };
        }
        else if( !On && c.on )
        {
            ae.eraseNote( note, c.start, -1 );
            c = Cell{};
        }
    }

    void DrumGrid::setValue( int Row, int Column, float Normalized )
    {
        auto& c = cells[(size_t)Row][(size_t)Column];
        if( !c.on ) // setting a value on an empty step creates the note
            c = Cell{ true, stepStartTick( Column ), data.ticksPerQl, DrumGridConstants::defaultVel, 100, 1 };

        Normalized = juce::jlimit( 0.f, 1.f, Normalized );
        switch( layer )
        {
            case Layer::Velocity: c.velocity = juce::jlimit( 1, 127, juce::roundToInt( Normalized * 127.f ) ); break;
            case Layer::Probability: c.probability = juce::jlimit( 0, 100, juce::roundToInt( Normalized * 100.f ) ); break;
            case Layer::Ratchet: c.ratchet = juce::jlimit( 1, 8, 1 + juce::roundToInt( Normalized * 7.f ) ); break;
            default: return;
        }
        writeCell( Row, Column );
    }

    void DrumGrid::writeCell( int Row, int Column )
    {
        const auto& c = cells[(size_t)Row][(size_t)Column];
        ae.addOrModifyNote( bankBase() + Row, c.start, c.length, c.velocity, c.probability, c.ratchet );
    }

    bool DrumGrid::isSelected( int Row, int Column ) const
    {
        return layer != Layer::Notes && juce::isPositiveAndBelow( Row, numRows ) && juce::isPositiveAndBelow( Column, numColumns() )
               && selected[(size_t)Row][(size_t)Column] && cells[(size_t)Row][(size_t)Column].on;
    }

    bool DrumGrid::hasSelection() const
    {
        for( int row = 0; row < numRows; ++row )
            for( int col = 0; col < numColumns(); ++col )
                if( isSelected( row, col ) )
                    return true;
        return false;
    }

    void DrumGrid::clearSelection()
    {
        for( auto& row : selected )
            row.fill( false );
    }

    void DrumGrid::selectOnly( int Row, int Column )
    {
        clearSelection();
        selected[(size_t)Row][(size_t)Column] = true;
    }

    void DrumGrid::selectRectangle( juce::Point<int> To )
    {
        selected = selectionAtDown; // Shift + drag adds the rectangle to the existing selection
        const auto r0 = clampedRowAt( juce::jmin( downPos.y, To.y ) ), r1 = clampedRowAt( juce::jmax( downPos.y, To.y ) );
        const auto c0 = clampedColumnAt( juce::jmin( downPos.x, To.x ) ), c1 = clampedColumnAt( juce::jmax( downPos.x, To.x ) );
        if( r0 < 0 || c0 < 0 )
            return;
        for( int row = r0; row <= r1; ++row )
            for( int col = c0; col <= c1; ++col )
                if( cells[(size_t)row][(size_t)col].on )
                    selected[(size_t)row][(size_t)col] = true;
    }

    bool DrumGrid::adjustSelected( int Delta )
    {
        if( !isVisible() || Delta == 0 || !hasSelection() )
            return false;

        // Relative change: the differences between the selected notes stay, each one stops at its limit.
        for( int row = 0; row < numRows; ++row )
        {
            for( int col = 0; col < numColumns(); ++col )
            {
                if( !isSelected( row, col ) )
                    continue;
                auto& c = cells[(size_t)row][(size_t)col];
                switch( layer )
                {
                    case Layer::Velocity: c.velocity = juce::jlimit( 1, 127, c.velocity + Delta ); break;
                    case Layer::Probability: c.probability = juce::jlimit( 0, 100, c.probability + Delta ); break;
                    case Layer::Ratchet: c.ratchet = juce::jlimit( 1, 8, c.ratchet + Delta ); break;
                    default: return false;
                }
                writeCell( row, col );
            }
        }
        repaint();
        return true;
    }

    void DrumGrid::applyAt( int Row, int Column, int Y )
    {
        switch( paintMode )
        {
            case Paint::Set: setStep( Row, Column, true ); break;
            case Paint::Clear: setStep( Row, Column, false ); break;
            case Paint::Value:
            {
                // The finger height inside the row sets the value; outside it clamps to 0 or max.
                auto r = cellBounds( Row, Column );
                setValue( Row, Column, 1.f - ( Y - r.getY() ) / (float)juce::jmax( 1, r.getHeight() ) );
                break;
            }
            default: break;
        }
    }

    void DrumGrid::mouseDown( const juce::MouseEvent& e )
    {
        // The centre dial goes to whichever control holds the keyboard focus (e.g. the view
        // button or a top list box). Release it so the dial reaches the grid again.
        ui::clearFocusComponent();

        const auto p = e.getPosition();

        if( tabArea.contains( p ) )
        {
            const auto idx = juce::jlimit( 0, 3, ( p.x - tabArea.getX() ) * 4 / juce::jmax( 1, tabArea.getWidth() ) );
            setLayer( (Layer)idx );
            return;
        }

        if( headerArea.contains( p ) )
        {
            const auto row = rowAt( p.y );
            if( row >= 0 )
                control.padValueListBox.setSelected( bankBase() + row, juce::sendNotification );
            refresh();
            return;
        }

        const auto row = rowAt( p.y );
        const auto col = columnAt( p.x );
        if( row < 0 || col < 0 )
            return;

        dragRow    = row;
        lastColumn = col;
        downPos    = p;
        dragging   = false;
        rubberBand = {};
        if( layer == Layer::Notes )
        {
            paintMode = cells[(size_t)row][(size_t)col].on ? Paint::Clear : Paint::Set;
            applyAt( row, col, p.y );
        }
        else if( contracts::DclContractsHelper::isShiftPressed() )
        {
            // Shift: tap toggles a note's selection, dragging adds a rectangle (decided on mouseUp/drag).
            paintMode       = Paint::Select;
            selectionAtDown = selected;
        }
        else
        {
            // A tap on a note selects only that note so the dial can fine-tune it; a tap on an
            // empty step creates the note at the tapped height. A swipe draws values.
            paintMode = Paint::Value;
            if( !cells[(size_t)row][(size_t)col].on )
                applyAt( row, col, p.y );
            selectOnly( row, col );
        }
        repaint();
    }

    void DrumGrid::mouseDrag( const juce::MouseEvent& e )
    {
        if( paintMode == Paint::None || dragRow < 0 )
            return;

        const auto p = e.getPosition();
        if( paintMode == Paint::Value && !dragging )
        {
            if( p.getDistanceFrom( downPos ) < DrumGridConstants::dragThreshold )
                return;
            dragging = true;
        }
        if( paintMode == Paint::Select )
        {
            if( !dragging && p.getDistanceFrom( downPos ) < DrumGridConstants::dragThreshold )
                return;
            dragging   = true;
            rubberBand = juce::Rectangle<int>( downPos, p ).getIntersection( gridArea );
            selectRectangle( p );
            repaint();
            return;
        }

        const auto col = columnAt( p.x );
        if( col < 0 )
            return;

        // Stay on the row where the swipe started and fill skipped columns on fast swipes.
        const auto from = juce::jmin( lastColumn, col );
        const auto to   = juce::jmax( lastColumn, col );
        for( auto c = from; c <= to; ++c )
            applyAt( dragRow, c, p.y );

        lastColumn = col;
        if( paintMode == Paint::Value )
            selectOnly( dragRow, col );
        repaint();
    }

    void DrumGrid::mouseUp( const juce::MouseEvent& )
    {
        if( paintMode == Paint::Select && !dragging )
        {
            // Shift + tap: toggle a note's selection; on an empty step it clears the selection.
            const auto row = rowAt( downPos.y ), col = columnAt( downPos.x );
            if( row >= 0 && col >= 0 && cells[(size_t)row][(size_t)col].on )
                selected[(size_t)row][(size_t)col] = !selected[(size_t)row][(size_t)col];
            else
                clearSelection();
        }

        rubberBand = {};
        paintMode  = Paint::None;
        dragRow    = -1;
        lastColumn = -1;
        dragging   = false;
        readNotes();
        repaint();
    }

    void DrumGrid::visibilityChanged()
    {
        if( isVisible() )
        {
            refresh();
            startTimerHz( DrumGridConstants::timerHz );
        }
        else
            stopTimer();
    }

    void DrumGrid::timerCallback()
    {
        auto newPlayColumn = -1;
        const auto cols    = numColumns();
        if( cols > 0 && ae.isSequencePlaying() )
        {
            const auto bars      = ae.getSequenceLength( aeSel.sequence );
            const auto total     = (double)gridData.getTicksUptoBar( (unsigned int)juce::jmax( 0, bars ) );
            const auto tick      = (int)( ae.getPlayhead().normized * total );
            const auto pageStart = stepStartTick( 0 );
            if( tick >= pageStart && tick < pageStart + cols * data.ticksPerQl )
                newPlayColumn = ( tick - pageStart ) / data.ticksPerQl;
        }

        auto changed = newPlayColumn != playColumn;
        playColumn   = newPlayColumn;

        if( paintMode == Paint::None && ++pollCounter >= DrumGridConstants::pollEveryTick )
        {
            pollCounter = 0;
            readPadNames();
            readNotes();
            changed = true;
        }

        if( changed )
            repaint();
    }

    void DrumGrid::paint( juce::Graphics& g )
    {
        using namespace DrumGridConstants;

        mt::utils::fillRoundedRect( g, getLocalBounds(), ui::Theme::backgroundColour( 1 ), 5.0f );

        // Layer tabs
        {
            auto t = tabArea;
            const auto w = t.getWidth() / 4;
            g.setFont( juce::Font( 16.0f, juce::Font::bold ) );
            for( int i = 0; i < 4; ++i )
            {
                auto r   = ( i == 3 ? t : t.removeFromLeft( w ) ).reduced( gap );
                auto sel = (int)layer == i;
                g.setColour( sel ? accent : ui::Theme::backgroundColour( 5 ) );
                g.fillRoundedRectangle( r.toFloat(), 4.0f );
                g.setColour( sel ? juce::Colours::black : ui::Theme::textColour( 1 ) );
                g.drawText( tabNames[i], r, juce::Justification::centred );
            }
        }

        const auto cols    = numColumns();
        const auto base    = bankBase();
        const auto selPad  = control.padValueListBox.getSelectedIndex();
        const auto bar     = juce::jmax( 0, control.getBar() );
        const auto beats   = juce::jmax( 1, gridData.getBeatsPerBar( bar ) );
        const auto perBeat = juce::jmax( 1, data.numSteps / beats );

        // Pad names
        for( int row = 0; row < numRows; ++row )
        {
            const auto note = base + row;
            const auto h    = headerArea.getHeight() / (float)numRows;
            auto       r    = juce::Rectangle<float>( (float)headerArea.getX(), headerArea.getY() + row * h, (float)headerArea.getWidth(), h ).reduced( gap * .5f );

            g.setColour( note == selPad ? accent.withAlpha( .35f ) : ui::Theme::backgroundColour( 5 ) );
            g.fillRoundedRectangle( r, 3.0f );

            auto label = juce::String::charToString( (juce::juce_wchar)( 'A' + note / 16 ) ) + juce::String( note % 16 + 1 ).paddedLeft( '0', 2 );
            g.setColour( ui::Theme::textColour( 1 ) );
            g.setFont( juce::Font( juce::jmin( 15.0f, r.getHeight() * .6f ), juce::Font::bold ) );
            auto tr = r.reduced( 6, 0 ).toNearestInt();
            g.drawText( label, tr.removeFromLeft( 36 ), juce::Justification::centredLeft );
            g.setFont( juce::Font( juce::jmin( 14.0f, r.getHeight() * .55f ) ) );
            g.setColour( ui::Theme::textColour( 1 ).withAlpha( padNames[(size_t)row].isEmpty() ? .35f : .9f ) );
            g.drawText( padNames[(size_t)row].isEmpty() ? juce::String( "-" ) : padNames[(size_t)row], tr, juce::Justification::centredLeft, true );
        }

        if( cols <= 0 )
        {
            g.setColour( ui::Theme::textColour( 1 ).withAlpha( .6f ) );
            g.setFont( 18.0f );
            g.drawText( "No steps in this bar", gridArea, juce::Justification::centred );
            return;
        }

        // Steps
        const auto valueLayer = layer != Layer::Notes;
        for( int row = 0; row < numRows; ++row )
        {
            for( int col = 0; col < cols; ++col )
            {
                const auto& c       = cells[(size_t)row][(size_t)col];
                auto        r       = cellBounds( row, col ).toFloat();
                const auto  step    = firstStep() + col;
                const auto  oddBeat = ( step / perBeat ) % 2 == 1;

                auto bg = ui::Theme::backgroundColour( oddBeat ? 5 : 3 );
                if( valueLayer && !c.on )
                    bg = bg.withMultipliedBrightness( .6f );
                g.setColour( bg );
                g.fillRoundedRectangle( r, 3.0f );

                if( !c.on )
                    continue;

                if( !valueLayer )
                {
                    const auto a = .35f + .65f * juce::jlimit( 0.f, 1.f, c.velocity / 127.f );
                    g.setGradientFill( juce::ColourGradient::vertical( accentHi.withAlpha( a ), r.getY(), accent.withAlpha( a ), r.getBottom() ) );
                    g.fillRoundedRectangle( r, 3.0f );
                }
                else
                {
                    const auto colour = layer == Layer::Velocity ? accent : valueBlue;
                    g.setColour( colour.withAlpha( .25f ) );
                    g.fillRoundedRectangle( r, 3.0f );
                    auto fill = r.withTrimmedTop( r.getHeight() * ( 1.f - normalizedOf( c ) ) );
                    g.setColour( colour );
                    g.fillRoundedRectangle( fill, 3.0f );

                    if( r.getWidth() > 28 && r.getHeight() > 14 )
                    {
                        g.setColour( juce::Colours::white.withAlpha( .85f ) );
                        g.setFont( juce::jmin( 13.0f, r.getHeight() * .6f ) );
                        g.drawText( juce::String( valueOf( c ) ), r.toNearestInt(), juce::Justification::centred );
                    }
                }
            }
        }

        // Selected pad row and playhead
        if( juce::isPositiveAndBelow( selPad - base, numRows ) )
        {
            auto r = cellBounds( selPad - base, 0 ).getUnion( cellBounds( selPad - base, cols - 1 ) ).toFloat().expanded( 1.5f );
            g.setColour( accent.withAlpha( .7f ) );
            g.drawRoundedRectangle( r, 4.0f, 1.5f );
        }
        g.setColour( juce::Colours::white );
        for( int row = 0; row < numRows; ++row )
            for( int col = 0; col < cols; ++col )
                if( isSelected( row, col ) )
                    g.drawRoundedRectangle( cellBounds( row, col ).toFloat().expanded( 1.0f ), 4.0f, 2.5f );
        if( !rubberBand.isEmpty() )
        {
            g.setColour( juce::Colours::white.withAlpha( .12f ) );
            g.fillRect( rubberBand );
            g.setColour( juce::Colours::white.withAlpha( .8f ) );
            g.drawRect( rubberBand, 1 );
        }
        if( juce::isPositiveAndBelow( playColumn, cols ) )
        {
            auto r = cellBounds( 0, playColumn ).getUnion( cellBounds( numRows - 1, playColumn ) ).toFloat();
            g.setColour( juce::Colours::white.withAlpha( .14f ) );
            g.fillRoundedRectangle( r, 3.0f );
        }
    }
}
