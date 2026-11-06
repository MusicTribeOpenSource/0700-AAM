#include "kick_Sampler.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/dcl/dcl_signals.h"

namespace kick_ui
{
    int Sampler::lastPressedPad = -1;
    Sampler::Sampler()
        : kick_ui::IScreen( kick_ui::Screen::MenuSampler )
    {
        mt::utils::addChildComponents( this, header, options, recorder, clear, exportt, sample, slice, padTap, padHold, cancel, slicePlus, save, play, discard, edit );
        mt::utils::makeHidden( cancel, slicePlus, save, play, discard, edit );
        mt::utils::setRadioGroupID( (int)kick_ui::RadioGroupId::SampleActions, sample, slice, padTap, padHold );
        mt::utils::setValueFontHeight( 18, sample, slice, padTap, padHold, cancel, slicePlus, save, play, discard, edit );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), sample, slice, padTap, padHold, cancel, slicePlus, save, play, discard, edit );

        options.showPanSlider( false );
        options.showRecordButton( false );
        options.showOutputs( false );

        options.setInputs( { "IN L - OUT MONO", "IN R - OUT MONO", "IN L+R - OUT MONO", "IN L - OUT STEREO", "IN R - OUT STEREO", "IN L+R - OUT STEREO" }, 5 );
        recorder.signalStrengthChanged.connect( [this]( mt::MTMeterData& values ) {
            options.signalStrengthChanged( { values[0].rms, values[1].rms, values[0].peak, values[1].peak } );
        } );

        recorder.recordModeChanged.connect( [&]( SamplerRecorder::RecordMode Mode ) {
            recordModeChanged( Mode );
        } );

        options.inputChanged.connect( [this]( int numChannels, const juce::String& ) {
            recorder.setRecordChannels( ( SamplerRecorder::RecordChannels )( numChannels - 1 ) );
        } );

        options.gainChanged.connect( [this]( float gain ) {
            recorder.setRecordThresholdDB( gain );
        } );

        options.outputChanged.connect( [this]( int numChannels, const juce::String& str ) {
        } );

        options.monitorStateChanged.connect( [this]( bool on ) {
            recorder.setMonitorOn( on );
        } );

        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        recorder.recordingStarted.connect( [this]( bool on ) {
            if( on )
            {
                exportt.setEnabled( on );
                clear.setEnabled( on );
            }
            //recorder.recording = on;
        } );

        exportt.onClick = [&]() {
            if( mode == SamplerRecorder::RecordMode::recordModeStopped )
            {
                // Export The Current Sample
            }
            exportt.setToggleState( false, juce::dontSendNotification );
        };
        clear.onClick = [&]() {
            //Clear All and Reset The Recording
            clear.setToggleState( false, juce::dontSendNotification );
        };

        /// Sampler Actions

        cancel.onClick = [&]() {
            auto p = new ui::FilePopup( "Do you want to cancel this Process " );
            mt::utils::showDialog(
                "WARNING!", p, [p, this]( int R ) {
                    if( R == UI::APPLY )
                    {
                        recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeNone );
                        recorder.clearRecording();
                    }
                    delete p;
                },
                ui::layout::warningPopup );
        };

        slicePlus.onClick = [&]() {
            if( samplePoints.size() == 0 )
                samplePoints.push_back( 0 );

            samplePoints.push_back( recorder.getCurrentRecordTime() );
        };

        save.onClick = [&]() {
            openSaveFilePopUp( false );
        };

        play.onClick = [&]() {
            if( play.getToggleState() )
                ae.startRecorderPreview( 0.0 );
            else
                ae.stopRecorderPreview();
        };

        discard.onClick = [&]() {
            recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeNone );
            recorder.clearRecording();
        };

        edit.onClick = [&]() {
            openSaveFilePopUp( true );
        };

        sample.onClick = [&]() {
            buttonState = SAMPLE;
        };

        slice.onClick = [&]() {
            buttonState = SLICE;
        };

        padTap.onClick = [&]() {
            buttonState = PADTAP;
            refreshPadsLED();
            lastPressedPad = -1;
        };

        padHold.onClick = [&]() {
            buttonState = PADHOLD;
            refreshPadsLED();
        };

        sample.triggerClick();

        ui::themeChanged.connect( [&]() {
            exportt.setImages( "Song_Export", "Song_Export_Active" );
            clear.setImages( "Song_Clear", "Song_Clear_Active" );
            header.setIcon( "Icon_Sampler" );
            sample.setToggleButtonText( "Sample", "Sample" );
            slice.setToggleButtonText( "Slice", "Slice" );
            play.setToggleButtonText( "Play", "Stop" );
        } );

        mt::signals::bankChanged.connect( [&]( int Index, juce::String Bank ) {
            if( kick_ui ::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuSampler )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( Index ) );
                if( padTap.getToggleState() || padHold.getToggleState() )
                {
                    refreshPadsLED();
                    lastPressedPad = -1;
                }
            }
        } );
    }

    void Sampler::recordModeChanged( SamplerRecorder::RecordMode newMode )
    {
        mode = newMode;
        if( mode == SamplerRecorder::RecordMode::recordModeNone )
        {
            startSamplePoints.clear();
            endSamplePoints.clear();
            samplePoints.clear();
            lastPressedPad = -1;
            mt::utils::makeVisible( sample, slice, padTap, padHold );
            mt::utils::makeHidden( cancel, slicePlus, save, play, discard, edit );
        }
        else if( mode == SamplerRecorder::RecordMode::recordModeRecord )
        {
            mt::utils::makeVisible( sample, slice, padTap, padHold, cancel );
            mt::utils::makeHidden( slicePlus, save, play, discard, edit );
        }
        else if( mode == SamplerRecorder::RecordMode::recordModeRecording )
        {
            mt::utils::makeVisible( slicePlus );
            mt::utils::makeHidden( sample, slice, padTap, padHold, cancel, save, play, discard, edit );
        }
        else if( mode == SamplerRecorder::RecordMode::recordModeStopped )
        {
            mt::utils::makeVisible( save, play, discard, edit );
            mt::utils::makeHidden( sample, slice, padTap, padHold, cancel, slicePlus );
        }
    }

    juce::String Sampler::saveSample( const juce::String& sampleName )
    {
        juce::File filename;
#if KICK_EMU
        // A leading slash makes the sibling an absolute path at the host's root, and juce::File
        // asserts on that under Windows. The emulator keeps the sample next to the project.
        const juce::String slash;
#else
        const juce::String slash( "/" );
#endif
        if( sampleName.isEmpty() )
            filename = ae.getProjectDir().getSiblingFile( slash + "newSample.wav" );
        else
            filename = ae.getProjectDir().getSiblingFile( slash + sampleName + ".wav" );
        juce::File f = filename.getNonexistentSibling();
        recorder.getRecordFile().copyFileTo( f );
        return f.getFullPathName();
    }

    void Sampler::openSaveFilePopUp( bool isEditing )
    {
        KeepDiscardSampleComponent* process = new KeepDiscardSampleComponent( buttonState );
        mt::utils::showOptions( "KEEP OR DISCARD SAMPLES", process, [process, this, isEditing]( int r ) {
            if( r == UI::BUTTONS::APPLY )
            {
                auto fileName  = process->getSampleName();
                auto padIndex  = process->getPadIndex();
                auto selPrg    = process->getSelectedProgram();
                auto sliceSave = process->getSliceSaveTypeIndex();

                delete process;
                auto sampleFileName = saveAndLoadRecordedSample( fileName, padIndex, selPrg, sliceSave );
                if( isEditing )
                {
                    if( juce::File( sampleFileName ).exists() )
                    {
                        kick_ui::sigSampleEditLoadFileForEditing( sampleFileName, SampleEditorMode::modeChop );
                        kick_ui::signals::sigScreenOpen( kick_ui::Screen::MenuSampleEdit );
                    }
                }
            }
            else
                delete process;
        } );
    }

    juce::String Sampler::saveAndLoadRecordedSample( const juce::String& fileName, int PadIndex, int SelProgram, int SliceSaveType )
    {
        auto filePath   = getValidFileName( ae.getProjectDir().getSiblingFile( fileName + ".wav" ).getFullPathName().toStdString() );
        auto sampleFile = ae.getProjectDir().getSiblingFile( filePath ).getFullPathName().toStdString();
        ae.saveRecording( sampleFile );
        recorder.clearRecording();
        ae.addAsset( sampleFile, mt::AssetTypes::Samples );
        addSlicesToSample( sampleFile );
        if( SelProgram != 0 && buttonState == SAMPLE )
        {
            auto progType = ae.getProgram( ae.getSelectedProgram() )->getType();
            if( progType == mt::MTProgramType::Drum || progType == mt::MTProgramType::Keygroup || progType == mt::MTProgramType::Clip || progType == mt::MTProgramType::Audio )
                ae.addSampleToProgram( ae.getSelectedProgram(), PadIndex, sampleFile );
        }
        if( buttonState == SLICE && SliceSaveType != 0 )
        {
            auto progIndex = ae.createProgram( mt::MTProgramType::Drum, juce::File( sampleFile ).getFileNameWithoutExtension() );
            if( SliceSaveType == 2 )
            {
                if( samplePoints.size() > 1 )
                    addSlicesToSample( progIndex, sampleFile, true );
                else
                    ae.addSampleToProgram( progIndex, 0, sampleFile );
            }
        }
        else if( buttonState == PADTAP )
        {
            addSlicesToPads( sampleFile );
        }
        else if( buttonState == PADHOLD )
        {
            addSlicesToPads( sampleFile );
        }

        recordModeChanged( SamplerRecorder::RecordMode::recordModeNone );

        return sampleFile;
    }

    void Sampler::addSlicesToPads( const juce::String& SampleFile )
    {
        int index = 0;
        for( auto samplePoint : startSamplePoints )
        {
            auto loopStart = (int)( samplePoint.second * 44100 );
            auto loopEnd   = (int)( endSamplePoints[samplePoint.first] * 44100 );
            if( index == startSamplePoints.size() - 1 && buttonState == 2 )
                loopEnd = (int)( recorder.getCurrentRecordTime() * 44100 );

            addAssetSlice( SampleFile, loopStart, loopEnd, index + 1 );
            ae.addSampleToProgram( ae.getSelectedProgram(), samplePoint.first, SampleFile, index + 1 );
            ++index;
        }
    }

    juce::String Sampler::getValidFileName( const std::string& filePath )
    {
        std::filesystem::path path( filePath );
        std::string           base      = path.parent_path().string();
        std::string           file      = path.stem().string();
        std::string           extension = path.extension().string();
        int                   counter   = 1;

        while( std::filesystem::exists( path ) )
        {
            path = base + "\\" + file + " " + std::to_string( counter ) + extension;
            counter++;
        }
        return path.filename().string();
    }

    void Sampler::addSlicesToSample( const juce::String& SampleFile )
    {
        if( juce::File( SampleFile ).exists() )
        {
            sigSampleEditLoadFileForEditing( SampleFile, SampleEditorMode::modeChop );

            for( auto it = samplePoints.begin(); it != samplePoints.end(); it++ )
                sigSampleEditAddRegionAt( *it );

            addSlicesToSample( ae.getSelectedProgram(), SampleFile, false );
        }
    }

    void Sampler::addSlicesToSample( int Program, const juce::String& SampleFile, bool IsSliceToPad )
    {
        std::list<double>::iterator it    = samplePoints.begin();
        auto                        index = 0;
        if( samplePoints.size() > 1 )
        {
            for( ; index < samplePoints.size() - 1; ++index )
            {
                auto loopStart = (int)*it * 44100; // Default Sampling rate
                std::advance( it, 1 );
                auto loopEnd = (int)*it * 44100; // Default Sampling rate

                IsSliceToPad ? ae.addSampleToProgram( Program, index, SampleFile, index + 1 ) : addAssetSlice( SampleFile, loopStart, loopEnd, index + 1 );
            }
            auto loopStart = (int)*it * 44100; // Default Sampling rate
            auto loopEnd   = (int)( recorder.getCurrentRecordTime() * 44100 ); // Default Sampleing rate

            IsSliceToPad ? ae.addSampleToProgram( Program, index, SampleFile, index + 1 ) : addAssetSlice( SampleFile, loopStart, loopEnd, index + 1 );
        }
    }

    void Sampler::addAssetSlice( const juce::String& SampleFile, int LoopStart, int LoopEnd, int Index )
    {
        auto sliceTree = createVTree( mt::IDs::SLICE, mt::IDs::id, CommonUtils::createID(),
                                      mt::IDs::loopStart, LoopStart,
                                      mt::IDs::loopEnd, LoopEnd );

        mt::ASoundSlice sliceObj{ sliceTree, &ae.getUndoManager() };
        ae.addAssetSlice( SampleFile, &sliceObj, Index );
    }

    void Sampler::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        sample.setBoundsRelative( .0097f, .919f, .1565f, 0.063f );
        slice.setBoundsRelative( .1702f, .919f, .1565f, 0.063f );
        padTap.setBoundsRelative( .3307f, .919f, .1565f, 0.063f );
        padHold.setBoundsRelative( .491f, .919f, .1565f, 0.063f );
        cancel.setBoundsRelative( .651f, .919f, .1565f, 0.063f );

        slicePlus.setBoundsRelative( .0097f, .919f, .1565f, 0.063f );

        save.setBoundsRelative( .0097f, .919f, .1565f, 0.063f );
        discard.setBoundsRelative( .1702f, .919f, .1565f, 0.063f );
        play.setBoundsRelative( .3307f, .919f, .1565f, 0.063f );
        edit.setBoundsRelative( .491f, .919f, .1565f, 0.063f );

        options.setBoundsRelative( .0097f, .1508f, .3346f, .7522f );
        recorder.setBoundsRelative( .3542f, .1588f, .6379f, .72f );
        exportt.setBoundsRelative( .7318f, .0258f, .0489f, .0823f );
        clear.setBoundsRelative( .7847f, .0258f, .0489f, .0823f );
    }

    void Sampler::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            int padNum   = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            int padState = contracts::DclContractsHelper::getPadState( f_HwMsg );
            DBG( "PadState " << padState );
            int padVelocity = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );

            if( sample.getToggleState() )
            {
            }
            else if( slice.getToggleState() )
            {
                if( padState == PAD_PRESSED )
                {
                    if( ae.isRecorderRunning() )
                        samplePoints.push_back( recorder.getCurrentRecordTime() );
                    else
                    {
                        recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeRecord );
                        if( samplePoints.size() == 0 )
                            samplePoints.push_back( 0 );
                    }
                }
            }
            else if( padTap.getToggleState() ) // TODO Need to clarify whether recording start at the moment the pad clicks or it need to be wait for threshold meets
            {
                if( padState == PAD_PRESSED )
                {
                    auto padIndex = padNum + ae.getBankOffset();
                    if( juce::isPositiveAndBelow( padNum, _NUM_PADS_IN_COMPONENT ) )
                    {
                        if( !ae.isRecorderRunning() )
                        {
                            recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeRecord );
                            startSamplePoints[padIndex] = recorder.getCurrentRecordTime();
                        }

                        if( lastPressedPad == padNum )
                        {
                            if( !isSliceRecorded )
                            {
                                isSliceRecorded           = true;
                                endSamplePoints[padIndex] = recorder.getCurrentRecordTime();
                            }
                            else if( isSliceRecorded )
                            {
                                startSamplePoints[padIndex] = recorder.getCurrentRecordTime();
                                isSliceRecorded             = false;
                            }
                            //TODO--> Need PadButton Colour from GD side nthen we can toggle Red/Green
                            if( 0 )
                            {
                                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                             setPadDataMessage( lastPressedPad,
                                                                                                contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                                contracts::DeviceControlContracts::padColourDarkRed ) );
                            }
                            else
                            {
                                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                             setPadDataMessage( lastPressedPad,
                                                                                                contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                                contracts::DeviceControlContracts::padColourlightGreen1 ) );
                            }
                        }

                        else if( lastPressedPad < 0 ) // If First time Pad Pressed
                        {
                            isSliceRecorded             = false;
                            lastPressedPad              = padNum;
                            startSamplePoints[padIndex] = recorder.getCurrentRecordTime();
                            //Need to add Pad Blinking in GD side and trigger from here using( Controls::InputEvents::setPadBlinking)
                            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                         setPadDataMessage( padNum, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourDarkRed ) );
                        }

                        else if( lastPressedPad != padNum )
                        {
                            if( !isSliceRecorded )
                            {
                                endSamplePoints[lastPressedPad + ae.getBankOffset()] = recorder.getCurrentRecordTime();
                                startSamplePoints[padIndex]                          = recorder.getCurrentRecordTime();
                                isSliceRecorded                                      = false;
                            }
                            else if( isSliceRecorded )
                            {
                                startSamplePoints[padIndex] = recorder.getCurrentRecordTime();
                                isSliceRecorded             = false;
                            }
                            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                         setPadDataMessage( lastPressedPad, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );

                            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                         setPadDataMessage( padNum, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourDarkRed ) );

                            lastPressedPad = padNum;
                        }
                    }
                }
            }
            else if( padHold.getToggleState() )
            {
                auto padIndex = padNum + ae.getBankOffset();

                auto track = ae.getSelected();
                if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
                {
                    if( !ae.isRecorderRunning() )
                        recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeRecord );
                    startSamplePoints[padIndex] = recorder.getCurrentRecordTime();

                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                 setPadDataMessage( padNum,
                                                                                    contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                    contracts::DeviceControlContracts::padColourDarkRed ) );
                }
                else if( padState == contracts::DeviceControlContracts::BTN_RELEASED )
                {
                    endSamplePoints[padIndex] = recorder.getCurrentRecordTime();

                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                 setPadDataMessage( padNum,
                                                                                    contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                    contracts::DeviceControlContracts::padColourlightGreen1 ) );
                }
            }
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
            {
                switch( knob )
                {
                    case contracts::DeviceControlContracts::EN_KNOB_3:
                        options.knobRotated( value );
                        break;
                }
                recorder.knobCommand( knob, value );
                options.knobCommand( knob, value );
            }
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); return;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); return;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); return;
            }
        }
    }

    void Sampler::refreshPadsLED()
    {
        std::vector<mt::MTISample*> samples;
        std::vector<int>            padDataList;

        auto bankOffset = ae.getBankOffset();

        ae.getProgramSamples( ae.getSelectedProgram(), samples );
        padDataList.clear();
        for( auto colourIndex = bankOffset; colourIndex < bankOffset + NUM_PADS_IN_BANK; colourIndex++ )
        {
            bool pushedToColourData = false;

            for( auto it : samples )
            {
                auto index = it->keyNote.get() - bankOffset;
                if( juce::isPositiveAndBelow( index, 16 ) && it->getSource( 0 ).length() && colourIndex == it->keyNote.get() )
                {
                    padDataList.push_back( contracts::DeviceControlContracts::padColourDarkYellow );
                    pushedToColourData = true;
                    break;
                }
            }
            if( !pushedToColourData )
                padDataList.push_back( contracts::DeviceControlContracts::PadColourTrans );
        }
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                     setAllPadDataColourInfoMessage( contracts::DeviceControlContracts::PadButtonState::StateOn, padDataList ) );
    }
    void Sampler::visibilityChanged()
    {
        auto handle_sampler = [this]( CPktMgr::HWMsg_t& f_HwMsg ) {

        };

        auto visible = isVisible();
        if( visible )
        {
            options.setGain( recorder.getRecordThresholdDB() );
            options.setMonitor( recorder.isMonitorOn() );
            options.setInputSelection( recorder.getRecordChannels(), juce::dontSendNotification );
            ae.setRecorderThresholdDB( recorder.getRecordThresholdDB() );
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
            //Controls::InputEvents::clearAllPads();
            if( padTap.getToggleState() || padHold.getToggleState() )
            {
                refreshPadsLED();
                lastPressedPad = -1;
            }
        }
        else
        {
            ae.stopRecorderPreview();

            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
            contracts::DclSignals::refreshPadLeds();
        }

        for( auto it : getChildren() )
            it->setVisible( visible );
        recorder.setRecordMode( mode );
    }

    void Sampler::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 1 ) );
        g.fillRoundedRectangle( { 10.0f, 86.0f, 342.0f, 396.0f }, 5.0f );
        g.fillRoundedRectangle( { 362.0f, 86.0f, 652.0f, 396.0f }, 5.0f );
    }
}
