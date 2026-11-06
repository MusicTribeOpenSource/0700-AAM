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

#pragma once

#include "kick_ui_settings_camera.h"

namespace kick_ui
{
    namespace
    {
        struct MappedGesture
        {
            contracts::GestureKind kind;
            const char*            label;
        };
    }

    SettingsCamera::SettingsCamera()
        : SettingsInterface( 6, 3, true )
    {
        ui::KickGrid::setBackgroundColour( juce::Colours::transparentBlack );
        setRowTitles( { "", "CAMERA", "GESTURES", "SWIPES", "HAND SHAPES", "LAST GESTURE" } );

        addComponent( cameraSwitch, 1, 0 );
        addComponent( cameraStatus, 1, 1, 2 );
        addComponent( gestureSwitch, 2, 0 );
        addComponent( sensitivity, 2, 1 );
        addComponent( sensitivityValue, 2, 2 );
        addComponent( swipeLeft, 3, 0 );
        addComponent( swipeRight, 3, 1 );
        addComponent( serviceStatus, 3, 2 );
        addComponent( palmHold, 4, 0 );
        addComponent( fist, 4, 1 );
        addComponent( pinch, 4, 2 );
        addComponent( lastGesture, 5, 0, 3 );

        sensitivity.setRange( 1.0, 10.0, 1.0 );

        // The switch asks the camera service and then follows what it reports. It does not decide the state itself.
        cameraSwitch.onClick = [this]() {
            kick_gesture::sharedCameraService().requestEnabled( cameraSwitch.getToggleState() );
        };

        gestureSwitch.onClick = [this]() {
            settings.gesture.enabled = gestureSwitch.getToggleState();
        };

        sensitivity.onValueChanged = [this]( double v ) {
            const int s = kick_gesture::clampSensitivity( static_cast<int>( v ) );
            settings.gesture.sensitivity = s;
            sensitivityValue.setText( "Sensitivity " + juce::String( s ) + " of 10", juce::dontSendNotification );
        };

        swipeLeft.onClick  = [this]() { cycleMapping( contracts::GestureKind::SwipeLeft ); };
        swipeRight.onClick = [this]() { cycleMapping( contracts::GestureKind::SwipeRight ); };
        palmHold.onClick   = [this]() { cycleMapping( contracts::GestureKind::PalmHold ); };
        fist.onClick       = [this]() { cycleMapping( contracts::GestureKind::Fist ); };
        pinch.onClick      = [this]() { cycleMapping( contracts::GestureKind::PinchChange ); };

        cameraConnection  = contracts::GestureSignals::cameraStatusChanged.connect( [this]( const contracts::CameraStatus& s ) { showCameraStatus( s ); } );
        serviceConnection = contracts::GestureSignals::serviceStatusChanged.connect( [this]( const contracts::GestureServiceStatus& s ) { showServiceStatus( s ); } );
        gestureConnection = contracts::GestureSignals::gestureReceived.connect( [this]( const contracts::GestureEvent& e ) {
            juce::String text = juce::String( contracts::gestureKindName( e.kind ) ) + ", " + contracts::gestureHandName( e.hand ) + " hand, " + juce::String( juce::roundToInt( e.confidence * 100.f ) ) + "%";
            if( e.hasValue )
                text << ", value " << juce::String( e.value, 2 );
            lastGesture.setText( text, juce::dontSendNotification );
        } );

        showCameraStatus( camera );
        showServiceStatus( {} );
    }

    SettingsCamera::~SettingsCamera() = default;

    juce::String SettingsCamera::mappingCaption( contracts::GestureKind kind ) const
    {
        juce::String who;
        switch( kind )
        {
            case contracts::GestureKind::SwipeLeft: who = "Swipe left: "; break;
            case contracts::GestureKind::SwipeRight: who = "Swipe right: "; break;
            case contracts::GestureKind::PalmHold: who = "Open palm: "; break;
            case contracts::GestureKind::Fist: who = "Fist: "; break;
            case contracts::GestureKind::PinchChange: who = "Pinch: "; break;
            default: break;
        }
        return who + kick_gesture::describeAction( kind, mapping.get( kind ) );
    }

    void SettingsCamera::showMapping( contracts::GestureKind kind )
    {
        ui::KickToggleButton* b = nullptr;
        switch( kind )
        {
            case contracts::GestureKind::SwipeLeft: b = &swipeLeft; break;
            case contracts::GestureKind::SwipeRight: b = &swipeRight; break;
            case contracts::GestureKind::PalmHold: b = &palmHold; break;
            case contracts::GestureKind::Fist: b = &fist; break;
            case contracts::GestureKind::PinchChange: b = &pinch; break;
            default: return;
        }
        const auto caption = mappingCaption( kind );
        b->setToggleButtonText( caption, caption );
        b->setButtonText( caption );
    }

    void SettingsCamera::cycleMapping( contracts::GestureKind kind )
    {
        const auto& choices = kick_gesture::actionChoices( kind );
        if( choices.empty() )
            return;
        size_t at = 0;
        for( size_t i = 0; i < choices.size(); ++i )
            if( choices[i] == mapping.get( kind ) )
                at = i;
        std::string why;
        if( !mapping.set( kind, choices[( at + 1 ) % choices.size()], why ) )
        {
            juce::Logger::writeToLog( "gesture: could not set mapping: " + juce::String( why ) );
            return;
        }
        mappingText = mapping.toString();
        settings.gesture.mapping = juce::String( mappingText );
        showMapping( kind );
    }

    void SettingsCamera::showCameraStatus( const contracts::CameraStatus& s )
    {
        camera = s;
        // The switch follows the camera service's capturing flag. A tap that the service refused snaps back.
        cameraSwitch.setToggleState( s.capturing, juce::dontSendNotification );
        cameraSwitch.setEnabled( s.state != contracts::CameraState::ServiceMissing );
        cameraStatus.setText( juce::String( kick_gesture::describeCameraStatus( s ) ), juce::dontSendNotification );
    }

    void SettingsCamera::showServiceStatus( const contracts::GestureServiceStatus& s )
    {
        juce::String text;
        if( !s.connected )
            text = "Gesture service not connected";
        else if( s.state.empty() || s.state == "running" )
            text = "Gesture service running";
        else
            text = "Gesture service " + juce::String( s.state ) + ( s.reason.empty() ? juce::String() : " (" + juce::String( s.reason ) + ")" );
        serviceStatus.setText( text, juce::dontSendNotification );
    }

    void SettingsCamera::refresh()
    {
        // Pick up a mapping that changed in the preferences file.
        const std::string text = settings.gesture.mapping().toStdString();
        if( text != mappingText )
        {
            mappingText = text;
            std::string problems;
            kick_gesture::MappingTable::fromString( text, mapping, problems );
        }
        for( auto kind : { contracts::GestureKind::SwipeLeft, contracts::GestureKind::SwipeRight, contracts::GestureKind::PalmHold, contracts::GestureKind::Fist, contracts::GestureKind::PinchChange } )
            showMapping( kind );

        gestureSwitch.setToggleState( settings.gesture.enabled(), juce::dontSendNotification );
        const int s = kick_gesture::clampSensitivity( settings.gesture.sensitivity() );
        sensitivity.setValue( s, juce::dontSendNotification );
        sensitivityValue.setText( "Sensitivity " + juce::String( s ) + " of 10", juce::dontSendNotification );
        showCameraStatus( camera );
    }

    void SettingsCamera::setupHeader( kick_ui::KickModeHeader& Hdr ) {}

    void SettingsCamera::handleCommand( CPktMgr::HWMsg_t& ) {}
}
