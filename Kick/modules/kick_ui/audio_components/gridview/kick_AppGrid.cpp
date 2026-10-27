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
namespace kick_ui
{
    constexpr int gNotesWidth              = 140;
    constexpr int gScaleHeight             = 40;
    constexpr int gParamViewHeight         = 200;
    constexpr int playhead_update_interval = 25;

    ///////////////////////////////////////////////////////////////////////
    // Grid Edit Options
    ///////////////////////////////////////////////////////////////////////
    GridEditOptions::GridEditOptions()
    {
        setName( "ARRANGEMENT EDIT" );

        auto index = 0;
        for( ; index < options.size(); ++index )
        {
            addAndMakeVisible( options[index] );
            options[index].setValueColour( ui::Theme::textColour( 1 ) );
            options[index].setValueFontHeight( 18 );
            options[index].setClickingTogglesState( false );
            options[index].setButtonText( gridEditOptionList[index] );
            options[index].onClick = [&, index]() {
                if( onOptionClicked )
                    onOptionClicked( index );
            };
        }
    }

    void GridEditOptions::close()
    {
        if( auto dw = getCurrentlyModalComponent() )
            dw->exitModalState( 0 );
    }

    void GridEditOptions::resized()
    {
        auto b         = getLocalBounds().reduced( 10 );
        auto h         = b.getHeight() / 3 - 20;
        auto compWidth = 146;
        auto compGap   = 10;

        b.removeFromTop( compGap * 2 );
        auto firstRowBounds = b.removeFromTop( h );
        b.removeFromTop( compGap );
        auto secondRowBounds = b.removeFromTop( h );
        b.removeFromTop( compGap );
        auto thirdRowBounds = b.removeFromTop( h );

        firstRowBounds.removeFromLeft( compWidth / 2 + compGap / 2 );
        options[0].setBounds( firstRowBounds.removeFromLeft( compWidth ) );
        firstRowBounds.removeFromLeft( compGap );
        options[1].setBounds( firstRowBounds.removeFromLeft( compWidth ) );
        firstRowBounds.removeFromLeft( compGap );
        options[2].setBounds( firstRowBounds.removeFromLeft( compWidth ) );
        firstRowBounds.removeFromLeft( compGap );
        options[3].setBounds( firstRowBounds.removeFromLeft( compWidth ) );

        options[4].setBounds( secondRowBounds.removeFromLeft( compWidth ) );
        secondRowBounds.removeFromLeft( compGap );
        options[5].setBounds( secondRowBounds.removeFromLeft( compWidth ) );
        secondRowBounds.removeFromLeft( compGap );
        options[6].setBounds( secondRowBounds.removeFromLeft( compWidth ) );
        secondRowBounds.removeFromLeft( compGap );
        options[7].setBounds( secondRowBounds.removeFromLeft( compWidth ) );
        secondRowBounds.removeFromLeft( compGap );
        options[8].setBounds( secondRowBounds.removeFromLeft( compWidth ) );

        thirdRowBounds.removeFromLeft( compWidth + compGap );
        options[9].setBounds( thirdRowBounds.removeFromLeft( compWidth ) );
        thirdRowBounds.removeFromLeft( compGap );
        options[10].setBounds( thirdRowBounds.removeFromLeft( compWidth ) );
        thirdRowBounds.removeFromLeft( compGap );
        options[11].setBounds( thirdRowBounds.removeFromLeft( compWidth ) );
    }

    ///////////////////////////////////////////////////////////////////////
    // Grid Playhead
    ///////////////////////////////////////////////////////////////////////
    GridPlayhead::GridPlayhead()
    {
        setName( "GridPlayhead-Canvas" );
        setInterceptsMouseClicks( false, false );
    }

    void GridPlayhead::drawAt( float NormalizedX, [[maybe_unused]] int MaxWidth )
    {
        auto prevX = normX;
        normX      = juce::jlimit( 0.f, (float)getWidth(), NormalizedX );

        if( prevX != normX )
            triggerAsyncUpdate();
    }

    void GridPlayhead::handleAsyncUpdate()
    {
        repaint();
    }

    void GridPlayhead::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds();

        if( b.isEmpty() )
            return;

        // Set colour...
        g.setColour( ui::Theme::selectionColour( 4 ) );

        // Draw Vertical line.
        auto centerX = (int)( normX );
        g.drawVerticalLine( centerX, 0, b.getHeight() );

