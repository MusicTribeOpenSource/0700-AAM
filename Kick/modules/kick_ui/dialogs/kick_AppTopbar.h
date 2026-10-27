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
namespace kick_ui
{
    class AppTopbar : public juce::Component
        , private mt::MTAudioInterfaceListener
    {
    public:
        AppTopbar();
        ~AppTopbar() override { ae.removeListener(this); }

    private:
        void                  resized() override;
        void                  visibilityChanged() override;
        void                  onProjectLoaded( juce::File ) override;
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };

    public:
        const int buttonWidth{ 84 };
        const int iconButtonWidth{ 64 };
        const int buttonMargin{ 2 };

        ui::KickImageButton menuButton{ TRANS( "Menu" ), "Main_normal", "Main_active" };
        ui::KickImageButton mainModeButton{ TRANS( "Home" ), "Home_normal", "Home_active" };
        ui::KickImageButton gridButton{ TRANS( "Grid" ), "Main Edit_normal", "Main Edit_active" };

        ui::KickImageButton stepSeqButton{ TRANS( "StepSequencer" ), "Step Seq_normal", "Step Seq_active" };
        ui::KickImageButton axisButton{ TRANS( "Axis" ), "XY_normal", "XY_active" };

        ui::KickImageButton metronomeButton{ TRANS( "Metronome" ), "Metronome_normal", "Metronome_active" };
        ui::KickImageButton kickPlayButton{ TRANS( "Play" ), "Play Control_normal", "Stop_Active" };
        ui::KickImageButton kickBrowseButton{ TRANS( "Browse" ), "File_normal", "File_active" };
        ui::KickImageButton newFileButton{ TRANS( "New File" ), "New File_normal", "New File_active" };

        ui::KickImageButton settingsButton{ TRANS( "Settings" ), "Settings_normal", "Settings_active" };
        ui::KickLabel       project{ "Unknown Project" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AppTopbar )
    };
}
