
#include "kick_PadMixerDrumProgram.h"
#include "kick_PadMixerKeygroupProgram.h"

namespace kick_ui
{
    //////////   PAD MIXER COMPONENT   /////////////
    constexpr int widthDiff = 700;

    /////////////////     PAD MIXER DISPLAY    ///////////

    PadMixerDisplay::PadMixerDisplay()
    {
        addMouseListener( this, true );
        setInterceptsMouseClicks( true, true );
        mt::utils::addAndMakeVisible( this, solo, mute, levelSlider, panSlider, nameLabel, padName );

        nameLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );
        padName.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        padName.setValueColour( ui::Theme::textColour( 1 ) );
        nameLabel.setTitleFontHeight( 18 );
        nameLabel.setTitleJustification( juce::Justification::centredLeft );
        padName.setFontHeight( 16 );
        nameLabel.setEnabled( false );

        levelSlider.showTitleAndValue( true, juce::Slider::TextBoxBelow );

        mt::utils::setValueFontHeight( 20, levelSlider, panSlider );
        mt::utils::setTitleFontHeight( 16, levelSlider, panSlider );
        mt::utils::setValueJustification( juce::Justification::centredBottom, levelSlider, panSlider );
        mt::utils::setTitleJustification( juce::Justification::centredTop, levelSlider, panSlider );

        levelSlider.setRange( GAIN::MIN, GAIN::MAX, GAIN::STEP );
        panSlider.setRange( PAN::MIN, PAN::MAX, PAN::STEP );
        levelSlider.setPostFix( " db" );
        panSlider.setCentered( true );

        solo.onClick = [&]() {
            if( pProg && pData )
            {
                pProg->setNoteSolo( pData->KeyNote, solo.getToggleState() );
                pData->solo = pProg->isNoteSolo( pData->KeyNote );
                onDataChanged( pData->KeyNote );
                onSoloMuteChanged();
            }
        };

        mute.onClick = [&]() {
            if( pProg && pData )
            {
                if( pProg->isNoteSolo( pData->KeyNote ) )
                    mute.setToggleState( false, juce::dontSendNotification );
                else if( mutedDueToNoteSolo )
                    mute.setToggleState( true, juce::dontSendNotification );
                else
                {
                    pProg->setNoteMute( pData->KeyNote, mute.getToggleState() );
                    pData->mute = pProg->isNoteMute( pData->KeyNote );
                    onDataChanged( pData->KeyNote );
                }
                onSoloMuteChanged();
            }
        };

        levelSlider.onValueChanged = [&]( double Value ) {
            if( pProg && pData )
            {
                pProg->setNoteGain( pData->KeyNote, Value );
                pData->level = pProg->getNoteGain( pData->KeyNote );
                onDataChanged( pData->KeyNote );
            }
        };

        panSlider.onValueChanged = [&]( double Value ) {
            if( pProg && pData )
            {
                pProg->setNotePan( pData->KeyNote, Value );
                pData->pan = pProg->getNotePan( pData->KeyNote );
                onDataChanged( pData->KeyNote );
            }
        };

