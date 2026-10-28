
namespace kick_ui
{
    GridPencilTool::GridPencilTool( GridComponent& gridComponent, GridData& data, juce::Viewport& ViewPort )
        : IGridTool( gridComponent, data, GridToolType::GridPencilTool, ViewPort )
    {
    }

    void GridPencilTool::paint( juce::Graphics& g )
    {
        if( !selectRect.isEmpty() )
        {
            g.setColour( ui::Theme::selectionColour( 1 ) );
            g.drawRect( selectRect, 1.5f );
        }
    }

    void GridPencilTool::mouseDown( const juce::MouseEvent& event )
    {
        IGridTool::mouseDown( event );
        shiftAtDown = contracts::DclContractsHelper::isShiftPressed();
        selectRect  = {};
    }

    void GridPencilTool::selectInRectangle( const juce::MouseEvent& event )
    {
        selectRect = juce::Rectangle<float>( event.mouseDownPosition, event.position ).getIntersection( gridComp.getLocalBounds().toFloat() );

        auto start = gridData.getTimeFromX( selectRect.getX(), false );
        auto end   = gridData.getTimeFromX( selectRect.getRight(), false );
        mt::NotesInRange range( gridData.getNoteFromY( selectRect.getY() ), gridData.getNoteFromY( selectRect.getBottom() ), start, end );
        ae.selectAllNotes( false );
        ae.selectNotesInRange( range, true );

        gridNeedsToMove( (int)event.position.getX(), (int)event.position.getY() );
        gridNeedsRepaint();
    }

    void GridPencilTool::drawInfoBar( int noteNum, int startTime, juce::MouseEvent event )
    {
        auto data = ae.getNoteData( noteNum, startTime, 0 );
        if( 0 <= data.id )
        {
            auto             Start      = ae.toBnB( aeSelected, (unsigned int)data.start );
            auto             resizeText = juce::String::formatted( "%d :: %d:%d:%d = %d", data.number.get() + 1, Start.bar + 1, Start.beat + 1, Start.tick, data.length.get() );
            juce::Point<int> dragPoint( event.x, event.y );
            gridComp.infoBar.displayTip( dragPoint, resizeText );
        }
    }

    void GridPencilTool::mouseDrag( const juce::MouseEvent& event )
    {
        if( shiftAtDown && 0 >= mDownNoteID )
        {
            if( event.mouseWasDraggedSinceMouseDown() )
                selectInRectangle( event );
            return;
        }

        if( event.mouseWasDraggedSinceMouseDown() )
        {
            auto noteNum  = gridData.getNoteFromY( event.position.getY() );
            auto noteTime = gridData.getTimeFromX( event.position.getX(), false );

            if( gridComp.getMouseCursor() == juce::MouseCursor::LeftRightResizeCursor )
            { /// RESIZE NOTE(S)
                // Check if any note exists at mouse down
                if( 0 < mDownNoteID )
                {
                    auto deltaValue  = event.getDistanceFromDragStartX();
                    int  deltaLength = ( deltaValue < 0 ) ? ( ( deltaValue < prevDragValue ) ? -5 : 5 ) : ( ( deltaValue > prevDragValue ) ? 5 : -5 );
                    prevDragValue    = deltaValue;
                    if( mDownSelectedNote )
                    {
                        ae.editSelectedNotesLength( aeSelected, deltaLength, isLeftEdge );
                    }
                    else
                    {
                        ae.editNoteLength( mDownNoteID, deltaLength, isLeftEdge );
                    }
                }
                drawInfoBar( noteNum, noteTime, event );
            }
            else if( gridComp.getMouseCursor() == juce::MouseCursor::DraggingHandCursor )
            { /// MOVE NOTE(S)
                // Check if any note exists at mouse down
                if( 0 < mDownNoteID )
                {
                    auto deltaNote = noteNum - mDownNoteNum;
                    auto deltaTime = noteTime - mDownTime;
                    if( deltaNote || deltaTime )
                    {
                        if( mDownSelectedNote )
                        {
                            ae.editSelectedNotes( aeSelected, deltaTime, deltaNote, 0 );
                        }
                        else
                        {
                            ae.editNote( mDownNoteID, deltaTime, deltaNote, 0 );
                        }
                        mDownTime    = noteTime;
                        mDownNoteNum = noteNum;
                    }
                }
                drawInfoBar( noteNum, noteTime, event );
            }
            else
            {
                validateSeqTrack();
                //Audio Engine API - Add Note if it does not exist
                auto bar        = gridData.getBarFromX( noteTime );
                auto ticksPerQl = gridData.getTicksPerQuantization( bar );
                noteTime        = gridData.getTimeFromX( event.position.getX() );
                if( !ae.isValidNote( noteNum, noteTime ) )
                {
                    ae.addOrModifyNote( noteNum, noteTime, ticksPerQl );
                }
            }
            gridNeedsRepaint();
        }
    }

