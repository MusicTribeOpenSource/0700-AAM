

#define STEP_SEQ_SPACE    16
#define STEP_SEQ_WIDTH    20
#define STEP_BUTTON_SPACE 5

#include "kick_ui/dialogs/kick_AppHome.h"
#include "contracts/dcl/dcl_contracts_helper.h"

namespace kick_ui
{
    StepComponent::StepComponent( StepSequencerData& Data, StepSequencerTopControl& stepControl, StepPresets& stepPreset )
        : data( Data ), control( stepControl ), preset( stepPreset )
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );
        preset.sliderOffsetOnDragOn = [&]( int value ) {
            if( ae.getNoteList().getNumChildren() > 0 )
            {
                triggerAsyncUpdate();
            }
        };

        preset.sliderOffsetOnDragEnd = [&]( int value ) {
            if( ae.getNoteList().getNumChildren() > 0 )
            {
                //Update velocites to engine...
                updateValuestoEngine( value );
            }
        };

        preset.addValues = [&]( std::vector<int>& Values ) {
            addValuestoEngine( Values );
        };

        preset.modifyValues = [&]( std::vector<int>& Values ) {
            updateValuestoEngine( Values );
        };

        gridData.gridPlayHeadChanged.connect( [&]() {
            if( ae.isSequencePlaying() )
            {
                notePlaying = ( ( ( playHeadBnB.bar + 1 ) * ( playHeadBnB.beat ) * ( playHeadBnB.tick ) ) / gridData.getNumQlPerBar( playHeadBnB.bar ) );
                triggerAsyncUpdate();
            }
        } );

        control.pageChanged = [&]() {
            if( mt::getAudioInterface().isProjectLoaded() )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( control.getPage() ) );
                triggerAsyncUpdate();
            }
        };
        control.barChanged = [&]() { refresh(); };
        control.padChanged = [&]() { refresh(); };

        mt::signals::playStateChanged.connect( [&]( mt::TransportState State ) {
            state = ( State == mt::TransportState::Playing );
        } );

        for( auto& i : buttonNum )
            mt::utils::addAndMakeVisible( i.get() );
    }

    void StepComponent::updateValuestoEngine( int Offset )
    {
        auto             startTime = range.startTime;
        auto             endTime   = startTime + data.ticksPerQl;
        mt::NotesInRange stepRange;
        stepRange.noteStart = range.noteStart;
        stepRange.noteEnd   = range.noteEnd;

        for( auto i = 0; i < data.numSteps; ++i )
        {
            // Calculate step time range...
            stepRange.startTime = startTime;
            stepRange.endTime   = endTime;

            // Apply velocity Probability Ratchet changes...
            if( control.paramValueListBox.getSelected() == "Velocity" )
            {
                auto velocity = juce::jlimit( MIDI::MIN, MIDI::MAX, ( data.indexValues[i] + Offset ) );
                ae.editNoteVelocity( aeSel, stepRange, velocity );
            }
            else if( control.paramValueListBox.getSelected() == "Probability" )
            {
                auto probability = juce::jlimit( 0, 100, data.indexProbValues[i] + Offset );
                ae.editNoteProbability( aeSel, stepRange, probability );
            }
            else if( control.paramValueListBox.getSelected() == "Ratchet" )
            {
                auto ratchet = juce::jlimit( 1, 8, data.indexRatchetValues[i] + Offset );
                ae.editNoteRatchet( aeSel, stepRange, ratchet );
            }

            // Next step...
            startTime += data.ticksPerQl;
            endTime += data.ticksPerQl;
        }

        triggerAsyncUpdate();
    }

    void StepComponent::updateValuestoEngine( std::vector<int>& Values )
    {
        auto stepRange    = range;
        stepRange.endTime = stepRange.startTime + data.ticksPerQl;
        for( auto i = 0; i < data.numSteps; ++i )
        {
            // Apply velocity Probability Ratchet changes...
            if( control.paramValueListBox.getSelected() == "Velocity" )
            {
                auto velocity = juce::jlimit( MIDI::MIN, MIDI::MAX, Values[i] );
                ae.editNoteVelocity( aeSel, stepRange, velocity );
            }
            else if( control.paramValueListBox.getSelected() == "Probability" )
            {
                auto probability = juce::jlimit( 0, 100, Values[i] );
                ae.editNoteProbability( aeSel, stepRange, probability );
            }
            else if( control.paramValueListBox.getSelected() == "Ratchet" )
            {
                auto ratchet = juce::jlimit( 1, 8, Values[i] );
                ae.editNoteRatchet( aeSel, stepRange, ratchet );
            }
            // Next step...
            stepRange.startTime += data.ticksPerQl;
            stepRange.endTime += data.ticksPerQl;
        }

        triggerAsyncUpdate();
    }

    void StepComponent::addValuestoEngine( std::vector<int>& Values )
    {
        auto stepRange    = range;
        auto bar          = gridData.getBarFromX( range.startTime );
        auto qlWidth      = gridData.getTicksPerQuantization( bar );
        stepRange.endTime = stepRange.startTime + data.ticksPerQl;
        for( auto i = 0; i < data.numSteps; ++i )
        {
            // Apply velocity changes...
            if( 0 >= Values[i] )
                ae.deleteNotesInRange( aeSel, stepRange );
            else
            {
                if( control.paramValueListBox.getSelected() == "Velocity" )
                    ae.addOrModifyNote( stepRange.noteStart, stepRange.startTime, qlWidth, Values[i] );
                else if( control.paramValueListBox.getSelected() == "Probability" )
                    ae.addOrModifyNote( stepRange.noteStart, stepRange.startTime, qlWidth, MIDI::MAX, Values[i] );
                else if( control.paramValueListBox.getSelected() == "Ratchet" )
                    ae.addOrModifyNote( stepRange.noteStart, stepRange.startTime, qlWidth, MIDI::MAX, 100, Values[i] );
            }

            // Next step...
            stepRange.startTime += data.ticksPerQl;
            stepRange.endTime += data.ticksPerQl;
        }

        triggerAsyncUpdate();
    }

    void StepComponent::refresh()
    {
        range.noteStart = range.noteEnd = control.padValueListBox.getSelectedIndex();

        auto selBar     = control.getBar();
        range.startTime = gridData.getTicksUptoBar( selBar );
        range.endTime   = gridData.getTicksUptoBar( selBar + 1 );

        // Find out quantization settings.
        data.ticksPerQl = gridData.getTicksPerQuantization( selBar );
        data.timediv    = control.getTimeDivision();
        // Calculate steps available at this quantization setting.
        data.numSteps = gridData.getNumQlPerBar( selBar );
        // Resize if required...
        if( data.selected.size() != data.numSteps )
            data.selected.resize( data.numSteps, false );
        if( data.indexValues.size() != data.numSteps )
            data.indexValues.resize( data.numSteps, 0 );
        if( data.indexProbValues.size() != data.numSteps )
            data.indexProbValues.resize( data.numSteps, 0 );
        if( data.indexRatchetValues.size() != data.numSteps )
            data.indexRatchetValues.resize( data.numSteps, 0 );

        // Resize canvas accordingly
        updateCanvasSize();
        // Trigger repaint
        triggerAsyncUpdate();
    }

    void StepComponent::handleAsyncUpdate()
    {
        auto stepRange    = range;
        stepRange.endTime = ( range.startTime + data.ticksPerQl );
        std::vector<int> colourID( 16, contracts::DeviceControlContracts::PadColourTrans );
        std::vector<int> velocity( 16, 0 );
        auto             page          = control.getPage();
        auto             idxStart      = ( page * data.numStepsPerPage );
        int              velocityIndex = 0;
        int              colourIndex   = 0;
        for( auto i = 0; i < data.numSteps; ++i )
        {
            data.selected[i]           = false;
            data.indexValues[i]        = 0;
            data.indexProbValues[i]    = 0;
            data.indexRatchetValues[i] = 0;

            // Find available notes in this step.
            ae.getNotesInRange( aeSel, notes, stepRange );

            auto avg = 0, probAvg = 0, ratAvg = 0;
            for( auto& note : notes )
            {
                avg += note.velocity.get();
                probAvg += note.probability.get();
                ratAvg += note.ratchet.get();
            }

            auto numNotes = notes.size();
            if( numNotes )
            {
                data.selected[i] = true;

                if( control.paramValueListBox.getSelected() == "Velocity" )
                {
                    //Maintaining minimum
                    data.indexValues[i] = ( avg / numNotes ) < 1 ? 1 : ( avg / numNotes );
                    if( !isPadSelected && ( i >= idxStart && i < idxStart + 16 ) )
                    {
                        colourID[colourIndex++]   = contracts::DeviceControlContracts::PadColourRed;
                        velocity[velocityIndex++] = data.indexValues[(size_t)i];
                    }
                }
                else if( control.paramValueListBox.getSelected() == "Probability" )
                {
                    data.indexProbValues[i] = ( probAvg / numNotes ) < 1 ? 1 : ( probAvg / numNotes ); //updating velocity
                    if( !isPadSelected && ( i >= idxStart && i < idxStart + 16 ) )
                    {
                        colourID[colourIndex++]   = contracts::DeviceControlContracts::PadColourLightSkyBlue;
                        velocity[velocityIndex++] = ( data.indexProbValues[(size_t)i] * 127 ) / 100;
                    }
                }
                else if( control.paramValueListBox.getSelected() == "Ratchet" )
                {
                    data.indexRatchetValues[i] = ( ratAvg / numNotes ); //updating velocity
                    if( !isPadSelected && ( i >= idxStart && i < idxStart + 16 ) ) //it should
                    {
                        colourID[colourIndex++]   = contracts::DeviceControlContracts::PadColourLightSkyBlue;
                        velocity[velocityIndex++] = ( data.indexRatchetValues[(size_t)i] * 127 ) / 8;
                    }
                }
            }
            else if( !isPadSelected && ( i >= idxStart && i < idxStart + 16 ) )
            {
                colourID[colourIndex++]   = contracts::DeviceControlContracts::PadColourTrans;
                velocity[velocityIndex++] = 0;
            }

            // Next Step.
            stepRange.startTime += data.ticksPerQl;
            stepRange.endTime += data.ticksPerQl;
        }

        if( !isPadSelected && ( kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::HomeSequencerView ) )
        {
            contracts::DclSignals::sendCommandToDcl(
                contracts::DclContractsHelper::setPadColourByVelocityMessage( contracts::DeviceControlContracts::PadButtonState::StateOn, colourID, velocity ) );
        }
        if( control.paramValueListBox.getSelected() == "Velocity" )
            preset.setPresetValues( data.indexValues, MIDI::MIN, MIDI::MAX );
        else if( control.paramValueListBox.getSelected() == "Probability" )
            preset.setPresetValues( data.indexProbValues, 0, 100 );
        else if( control.paramValueListBox.getSelected() == "Ratchet" )
            preset.setPresetValues( data.indexRatchetValues, 1, 8 );

        repaint();
        if( onFirstLaunch )
        {
            onFirstLaunch = false;
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( mt::PadColorMode::DISABLE_PAD_COLOUR_MODE ) ); //update the mode only for current screen
        }
    };

    void StepComponent::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, getLocalBounds(), ui::Theme::backgroundColour( 1 ), 5.0f );

        // draw Level markings.
        {
            const auto lineMarkings = 6;
            g.setColour( ui::Theme::outlineColour( 1 ).withBrightness( 0.1 ) );

            auto h  = noteArea.getHeight() / lineMarkings;
            int  y  = noteArea.getY();
            auto sx = noteArea.getX();
            auto ex = sx + noteArea.getWidth();
            for( auto i = 0; i <= lineMarkings; ++i )
            {
                g.drawHorizontalLine( y, sx, ex );
                y += h;
            }
        }
        //draw note steps..
        drawSteps( g );
    }

    void StepComponent::drawSteps( juce::Graphics& g )
    {
        auto nArea = noteArea.reduced( STEP_SEQ_SPACE, 0 );
        auto bArea = buttonArea.reduced( STEP_SEQ_SPACE, 0 );

        juce::ColourGradient fillColour = juce::ColourGradient::vertical( juce::Colour( 0xff, 0x2a, 0x00 ), juce::Colour( 0xff, 0xb6, 0x08 ), nArea );

        auto offset    = preset.getOffset();
        auto page      = control.getPage();
        auto idxStart  = ( page * data.numStepsPerPage );
        auto beatNum   = gridData.getNumQlPerBar( control.getBar() ) / gridData.getBeatsPerBar( control.getBar() );
        auto beatValue = ( beatNum == 0 ) ? 1 : beatNum;

        for( auto i = 0; idxStart < data.numSteps && i < data.numStepsPerPage; ++i, ++idxStart )
        {
            auto nA = nArea.removeFromLeft( data.stepWidth ).reduced( STEP_BUTTON_SPACE, 0 );
            auto bA = bArea.removeFromLeft( data.stepWidth ).reduced( STEP_BUTTON_SPACE, 0 );

            // Note Background
            auto colour = ( 0 == ( idxStart % beatValue ) ) ? 3 : 5;
            mt::utils::fillRoundedRect( g, nA, ui::Theme::backgroundColour( colour ) );

            // Note Fill..
            if( control.paramValueListBox.getSelected() == "Velocity" && data.selected[idxStart] )
            {
                if( idxStart < data.indexValues.size() )
                {
                    auto velocity = juce::jlimit( MIDI::MIN, MIDI::MAX, ( data.indexValues[idxStart] + offset ) );
                    auto h        = nA.proportionOfHeight( ( MIDI::MAX - velocity ) / 127.f );
                    auto fA       = nA.withTrimmedTop( h );
                    mt::utils::fillGradientRoundedRect( g, fA, fillColour, 1.0f );
                }
            }
            else if( control.paramValueListBox.getSelected() == "Probability" && data.selected[idxStart] )
            {
                if( idxStart < data.indexProbValues.size() )
                {
                    auto probability = juce::jlimit( 0, 100, ( data.indexProbValues[idxStart] + offset ) );
                    auto h           = nA.proportionOfHeight( ( 100 - probability ) / 100.f );
                    auto fA          = nA.withTrimmedTop( h );
                    mt::utils::fillRoundedRect( g, fA, juce::Colours::skyblue, 1.0f, true, true, false, false );
                }
            }
            else if( control.paramValueListBox.getSelected() == "Ratchet" && data.selected[idxStart] )
            {
                if( idxStart < data.indexRatchetValues.size() )
                {
                    auto ratchet = juce::jlimit( 0, 8, ( data.indexRatchetValues[idxStart] + offset ) );
                    auto h       = nA.proportionOfHeight( ( 8 - ratchet ) / 8.f );
                    auto fA      = nA.withTrimmedTop( h );
                    mt::utils::fillRoundedRect( g, fA, juce::Colours::skyblue, 1.0f, true, true, false, false );
                }
            }

            // Button...
            std::unique_ptr<juce::Drawable> image;
            if( data.selected[idxStart] )
                image = ui::Theme::getDrawable( "stepseq/active/button" + juce::String( i + 1 ) + ".svg" );
            else
                image = ui::Theme::getDrawable( "stepseq/default/button" + juce::String( i + 1 ) + ".svg" );
            //g.drawImage( image, bA.toFloat(), juce::RectanglePlacement::centred );
            image->drawWithin( g, bA.toFloat(), juce::RectanglePlacement::centred, 1.0f );
        }
    }

    void StepComponent::setMinimumSize( int W, int H )
    {
        minWidth = W;
        height   = H;

        // calc steps to display on one page.
        data.numStepsPerPage = juce::jlimit( 1, 16, data.timediv );
        data.stepWidth       = juce::jmax( StepSequencerConstants::stepMinWidth, ( ( ( ( W - ( 2 * STEP_SEQ_SPACE ) - STEP_SEQ_WIDTH ) ) / data.numStepsPerPage ) ) );

        // Also update number of pages.
        control.setMaxPages( std::ceil( (float)data.numSteps / (float)data.numStepsPerPage ) );

        // Set canvas size.
        width = juce::jmax( minWidth, ( data.stepWidth * data.numSteps ) );
        setSize( width, height );
    }

    void StepComponent::resized()
    {
        noteArea   = getLocalBounds().reduced( StepSequencerConstants::space );
        buttonArea = noteArea.removeFromBottom( StepSequencerConstants::stepButtonHeight );
        noteArea.removeFromBottom( StepSequencerConstants::space );
    }

    int StepComponent::getStepFromX( int X )
    {
        auto bArea = buttonArea.reduced( STEP_SEQ_SPACE, 0 );
        auto idx   = 0;
        for( ; idx < data.numStepsPerPage; ++idx )
        {
            auto bA = bArea.removeFromLeft( data.stepWidth ).reduced( STEP_BUTTON_SPACE, 0 );
            auto sX = bA.getX();
            auto eX = sX + bA.getWidth();
            if( sX <= X && X <= eX )
                break;
        }
        return ( idx >= data.numStepsPerPage ) ? -1 : ( idx + ( data.numStepsPerPage * control.getPage() ) );
    }

    float StepComponent::getNormalizedHeightFromY( int Y )
    {
        auto minY   = noteArea.getY();
        auto spread = noteArea.getHeight();
        auto maxY   = minY + spread;
        return ( spread > 0 ) ? (float)( maxY - Y ) / (float)( spread ) : 0;
    }

    void StepComponent::toggleStep( int Index, int Value )
    {
        if( juce::isPositiveAndBelow( Index, data.numSteps ) ) // Safety Check.
        {
            auto st  = range.startTime + ( Index * data.ticksPerQl );
            auto et  = st + data.ticksPerQl;
            auto bar = gridData.getBarFromX( st );

            data.selected[(size_t)Index] = !data.selected[(size_t)Index];
            if( data.selected[(size_t)Index] )
            {
                if( control.paramValueListBox.getSelected() == "Velocity" )
                {
                    data.indexProbValues[(size_t)Index]    = 100;
                    data.indexRatchetValues[(size_t)Index] = 1;
                    ae.addOrModifyNote( range.noteStart, st, gridData.getTicksPerQuantization( bar ), Value, data.indexProbValues[(size_t)Index], data.indexRatchetValues[(size_t)Index] );
                }
                else if( control.paramValueListBox.getSelected() == "Probability" )
                {
                    data.indexValues[(size_t)Index]        = MIDI::MAX;
                    data.indexRatchetValues[(size_t)Index] = 1;
                    ae.addOrModifyNote( range.noteStart, st, gridData.getTicksPerQuantization( bar ), data.indexValues[(size_t)Index], Value, data.indexRatchetValues[(size_t)Index] );
                }
                else if( control.paramValueListBox.getSelected() == "Ratchet" )
                {
                    data.indexValues[(size_t)Index]     = MIDI::MAX;
                    data.indexProbValues[(size_t)Index] = 100;
                    ae.addOrModifyNote( range.noteStart, st, gridData.getTicksPerQuantization( bar ), data.indexValues[(size_t)Index], data.indexProbValues[(size_t)Index], Value );
                }
            }
            else
            {
                ae.deleteNoteInTimeRange( aeSel, st, et );
            }
            triggerAsyncUpdate();
        }
    }

    void StepComponent::knobRotated( int Index, int value )
    {
        if( juce::isPositiveAndBelow( Index, data.numSteps ) ) // Velocity
        {
            auto st  = range.startTime + ( Index * data.ticksPerQl );
            auto et  = st + data.ticksPerQl;
            auto bar = gridData.getBarFromX( st );

            contracts::DeviceControlContracts::PadColourIds padColour = contracts::DeviceControlContracts::PadColourTrans;
            data.selected[(size_t)Index]                              = !data.selected[(size_t)Index];

            auto velocity    = data.indexValues[Index];
            auto probability = data.indexProbValues[Index];
            auto ratchet     = data.indexRatchetValues[Index];

            if( control.paramValueListBox.getSelected() == "Velocity" )
                velocity = juce::jlimit( MIDI::MIN, MIDI::MAX, ( data.indexValues[Index] + value ) );
            else if( control.paramValueListBox.getSelected() == "Probability" )
                probability = juce::jlimit( 0, 100, ( data.indexProbValues[Index] + value ) );
            else if( control.paramValueListBox.getSelected() == "Ratchet" )
                ratchet = juce::jlimit( 1, 8, ( data.indexRatchetValues[Index] + value ) );

            ae.addOrModifyNote( range.noteStart, st, gridData.getTicksPerQuantization( bar ), velocity, probability, ratchet );

            triggerAsyncUpdate();
        }
    }

    void StepComponent::updateStepValue( int Index, int Value )
    {
        if( juce::isPositiveAndBelow( Index, data.numSteps ) ) // Safety Check.
        {
            auto r = range;
            r.startTime += ( Index * data.ticksPerQl );
            r.endTime = ( r.startTime + data.ticksPerQl );

            if( control.paramValueListBox.getSelected() == "Velocity" )
                ae.editNoteVelocity( aeSel, r, Value );
            else if( control.paramValueListBox.getSelected() == "Probability" )
                ae.editNoteProbability( aeSel, r, Value );
            else if( control.paramValueListBox.getSelected() == "Ratchet" )
                ae.editNoteRatchet( aeSel, r, Value > 0 ? Value : 1 );
            triggerAsyncUpdate();
        }
    }

    void StepComponent::updateStepValue( int Index, float Normalized )
    {
        if( control.paramValueListBox.getSelected() == "Velocity" )
        {
            updateStepValue( Index, (int)( MIDI::MAX * Normalized ) );
        }
        else if( control.paramValueListBox.getSelected() == "Probability" )
            updateStepValue( Index, (int)( 100 * Normalized ) );
        else if( control.paramValueListBox.getSelected() == "Ratchet" )
            updateStepValue( Index, (int)( 8 * Normalized ) );
    }

    void StepComponent::nudge( bool direction )
    {
        if( ae.getNoteList().getNumChildren() > 0 )
        {
            ae.nudgeNotes( aeSel, range.noteStart, ( direction ? data.ticksPerQl : -data.ticksPerQl ) );
            triggerAsyncUpdate();
        }
    }

    void StepComponent::handleMouse( const juce::MouseEvent& event )
    {
        auto index = getStepFromX( event.position.getX() );
        if( noteArea.contains( event.position.toInt() ) )
        {
            updateStepValue( index, getNormalizedHeightFromY( event.position.getY() ) );
        }
        else if( buttonArea.contains( event.position.toInt() ) && ( lastIndex != index ) )
        {
            if( control.paramValueListBox.getSelected() == "Velocity" )
                toggleStep( index, MIDI::MAX );
            else if( control.paramValueListBox.getSelected() == "Probability" )
                toggleStep( index, 100 );
            else if( control.paramValueListBox.getSelected() == "Ratchet" )
                toggleStep( index, 1 );
            lastIndex = index;
        }
    }

    void StepComponent::mouseUp( const juce::MouseEvent& event )
    {
        if( !event.mouseWasDraggedSinceMouseDown() )
        {
            lastIndex = -1;
            handleMouse( event );
        }
    }

    void StepComponent::mouseDrag( const juce::MouseEvent& event )
    {
        handleMouse( event );
    }

    /////////////////////////////////////////////////////////////////
    ///
    /// Class: StepSequencer
    ///
    /////////////////////////////////////////////////////////////////

    StepSequencer::StepSequencer()
        : kick_ui::IScreen( Screen::HomeSequencerView )
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );

        //header
        header.onBackButtonPressed = [this]( bool ) { closeScreen( Screen::HomeListView ); };

        // Setup step sequencer...
        mt::utils::addAndMakeVisible( this, stepControl, stepPreset, stepViewport, header, topControl, tracks, totalBars, trackLength, bpmTempo, warningLabel );
        mt::utils::setTitleProportion( .4f, totalBars, bpmTempo );
        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), totalBars, bpmTempo );
        mt::utils::setTitleFontHeight( 16, totalBars, bpmTempo, tracks );

        warningLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        warningLabel.setValueFontHeight( 25 );

        stepViewport.setViewedComponent( &stepComponent, false );
        stepViewport.setScrollBarPosition( false, true );

        trackLength.setTitleColour( ui::Theme::titleColour( 3 ) );
        trackLength.setJustificationType( juce::Justification::centredLeft );

        tracks.setTitleColour( ui::Theme::titleColour( 2 ) );
        tracks.setJustificationType( juce::Justification::centredLeft );

        // Handle control changes...
        stepControl.leftNudgeButton.onClick = [&]() {
            stepComponent.nudge( false );
            stepControl.leftNudgeButton.setToggleState( false, juce::dontSendNotification );
        };

        stepControl.rightNudgeButton.onClick = [&]() {
            stepComponent.nudge( true );
            stepControl.rightNudgeButton.setToggleState( false, juce::dontSendNotification );
        };

        stepControl.clearpad.onClick = [&]() {
            clearPadData();
            stepControl.clearpad.setToggleState( false, juce::dontSendNotification );
        };

        stepControl.clearbar.onClick = [&]() {
            clearBarData();
            stepControl.clearbar.setToggleState( false, juce::dontSendNotification );
        };

        stepControl.padMinus.onClick = [&]() {
            topControl.padValueListBox.knobRotated( -1 );
            stepControl.padMinus.setToggleState( false, juce::dontSendNotification );
            if( isPadSelect )
            {
                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setPadColourByVelocityMessage(
                        lastPressedPad, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourOrange ) );

                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setPadColourByVelocityMessage(
                        topControl.padValueListBox.getSelectedIndex() % 16, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );

                lastPressedPad = topControl.padValueListBox.getSelectedIndex() % 16;
            }
        };

        stepControl.padPlus.onClick = [&]() {
            topControl.padValueListBox.knobRotated( 1 );
            stepControl.padPlus.setToggleState( false, juce::dontSendNotification );
            if( isPadSelect )
            {
                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setPadColourByVelocityMessage(
                        lastPressedPad, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourOrange ) );

                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setPadColourByVelocityMessage(
                        topControl.padValueListBox.getSelectedIndex() % 16, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );

                lastPressedPad = topControl.padValueListBox.getSelectedIndex() % 16;
            }
        };

        stepControl.barPlus.onClick = [&]() {
            topControl.barValueListBox.knobRotated( 1 );
            stepControl.barPlus.setToggleState( false, juce::dontSendNotification );
            if( isPadSelect )
            {
                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setPadColourByVelocityMessage(
                        lastPressedPad, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourOrange ) );

                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setPadColourByVelocityMessage(
                        topControl.padValueListBox.getSelectedIndex() % 16, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );
            }
        };

        stepControl.barMinus.onClick = [&]() {
            topControl.barValueListBox.knobRotated( -1 );
            stepControl.barMinus.setToggleState( false, juce::dontSendNotification );
            if( isPadSelect )
            {
                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setPadColourByVelocityMessage(
                        lastPressedPad,
                        contracts::DeviceControlContracts::PadButtonState::StateOn,
                        contracts::DeviceControlContracts::PadColourOrange ) );

                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setPadColourByVelocityMessage(
                        topControl.padValueListBox.getSelectedIndex() % 16,
                        contracts::DeviceControlContracts::PadButtonState::StateOn,
                        contracts::DeviceControlContracts::padColourlightGreen1 ) );
            }
        };

        stepControl.padSelect.onClick = [&]() {
            isPadSelect = stepControl.padSelect.getToggleState();
            if( isPadSelect )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( topControl.padValueListBox.getSelectedIndex() / 16 ) );
                topControl.setPageEnable( false );

                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setAllPadColourByVelocityMessage(
                        contracts::DeviceControlContracts::PadButtonState::StateOn,
                        contracts::DeviceControlContracts::PadColourOrange ) );

                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setPadColourByVelocityMessage(
                        topControl.padValueListBox.getSelectedIndex() % 16,
                        contracts::DeviceControlContracts::StateOn,
                        contracts::DeviceControlContracts::padColourlightGreen1 ) );
            }
            else
            {
                topControl.setPageEnable( true );
                stepControl.padSelect.setToggleState( false, juce::dontSendNotification );
                stepComponent.refresh();
            }
            stepComponent.setPadSelect( isPadSelect );
        };

        totalBars.onValueChanged = [&]( int V ) {
            ae.setSequenceLength( aeSel.sequence, V );
            refresh();
        };

        bpmTempo.onValueChanged = [&]( int Value ) {
            ae.setSequenceBPM( aeSel.sequence, Value );
        };

        gridData.onNoteSelectionChanged.connect( [this]( int SelectedNote ) {
            topControl.padValueListBox.setSelected( SelectedNote, juce::dontSendNotification );
            stepComponent.refresh();
        } );

        topControl.padValueListBox.onSelectionChanged = [&]( int Index, const juce::String& ) {
            gridData.onNoteSelectionChanged( Index );
        };

        topControl.timeDivValueListBox.onSelectionChanged = [&]( int, const juce::String& Value ) {
            auto tempoMgr = ae.getTempoMgr( aeSel.sequence );
            if( tempoMgr )
                tempoMgr->setQuantization( gridData.getTimeBase( Value ) );
            stepComponent.refresh();
        };

        topControl.paramValueListBox.onSelectionChanged = [&]( int Index, const juce::String& Param ) {
            stepComponent.refresh();
        };

        tracks.onSelectionChanged = [&]( int Sel, const juce::String& ) {
            ae.setSelectedTrack( Sel );
            refresh();
        };

        ui::themeChanged.connect( [&]() {
            header.setIcon( "Icon_Stepsequencer" );
        } );
    }
    int StepSequencer::lastPressedPad = -1;

    void StepSequencer::resized()
    {
        header.setBoundsRelative( .0f, .0f, 1.0f, .1423f );
        tracks.setBoundsRelative( .230, .0299f, .1956f, .0823f );
        trackLength.setBoundsRelative( 440, .0299f, .1565f, .0823f );
        bpmTempo.setBoundsRelative( .7612f, .0299f, .0724f, .0823f );
        totalBars.setBoundsRelative( .6692f, .0299f, .0724f, .0823f );
        stepControl.setBoundsRelative( .0f, .9213f, 1.0f, .1011f );
        topControl.setBoundsRelative( .0100f, .1610f, .9814f, .0823f );
        stepViewport.setBoundsRelative( .010f, .2621f, .8775f, .6404f );
        stepPreset.setBoundsRelative( .8943f, .2621f, .0978f, .6404f );
        warningLabel.setBounds( getLocalBounds().removeFromTop( 300 ).withTrimmedTop( 200 ).removeFromLeft( 900 ).withTrimmedLeft( 300 ) );

        stepComponent.setMinimumSize( stepViewport.getWidth(), stepViewport.getHeight() );
    }

    void StepSequencer::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_B:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_C:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_D:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_E:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_F:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_G:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_H:
                    if( isPadSelect )
                    {
                        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( buttonCode ) );
                        contracts::DclSignals::sendCommandToDcl(
                            contracts::DclContractsHelper::setAllPadColourByVelocityMessage(
                                contracts::DeviceControlContracts::PadButtonState::StateOn,
                                contracts::DeviceControlContracts::PadColourOrange ) );

                        auto offset = ae.getBankOffset();
                        if( topControl.padValueListBox.getSelectedIndex() >= ae.getBankOffset() && topControl.padValueListBox.getSelectedIndex() <= ae.getBankOffset() + 16 )
                        {
                            contracts::DclSignals::sendCommandToDcl(
                                contracts::DclContractsHelper::setPadColourByVelocityMessage(
                                    topControl.padValueListBox.getSelectedIndex() % 16, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );

                            //Controls::InputEvents::setPadColourByVelocity( topControl.padValueListBox.getSelectedIndex() % 16, Controls::PadButtonState::StateOn, Controls::padColourlightGreen1 );
                            lastPressedPad = topControl.padValueListBox.getSelectedIndex() % 16;
                        }
                        else
                            lastPressedPad = -1;
                    }
                    else
                    {
                        //TODO - Bank Updation not proper 4T,8T,16T... need to take it later
                        auto maxPage = data.numSteps / data.numStepsPerPage;
                        if( buttonCode < maxPage )
                        {
                            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( buttonCode ) );
                            stepComponent.setPage( buttonCode );
                            stepComponent.asyncUpdate();
                        }
                    }
                default: break;
            }
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
            }
        }
        else if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            int padNum      = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            int padState    = contracts::DclContractsHelper::getPadState( f_HwMsg );
            int padVelocity = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );

            if( isPadSelect )
            {
                if( lastPressedPad < 0 )
                {
                    lastPressedPad = padNum;

                    contracts::DclSignals::sendCommandToDcl(
                        contracts::DclContractsHelper::setPadColourByVelocityMessage(
                            padNum, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );

                    lastPressedPad = padNum;
                    topControl.padValueListBox.setSelected( padNum + ae.getBankOffset() );
                }
                else if( lastPressedPad != padNum )
                {
                    contracts::DclSignals::sendCommandToDcl(
                        contracts::DclContractsHelper::setPadColourByVelocityMessage(
                            lastPressedPad, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourOrange ) );

                    contracts::DclSignals::sendCommandToDcl(
                        contracts::DclContractsHelper::setPadColourByVelocityMessage(
                            topControl.padValueListBox.getSelectedIndex() % 16, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );

                    lastPressedPad = padNum;
                    topControl.padValueListBox.setSelected( padNum + ae.getBankOffset() );
                }
            }
            else if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
            {
                if( contracts::DclContractsHelper::isHalfLevel() )
                {
                    if( topControl.paramValueListBox.getSelected() == "Velocity" )
                    {
                        auto currentIndex = ( stepComponent.getPage() * data.numStepsPerPage ) + padNum;
                        stepComponent.toggleStep( currentIndex, padVelocity );
                    }
                }
                else
                {
                    if( topControl.paramValueListBox.getSelected() == "Velocity" )
                    {
                        auto currentIndex = ( stepComponent.getPage() * data.numStepsPerPage ) + padNum;
                        stepComponent.toggleStep( currentIndex );
                    }
                }

                if( topControl.paramValueListBox.getSelected() == "Probability" )
                    stepComponent.toggleStep( padNum, 100 );
                else if( topControl.paramValueListBox.getSelected() == "Ratchet" )
                    stepComponent.toggleStep( padNum, 1 );
            }
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob         = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto dir          = contracts::DclContractsHelper::getKnobDirection( f_HwMsg );
            auto value        = contracts::DclContractsHelper::getKnobValue( f_HwMsg );

            if( knob != contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER )
                stepComponent.knobRotated( knob - 1, value );
            else
            {
                switch( knob )
                {
                    case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER:

                        if( dir == contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE )
                        {
                            stepComponent.nudge( TRUE );
                        }
                        else
                        {
                            stepComponent.nudge( FALSE );
                        }
                        break;
                }
            }
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) )
        {
            int BtnNum = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            switch( BtnNum )
            {
                case contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_CW:
                    stepComponent.nudge( TRUE );
                    break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_CCW:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS:
                    stepComponent.nudge( FALSE );
                    break;
            }
        }
    }

    void StepSequencer::visibilityChanged()
    {
        stepComponent.setOnFirstLaunch( isVisible() );

        for( auto it : getChildren() )
            it->setVisible( isVisible() );

        if( isVisible() )
        {
            if( !ae.getNumTracks( ae.getSelectedSequence() ) )
            {
                for( auto it : getChildren() )
                    it->setVisible( !isVisible() );

                header.setVisible( true );
                warningLabel.setVisible( true );
            }
            else
            {
                lastPressedPad = topControl.padValueListBox.getSelectedIndex() % 16;
                tracks.setListOptions( ae.getTrackList( aeSel.sequence ), aeSel.track );
                auto tempoMgr = ae.getTempoMgr( aeSel.sequence );
                auto timeDiv  = tempoMgr ? tempoMgr->getQuantization( aeSel.sequence ) : magic_enum::enum_integer<mt::MTTimeBase>( mt::defaultQuantize );
                topControl.timeDivValueListBox.setSelected( gridData.getTimeBaseIndex( timeDiv ), juce::sendNotification );
                setTempo();
                stepControl.padSelect.setToggleState( false, juce::dontSendNotification );
                //Updating PadSelectState
                stepComponent.setPadSelect( false );
                isPadSelect = false;
                topControl.setPageEnable( true );
                warningLabel.setVisible( false );
            }
        }
        else
        {
            auto MTIProg = ae.getProgramData( ae.getSelectedProgram() );

            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( MTIProg ? MTIProg->colourMode.get() : mt::OFF ) );

            contracts::DclSignals::refreshPadLeds();
        }
    }

    void StepSequencer::refresh()
    {
        auto length = ae.getSequenceLength( aeSel.sequence );
        totalBars.setValue( length, juce::dontSendNotification );
        topControl.setTotalBars( length );
        stepComponent.refresh();
    }

    void StepSequencer::clearPadData()
    {
        range.noteStart = range.noteEnd = topControl.padValueListBox.getSelectedIndex();
        range.startTime                 = 0 * gridData.getTicksUptoBar( topControl.barValueListBox.getNumOptions() );
        range.endTime                   = topControl.barValueListBox.getNumOptions() * gridData.getTicksUptoBar( topControl.barValueListBox.getNumOptions() );
        ae.deleteNotesInRange( aeSel, range );
        stepComponent.refresh();
    }

    void StepSequencer::clearBarData()
    {
        range.noteStart = range.noteEnd = topControl.padValueListBox.getSelectedIndex();
        range.startTime                 = topControl.barValueListBox.getSelectedIndex() * gridData.getTicksPerBar( topControl.barValueListBox.getSelectedIndex() + 1 );
        range.endTime                   = ( topControl.barValueListBox.getSelectedIndex() + 1 ) * gridData.getTicksPerBar( topControl.barValueListBox.getSelectedIndex() + 1 );
        ae.deleteNotesInRange( aeSel, range );
        stepComponent.refresh();
    }

    void StepSequencer::setTempo()
    {
        auto pSeq = ae.getSequenceData( aeSel.sequence );
        bpmTempo.setValue( pSeq ? pSeq->tempo.get() : mt::getSettings().projects.tempo(), juce::dontSendNotification );
    }

    void StepSequencer::handleAsyncUpdate()
    {
        repaint();
    }

    void StepSequencer::mouseDrag( const juce::MouseEvent& event )
    {
    }

    void StepSequencer::mouseUp( const juce::MouseEvent& )
    {
    }

    void StepSequencer::mouseDown( const juce::MouseEvent& event )
    {
    }
} //kick_ui
