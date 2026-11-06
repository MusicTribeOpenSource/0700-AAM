/*
 * Copyright (c) 2024 MUSIC Tribe
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

#include "kick_ui_settings.h"

// Module source files...
#include "source/kick_ui_settings_audio.cpp"
#include "source/kick_ui_settings_camera.cpp"
#include "source/kick_ui_settings_cv.cpp"
#include "source/kick_ui_settings_defaults.cpp"
#include "source/kick_ui_settings_etnernet.cpp"
#include "source/kick_ui_settings_hardware.cpp"
#include "source/kick_ui_settings_info.cpp"
#include "source/kick_ui_settings_midi_sync.cpp"
#include "source/kick_ui_settings_wifi.cpp"

namespace kick_ui
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  TextValue Component...
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    TextValue::TextValue( const juce::String& T, const juce::String& V )
    {
        title.setText( T + " :", juce::dontSendNotification );
        value.setText( V, juce::dontSendNotification );
        title.setTextColour( ui::Theme::titleColour( 1 ) );
        value.setTextColour( ui::Theme::textColour( 1 ) );
        title.setValueBackgroundColour( juce::Colours::transparentBlack );
        value.setValueBackgroundColour( juce::Colours::transparentBlack );
        title.showBorder( false );
        value.showBorder( false );
        title.setFontHeight( 20 );
        value.setFontHeight( 20 );
        title.setPadding( 10 );
        value.setPadding( 10 );
        value.setValueJustification( juce::Justification::right );

        mt::utils::addAndMakeVisible( this, title, value );
    }

    void TextValue::resized()
    {
        auto b = getLocalBounds();
        title.setBounds( b.removeFromLeft( b.proportionOfWidth( 0.5 ) ) );
        value.setBounds( b );
    }

    void TextValue::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 3 ) );
        g.fillRoundedRectangle( getLocalBounds().toFloat(), 5.f );
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  KickChannelMixer
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    Settings::Settings()
        : kick_ui::IScreen( kick_ui::Screen::HomePreferences )
    {
        // Add header...
        mt::utils::addChildComponents( this, header );

        // Create views...
        pViews.emplace_back( std::make_unique<SettingsInfo>() );
        pViews.emplace_back( std::make_unique<SettingsPDefault>() );
        pViews.emplace_back( std::make_unique<SettingsHardware>() );
        // pViews.emplace_back( new SettingsAudio );
        pViews.emplace_back( std::make_unique<SettingsMidiSync>() );
        pViews.emplace_back( std::make_unique<SettingsCV>() );
        pViews.emplace_back( std::make_unique<SettingsEthernet>() );
        pViews.emplace_back( std::make_unique<SettingsWifi>() );
pViews.emplace_back( std::make_unique<SettingsCamera>() );

        // Add selector buttons...
        auto idx = 0;
        for( auto& it : selectors )
        {
            auto name = selectorStrings[idx];
            it.setName( name );
            it.setTitle( name );
            it.setToggleButtonText( name, name );
            it.setRadioGroupId( (int)RadioGroupId::SettingsScreen, juce::dontSendNotification );
            mt::utils::addChildComponents( this, it );

            it.onClick = [this, idx]() {
                auto state = selectors[idx].getToggleState();
                if( state )
                {
                    showView( idx );
                }
            };
            ++idx;
        }
        selectors[0].setToggleState( true, juce::sendNotification );

        header.onBackButtonPressed = [&]( bool ) { closeScreen(); };

        // Defaults...
        selectors[0].setToggleState( true, juce::sendNotification );

        ui::themeChanged.connect( [&]() {
            header.setIcon( "Icon_SettingsScreen" );
        } );
    }

    void Settings::showView( int Index )
    {
        if( pView )
        {
            pView->setVisible( false );
            removeChildComponent( pView );
        }
        if( juce::isPositiveAndBelow( Index, pViews.size() ) )
        {
            pView = pViews[Index].get();
            if( pView )
            {
                pView->setVisible( true );
                addChildComponent( pView );
            }
        }
    }

    void Settings::timerCallback()
    {
        if( pView )
            pView->refresh();
    }

    void Settings::handleCommand( CPktMgr::HWMsg_t& hwMsg )
    {
        if( pView )
            pView->handleCommand( hwMsg );
    }

    void Settings::resized()
    {
        auto b = getLocalBounds();
        // set header bounds
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        // Set selector bounds
        b               = b.reduced( 5, 0 );
        auto bw         = b.getWidth() / (int)selectors.size();
        auto bSelectors = b.removeFromTop( 64 );
        for( auto i = 0; i < selectors.size(); ++i )
        {
            selectors[i].setBounds( bSelectors.removeFromLeft( bw ).reduced( 5, 10 ) );
        }
        // Set view bounds
        bCont = b.reduced( 5, 0 );
        for( const auto& it : pViews )
            it->setBounds( bCont );
    }

    void Settings::visibilityChanged()
    {
        bool visible = isVisible();
        auto str     = "Settings " + visible ? "VISIBLE" : "NOT VISIBLE";
        DBG( str );

        if( visible )
        {
            startTimerHz( 1 );
        }
        else
        {
            stopTimer();
        }

        for( auto it : getChildren() )
            it->setVisible( visible );
    }
}
