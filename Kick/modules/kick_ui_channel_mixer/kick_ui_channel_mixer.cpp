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

#ifdef KICK_UI_COMPONENTS_INCLUDE
/* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
    #error "Incorrect use of JUCE cpp file"
#endif

#include "kick_ui_channel_mixer.h"

// Module source files...
#include "source/kick_program_mix.cpp"
#include "source/kick_sends_mix.cpp"
#include "source/kick_returns_mix.cpp"
#include "source/kick_submixes_mix.cpp"
#include "source/kick_track_midi_mix.cpp"
#include "source/kick_audio_track_mix.cpp"
#include "source/kick_output_track_mix.cpp"

namespace kick_ui
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  ChannelMixerHeaderComponent
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ChannelMixerHeaderComponent::ChannelMixerHeaderComponent()
    {
        mt::utils::addAndMakeVisible( this, type );
        type.setListOptions( { "Programs", "Sends", "Returns", "Submixes", "Output", "Midi", "Audio" } );
    }

    void ChannelMixerHeaderComponent::resized()
    {
        type.setBounds( getLocalBounds() );
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  KickChannelMixer
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ChannelMixer::ChannelMixer()
        : kick_ui::IScreen( kick_ui::Screen::MenuChannelMixer )
    {
        pMaster.reset( new MasterRouteDisplay() );

        // Create views...
        pViews.emplace_back( new kick_ui::ProgramMixer() );
        pViews.emplace_back( new kick_ui::SendsMixer() );
        pViews.emplace_back( new kick_ui::RetrunsMixer() );
        pViews.emplace_back( new kick_ui::SubmixMixer() );
        pViews.emplace_back( new kick_ui::OutputTrackMixer() );
        pViews.emplace_back( new kick_ui::TrackMidiMixer() );
        pViews.emplace_back( new kick_ui::AudioTrackMixer() );

        mt::utils::addChildComponents( this, header, pMaster.get() );
        mt::utils::addAndMakeVisible( this, headerComp );
        for( const auto& it : pViews )
            mt::utils::addChildComponents( this, it.get() );

        header.onBackButtonPressed = [&]( bool ) { closeScreen(); };
        headerComp.type.setJustificationType( juce::Justification::centredLeft );
        headerComp.type.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        headerComp.type.setBackGroundColours( ui::Theme::backgroundColour( 13 ), ui::Theme::backgroundColour( 2 ) );

        headerComp.type.onSelectionChanged = [&]( int Sel, const juce::String& ) {
            if( 0 > Sel )
                headerComp.type.setSelected( 0 );
            else
                showView( Sel );
        };

        // Defaults...
        headerComp.type.setSelected( 0, juce::sendNotification );

        ui::themeChanged.connect( [&]() {
            header.setIcon( "Icon_Mixer" );
        } );
    }

    ChannelMixer::~ChannelMixer()
    {
        pMaster.reset();
    }

    void ChannelMixer::showView( int Index )
    {
        for( const auto& it : pViews )
            it->setVisible( false );

        pViews[Index]->setVisible( true );
        pView = pViews[Index].get();
        index = Index;
    }

    void ChannelMixer::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 4 ) );
    }

    void ChannelMixer::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        headerComp.setBoundsRelative( 0.225f, .029f, 0.1956f, .082f );
        if( pMaster ) pMaster->setBounds( b.removeFromBottom( 122 ).withTrimmedBottom( 10 ) );
        bCont = b;
        bCont.removeFromLeft( 10 );
        for( const auto& it : pViews )
            it->setBounds( bCont );
    }

    void ChannelMixer::visibilityChanged()
    {
        bool visible = isVisible();
        if( visible )
        {
            headerComp.type.setSelected( headerComp.type.getSelectedIndex(), juce::sendNotification );
            startTimerHz( 25 );
            DBG( "ChannelMixer VISIBLE" );
        }
        else
        {
            stopTimer();
            //Need to make it off
            DBG( "ChannelMixer is not VISIBLE" );
        }

        header.setVisible( visible );
        if( pMaster ) pMaster->setVisible( visible );
    }

    void ChannelMixer::timerCallback()
    {
        if( auto p = dynamic_cast<MasterRouteDisplay*>( pMaster.get() ) )
            p->meterUpdate();
        if( pView )
            pView->meterUpdate();
    }
    void ChannelMixer::knobCommand( int knob, int value )
    {
        if( pMaster )
            dynamic_cast<MasterRouteDisplay*>( pMaster.get() )->knobCommand( knob, value );

        switch( index )
        {
            case 0:
                dynamic_cast<ProgramMixer*>( pView )->knobCommand( knob, value );
                break;
            case 1:
                dynamic_cast<SendsMixer*>( pView )->knobCommand( knob, value );
                break;
            case 2:
                dynamic_cast<RetrunsMixer*>( pView )->knobCommand( knob, value );
                break;
            case 3:
                dynamic_cast<SubmixMixer*>( pView )->knobCommand( knob, value );
                break;
            case 4:
                dynamic_cast<TrackMidiMixer*>( pView )->knobCommand( knob, value );
                break;
            case 5:
                dynamic_cast<OutputTrackMixer*>( pView )->knobCommand( knob, value );
                break;
            case 6:
                dynamic_cast<AudioTrackMixer*>( pView )->knobCommand( knob, value );
                break;
        }
    }

    void ChannelMixer::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
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
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
                knobCommand( knob, value );
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
    }
}
