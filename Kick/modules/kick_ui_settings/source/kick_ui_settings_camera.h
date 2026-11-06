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

// Module include files.
#include <gesture/gesture.h>

namespace kick_ui
{
    // Camera and gesture control: the camera switch, the gesture switch, sensitivity, what each gesture does, and a
    // line showing the last gesture the app heard.
    class SettingsCamera : public SettingsInterface
    {
    public:
        SettingsCamera();
        ~SettingsCamera() override;

        // Virtual functions
        void refresh() override;
        void setupHeader( kick_ui::KickModeHeader& Hdr ) override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;

    private:
        void showCameraStatus( const contracts::CameraStatus& s );
        void showServiceStatus( const contracts::GestureServiceStatus& s );
        void cycleMapping( contracts::GestureKind kind );
        void showMapping( contracts::GestureKind kind );
        juce::String mappingCaption( contracts::GestureKind kind ) const;

    private:
        mt::KickSettings& settings{ mt::getSettings() };

        ui::KickToggleButton cameraSwitch{ "camera", "ON", "OFF" };
        ui::KickLabel        cameraStatus{ "cameraStatus", "" };
        ui::KickToggleButton gestureSwitch{ "gestures", "ON", "OFF" };
        ui::KickSlider       sensitivity{ "Sensitivity", juce::Slider::LinearBar, juce::Slider::NoTextBox };
        ui::KickLabel        sensitivityValue{ "sensitivityValue", "" };
        ui::KickLabel        serviceStatus{ "serviceStatus", "" };

        // One button per mapped gesture. A tap moves to the next choice in kick_gesture::actionChoices.
        ui::KickToggleButton swipeLeft{ "swipeLeft", "", "" };
        ui::KickToggleButton swipeRight{ "swipeRight", "", "" };
        ui::KickToggleButton palmHold{ "palmHold", "", "" };
        ui::KickToggleButton fist{ "fist", "", "" };
        ui::KickToggleButton pinch{ "pinch", "", "" };

        ui::KickLabel lastGesture{ "lastGesture", "No gesture yet" };

        kick_gesture::MappingTable mapping;
        std::string                mappingText{ "\x01" };
        contracts::CameraStatus    camera;

        sigslot::scoped_connection cameraConnection, serviceConnection, gestureConnection;
    };
}