    void GridPencilTool::mouseUp( const juce::MouseEvent& event )
    {
        gridComp.infoBar.hideTip();

        if( shiftAtDown && ( event.mouseWasClicked() || 0 >= mDownNoteID ) )
        {
            // Shift + tap deletes the note (or clears the selection on an empty spot);
            // the rectangle selection is already done. Dragging a note still moves it.
            if( event.mouseWasClicked() )
            {
                if( 0 < mDownNoteID )
                    ae.eraseNote( mDownNoteNum, mDownTime, -1 );
                else
                    ae.selectAllNotes( false );
            }
            shiftAtDown = false;
            selectRect  = {};
            isLeftEdge  = false;
            gridNeedsRepaint();
            return;
        }
        shiftAtDown = false;

        if( event.mouseWasClicked() )
        {
            validateSeqTrack();
            auto noteNum   = gridData.getNoteFromY( event.getMouseDownY() );
            auto startTime = gridData.getTimeFromX( event.getMouseDownX(), false );
            auto noteTime  = gridData.getTimeFromX( event.getMouseDownX() );
            //Audio Engine API - Add Note if it does not exist
            auto bar        = gridData.getBarFromX( startTime );
            auto ticksPerQl = gridData.getTicksPerQuantization( bar );
            if( ae.isValidNote( noteNum, startTime ) )
            {
                if( !ae.isValidSelectedNote( noteNum, startTime, ticksPerQl ) )
                {
                    ae.selectAllNotes( false );
                    ae.selectNote( aeSelected, noteNum, startTime, true );
                }
            }
            else
                ae.addOrModifyNote( noteNum, noteTime, ticksPerQl );
            gridNeedsRepaint();
        }

        if( gridComp.getMouseCursor() == juce::MouseCursor::DraggingHandCursor )
        {
            if( ae.isInvalidNoteExists() )
                ae.eraseInvalidNotes();
        }

        isLeftEdge = false;
    }

    void GridPencilTool::mouseDoubleClick( const juce::MouseEvent& event )
    {
        validateSeqTrack();
        auto startTime = gridData.getTimeFromX( event.position.getX() );
        auto noteNum   = gridData.getNoteFromY( event.position.getY() );

        ae.eraseNote( noteNum, startTime, -1 );
        gridNeedsRepaint();
    }

    void GridPencilTool::mouseMove( const juce::MouseEvent& event )
    {
        auto mouseNoteNum  = gridData.getNoteFromY( event.position.getY() );
        auto mouseNoteTime = gridData.getTimeFromX( event.position.getX(), false );

        auto nData = ae.getNoteData( mouseNoteNum, mouseNoteTime, 0 );
        if( 0 <= nData.id )
        {
            auto nStartXPos = gridData.getXFromTime( nData.start );
            auto nEndXPos   = gridData.getXFromTime( nData.length + nData.start );

            if( ( nStartXPos <= event.x ) && ( ( nStartXPos + 5 ) >= event.x ) && gridComp.getEditNoteStart() )
            {
                gridComp.setMouseCursor( juce::MouseCursor::LeftRightResizeCursor );
                isLeftEdge = true;
            }
            else if( ( ( nEndXPos - 6 ) <= event.x ) && ( ( nEndXPos - 1 ) >= event.x ) && gridComp.getEditNoteEnd() )
            {
                gridComp.setMouseCursor( juce::MouseCursor::LeftRightResizeCursor );
            }
            else
            {
                gridComp.setMouseCursor( juce::MouseCursor::DraggingHandCursor );
            }
        }
        else
        {
            gridComp.setMouseCursor( juce::MouseCursor::NormalCursor );
        }
        gridNeedsRepaint();
    }