        ui::themeChanged.connect( [&]() {
            solo.setImages( "Solo_normal", "Solo_active" );
            mute.setImages( "Mute_normal", "Mute_active" );
        } );
    }

    void PadMixerDisplay::setData( mt::MTIProgram* Program, PadMixerData* Data )
    {
        pProg = Program;
        pData = Data;

        for( auto& child : getChildren() )
            child->setEnabled( pData ? pData->sampleName.isNotEmpty() : false );

        padName.setText( pData ? pData->padName : "", juce::dontSendNotification );
        nameLabel.setText( pData ? pData->sampleName : "", juce::dontSendNotification );
        levelSlider.setValue( pData ? pData->level : 0, juce::dontSendNotification );
        panSlider.setValue( pData ? pData->pan : 0, juce::dontSendNotification );
        solo.setToggleState( pData ? pData->solo : false, juce::dontSendNotification );
        mute.setToggleState( pData ? pData->mute : false, juce::dontSendNotification );
    }

    void PadMixerDisplay::setMute( bool State, bool NoteMutedDueToSolo )
    {
        mutedDueToNoteSolo = NoteMutedDueToSolo;
        mute.setToggleState( State, juce::dontSendNotification );
    }

    void PadMixerDisplay::linkRotated( int ID, int Value )
    {
        switch( ID )
        {
            case 0: levelSlider.knobRotated( Value ); break;
            case 1: panSlider.knobRotated( Value ); break;
            case 2:
                mute.setToggleState( Value > 0, juce::dontSendNotification );
                solo.setToggleState( Value <= 0, juce::dontSendNotification );
                break;
        }
    }

    juce::Rectangle<int> PadMixerDisplay::getElementBounds( int state )
    {
        switch( state )
        {
            case 0: return levelSlider.getBounds(); break;
            case 1: return panSlider.getBounds(); break;
            case 2: return mute.getBounds(); break;
            default: return levelSlider.getBounds(); break;
        }
    }

    void PadMixerDisplay::resized()
    {
        auto displayWidth = getWidth();
        auto isPadEnabled = displayWidth > widthDiff;
        panSlider.setSliderStyle( isPadEnabled ? juce::Slider::LinearBar : juce::Slider::Rotary );
        padName.setBoundsRelative( isPadEnabled ? .0098f : .0172f, .0934f, isPadEnabled ? .1581f : .2758f, .1308f );
        nameLabel.setBoundsRelative( isPadEnabled ? .0098f : .0172f, .2803f, isPadEnabled ? .1581f : .2758f, .4112f );
        levelSlider.setBoundsRelative( isPadEnabled ? .1877f : .3275f, .0934f, isPadEnabled ? .3517f : .4138f, .8411f );
        panSlider.setBoundsRelative( isPadEnabled ? .5592f : .7758f, .0934f, isPadEnabled ? .3517f : .0689f, .8411f );
        solo.setBoundsRelative( isPadEnabled ? .9337f : .8793f, .0654f, isPadEnabled ? .0592f : .1034f, .4112f );
        mute.setBoundsRelative( isPadEnabled ? .9337f : .8793f, .514f, isPadEnabled ? .0592f : .1034f, .4112f );
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////KickChannelMixer
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    PadMixer::PadMixer()
        : kick_ui::IScreen( kick_ui::Screen::MenuPadMixer )
    {
        mt::utils::addChildComponents( this, header, padHeaderComp, padEnabler, programLabel, trackLabel, padContent, sampleAssign );

        programLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        trackLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        programLabel.setValueColour( ui::Theme::textColour( 1 ) );
        trackLabel.setValueColour( ui::Theme::textColour( 1 ) );
        programLabel.setValueJustification( juce::Justification::centred );
        trackLabel.setValueJustification( juce::Justification::centred );
        sampleAssign.setValueColour( ui::Theme::textColour( 1 ) );

        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        padEnabler.onClick = [&]() {
            padContent.setPadEnable( padEnabler.getToggleState() );
        };

        sampleAssign.onClick = [&]() {
            closeScreen();
            kick_ui::signals::sigScreenOpen( kick_ui::Screen::MenuAssignSample );
        };

        padHeaderComp.programs.onSelectionChanged = [&]( int Sel, const juce::String& /*SelStr*/ ) {
            auto& ae = mt::getAudioInterface();
            if( Sel != ae.getSelectedProgram() )
                ae.setSelectedProgram( Sel );

            padContent.programChanged();
        };

        padHeaderComp.tracks.onSelectionChanged = [&]( int Sel, const juce::String& /*SelStr*/ ) {
            auto& ae = mt::getAudioInterface();
            if( auto pTrack = ae.getTrackData( ae.getSelectedSequence(), Sel ) )
            {
                if( pTrack->prog )
                    padHeaderComp.programs.setSelected( pTrack->prog->name.get() );
            }
        };

        ui::themeChanged.connect( [&]() {
            padEnabler.setImages( "PadMix_Enable", "PadMix_Enable_Active" );
            header.setIcon( "Icon_PadMix" );
        } );

        padEnabler.setToggleState( true, juce::sendNotification );
    }

    void PadMixer::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 4 ) );
    }

    void PadMixer::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        padHeaderComp.setBoundsRelative( 0.225f, 0.0503f, 0.3620f, 0.0636f );
        padEnabler.setBoundsRelative( 0.8063f, 0.0299f, 0.0430f, 0.0824f );
        programLabel.setBoundsRelative( .225f, 0.0000f, .0648f, .0583f );
        trackLabel.setBoundsRelative( .424f, 0.0000f, .0648f, .0583f );
        sampleAssign.setBoundsRelative( .685f, .0299f, .1174f, .0824f );

        padContent.setBounds( b );
    }

    void PadMixer::meterUpdate()
    {
        padContent.meterUpdate();
    }

    void PadMixer::timerCallback()
    {
        meterUpdate();
    }

    void PadMixer::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto&            ae         = mt::getAudioInterface();
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
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& ae         = mt::getAudioInterface();
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
            }
        }
        padContent.handleCommand( f_HwMsg );
    }

    void PadMixer::visibilityChanged()
    {
        auto visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );

        if( visible )
        {
            auto& ae         = mt::getAudioInterface();
            auto  selProgram = ae.getSelectedProgram();
            if( 0 <= selProgram )
                padHeaderComp.programs.setSelected( selProgram );
            else
            {
                auto selTrack = ae.getSelectedTrack();
                padHeaderComp.tracks.setSelected( selTrack );
            }
            startTimerHz( 25 );
        }
        else
        {
            stopTimer();
        }
    }

    PadMixerContent::PadMixerContent()
        : pads( std::make_unique<ui::KickPadButtonGroupComponent>( ui::KickPadButton::PadButtonType::padMixer ) )
    {
        tableView.reset( new ui::KickTableView( "PAD MIXER", provider ) );

        mt::utils::addAndMakeVisible( this, tableView.get(), pads.get(), samples, infoLabel );
        mt::utils::makeHidden( infoLabel );
        infoLabel.setValueColour( ui::Theme::textColour( 1 ) );
        infoLabel.setValueFontHeight( 24 );
        infoLabel.setValueJustification( juce::Justification::centred );

        tableView->setRowSpacing( 1 );
        tableView->setRowHeight( ROW_HEIGHT );
        tableView->setRowBackgroundColour( ui::Theme::backgroundColour( 1 ) );
        tableView->setSelectedRowBackgroundColour( ui::Theme::backgroundColour( 0 ) );
        tableView->setAutoScrollToSelection( true );

        pads->setFocusOutline( true );
        pads->setActive( -1, false );

        for( auto index = 0; index < 128; ++index )
        {
            pads->setMute( index, false, juce::dontSendNotification );
            pads->setSolo( index, false, juce::dontSendNotification );
        }
        pads->setHighlight( -1 );
        pads->setPrefix( "PAD" );
        pads->setBorderColor( ui::Theme::outlineColour( 3 ), 1.0f, juce::dontSendNotification );
        pads->setRow2Colour( ui::Theme::textColour( 3 ) );

        provider.numRows = [&]() {
            return (int)rows.size();
        };

        provider.cellComponent = [&]( int Row, juce::Component* Existing ) -> juce::Component* {
            auto comp = dynamic_cast<PadMixerDisplay*>( Existing );

            if( juce::isPositiveAndBelow( Row, rows.size() ) )
            {
                auto idx = rows[Row];
                if( comp == nullptr )
                {
                    comp = new PadMixerDisplay();
                }

                if( comp )
                {
                    comp->setData( pProg, &padData[idx] );
                    comp->onDataChanged     = [&]( int Idx ) { refreshPadData( Idx ); };
                    comp->onSoloMuteChanged = [&]() { refreshSoloMute(); };
                }
                return comp;
            }
            return nullptr;
        };

        provider.onRowSelected = [&]( int Row ) {
            auto& comps = tableView->getRowComponents();
            auto  key   = dynamic_cast<PadMixerDisplay*>( comps[Row] )->getKeyNote();
            bankIndex   = key / 16;
            pads->setBank( bankIndex );
            refreshPadData();
        };

        pads->OnButtonClicked = [&]( int Index ) {
            selectedPad = Index;

            auto rowIndex = rows.indexOf( selectedPad );
            if( 0 <= rowIndex )
            {
                tableView->selectRow( rowIndex );
            }
        };

        pads->OnDoubleClicked = [&]( int Index ) {
            selectedPad = Index;
            samples.show( "Sample Assign", [&]( int R ) {
                if( R == UI::APPLY )
                {
                    auto index = samples.getSelectedIndex();
                    if( index != -1 )
                    {
                        auto sample = ae.getAsset( mt::AssetTypes::Samples, index );
                        ae.addSampleToProgram( ae.getSelectedProgram(), selectedPad, sample );
                        programChanged();
                    }
                }
            } );
        };

        pads->OnBankChanged = [&]( const juce::String& ) {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( pads->getBankIndex() ) );
        };

        samples.onSelectionChanged = [&]( int index, const juce::String& /* Option*/ ) {
            auto sample = ae.getAsset( mt::AssetTypes::Samples, index );
            ae.addSampleToProgram( ae.getSelectedProgram(), selectedPad, sample );
            programChanged();
        };

        mt::signals::bankChanged.connect( [&]( int index, juce::String bankName ) {
            if( kick_ui ::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuPadMixer )
            {
                bankIndex = index;
                pads->setBank( bankIndex );
                refreshPadData();
                tableView->refresh();
                updateView();
            }
        } );
    }

    void PadMixerContent::setPadEnable( bool Enable )
    {
        isPadEnabled = Enable;
        resized();
        tableView->refresh();
    }

    void PadMixerContent::programChanged()
    {
        const auto sel = ae.getSelectedProgram();
        pProg          = ae.getProgram( sel );

        if( pProg )
        {
            if( pProg->type.get() == mt::MTProgramType::Drum || pProg->type.get() == mt::MTProgramType::Clip )
                setDisplay( true );
            else
                setDisplay( false );
        }
        else
            setDisplay( false );
    }

    void PadMixerContent::setDisplay( bool isProgramAvailable )
    {
        if( isProgramAvailable )
        {
            mt::utils::makeHidden( infoLabel );

            auto selProg = ae.getSelectedProgram();
            ae.getProgramSamples( selProg, sampleList );

            refresh();
            tableView->refresh();
        }
        else
        {
            mt::utils::makeVisible( infoLabel );

            sampleList.clear();
            auto prog = pProg ? juce::String( mt::toString( pProg->type.get() ) ) : "Unknown ";
            infoLabel.setText( "Pad Mixer Is Not Applicable For " + prog + " Program", juce::dontSendNotification );
        }

        tableView->setVisible( isProgramAvailable );
        pads->setVisible( isProgramAvailable && isPadEnabled );
    }

    void PadMixerContent::resized()
    {
        auto b = getLocalBounds();
        if( isPadEnabled )
        {
            tableView->setBounds( b.removeFromLeft( 590 ).reduced( 0, 10 ) );
            b.removeFromTop( 5 );
            pads->setBounds( b.reduced( 5, 5 ) );
            pads->setVisible( true );
        }
        else
        {
            tableView->setBounds( b.reduced( 0, 10 ) );
            pads->setVisible( false );
        }
        infoLabel.setBoundsRelative( .1f, .4f, .8f, .2f );
    }

    void PadMixerContent::refresh()
    {
        rows.clearQuick();
        for( auto& it : padData )
            it.clear();

        for( const auto& sample : sampleList )
        {
            auto src = sample->getSource();
            if( src.isNotEmpty() )
            {
                padData[sample->keyNote.get()].sampleName = juce::File( src ).getFileNameWithoutExtension();
                padData[sample->keyNote.get()].KeyNote    = sample->keyNote.get();
                padData[sample->keyNote.get()].mute       = sample->mute.get();
                padData[sample->keyNote.get()].solo       = pProg ? pProg->isNoteSolo( sample->keyNote.get() ) : false;
                padData[sample->keyNote.get()].level      = sample->gain.get();
                padData[sample->keyNote.get()].pan        = sample->pan.get();

                rows.addIfNotAlreadyThere( sample->keyNote.get() );
            }
        }
        rows.sort(); // Keep sorted.

        for( auto Id = 0; Id < _NUM_PADS; ++Id )
        {
            char bank           = static_cast<char>( 'A' + ( Id / 16 ) );
            padData[Id].padName = juce::String( "PAD " ) + bank + juce::String( ( Id % 16 ) + 1 ).paddedLeft( '0', 2 );
        }

        refreshPadData();
    }

    void PadMixerContent::refreshPadData( int Pad )
    {
        pads->setHeaderText( Pad, padData[Pad].padName, juce::dontSendNotification );
        pads->setRow1Text( Pad, juce::String( padData[Pad].pan ), juce::dontSendNotification );
        pads->setRow2Text( Pad, juce::String( padData[Pad].level ) + " db", juce::dontSendNotification );
        pads->setMute( Pad, padData[Pad].mute, juce::dontSendNotification );
        pads->setSolo( Pad, padData[Pad].solo, juce::dontSendNotification );

        pads->setElementsEnable( Pad, padData[Pad].sampleName.isNotEmpty() );
        pads->setActive( Pad, padData[Pad].sampleName.isNotEmpty() );
    }

    void PadMixerContent::refreshPadData()
    {
        pads->clearAllPadData();
        for( auto padId = 0; padId < _NUM_PADS_IN_COMPONENT; ++padId )
        {
            refreshPadData( padId + pads->getBankOffset() );
        }
    }

    void PadMixerContent::meterUpdate()
    {
        if( pProg )
        {
            auto outMeter = pProg->readOutMeter(); // TODO - Need to call different API for Pad
            for( auto sample : sampleList )
            {
                if( sample->keyNote.get() >= ae.getBankOffset() && sample->keyNote.get() <= ( ae.getBankOffset() + 15 ) )
                    pads->updateLevelMeter( (int)sample->keyNote.get() % 16, outMeter[0].rms, outMeter[1].rms, outMeter[0].peak, outMeter[0].peak );
            }
        }
    }

    void PadMixerContent::updateView()
    {
        auto rowIndex = -1;
        for( auto it = 0; it < rows.size(); ++it )
        {
            if( padData[rows[it]].KeyNote >= ae.getBankOffset() && padData[rows[it]].KeyNote <= ( ae.getBankOffset() + 15 ) )
            {
                rowIndex = it;
                break;
            }
        }
        if( rowIndex != -1 )
        {
            auto rowBounds = tableView->getRowBounds( rowIndex );
            tableView->setTableViewPosition( rowBounds.getY() );
        }
    }

    void PadMixerContent::refreshSoloMute()
    {
        //Reflect Mute onSolo Changes
        auto selProg = ae.getSelectedProgram();
        ae.getProgramSamples( selProg, sampleList );

        bool isSoloed{ false };
        for( const auto& sample : sampleList )
            if( padData[sample->keyNote.get()].solo )
            {
                isSoloed = true;
                break;
            }
        for( auto padId = 0; padId < _NUM_PADS_IN_COMPONENT; ++padId )
        {
            auto padIndex = padId + pads->getBankOffset();
            pads->setMute( padIndex, padData[padIndex].solo ? false : isSoloed ? true :
                                                                                 padData[padIndex].mute,
                           juce::dontSendNotification );
        }
        auto& comps = tableView->getRowComponents();
        for( auto& comp : comps )
        {
            auto pComp = dynamic_cast<PadMixerDisplay*>( comp );
            pComp->setMute( padData[pComp->getKeyNote()].solo ? false : isSoloed ? true :
                                                                                   padData[pComp->getKeyNote()].mute,
                            padData[pComp->getKeyNote()].solo ? false : isSoloed ? true :
                                                                                   false );
        }
    }

    void PadMixerContent::visibilityChanged()
    {
        if( isVisible() )
        {
            samples.setContent( ae.getAssets() );
            programChanged();
        }
    }

    void PadMixerContent::knobCommand( int knob, int value )
    {
        auto& rowComp       = tableView->getRowComponents();
        auto  totalRows     = tableView->getNumRows();
        auto  comp1         = dynamic_cast<PadMixerDisplay*>( rowComp[0] );
        auto  elementBounds = comp1->getElementBounds( ( knob - 1 ) % 4 + 1 );
        auto  rowList       = tableView->getRowNumbers( 4, ( ( knob - 1 ) / 4 ) + 1, elementBounds.getX(), elementBounds.getWidth(), 50 );

        std::vector<PadMixerDisplay*> entry;

        for( auto i = 0; i < rowList.size(); i++ )
        {
            int rowIndex = rowList[i] - 1;
            if( rowIndex < totalRows )
            {
                auto comp = dynamic_cast<PadMixerDisplay*>( rowComp[rowIndex] );
                entry.push_back( comp );
            }
        }

        switch( knob )
        {
            case contracts::DeviceControlContracts::EN_KNOB_1:
            case contracts::DeviceControlContracts::EN_KNOB_2:
            case contracts::DeviceControlContracts::EN_KNOB_3:
            case contracts::DeviceControlContracts::EN_KNOB_4:
                if( entry.size() >= 1 && entry[0] ) entry[0]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_1 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_5:
            case contracts::DeviceControlContracts::EN_KNOB_6:
            case contracts::DeviceControlContracts::EN_KNOB_7:
            case contracts::DeviceControlContracts::EN_KNOB_8:
                if( entry.size() >= 2 && entry[1] ) entry[1]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_5 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_9:
            case contracts::DeviceControlContracts::EN_KNOB_10:
            case contracts::DeviceControlContracts::EN_KNOB_11:
            case contracts::DeviceControlContracts::EN_KNOB_12:
                if( entry.size() >= 3 && entry[2] ) entry[2]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_9 ), value );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_13:
            case contracts::DeviceControlContracts::EN_KNOB_14:
            case contracts::DeviceControlContracts::EN_KNOB_15:
            case contracts::DeviceControlContracts::EN_KNOB_16:
                if( entry.size() >= 4 && entry[3] ) entry[3]->linkRotated( ( knob - contracts::DeviceControlContracts::EN_KNOB_13 ), value );
                break;
        }
    }

    void PadMixerContent::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto padNo = ae.getBankOffset() + contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            pads->setSelected( padNo, juce::sendNotification );
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
                knobCommand( knob, value );
        }
    }
}
