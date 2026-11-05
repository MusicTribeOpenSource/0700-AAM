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
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////PadMuteHeaderComponent
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    KickPadMute::KickPadMute()
        : IScreen( Screen::MenuPadMute ), tab( std::make_unique<KickPadMuteTab>() )
    {
        mt::utils::addChildComponents( this, header, hdrComp, padMute, padGroup, *tab, programLabel, infoLabel );
        tab->setVisible( false );
        header.showSettingsButton( true );
        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        padMute.setToggleState( true, juce::sendNotification );
        padMute.toFront( true );
        padGroup.toFront( true );

        programLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        programLabel.setValueColour( ui::Theme::textColour( 1 ) );
        infoLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        infoLabel.setValueColour( ui::Theme::textColour( 1 ) );
        infoLabel.setVisible( false );
        infoLabel.setValueJustification( juce::Justification::centred );
        infoLabel.setValueFontHeight( 24 );
        infoLabel.setText( "PAD MUTE IS NOT AVAILABLE FOR SELECTED TRACK", juce::dontSendNotification );

        padMute.setValueColour( ui::Theme::textColour( 1 ) );
        padMute.setValueFontHeight( 18 );
        padGroup.setValueColour( ui::Theme::textColour( 1 ) );
        padGroup.setValueFontHeight( 18 );

        padMute.onClick = [&]() {
            tab->showMiniMap();
            padGroup.setToggleState( false, juce::dontSendNotification );
            if( !padMute.getToggleState() )
                padMute.setToggleState( true, juce::dontSendNotification );
        };

        padGroup.onClick = [&]() {
            tab->showGroups();
            padMute.setToggleState( false, juce::dontSendNotification );
            if( !padGroup.getToggleState() )
                padGroup.setToggleState( true, juce::dontSendNotification );
        };

        hdrComp.tracks.onSelectionChanged = [&]( int Index, const juce::String& Track ) {
            auto& ae = mt::getAudioInterface();
            ae.setSelectedTrack( Index );
            setVisibility();
        };

        header.onSettingsClick = [&]() {
            auto p = new PadMuteSettings();
            mt::utils::showOptions( "PAD MUTE AUTOMATION SETTINGS", p, [p, this]( int R ) { delete p; } );
        };
        ui::themeChanged.connect( [&]() {
            padMute.setToggleButtonText( "Pad Mute", "Pad Mute" );
            padGroup.setToggleButtonText( "Pad Group", "Pad Group" );
            header.setIcon( "Icon_PadMute" );
        } );
    }

    void KickPadMute::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 4 ) );
    }

    void KickPadMute::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        hdrComp.setBoundsRelative( 0.225f, 0.046f, 0.1956f, 0.0735f );
        padMute.setBoundsRelative( 0.0097f, 0.9230f, 0.1653f, 0.063f );
        padGroup.setBoundsRelative( 0.1790f, 0.9230f, 0.1653f, 0.063f );
        tab->setBoundsRelative( 0, 0.1270f, 1.00f, 0.8662f );
        programLabel.setBoundsRelative( .2294f, 0.03f, .0648f, .0283f );
        infoLabel.setBoundsRelative( .25f, .4f, .5f, .2f );
    }

    void KickPadMute::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED )
        {
            int   bankId;
            int   btnId = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& ae    = mt::getAudioInterface();
            auto& aeSel = ae.getSelected();
            DBG( "Pad Mute: " << btnId );
            switch( btnId )
            {
                    // Bank buttons...
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A: tab->changeBank( "A" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_B: tab->changeBank( "B" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_C: tab->changeBank( "C" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_D: tab->changeBank( "D" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_E: tab->changeBank( "E" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_F: tab->changeBank( "F" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_G: tab->changeBank( "G" ); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_H:
                    tab->changeBank( "H" );
                    break;
                    //Play  Buttons..
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
                default: break; // DBG( "Unknown" ); break;
            }
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
                tab->knobCommand( knob, value );
        }
    }

    void KickPadMute::setVisibility()
    {
        auto& ae     = mt::getAudioInterface();
        auto  pTrack = ae.getTrackData( ae.getSelectedSequence(), ae.getSelectedTrack() );
        auto  type   = mt::MTProgramType::Unknown;
        if( pTrack )
        {
            if( auto pProg = pTrack->prog )
                type = pProg->type.get();
        }
        if( type == mt::MTProgramType::Drum )
        {
            mt::utils::makeVisible( *tab, padMute, padGroup );
            infoLabel.setVisible( false );
        }
        else
        {
            mt::utils::makeHidden( *tab, padMute, padGroup );
            infoLabel.setVisible( true );
        }
    }

    void KickPadMute::visibilityChanged()
    {
        auto& ae          = mt::getAudioInterface();
        auto  programType = ae.getProgramType( ae.getSelectedProgram() );
        if( programType == mt::MTProgramType::Drum || programType == mt::MTProgramType::Clip )
            tab->setScreenState( true );
        else
            tab->setScreenState( false );

        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );

        if( visible )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( mt::PadColorMode::DISABLE_PAD_COLOUR_MODE ) );
            setVisibility();
        }
        else
        {
            auto MTIProg = mt::getAudioInterface().getProgramData( mt::getAudioInterface().getSelectedProgram() );
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( MTIProg ? MTIProg->colourMode.get() : mt::OFF ) );
            contracts::DclSignals::refreshPadLeds();
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  KickPadMuteTab
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    KickPadMuteTab::KickPadMuteTab()
        : pads( std::make_unique<ui::KickPadButtonGroupComponent>( ui::KickPadButton::PadButtonType::muteGroup ) )
    {
        mt::utils::addChildComponents( this, miniMap, groups, *pads, clearSoloBtn, timeDivList, soloBtn, instantAndTD );

        miniMap.setColours( -1, ui::Theme::highlightColour( 3 ), ui::Theme::highlightColour( 2 ), ui::Theme::backgroundColour( 14 ) );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), clearSoloBtn, instantAndTD, soloBtn );
        mt::utils::setValueFontHeight( 18, clearSoloBtn, instantAndTD, soloBtn );

        timeDivList.setEnabled( false );
        timeDivList.setJustificationType( juce::Justification::centred );

        pads->setActive( -1, false );
        pads->setPadButtonSpacer( padButtonSpacer );
        pads->setFocusOutline( false );
        pads->OnButtonClicked = [this]( int Id ) {
            auto pProg = ae.getProgramForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() );
            if( pProg )
            {
                pads->setSelected( Id, juce::dontSendNotification );

                auto prog = ae.getSelectedProgram();
                if( isMiniMap )
                {
                    if( soloBtn.getToggleState() )
                    {
                        if( !pProg->isNoteSolo( Id ) )
                            pProg->setNoteSolo( Id, true );
                        else
                            pProg->setNoteSolo( Id, false );
                    }
                    else
                    {
                        pProg->toggleNoteMute( Id );
                    }
                }
                else
                { // Add to group or remove from group...
                    if( 0 == groups.getSelectedIndex() )
                        pProg->setNoteMuteGroup( Id, -1 );
                    else
                        pProg->setNoteMuteGroup( Id, groups.getSelectedIndex() );
                }
                refreshData();
                setPadLED( Id );
            }
        };

        pads->OnBankChanged = [&]( const juce::String& Bank ) {
            int idx = *Bank.getCharPointer() - 'A';
            miniMap.select( idx, true );
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( idx ) );
            refreshData();
            refreshAllLEDs();
        };

        miniMap.onClicked = [&]( juce::String Bank, int Pad ) {
            pads->setBank( Bank );
        };

        instantAndTD.onClick = [&]() {
            timeDivList.setEnabled( instantAndTD.getToggleState() );
        };

        clearSoloBtn.onClick = [&]() {
            if( auto pProg = ae.getProgramForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() ) )
            {
                if( !dynamic_cast<mt::MTDrumProgram*>( pProg )->resetNotesSolo() )
                {
                    refreshData();
                    refreshAllLEDs();
                }
            }
        };

        ui::themeChanged.connect( [&]() {
            soloBtn.setToggleButtonText( "Solo", "Solo" );
            groups.setContent( groupNumbers, groupActiveNumbers, groups.getSelectedIndex() );
        } );
    }

    void KickPadMuteTab::changeBank( juce::String bank )
    {
        int idx = *bank.getCharPointer() - 'A';
        pads->setBank( bank );
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( idx ) );
    }

    void KickPadMuteTab::displayPadState( const int id, const bool state, const juce::String& name, int muteGroup )
    {
        pads->setActive( id, true );
        pads->setRow1Text( id, juce::File( name ).getFileNameWithoutExtension(), juce::dontSendNotification );
        pads->setMute( id, state, juce::dontSendNotification );
        auto pProg = ae.getProgramForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() );
        if( pProg->isNoteSolo( id ) )
        {
            pads->setBorderColor( id, ui::Theme::padButtonColour( 8 ), borderThickness, juce::dontSendNotification );
            miniMap.setColours( id, ui::Theme::highlightColour( 3 ), ui::Theme::highlightColour( 2 ), ui::Theme::backgroundColour( 14 ), ui::Theme::padButtonColour( 8 ), true ); //solo - blue
        }
        else
        {
            if( pProg->getNumSoloNotes() )
            {
                pads->setBorderColor( id, state ? ui::Theme::highlightColour( 2 ) : ui::Theme::padButtonColour( 11 ), borderThickness, juce::dontSendNotification );
                miniMap.setColours( id, ui::Theme::padButtonColour( 11 ), ui::Theme::highlightColour( 2 ), ui::Theme::backgroundColour( 14 ) ); //solo normal - light pink
            }
            else
            {
                pads->setBorderColor( id, state ? ui::Theme::highlightColour( 2 ) : ui::Theme::highlightColour( 3 ), borderThickness, juce::dontSendNotification );
                miniMap.setColours( id, ui::Theme::highlightColour( 3 ), ui::Theme::highlightColour( 2 ), ui::Theme::backgroundColour( 14 ) );
            }
        }
        pads->setGroupText( id, 0 < muteGroup ? juce::String( muteGroup ) : juce::String( "" ), juce::dontSendNotification );

        miniMap.setActive( id, true );
        miniMap.setHighlight( id, state );
    }

    void KickPadMuteTab::refreshData()
    {
        //std::vector<contracts::PadData> padData;

        pads->clearAllPadData();
        pads->setActive( -1, false );

        miniMap.setActive( -1, false );
        miniMap.clearAll();

        // Update pad name and mute status...
        samples.clear();

        if( auto pProg = ae.getProgramForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() ) )
            pProg->getSamples( samples );
        for( const auto it : samples )
        {
            displayPadState( it->keyNote.get(), it->mute.get(), it->getSource( 0 ), it->muteGroup.get() );
        }
        // Highlight selected minimap
        int idx = *pads->getBank().getCharPointer() - 'A';
        miniMap.select( idx, true );
    }

    void KickPadMuteTab::refreshAllLEDs()
    {
        ae.getProgramSamples( ae.getSelectedProgram(), samples );
        std::vector<contracts::PadData> padData;
        auto                            bankOffset = pads->getBankOffset();

        for( auto colourIndex = bankOffset; colourIndex < bankOffset + NUM_PADS_IN_BANK; colourIndex++ )
        {
            bool pushedToColourData = false;

            for( auto& it : samples )
            {
                auto index = it->keyNote.get() - bankOffset;
                if( juce::isPositiveAndBelow( index, 16 ) && it->getSource( 0 ).length() && colourIndex == it->keyNote.get() )
                {
                    if( isScreenEnable )
                    {
                        padData.push_back( { colourIndex - bankOffset, contracts::DeviceControlContracts::PadButtonState::StateOn, it->mute.get() ? contracts::DeviceControlContracts::PadColourIds::PadColourRed : contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 } );
                        pushedToColourData = true;
                    }
                    break;
                }
            }
            if( !pushedToColourData )
                padData.push_back( { colourIndex - bankOffset, contracts::DeviceControlContracts::PadButtonState::StateOff, contracts::DeviceControlContracts::PadColourIds::PadColourTrans } );
        }
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setPadColourAndStateInfoMessage( padData ) );
    }

    void KickPadMuteTab::setPadLED( int padIndex )
    {
        ae.getProgramSamples( ae.getSelectedProgram(), samples );

        //Check whether the selected pad is a GroupTrack or not
        int  muteGroup   = -1;
        bool isMuteGroup = false;
        for( auto& it : samples )
        {
            int muteGroupNum = it->muteGroup.get();
            if( padIndex == it->keyNote.get() && muteGroupNum != -1 )
            {
                isMuteGroup = true;
                muteGroup   = it->muteGroup.get();
                break;
            }
            else
                isMuteGroup = false;
        }

        for( auto& it : samples )
        {
            auto index = (int)it->keyNote.get() - pads->getBankOffset();
            if( !isMuteGroup ) //it is for non group Pad
            {
                if( juce::isPositiveAndBelow( index, 16 ) && it->getSource( 0 ).length() && padIndex == (int)it->keyNote.get() )
                {
                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                 setPadDataMessage( index,
                                                                                    contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                    it->mute.get() ? contracts::DeviceControlContracts::PadColourIds::PadColourRed :
                                                                                                     contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 ) );

                    break;
                }
            }
            else //it is for non group Pad
            {
                if( juce::isPositiveAndBelow( index, 16 ) && it->getSource( 0 ).length() && muteGroup == it->muteGroup.get() )
                {
                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                 setPadDataMessage( index,
                                                                                    contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                    it->mute.get() ? contracts::DeviceControlContracts::PadColourIds::PadColourRed :
                                                                                                     contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 ) );
                }
            }
        }
    }

    void KickPadMuteTab::visibilityChanged()
    {
        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );

        if( visible )
        {
            refreshData();
            refreshAllLEDs();
        }

        resized();
    }

    void KickPadMuteTab::showMiniMap()
    {
        isMiniMap = true;
        resized();
    }

    void KickPadMuteTab::showGroups()
    {
        isMiniMap = false;
        resized();
    }

    void KickPadMuteTab::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, miniMap.getBounds(), ui::Theme::backgroundColour( 3 ), 5.0f );
        mt::utils::renderRoundedRect( g, miniMap.getBounds(), ui::Theme::textColour( 5 ), 1, 6.0f );
    }

    void KickPadMuteTab::knobCommand( int knob, int value )
    {
        pads->setKnobBorder( knob );
        assignKnobToPad( knob, value );
    }

    void KickPadMuteTab::assignKnobToPad( int knobId, int value )
    {
        int padId                 = knobId - 1;
        int currentGroupNumberStr = pads->getGroupNumber( padId );
        int currentGroupNumber    = currentGroupNumberStr + value;
        currentGroupNumber        = juce::jlimit( 1, 16, currentGroupNumber );
        ae.padAddToGroup( ae.getSelectedSequence(), padId, currentGroupNumber );
        pads->setGroupText( padId, juce::String( currentGroupNumber ), juce::dontSendNotification );
    }

    void KickPadMuteTab::resized()
    {
        groups.setVisible( !isMiniMap );
        miniMap.setVisible( isMiniMap );

        miniMap.setBoundsRelative( 0.0097f, 0.0393f, 0.3346f, 0.8644f );
        groups.setBoundsRelative( 0.0097f, 0.0389f, 0.3346f, 0.8644f );
        pads->setBoundsRelative( 0.3581f, 0.0389f, 0.6301f, 0.8690f );

        clearSoloBtn.setBoundsRelative( .504f, 0.9230f, 0.1174f, 0.0732f );
        instantAndTD.setBoundsRelative( 0.626f, 0.9230f, 0.1174f, 0.0732f );
        timeDivList.setBoundsRelative( 0.7480f, 0.9230f, 0.1174f, 0.0732f );
        soloBtn.setBoundsRelative( 0.869f, 0.9230f, 0.1174f, 0.0732f );
    }

    PadMuteSettings::PadMuteSettings()
    {
        mt::utils::addAndMakeVisible( this, label1, label2, label3, apply, cancel, instantPadMute, playPadMuteSoloEvents, recordPadMuteSoloEvents );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), label1, label2, label3, apply, cancel );
        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), label1, label2, label3 );
        mt::utils::setValueFontHeight( 20, label1, label2, label3, instantPadMute, playPadMuteSoloEvents, recordPadMuteSoloEvents );
        mt::utils::setValueFontHeight( 18, apply, cancel );

        apply.onClick  = [&]() { mt::utils::exitDialog( this, UI::APPLY ); };
        cancel.onClick = [&]() { mt::utils::exitDialog( this, UI::CANCEL ); };
    }

    void PadMuteSettings::resized()
    {
        label1.setBoundsRelative( .1083f, .3538f, .3846f, .054f );
        label2.setBoundsRelative( .1083f, .5135f, .3846f, .054f );
        label3.setBoundsRelative( .1083f, .6732f, .3846f, .054f );

        instantPadMute.setBoundsRelative( .5538f, .3292f, .2461f, .1081f );
        playPadMuteSoloEvents.setBoundsRelative( .5538f, .4889f, .2461f, .1081f );
        recordPadMuteSoloEvents.setBoundsRelative( .5538f, .6486f, .2461f, .1081f );
    }
}