    bool GridPencilTool::keyPressed( const juce::KeyPress& key, juce::Component* /*originatingComponent*/ )
    {
        if( getType() == GridToolType::GridPencilTool )
        {
            if( juce::ModifierKeys::getCurrentModifiers().isShiftDown() )
            {
                if( juce::ModifierKeys::getCurrentModifiers().isCtrlDown() )
                {
                    if( key.getKeyCode() == ( juce::KeyPress::endKey ) )
                    {
                        ae.editMuteSelectedNotes( aeSelected, true );
                        gridNeedsRepaint();
                        return true;
                    }
                    else if( key.getKeyCode() == ( juce::KeyPress::homeKey ) )
                    {
                        ae.editMuteSelectedNotes( aeSelected, false );
                        gridNeedsRepaint();
                        return true;
                    }
                }
                else if( key.getKeyCode() == ( juce::KeyPress::rightKey ) )
                {
                    auto ticks = gridData.getTicksPerBarAt( 0 );
                    ae.editSelectedNotes( aeSelected, ticks, 0, 0 );
                    gridNeedsRepaint();
                    return true;
                }
                else if( key.getKeyCode() == ( juce::KeyPress::leftKey ) )
                {
                    auto ticks = (int)gridData.getTicksPerBarAt( 0 );
                    ae.editSelectedNotes( aeSelected, -ticks, 0, 0 );
                    gridNeedsRepaint();
                    return true;
                }
                else if( key.getKeyCode() == ( juce::KeyPress::upKey ) )
                {
                    ae.editSelectedNotes( aeSelected, 0, numNotesInOctave, 0 );
                    gridNeedsRepaint();
                    return true;
                }
                else if( key.getKeyCode() == ( juce::KeyPress::downKey ) )
                {
                    ae.editSelectedNotes( aeSelected, 0, -numNotesInOctave, 0 );
                    gridNeedsRepaint();
                    return true;
                }
            }
            else if( key.getKeyCode() == juce::KeyPress::rightKey )
            {
                auto ticks = gridData.getTicksPerQuantizationAt( 0 );
                ae.editSelectedNotes( aeSelected, ticks, 0, 0 );
                //ae.pasteSelectedNotesDefault();
                gridNeedsRepaint();
                return true;
            }
            else if( key.getKeyCode() == juce::KeyPress::leftKey )
            {
                auto ticks = (int)gridData.getTicksPerQuantizationAt( 0 );
                ae.editSelectedNotes( aeSelected, -ticks, 0, 0 );
                gridNeedsRepaint();
                return true;
            }
            else if( ( key.getKeyCode() == juce::KeyPress::upKey ) && gridComp.getTranspose() )
            {
                ae.editSelectedNotes( aeSelected, 0, numNotesInSemiTone, 0 );
                gridNeedsRepaint();
                return true;
            }
            else if( ( key.getKeyCode() == juce::KeyPress::downKey ) && gridComp.getTranspose() )
            {
                ae.editSelectedNotes( aeSelected, 0, -numNotesInSemiTone, 0 );
                gridNeedsRepaint();
                return true;
            }
        }
        return false;
    }

    void GridPencilTool::validateSeqTrack()
    {
        if( !ae.getNumSequences() )
        {
            ae.createSequence();
            ae.createTrack( aeSelected.sequence );
        }
        else if( ae.getNumSequences() )
        {
            if( !ae.getNumTracks( aeSelected.sequence ) )
                ae.createTrack( aeSelected.sequence );
        }
    }
}
