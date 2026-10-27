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

#pragma once
#include "kick_ui/audio_components//gridview/kick_Locate.h"
namespace kick_ui
{

    class LeftControl : public juce::Component
    {
    public:
        LeftControl();
        ~LeftControl() = default;
        mt::ac::LocateSTP stp;

    private:
        void resized() override;
        void mouseDown( const juce::MouseEvent& e ) override;
    };

    class RightControl : public juce::Component
    {
    public:
        RightControl();
        ~RightControl() = default;
        mt::ac::Locate bbt;

    private:
        void resized() override;
        void mouseDown( const juce::MouseEvent& e ) override;
    };

    class AppStatusbar : public juce::Component, public juce::DeletedAtShutdown
    {
    public:
        ~AppStatusbar() = default;

        static AppStatusbar& Instance();

        void setBoxInfo( juce::String );

    private:
        mt::MTPlayhead&   playHead{ mt::getAudioInterface().getPlayhead() };
        juce::ProgressBar progressBar{ playHead.normized };
        LeftControl       left;
        RightControl      right;
        ui::KickLabel     boxInfo{ "Box Info" };

        // Don't any direct instantiaion of this class.
        AppStatusbar();

        bool init{ false };

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AppStatusbar )
    };

}