        // Draw triangle playhead
        juce::Path triangle;
        triangle.addTriangle( ( centerX - playHeadWidth_2 ), 0, ( centerX + playHeadWidth_2 ), 0, centerX, playHeadHeight );
        g.fillPath( triangle );
    }

    AppGrid::AppGrid()
        : kick_ui::IScreen( Screen::HomeGridView )
    {
        // Add header component...
        header.onBackButtonPressed = [this]( bool ) { closeScreen( Screen::HomeListView ); };
        header.showSettingsButton( true );

        // initialize grid
        mt::utils::addChildComponents( this, header, progressBar, scaleViewport, gridViewport, parameterViewport, noteViewport, phViewport, parameterControl, ph, hdrComp, timeDivListBox, parameterScale );

        //hrComp
        hdrComp.tracks.onSelectionChanged = [&]( int Sel, const juce::String& ) {
            if( aeSel.track >= 0 )
            {
                auto pProg = ae.getProgramDataForTrack( aeSel.sequence, Sel );
                ae.setSelectedTrack( Sel );
                if( pProg && pProg->type.get() == mt::MTProgramType::Audio )
                    kick_ui::signals::sigScreenClose( kick_ui::Screen::MenuProgramEdit );

                hdrComp.bars.setValue( ae.getTrackLength( aeSel.sequence, aeSel.track ) );
            }
            redraw( ae.getSelectedSequence(), Sel );
            gridComponent.triggerAsyncUpdate();
        };

        hdrComp.bars.onValueChanged = [&]( int Value ) {
            /* auto trackData   = ae.getTrackData( aeSel.sequence, aeSel.track );
            auto isSeqLength = trackData ? trackData->isSeqLength.get() : true;
            if( isSeqLength )
                ae.setSequenceLength( ae.getSelectedSequence(), Value );
            else*/
            int seqLen = ae.getSequenceLength();
            if( Value > seqLen ) // validate and set bounded value.
                hdrComp.bars.setValue( seqLen );
            else
                ae.setTrackLength( aeSel.sequence, aeSel.track, Value );

            redraw( ae.getSelectedSequence(), ae.getSelectedTrack() );
            gridComponent.triggerAsyncUpdate();
        };

        header.onSettingsClick = [&]() {
            gridSettings.setScreen( GridSettings::Grid );
            mt::utils::showDialog( "SETTINGS", &gridSettings, [this]( int R ) {}, { 0, 0, 650, 520 } );
        };

        mt::signals::bankChanged.connect( [&]( int Index, const juce::String& bank ) {
            auto viewPos = gridViewport.getViewPosition();
            auto yPos    = gridData.getYFromNote( ae.getBankOffset() ) - gridViewport.getHeight() + gridData.getNoteHeight();
            gridViewport.setViewPosition( viewPos.getX(), yPos );
        } );

        // Progress Bar...
        progressBar.setTextToDisplay( "" );
        progressBar.setColour( juce::ProgressBar::backgroundColourId, ui::Theme::backgroundColour( 10 ) );
        progressBar.setColour( juce::ProgressBar::foregroundColourId, ui::Theme::foregroundColour( 2 ) );

        gridViewport.setScrollOnDragMode( juce::Viewport::ScrollOnDragMode::never );
        parameterView.setMinimizeHeight( 0 );

        // Add components to viewports...
        gridViewport.setViewedComponent( &gridComponent, false );
        scaleViewport.setViewedComponent( &scaleComp, false );
        parameterViewport.setViewedComponent( &parameterView, false );
        noteViewport.setViewedComponent( &notesComp, false );
        phViewport.setViewedComponent( &ph, false );

        // Scroll bars...
        gridViewport.setScrollBarsShown( false, false, true, true );
        scaleViewport.setScrollBarsShown( false, false, false, false );
        parameterViewport.setScrollBarsShown( false, false, false, true );
        noteViewport.setScrollBarsShown( false, false, true, false );
        phViewport.setScrollBarsShown( false, false, true, false );

        // Viewport Listners..
        gridViewport.addListener( this );
        scaleViewport.addListener( this );
        parameterViewport.addListener( this );
        noteViewport.addListener( this );
        phViewport.addListener( this );

        // sig slots
        notesComp.noteSelectionChanged = [&]( int SelectedNote, bool State ) {
            if( State )
                ae.selectAllInNote( aeSel, SelectedNote, true );
            gridData.onNoteSelectionChanged( SelectedNote );
        };

        gridData.onNoteSelectionChanged.connect( [this]( int SelectedNote ) {
            gridComponent.noteSelectionChanged( SelectedNote );
        } );

        // On zoom changed...
        gridData.gridChanged.connect( [this]() {
            // Update canvas sizes...
            gridComponent.setSize( gridData.getWidth(), gridData.getHeight() );
            scaleComp.setSize( gridData.getWidth(), scaleViewport.getHeight() );
            parameterView.setSize( gridData.getWidth(), parameterViewport.getHeight() );
            notesComp.setSize( gNotesWidth, gridData.getHeight() );
            notesComp.setNoteHeight( gridData.getNoteHeight() );
            ph.setSize( gridData.getWidth(), gridViewport.getHeight() + scaleComp.getHeight() );
        } );

        hdrComp.toolSelectionChanged.connect( [&]( GridToolType Type ) {
            gridComponent.toolTypeChanged( Type );
        } );

        hdrComp.onCutClick = [&]() {
            selectedNotes.clear();
            selectedNotes = ae.getSelectedNoteList();
            ae.deleteSelectedNotes( aeSel );
            gridComponent.triggerAsyncUpdate();
        };

        hdrComp.onCopyClick = [&]() {
            selectedNotes = ae.getSelectedNoteList();
        };

        hdrComp.onPasteClick = [&]() {
            auto phPos     = ph.getPlayHeadPosition();
            auto startTime = gridData.getTimeFromX( phPos );
            ae.pasteNotes( aeSel, selectedNotes, startTime );
            gridComponent.triggerAsyncUpdate();
        };

        timeDivListBox.onSelectionChanged = [&]( int, const juce::String& TimeBase ) {
            auto tempMgr = ae.getTempoMgr( aeSel.sequence );
            tempMgr->setQuantization( gridData.getTimeBase( TimeBase ) );
            gridComponent.triggerAsyncUpdate();
            parameterControl.repaint();
            scaleComp.repaint();
        };

        gridNeedsToMove.connect( [&]( int xPos, int yPos ) {
            auto viewArea = gridViewport.getViewArea();
            auto topY     = viewArea.getY();
            auto bottomY  = topY + gridViewport.getHeight();
            auto leftX    = viewArea.getX();
            auto rightX   = leftX + gridViewport.getWidth();
            if( topY > yPos || yPos > bottomY )
                gridViewport.setViewPosition( viewArea.getX(), topY > yPos ? yPos : yPos - gridViewport.getHeight() );
            viewArea = gridViewport.getViewArea();
            if( xPos < leftX || xPos > rightX )
                gridViewport.setViewPosition( xPos < leftX ? xPos : xPos - gridViewport.getWidth(), viewArea.getY() );
            gridNeedsRepaint();
        } );

        timeDivListBox.setJustificationType( juce::Justification::centred );

        scaleComp.onMouseClick = [&]( float Value ) {
            // Based on PlayHead Need to Update Bar Beat Tick
            auto timeFromX = gridData.getTimeFromX( Value, false );
            ae.seek( timeFromX );
            isSeek = true;
            updatePlayhead();

            /*auto xFromTime = gridData.getXFromTime( timeFromX );
            ph.drawAt( xFromTime, gridComponent.getWidth() );*/
        };

        parameterControl.ecBtn.onClick = [&]() {
            parameterViewExpanded = !parameterViewExpanded;
            resized();
        };

        parameterControl.plusBtn.onClick = [this]() {
            setZoomInZoomOut( 20, 1, true );
            parameterControl.plusBtn.setToggleState( false, juce::dontSendNotification );
        };

        parameterControl.minusBtn.onClick = [this]() {
            setZoomInZoomOut( -20, -1, false );
            parameterControl.minusBtn.setToggleState( false, juce::dontSendNotification );
        };

        parameterControl.eStartBtn.onClick = [this]() {
            gridComponent.setEditNoteStart( parameterControl.eStartBtn.getToggleState() );
        };

        parameterControl.eEndBtn.onClick = [this]() {
            gridComponent.setEditNoteEnd( parameterControl.eEndBtn.getToggleState() );
        };

        parameterControl.transposeBtn.onClick = [this]() {
            gridComponent.setTranspose( parameterControl.transposeBtn.getToggleState() );
        };

        parameterControl.defaultZoomBtn.onClick = [this]() {
            gridData.setDefaultZoom();
            gridViewport.setViewPosition( 0, gridData.getYFromNote( 15 ) );
            parameterControl.defaultZoomBtn.setToggleState( false, juce::dontSendNotification );
        };

        parameterControl.paramList.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
            if( Value == "Velocity" )
            {
                parameterScale.setMinMaxValues( MIDI::MIN, MIDI::MAX );
                parameterView.setMinMaxValues( MIDI::MIN, MIDI::MAX );
            }
            else if( Value == "Probability" )
            {
                parameterScale.setMinMaxValues( 0, 100 );
                parameterView.setMinMaxValues( 0, 100 );
            }
            else if( Value == "Ratchet" )
            {
                parameterScale.setMinMaxValues( 1, 8 );
                parameterView.setMinMaxValues( 1, 8 );
            }
        };

        editOptions.onOptionClicked = [&]( int Index ) {
            editOptions.close();

            switch( Index )
            {
                case 0:
                    ae.deleteAllNotes( aeSel );
                    break; //clear
                case 1:
                    ae.doubleLengthWithEvents( aeSel );
                    break; //double length with events
                case 2:
                    ae.halfSpeedNotes( aeSel );
                    break; //half speed
                case 3:
                    ae.doubleSpeedNotes( aeSel );
                    break; //double speed
                case 4:
                    //Trim To Length is Related to doubleLengthWithEvents
                    break; //trim to length
                case 5:
                    openPitchQuantizePopUp();
                    break; //pitch quantize
                case 6:
                    openHumanizePopUp();
                    break; //humanize
                case 7:
                    openRandomEventsPopUp();
                    break; //random evenets
                case 8:
                    openSplitEventsPopUp();
                    break; //split events
                case 9: break; //convert to progression
                case 10: break; //bounce to sample
                case 11: break; //bounce to audio track
                default:
                    break;
            }
            gridComponent.repaint();
        };

        /* gridData.gridPlayHeadChanged.connect( [&]() {
            updatePlayhead();
        } );*/

        ui::themeChanged.connect( [&]() {
            header.setIcon( "Icon_Grid" );
        } );
    }

    //==============================================================================
    void AppGrid::resized()
    {
        auto b = getLocalBounds();
        // Header...
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        parameterControl.setBounds( b.removeFromBottom( 53 ) );
        b = b.reduced( 10, 0 );
        b.removeFromTop( 5 );
        progressBar.setBounds( b.removeFromTop( 8 ).withTrimmedBottom( 4 ) );
        auto scaleBounds = b.removeFromTop( gScaleHeight );
        timeDivListBox.setBounds( scaleBounds.removeFromLeft( gNotesWidth ) );
        scaleViewport.setBounds( scaleBounds );
        hdrComp.setBoundsRelative( .225f, 0, .587f, .127f );

        // Bottom parameter view...
        if( parameterViewExpanded )
        {
            auto paramBounds = b.removeFromBottom( gParamViewHeight );
            parameterScale.setBounds( paramBounds.removeFromLeft( gNotesWidth ) );
            parameterViewport.setBounds( paramBounds );
        }
        else
        {
            parameterScale.setBounds( { 0, 0, 0, 0 } );
            parameterViewport.setBounds( { 0, 0, 0, 0 } );
        }

        //Key Panel
        noteViewport.setBounds( b.removeFromLeft( gNotesWidth ) );
        gridViewport.setBounds( b );

        if( !initialized )
        {
            bottomLeftNote.setXY( 0, gridData.getHeight() );
            initialized = true;
        }

        // Playhead...
        phViewport.setBounds( gNotesWidth + 10, scaleViewport.getY() + progressBar.getHeight(), gridViewport.getWidth(), gridViewport.getHeight() + scaleViewport.getHeight() );

        gridData.gridChanged();
    }

    void AppGrid::updateOnViewPositionChange( ui::KickViewport* pViewport )
    {
        if( pViewport == &gridViewport )
        {
            noteViewport.setViewPosition( noteViewport.getViewPositionX(), gridViewport.getViewPositionY() );
            scaleViewport.setViewPosition( gridViewport.getViewPositionX(), scaleViewport.getViewPositionY() );
            parameterViewport.setViewPosition( gridViewport.getViewPositionX(), parameterViewport.getViewPositionY() );
            phViewport.setViewPosition( gridViewport.getViewPositionX(), phViewport.getViewPositionY() );
        }
        else if( pViewport == &noteViewport )
        {
            auto y = noteViewport.getViewPositionY();
            bottomLeftNote.setY( gridData.getNoteFromY( y ) );
            gridViewport.setViewPosition( gridViewport.getViewPositionX(), noteViewport.getViewPositionY() );
        }
        else //if( pViewport == &scaleViewport || pViewport == &parameterViewport )
        {
            auto x = pViewport->getViewPositionX();
            bottomLeftNote.setX( gridData.getTimeFromX( x ) );
            gridViewport.setViewPosition( x, gridViewport.getViewPositionY() );
        }
    }

    void AppGrid::setViewType( mt::MTProgramType Program )
    {
        switch( Program )
        {
            case mt::MTProgramType::Drum:
            case mt::MTProgramType::Clip:
                viewType = GridViewType::DrumView;
                notesComp.setType( NotesView::Type::Drum );
                break;
            case mt::MTProgramType::Keygroup:
            case mt::MTProgramType::Plugin:
            case mt::MTProgramType::Midi:
            case mt::MTProgramType::Cv:
                viewType = GridViewType::MidiView;
                notesComp.setType( NotesView::Type::Keys );
                break;
        }
    }

    void AppGrid::redraw( int Seq, int Trk )
    {
        auto pProg = ae.getProgramDataForTrack( Seq, Trk );
        auto type  = pProg ? pProg->type.get() : mt::MTProgramType::Unknown;
        setViewType( type );

        gridData.refresh( true );
    }

    void AppGrid::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        static int lastpadPressed = -1;

        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) )
        {
            int btnNum = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            if( btnNum == contracts::DeviceControlContracts::BtnLedSequence::EN_COPY )
            {
                ae.pasteSelectedNotesDefault();
                gridNeedsRepaint();
            }
            if( btnNum == contracts::DeviceControlContracts::BtnLedSequence::EN_ERASE )
            {
                const mt::MTSelected& aeSelected{ ae.getSelected() };
                ae.deleteSelectedNotes( aeSelected );
                gridNeedsRepaint();
            }
            else if( btnNum == contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_CW )
            {
                ///TBD:::ae.editSelectedNotes( aeSel, gridData.data.ticksPerQl, 0, 0 );
                gridComponent.repaint();
            }
            else if( btnNum == contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_CCW )
            {
                ///TBD:::ae.editSelectedNotes( aeSel, -gridData.data.ticksPerQl, 0, 0 );
                gridComponent.repaint();
            }
            else if( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED )
            {
                switch( btnNum )
                {
                    case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); return;
                    case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); return;
                    case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); return;
                }
            }
        }
        else if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            int              bankOffset = ae.getBankOffset();
            auto             padNo      = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            auto             padState   = contracts::DclContractsHelper::getPadState( f_HwMsg );
            auto             velocity   = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );
            std::vector<int> pads;

            padNo += bankOffset;
            pads.push_back( padNo );

            for( auto& it : pads )
            {
                juce::MidiMessage Msg;

                if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
                    Msg = juce::MidiMessage::noteOn( 1, it, (juce::uint8)velocity );
                else if( padState == contracts::DeviceControlContracts::BTN_RELEASED )
                    Msg = juce::MidiMessage::noteOff( 1, it );

                ae.addLiveMidiMsg( ae.getSelected(), Msg );
            }
            pads.clear();
            juce::MessageManager::callAsync( [this, padNo] {
                notesComp.selectNote( padNo + ae.getBankOffset(), gridSettings.getHittingPadMode() );
            } );
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto  knob     = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto  value    = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            auto  Snap     = parameterControl.snapBtn.getToggleState();
            auto& selNotes = ae.getSelectedNoteList();
            auto  qlWidth  = gridData.getTicksPerQuantization( 0 );

            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
                knobCommand( knob, value );
            else
            {
                if( parameterControl.eStartBtn.getToggleState() )
                {
                    editStartForSelectedNotes( selNotes, value, qlWidth, Snap );
                }
                else if( parameterControl.eEndBtn.getToggleState() )
                {
                    editEndForSelectedNotes( selNotes, value, qlWidth, Snap );
                }
                else if( parameterControl.transposeBtn.getToggleState() )
                {
                    transpseSelectedNotes( selNotes, value );
                }
                else if( parameterControl.nudgeBtn.getToggleState() )
                {
                    nudgeSelectedNotes( selNotes, value, qlWidth, Snap );
                }
            }
            gridComponent.triggerAsyncUpdate();
        }
    }

    void AppGrid::knobCommand( int knob, int value )
    {
        switch( knob )
        {
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1:
                gridViewport.knobRotated( knob, value );
                break;
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_2:
                gridViewport.knobRotated( knob, value );
                break;
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_3:
                gridData.setHorizantalZoomFactor( (float)value / 100, false );
                break;
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_4:
                gridData.setVerticalZoomFactor( (float)value / 100, false );
                break;
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_5:
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_6:
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_7:
                ae.setBBT( knob % 4 - 1, value );
                break;
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_8:
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_9:
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_10:
                metronome.linkRotated( knob % 8, value );
                break;
        }
        gridComponent.repaint();
    }

    void AppGrid::transpseSelectedNotes( juce::Array<juce::ValueTree>& SelNotes, int Value )
    {
        if( !SelNotes.isEmpty() )
        {
            ae.transposeSelectedNotes( aeSel, Value );
            selectedNotes = SelNotes;
            updateViewPosition( Value, true );
        }
    }

    void AppGrid::nudgeSelectedNotes( juce::Array<juce::ValueTree>& SelNotes, int Value, double qlWidth, bool Snap )
    {
        if( !SelNotes.isEmpty() )
        {
            auto nudgeWidth = Value < 0 ? -qlWidth : qlWidth;
            auto totalTicks = gridData.getTicksUptoBar( gridData.getNumBars() );
            if( gridSettings.getSnapMode() == 1 )
                ae.nudgeSelectedNotes( aeSel, Snap ? Value : nudgeWidth, Snap, totalTicks );
            else if( gridSettings.getSnapMode() == 0 )
            {
                if( Snap )
                    ae.nudgeSelectedNotes( aeSel, Value, Snap, totalTicks );
                else
                {
                    selectedNotes = SelNotes;
                    auto diff     = nudgeWidth;
                    auto min{ -1 }, max{ -1 };
                    for( auto& note : selectedNotes )
                    {
                        mt::MTNote mtNote( note );
                        auto       noteStart = mtNote.start.get();
                        if( min == -1 && max == -1 )
                        {
                            min = noteStart;
                            max = noteStart;
                        }
                        else if( noteStart < min )
                            min = noteStart;
                        else if( noteStart > min )
                            max = noteStart;
                    }
                    if( Value < 0 )
                    {
                        auto xTime = gridData.getXFromTime( min );
                        auto timeX = gridData.getTimeFromX( xTime );
                        if( min != timeX )
                            diff = -( min - timeX );

                        ae.nudgeSelectedNotes( aeSel, diff, Snap, totalTicks );
                    }
                    else
                    {
                        auto xTime = gridData.getXFromTime( max );
                        auto timeX = gridData.getTimeFromX( xTime );
                        auto bar   = gridData.getBarFromX( xTime );
                        timeX      = timeX + gridData.getTicksPerQuantization( bar );
                        if( max != timeX )
                            diff = timeX - max;
                        ae.nudgeSelectedNotes( aeSel, diff, Snap, totalTicks );
                    }
                }
            }
            updateViewPosition( Value, false );
        }
    }

    void AppGrid::updateViewPosition( int Value, bool IsTranspose )
    {
        auto viewPos        = gridViewport.getViewPosition();
        auto viewStart      = gridData.getTimeFromX( viewPos.x );
        auto viewEnd        = gridData.getTimeFromX( viewPos.x + gridViewport.getWidth() );
        auto viewNoteTop    = gridData.getNoteFromY( viewPos.y );
        auto viewNoteBottom = gridData.getNoteFromY( viewPos.y + gridViewport.getHeight() );

        bool isEdgeNote{ false };

        for( auto& note : selectedNotes )
        {
            mt::MTNote n( note );
            if( IsTranspose )
            {
                if( viewNoteTop == n.number.get() || viewNoteBottom > n.number.get() )
                {
                    isEdgeNote = true;
                    break;
                }
            }
            else
            {
                if( viewStart > n.start.get() || viewEnd < n.start.get() )
                {
                    isEdgeNote = true;
                    break;
                }
            }
        }
        if( isEdgeNote )
        {
            auto xPos = IsTranspose ? viewPos.x : ( Value < 0 ? viewPos.x - gridData.getQLWidth( 0 ) : viewPos.x + gridData.getQLWidth( 0 ) );
            auto yPos = IsTranspose ? ( Value < 0 ? viewPos.y + gridData.getNoteHeight() : viewPos.y - gridData.getNoteHeight() ) : viewPos.y;
            gridViewport.setViewPosition( xPos, yPos );
        }
    }

    void AppGrid::editStartForSelectedNotes( juce::Array<juce::ValueTree>& SelNotes, int Value, double qlWidth, bool Snap )
    {
        if( !SelNotes.isEmpty() )
        {
            auto nudgeWidth = Value < 0 ? -qlWidth : qlWidth;
            if( gridSettings.getSnapMode() == 1 )
                ae.editSelectedNotesStart( aeSel, Snap ? Value : Value * qlWidth, Snap );
            else
            {
                if( Snap )
                    ae.editSelectedNotesStart( aeSel, Value, Snap );
                else
                {
                    selectedNotes = ae.getSelectedNoteList();
                    auto diff     = nudgeWidth;
                    auto min{ -1 };
                    for( auto& note : selectedNotes )
                    {
                        mt::MTNote mtNote( note );
                        auto       noteStart = mtNote.start.get();
                        if( min == -1 )
                            min = noteStart;
                        else if( noteStart < min )
                            min = noteStart;
                    }
                    auto xTime = gridData.getXFromTime( min );
                    auto timeX = gridData.getTimeFromX( xTime );
                    if( Value < 0 )
                    {
                        if( min != timeX )
                            diff = -( min - timeX );
                        ae.editSelectedNotesStart( aeSel, diff, true );
                    }
                    else
                    {
                        auto bar = gridData.getBarFromX( xTime );
                        timeX    = timeX + gridData.getTicksPerQuantization( bar );
                        if( min != timeX )
                            diff = timeX - min;
                        ae.editSelectedNotesStart( aeSel, diff, Snap );
                    }
                }
            }
        }
    }

    void AppGrid::editEndForSelectedNotes( juce::Array<juce::ValueTree>& SelNotes, int Value, double qlWidth, bool Snap )
    {
        if( !SelNotes.isEmpty() )
        {
            auto nudgeWidth = Value < 0 ? -qlWidth : qlWidth;
            if( gridSettings.getSnapMode() == 1 )
                ae.editSelectedNotesEnd( aeSel, Snap ? Value : Value * qlWidth );
            else
            {
                if( Snap )
                    ae.editSelectedNotesEnd( aeSel, Value );
                else
                {
                    selectedNotes = ae.getSelectedNoteList();
                    auto diff     = nudgeWidth;
                    auto max{ -1 };
                    for( auto& note : selectedNotes )
                    {
                        mt::MTNote mtNote( note );
                        auto       noteEnd = mtNote.start.get() + mtNote.length.get();
                        if( max == -1 )
                            max = noteEnd;
                        if( noteEnd > max )
                            max = noteEnd;
                    }

                    auto xTime = gridData.getXFromTime( max );
                    auto timeX = gridData.getTimeFromX( xTime );
                    if( Value < 0 )
                    {
                        if( max != timeX )
                            diff = -( max - timeX );
                        ae.editSelectedNotesEnd( aeSel, diff );
                    }
                    else
                    {
                        auto bar = gridData.getBarFromX( xTime );
                        timeX    = timeX + gridData.getTicksPerQuantization( bar );

                        if( max != timeX )
                            diff = timeX - max;
                        ae.editSelectedNotesEnd( aeSel, diff );
                    }
                }
            }
        }
    }

    void AppGrid::setZoomInZoomOut( int xValue, int yValue, bool isZoomIn )
    {
        auto gridwidth  = gridData.getWidth();
        auto gridHeight = gridData.getHeight();
        auto p          = gridViewport.getViewArea().getBottomLeft();
        auto viewPos    = gridViewport.getViewPosition();
        auto bar        = gridData.getBarFromX( viewPos.x );
        auto numQl      = viewPos.x / gridData.getQLWidth( bar );
        gridData.setZoomFactor( xValue, yValue );
        if( gridwidth != gridData.getWidth() )
        {
            auto xPos = numQl * gridData.getQLWidth( bar ) + xValue;
            auto yPos = viewPos.y;
            if( isZoomIn )
            {
                if( gridHeight != gridData.getHeight() )
                    yPos = p.y - gridViewport.getHeight() + ( gridData.getNotesMax() ) - ( gridData.getNotesMax() / 16 );
            }
            else
            {
                if( gridHeight != gridData.getHeight() )
                    yPos = p.y - gridData.getNotesMax() / 16;
            }
            gridViewport.setViewPosition( xPos, yPos );
        }
    }

    void AppGrid::visibilityChanged()
    {
        auto visible = isVisible();

        if( visible )
        {
            gridViewport.getVerticalScrollBar().scrollToBottom();
            if( 0 <= aeSel.sequence )
            {
                hdrComp.bars.setValue( ae.getTrackLength( aeSel.sequence, aeSel.track ) );
                auto timeDiv = ae.getTempoMgr( aeSel.sequence )->getQuantization( aeSel.sequence );
                timeDivListBox.setSelected( gridData.getTimeBaseIndex( timeDiv ), juce::dontSendNotification );
            }
            DBG( "IN APPGRID VISIBILE" );
            startTimer( playhead_update_interval );
        }
        else
        {
            DBG( "IN APPGRID NOT VISIBILE" );
            stopTimer();
        }

        for( auto it : getChildren() )
        {
            if( it != nullptr )
                it->setVisible( visible );
        }
    }

    void AppGrid::timerCallback()
    {
        updatePlayhead();
    }

    void AppGrid::updatePlayhead()
    {
        if( isVisible() && ( isSeek || ae.isSequencePlaying() ) )
        {
            auto gridWidth = gridComponent.getWidth();
            auto gridX     = (int)( gridData.trackPlayHead.normized * gridWidth );
            auto viewWidth = gridViewport.getWidth();
            auto yPos      = gridViewport.getViewPositionY();

            ph.drawAt( gridX, gridWidth ); //

            auto scrollType = gridSettings.getAutoScrollType();
            if( scrollType == Follow )
            {
                auto halfWidth = viewWidth / 2;
                if( gridX < halfWidth )
                {
                    gridViewport.setViewPosition( 0, yPos );
                }

                else if( ( gridWidth - gridX ) >= halfWidth && gridX > halfWidth )
                {
                    gridViewport.setViewPosition( gridX - halfWidth, yPos );
                }
            }
            else if( scrollType == Page )
            {
                int tempPage = gridX / viewWidth;
                if( gridPage != tempPage )
                {
                    gridPage = tempPage;
                    gridViewport.setViewPosition( gridX, yPos );
                }
            }
            if( isSeek ) isSeek = !isSeek;
        }
    }

    void AppGrid::openPitchQuantizePopUp()
    {
        auto p = new APPTrackOptionPitchQuantize();
        mt::utils::showOptions( p->getName(), p, [&, p]( int R ) {
            if( R == UI::APPLY ) ae.pitchQuantizeNotes( aeSel, p->getData() );
            if( p ) delete p;
        } );
    }

    void AppGrid::openHumanizePopUp()
    {
        auto p = new APPTrackOptionHumanize();
        mt::utils::showOptions( p->getName(), p, [&, p]( int R ) {
            if( R == UI::APPLY ) ae.humanizeNotes( aeSel, p->getData() );
            if( p ) delete p;
        } );
    }

    void AppGrid::openRandomEventsPopUp()
    {
        auto p = new APPTrackOptionRandomEvents();
        mt::utils::showOptions( p->getName(), p, [&, p]( int R ) {
            if( R == UI::APPLY ) ae.randomNotes( aeSel, p->getData() );
            if( p ) delete p;
        } );
    }

    void AppGrid::openSplitEventsPopUp()
    {
        auto p = new APPTrackOptionSplitEvents();
        mt::utils::showOptions(
            p->getName(), p, [&, p]( int R ) {
                if( R == UI::APPLY )
                {
                    auto& Info = p->getData();
                    if( Info.onlySelected )
                    {
                        auto selNotes = ae.getSelectedNoteList();
                        if( !selNotes.isEmpty() )
                            ae.splitSelectedNotes( aeSel, Info );
                        else
                            openWarningPopUp( "No Selected Events Are Present To Split" );
                    }
                    else
                    {
                        if( ae.getNumNotes( ae.getSelected() ) > 0 )
                            ae.splitNotes( aeSel, Info );
                        else
                            openWarningPopUp( "No Events Are Present To Split" );
                    }

                    if( p ) delete p;
                }
            },
            ui::layout::warningPopup );
    }

    void AppGrid::openWarningPopUp( const juce::String& Text )
    {
        auto wP = new ui::FilePopup( Text, false, false );
        mt::utils::showDialog( "WARNING!", wP, [wP, this]( int R ) { delete wP; }, { 0, 0, 600, 360 } );
    }

} //namespace kick_ui
