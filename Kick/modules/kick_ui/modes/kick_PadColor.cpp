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
    KickPadColorMode::KickPadColorMode()
        : kick_ui::IScreen( kick_ui::Screen::MenuColorEdit )
    {
        left.selectedColor = &transparent;
        mt::utils::addAndMakeVisible( this, left, right, hdrComp, warningLabel, bankLabel, leftNav, rightNav );
        warningLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        warningLabel.setValueFontHeight( 25 );
        setLookAndFeel( ui::KickLooknFeel::getLnf() );
        mt::utils::addChildComponents( this, header, hdrComp );
        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        header.showSettingsButton( true );

        bankLabel.setText( right.pads->getBank(), juce::dontSendNotification );
        bankLabel.setTextColour( ui::Theme::textColour( 1 ) );
        bankLabel.setValueFontHeight( 24 );
        bankLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 3 ) );
        bankLabel.setValueJustification( juce::Justification::centred );

        left.emptyPadMode.onClick = [&]() {
            ae.setProgramEmptyPadColorMode( ae.getSelectedProgram(), left.emptyPadMode.getToggleState() ? mt::EmptyPadColorMode::Dim : mt::EmptyPadColorMode::Off );
            sendColourModeToHardware( left.padOff.getCurrentSelection(), left.padOn.getCurrentSelection() );
        };

        left.padOff.onClick = [&]() {
            left.padOff.incrementSelection();
            setPadOffModeSelection( left.padOff.getCurrentSelection() );
            sendColourModeToHardware( left.padOff.getCurrentSelection(), left.padOn.getCurrentSelection() );
        };

        left.padOn.onClick = [&]() {
            left.padOn.incrementSelection();
            setPadOnModeSelection( left.padOn.getCurrentSelection() );
            sendColourModeToHardware( left.padOff.getCurrentSelection(), left.padOn.getCurrentSelection() );
        };

        hdrComp.onSelectionChanged = [&]( const juce::String& Program, const mt::MTProgramType Type ) {
            ae.setSelectedProgram( Program );
            refresh();
            refreshLEDs();
        };

        // Add listener function for pad button clicks...
        right.pads->OnButtonClicked = [this]( int Id ) {
            buttonClicked( Id );
        };

        leftNav.onClick = [&]() {
            auto index = right.pads->getBankIndex();
            if( index == 0 )
                right.pads->setBank( 0 );
            else
                right.pads->setBank( index - 1 );
            ae.setBank( right.pads->getBank() );
        };

        rightNav.onClick = [&] {
            auto index = right.pads->getBankIndex();
            if( index == 7 )
                right.pads->setBank( 7 );
            else
                right.pads->setBank( index + 1 );
            ae.setBank( right.pads->getBank() );
        };

        mt::signals::bankChanged.connect( [&]( int Index, juce::String Bank ) {
            setBank( Index, Bank );
        } );

        // Set default states...
        left.selectedColor->setToggleState( true, juce::NotificationType::dontSendNotification );
        hdrComp.toFront( true );
        leftNav.toFront( true );
        rightNav.toFront( true );
        bankLabel.toFront( true );

        ui::themeChanged.connect( [&]() {
            header.setIcon( "Icon_ColourEdit" );
        } );

        right.defaultBtn.onClick = [&]() {
            ae.setProgramPadColour( ae.getSelectedProgram(), 3, -1 );
            refresh();
            right.defaultBtn.setToggleState( false, juce::dontSendNotification );
        };
    }

    void KickPadColorMode::setPadOnModeSelection( int selection )
    {
        if( selection == PAD_ON_STATE::PAD_ON_VELOCITY )
            left.padOn.setButtonText( "Velocity" );
        else if( selection == PAD_ON_STATE::PAD_ON_FULL )
            left.padOn.setButtonText( "Full" );
        else if( selection == PAD_ON_STATE::PAD_ON_OFF )
            left.padOn.setButtonText( "Off" );
    }

    void KickPadColorMode::setPadOffModeSelection( int selection )
    {
        if( selection == PAD_OFF_STATE::PAD_OFF_DIM )
            left.padOff.setButtonText( "Dim" );
        else if( selection == PAD_OFF_STATE::PAD_OFF_OFF )
            left.padOff.setButtonText( "Off" );
    }
    void KickPadColorMode::sendColourModeToHardware( int padOff, int padOn )
    {
        mt::PadColorMode padColorMode = mt::PadColorMode::INVALID;
        if( padOff == mt::PadModeState::PAD_OFF_STATE )
        {
            switch( padOn )
            {
                case 0: padColorMode = mt::PadColorMode::OFF_VELOCITY; break;
                case 1: padColorMode = mt::PadColorMode::OFF_FULL; break;
                case 2: padColorMode = mt::PadColorMode::OFF; break;
            }
        }

        else if( padOff == mt::PadModeState::PAD_DIM_STATE )
        {
            //check empty pad off or dim if dim process below or write one more switch

            if( left.emptyPadMode.getToggleState() )
            {
                switch( padOn )
                {
                    case 0: padColorMode = mt::PadColorMode::DIM_VELOCITY; break;
                    case 1: padColorMode = mt::PadColorMode::DIM_FULL; break;
                    case 2: padColorMode = mt::PadColorMode::DIM_OFF; break;
                }
            }
            else //empty pads off
            {
                switch( padOn )
                {
                    case 0: padColorMode = mt::PadColorMode::OFF_DIM_VELOCITY; break;
                    case 1: padColorMode = mt::PadColorMode::OFF_DIM_FULL; break;
                    case 2: padColorMode = mt::PadColorMode::OFF_DIM_OFF; break;
                }
            }
        }
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                     setPadColorModeMessage( padColorMode ) );
        ae.setProgramColourMode( ae.getSelectedProgram(), padColorMode );
    }

    int KickPadColorMode::getPadOffColourMode( mt::PadColorMode mode )
    {
        switch( mode )
        {
            case mt::PadColorMode::OFF:
            case mt::PadColorMode::OFF_VELOCITY:
            case mt::PadColorMode::OFF_FULL:
                return PAD_OFF_STATE::PAD_OFF_OFF;

            case mt::PadColorMode::DIM_OFF:
            case mt::PadColorMode::DIM_VELOCITY:
            case mt::PadColorMode::DIM_FULL:
            case mt::PadColorMode::OFF_DIM_OFF:
            case mt::PadColorMode::OFF_DIM_VELOCITY:
            case mt::PadColorMode::OFF_DIM_FULL:
                return PAD_OFF_STATE::PAD_OFF_DIM;
            default: return PAD_OFF_STATE::PAD_OFF_OFF;
        }
    }

    int KickPadColorMode::getPadOnColourMode( mt::PadColorMode mode )
    {
        switch( mode )
        {
            case mt::PadColorMode::OFF:
            case mt::PadColorMode::DIM_OFF:
            case mt::PadColorMode::OFF_DIM_OFF:
                return PAD_ON_STATE::PAD_ON_OFF;

            case mt::PadColorMode::OFF_VELOCITY:
            case mt::PadColorMode::DIM_VELOCITY:
            case mt::PadColorMode::OFF_DIM_VELOCITY:
                return PAD_ON_STATE::PAD_ON_VELOCITY;

            case mt::PadColorMode::OFF_FULL:
            case mt::PadColorMode::DIM_FULL:
            case mt::PadColorMode::OFF_DIM_FULL:
                return PAD_ON_STATE::PAD_ON_FULL;
            default: return PAD_ON_STATE::PAD_ON_OFF;
        }
    }

    bool KickPadColorMode::getEmptyColourMode( mt::PadColorMode mode )
    {
        switch( mode )
        {
            case mt::PadColorMode::OFF:
            case mt::PadColorMode::OFF_VELOCITY:
            case mt::PadColorMode::OFF_FULL:
            case mt::PadColorMode::OFF_DIM_OFF:
            case mt::PadColorMode::OFF_DIM_VELOCITY:
            case mt::PadColorMode::OFF_DIM_FULL:
                return OFF;

            case mt::PadColorMode::DIM_OFF:
            case mt::PadColorMode::DIM_VELOCITY:
            case mt::PadColorMode::DIM_FULL:
                return DIM;
            default: return OFF;
        }
    }

    void KickPadColorMode::refresh()
    {
        right.pads->clearAllPadData();

        auto progData = ae.getSelectedProgramData();
        right.pads->setActive( -1, true );
        for( auto i = 0; i < 128; i++ )
        {
            right.pads->setBorderColor( i, ui::Theme::padButtonColour( progData ? progData->padColour[i].get() : 0 ), 2.0f, juce::dontSendNotification );
        }

        samples.clear();
        ae.getProgramSamples( ae.getSelectedProgram(), samples );
        if( samples.size() )
        {
            if( ae.getProgram( ae.getSelectedProgram() )->getType() == mt::MTProgramType::Keygroup )
            {
                auto text  = juce::File( samples[0]->getSource() ).getFileNameWithoutExtension();
                bankOffset = right.pads->getBankOffset();
                for( auto index = bankOffset; index < bankOffset + _NUM_PADS_IN_COMPONENT; ++index )
                {
                    if( !text.isEmpty() )
                    {
                        right.pads->setRow1Text( index, text, juce::dontSendNotification );
                    }
                }
            }
            else
            {
                for( const auto& sample : samples )
                {
                    if( sample )
                    {
                        auto text  = juce::File( sample->getSource() ).getFileNameWithoutExtension();
                        bankOffset = right.pads->getBankOffset();
                        for( auto index = bankOffset; index < bankOffset + _NUM_PADS_IN_COMPONENT; ++index )
                        {
                            if( !text.isEmpty() )
                            {
                                if( index == (int)sample->keyNote.get() )
                                {
                                    right.pads->setRow1Text( index, text, juce::dontSendNotification );
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        //refreshLEDs();
    }

    void KickPadColorMode::refreshLEDs()
    {
        contracts::DclSignals::refreshPadLeds();
    }

    void KickPadColorMode::buttonClicked( int Id )
    {
        if( left.selectedColor )
        {
            if( !left.padType.getToggleState() )
            {
                ae.setProgramPadColour( ae.getSelectedProgram(), left.selectedColourId, Id );
                int pad  = mt::utils::getPadIndexInBank( Id );
                int bank = mt::utils::getBankIndex( Id );
                int data = 0;
                data     = (juce::int8)left.selectedColourId;
                DBG( "Pad Colour - ButtonClicked" );
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                             setPadDataMessage( contracts::DeviceControlContracts::PadSequence::EN_PAD1 + pad,
                                                                                contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                data ) );

                right.pads->setBorderColor( pad, juce::Colour( left.selectedColourId ), 2.0f, juce::dontSendNotification );
            }
            else
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                             setAllPadDataColourInfoMessage(
                                                                 contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                 static_cast<juce::int8>( left.selectedColourId ) ) );

                ae.setProgramPadColour( ae.getSelectedProgram(), left.selectedColourId, -1 );
            }

            refresh();
        }
    }

    void KickPadColorMode::setBank( int Index, juce::String Bank )
    {
        if( kick_ui ::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuColorEdit )
        {
            juce::MessageManager::callAsync( [this, Index, Bank] {
                right.pads->setBank( Index );
                bankLabel.setText( right.pads->getBank(), juce::dontSendNotification );
                refresh();
                refreshLEDs();
            } );
        }
    }

    void KickPadColorMode::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 4 ) );
    }

    void KickPadColorMode::resized()
    {
        header.setBounds( getLocalBounds().removeFromTop( ui::layout::header.getHeight() ) );
        hdrComp.setBoundsRelative( .225f, 0.0299f, .195f, 0.0824f );
        left.setBoundsRelative( 0.009f, 0.161f, 0.3346f, 0.8239f );
        right.setBoundsRelative( 0.359f, 0.17f, 0.63f, 0.816f );
        warningLabel.setBounds( getLocalBounds().removeFromTop( 300 ).withTrimmedTop( 200 ).removeFromLeft( 800 ).withTrimmedLeft( 400 ) );
        auto bankBounds = getLocalBounds().removeFromLeft( 804 ).withTrimmedLeft( 604 ).removeFromTop( 76 ).withTrimmedTop( 16 ).withTrimmedBottom( 16 );
        leftNav.setBounds( bankBounds.removeFromLeft( 44 ) );
        bankLabel.setBounds( bankBounds.removeFromLeft( 112 ) );
        rightNav.setBounds( bankBounds.removeFromLeft( 44 ) );
    }

    void KickPadColorMode::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        DBG( "Inside HwCmd Handel Padcolor" );
        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
                default: break;
            }
        }
        else if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            bankOffset                = ae.getBankOffset();
            auto             padNo    = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            auto             padState = contracts::DclContractsHelper::getPadState( f_HwMsg );
            auto             velocity = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );
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
    }

    void KickPadColorMode::visibilityChanged()
    {
        bool visible = isVisible();
        if( visible )
        {
            right.pads->setBank( ae.getBank() );
            bankLabel.setText( ae.getBank(), juce::dontSendNotification );
            refresh();
            refreshLEDs();
            auto idx = ae.getSelectedProgram();
            hdrComp.title.setText( ae.getProgramNameByIndex( idx ), juce::dontSendNotification );

            auto progData = ae.getProgramData( ae.getSelectedProgram() );

            auto padOffSel = getPadOffColourMode( progData ? progData->colourMode.get() : mt::OFF );
            auto padOnSel  = getPadOnColourMode( progData ? progData->colourMode.get() : mt::OFF );
            setPadOffModeSelection( padOffSel );
            setPadOnModeSelection( padOnSel );
            left.padOff.setCurrentSelection( padOffSel );
            left.padOn.setCurrentSelection( padOnSel );
            left.emptyPadMode.setToggleState( getEmptyColourMode( progData ? progData->colourMode.get() : mt::OFF ), juce::dontSendNotification );
            ae.setProgramEmptyPadColorMode( ae.getSelectedProgram(), left.emptyPadMode.getToggleState() ? mt::EmptyPadColorMode::Dim : mt::EmptyPadColorMode::Off );
        }
        if( !ae.getNumPrograms() && visible )
        {
            left.setVisible( false );
            right.setVisible( false );
            hdrComp.setVisible( true );
            header.setVisible( true );
            warningLabel.setVisible( true );
            hdrComp.title.setText( "None", juce::dontSendNotification );
        }
        else
        {
            for( auto it : getChildren() )
                it->setVisible( visible );
            warningLabel.setVisible( false );
        }
    }

    //////////////////////////////
    ////KickColorEditorLeftPanel
    //////////////////////////////

    KickColorEditorLeftPanel::KickColorEditorLeftPanel()
        : TabbedComponent( juce::TabbedButtonBar::Orientation::TabsAtBottom )
    {
        mt::utils::addAndMakeVisible( this, emptyPadMode, padType, padOn, padOff, emptyPadModeLabel, padTypeLabel, padOffLabel, padOnLabel, assignedPadLabel );

        padOff.setValueColour( ui::Theme::textColour( 2 ) );
        padOn.setValueColour( ui::Theme::textColour( 2 ) );

        assignedPadLabel.setValueJustification( juce::Justification::centred );
        assignedPadLabel.setValueFontHeight( 14.0f );
        assignedPadLabel.setValueColour( ui::Theme::textColour( 2 ) );

        padOffLabel.setValueJustification( juce::Justification::centred );
        padOffLabel.setTitleFontHeight( 14.0f );
        padOffLabel.setValueColour( ui::Theme::textColour( 1 ) );

        padOnLabel.setValueJustification( juce::Justification::centred );
        padOnLabel.setValueFontHeight( 14.0f );
        padOnLabel.setValueColour( ui::Theme::textColour( 1 ) );

        padTypeLabel.setValueJustification( juce::Justification::centred );
        padTypeLabel.setValueFontHeight( 14.0f );
        padTypeLabel.setValueColour( ui::Theme::textColour( 1 ) );

        emptyPadModeLabel.setValueJustification( juce::Justification::centred );
        emptyPadModeLabel.setValueFontHeight( 14.0f );
        emptyPadModeLabel.setValueColour( ui::Theme::textColour( 1 ) );

        // Add all colour buttons...

        for( auto i = 0; i < 18; i++ )
        {
            addAndMakeVisible( padButtons[i] );
            padButtons[i].addListener( this );
            padButtons[i].setRadioGroupId( (int)kick_ui::RadioGroupId::ColourEdit );
            padButtons[i].setID( i );
            padButtons[i].setColor( ui::Theme::padButtonColour( i ), ui::Theme::padButtonColour( i ) );
        }

        ui::themeChanged.connect( [&]() {
        } );
    }

    void KickColorEditorLeftPanel::resized()
    {
        padOff.setBoundsRelative( .0292f, .0863f, .4649f, .0909f );
        padOn.setBoundsRelative( .5058f, .0863f, .4649f, .0909f );

        padType.setBoundsRelative( .0292f, .2636f, .4649f, .0909f );
        emptyPadMode.setBoundsRelative( .5058f, .2636f, .4649f, .0909f );

        padOffLabel.setBoundsRelative( .0292f, .0454f, .4649f, .0318f );
        padOnLabel.setBoundsRelative( .5058f, .0454f, .4649f, .0318f );

        padTypeLabel.setBoundsRelative( .0292f, .2227f, .4649f, .0318f );
        emptyPadModeLabel.setBoundsRelative( .5058f, .2227f, .4649f, .0318f );

        for( auto row = 0; row < numRows; ++row )
        {
            x = 0.029f;
            y = row * padButtonHeight + 0.398f;
            for( auto col = 0; col < numCols; ++col )
            {
                auto idx = ( numRows - row - 1 ) * numCols + col;
                padButtons[idx].setBoundsRelative( x, y, padButtonWidth, padButtonHeight );
                x += padButtonWidth;
            }
        }
    }
    void KickColorEditorLeftPanel::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds();
        mt::utils::renderRoundedRect( g, { b.getX(), b.getY(), b.getWidth(), b.getHeight() }, ui::Theme::outlineColour( 3 ), 1, 12.0f );
        mt::utils::fillRoundedRect( g, { b.getX(), b.getY(), b.getWidth(), b.getHeight() }, ui::Theme::backgroundColour( 3 ), 7.0f );
    }
    void KickColorEditorLeftPanel::buttonClicked( juce::Button* ButtonClicked )
    {
        if( (int)kick_ui::RadioGroupId::ColourEdit == ButtonClicked->getRadioGroupId() )
        {
            selectedColor = dynamic_cast<ui::KickColorButton*>( ButtonClicked );
            selectedColor->showBorder( selectedColor->getToggleState() );

            selectedColourId = magic_enum::enum_cast<contracts::DeviceControlContracts::PadColourIds>( selectedColor->getID() ).value();
        }
    }
    //////////////////////////////
    ////KickColorEditorRightPanel
    //////////////////////////////

    KickColorEditorRightPanel::KickColorEditorRightPanel()
        : pads( std::make_unique<ui::KickPadButtonGroupComponent>( ui::KickPadButton::PadButtonType::colorMode ) )
    {
        mt::utils::addAndMakeVisible( this, pads.get(), defaultBtn );

        pads->setActive( -1, false );
        pads->setPadButtonSpacer( padButtonSpacer );

        defaultBtn.setValueColour( ui::Theme::textColour( 1 ) );
        defaultBtn.setValueFontHeight( 18 );

        ui::themeChanged.connect( [&]() {
            defaultBtn.setToggleButtonText( "Make Default", "Make Default" );
        } );
    }

    void KickColorEditorRightPanel::resized()
    {
        pads->setBoundsRelative( 0.f, 0.f, 1.f, 0.8899f );
        defaultBtn.setBoundsRelative( 0.8136f, 0.922f, 0.186f, 0.078f );
    }
    void KickColorEditorRightPanel::paint( juce::Graphics& g )
    {
    }

    void KickColorEditorRightPanel::visibilityChanged()
    {
        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );
    }
}
