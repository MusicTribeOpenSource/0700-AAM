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
    class KickPadMuteTab;
    class KickPadMute : public kick_ui::IScreen
        , public juce::Component
    {
    public:
        KickPadMute();

        bool canClose() { return true; }
        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void handleCommand( CPktMgr::HWMsg_t& ) override;

    private:
        AppStatusbar& statusBar{ AppStatusbar::Instance() };

        KickModeHeader                  header{ "Pad Mute", "Icon_PadMute" };
        std::unique_ptr<KickPadMuteTab> tab;
        HeaderBarTrackSelector          hdrComp;
        ui::KickLabel                   programLabel{ "PROGRAM" };
        ui::KickLabel                   infoLabel{ "" };
        ui::KickToggleButton            padMute{ "PadMute", "Pad Mute", "Pad Mute" };
        ui::KickToggleButton            padGroup{ "Pad Group", "Pad Group", "Pad Group" };
        juce::Rectangle<int>            rect{ 10, 86, 342, 396 };
        bool                            isvisible{ false };

    private:
        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;
        void setVisibility();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickPadMute )
    };

    class KickPadMuteTab : public juce::Component
    {
    public:
        KickPadMuteTab();
        void showMiniMap();
        void showGroups();
        void refreshData();
        void refreshAllLEDs();
        void setPadLED( int );
        void changeBank( juce::String bank );
        void knobCommand( int knob, int value );
        void assignKnobToPad( int knobId, int value );
        void setScreenState( bool IsScreenEnable ) { isScreenEnable = IsScreenEnable; }

    private:
        bool                                             isMiniMap{ true };
        bool                                             isScreenEnable{ false };
        mt::MTAudioInterface&                            ae{ mt::getAudioInterface() };
        std::unique_ptr<ui::KickPadButtonGroupComponent> pads;
        ui::KickPadMiniMap                               miniMap;
        ui::KickListWithImage                            groups{ "OPTIONS", groupNumbers, groupActiveNumbers, 0, 2 };
        ui::KickListBox                                  timeDivList{ ui::KickListBox::withoutTitleBar, "Time Division", { "1/16", "1/8", "1/4", "1/2", "1" }, 2 };
        ui::KickToggleButton                             soloBtn{ "Solo", "Solo", "Solo" };
        ui::KickToggleButton                             instantAndTD{ "Instant", "Time Division", "Instant" };
        ui::KickButton                                   clearSoloBtn{ "Clear Solos" };
        std::vector<mt::MTISample*>                      samples;

        void displayPadState( const int id, const bool state, const juce::String& name, int muteGroup );
        void paint( juce::Graphics& ) override;

        void resized() override;
        void visibilityChanged() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickPadMuteTab )
    };

    class PadMuteSettings : public juce::Component
    {
    public:
        PadMuteSettings();

    private:
        ui::KickButton       apply{ "Apply" };
        ui::KickButton       cancel{ "Cancel" };
        ui::KickLabel        label1{ "Instant Pad Mute" };
        ui::KickLabel        label2{ "Play Pad Mute and Solo Events" };
        ui::KickLabel        label3{ "Record Pad Mute and Solo Events" };
        ui::KickToggleButton instantPadMute{ "Off", "On", "Off" };
        ui::KickToggleButton playPadMuteSoloEvents{ "Off", "On", "Off" };
        ui::KickToggleButton recordPadMuteSoloEvents{ "Off", "On", "Off" };

        void resized() override;
    };
}
