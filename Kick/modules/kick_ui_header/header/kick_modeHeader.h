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
    class KickModeHeader : public juce::Component
    {
    public:
        std::function<void( bool )> onBackButtonPressed;
        std::function<void( void )> onSettingsClick;
        std::function<void( bool )> onPlayStopClick;
        std::function<void( void )> onAutoLoadClick;
        std::function<void( void )> onNewProjectClick;

        KickModeHeader( const juce::String& Name, const juce::String& Image );

        void showAutoLoad( bool show );
        void showBackButton( bool Show );
        void setPlayState( bool State, juce::NotificationType Notify = juce::sendNotification );
        void showSettingsButton( bool Show );
        void showPlayButton( bool Show );
        void showNewProjectButton( bool Show );
        void showOptions( bool ShowMetronome, bool ShowAutomation );
        void setIcon( const juce::String& Image );
        void addComponent( juce::Component* Child );
        void addButton( juce::Component* Button, int Width = 44 );
        void removeButton( juce::Component* Button );
        void removeAllButtons();
        void setEnableNewProject( bool enable );
        void setNewProjectToggleState( bool state, juce::NotificationType notification = juce::dontSendNotification );
        void setAutoLoadToggleState( bool state, juce::NotificationType notification = juce::dontSendNotification );

        juce::Component* getComponent() { return comp; }

    private:
        const float iconSpace{ 4.0 / 1022.0 };
        const float buttonSpace{ 2.0 / 1022.0 };
        const float iconWidth{ 44.0 / 1022.0 };
        const float iconX{ ( 1022.0f - 56.0f ) / 1022.0f };
        const float buttonX{ ( 18.0f / 1022.0f ) };
        const float iconSize{ iconWidth + iconSpace };
        const float newProjectWidth{ 90.0f / 1022.0f };
        const float newProjectSize{ newProjectWidth + iconSpace };

        bool                                          bShowSettings{ false };
        bool                                          bShowPlay{ false };
        bool                                          bShowNewProject{ false };
        bool                                          bShowAutomation{ true }, bShowMetronome{ true };
        bool                                          bshowBackbutton{ true };
        bool                                          bAutoLoad{ false };
        ui::KickImage                                 icon{ "Icon" };
        ui::KickLabel                                 headerLabel{ "" };
        ui::KickImageButton                           resources{ "Resources", "Resource" };
        ui::KickImageButton                           metronome{ "Metronome", "Metronome" };
        ui::KickImageButton                           automation{ "Automation", "Automation_Normal" };
        ui::KickImageButton                           settings{ "Settings", "Settings_Header" };
        ui::KickImageButton                           playStop{ "Play/Stop", "Play_Grid", "Stop_Active" };
        ui::KickImageButton                           newProject{ "NewProject", "NewProject" };
        ui::KickImageButton                           back{ "Back", "Back", "Back_Active" };
        ui::KickToggleButton                          autoLoad{ "A", "A", "A" };
        juce::Component*                              comp{ nullptr };
        std::vector<std::pair<juce::Component*, int>> buttons;

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickModeHeader )
    };

    class MetronomePopup : public juce::Component, public juce::DeletedAtShutdown
    {
    public:
        MetronomePopup();
        ~MetronomePopup() = default;

        double getLevelSlider() { return level.getValue(); }
        void   linkRotated( int knob, double value );

        static MetronomePopup& Instance()
        {
            ui::KickLooknFeel::getLnf();
            static auto* metronome = new MetronomePopup;
            return *metronome;
        }
        static void Show();

    private:
        mt::KickMetronome& metronome{ mt::getSettings().metronome };

        ui::KickSlider        level{ "Level", juce::Slider::LinearBar };
        ui::KickToggleOptions onOffButton{ "ENABLE", { "Off", "On" } };
        ui::KickListBox       mode{ "MODE", { "RECORD", "PLAY", "RECORD + PLAY" } };
        ui::KickListBox       countIn{ "COUNTIN", { "Off" } };
        ui::KickListBox       rate{ "RATE", { "1/16", "1/8", "1/4", "1/2" }, 2 };
        ui::KickListBox       style{ "STYLE", { "STYLE-1", "STYLE-2", "STYLE-3", "STYLE-4", "STYLE-5", "STYLE-6", "STYLE-7" }, 0, 2 };

        void resized();
        void close();
    };
}
