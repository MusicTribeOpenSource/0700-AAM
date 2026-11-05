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
constexpr float padButtonSpaceX = 0.0117f;
constexpr float padButtonSpaceY = 0.009f;
constexpr float padButtonWidth  = 0.304f + padButtonSpaceX;
constexpr float padButtonHeight = 0.087f + padButtonSpaceY;

constexpr int numRows = 6;
constexpr int numCols = 3;

namespace kick_ui
{
    enum EMPTY_PAD_STATE
    {
        OFF,
        DIM
    };

    enum PAD_OFF_STATE
    {
        PAD_OFF_DIM,
        PAD_OFF_OFF
    };

    enum PAD_ON_STATE
    {
        PAD_ON_VELOCITY,
        PAD_ON_FULL,
        PAD_ON_OFF
    };

    class KickColorEditorLeftPanel : public juce::Button::Listener
        , public juce::TabbedComponent
    {
    public:
        KickColorEditorLeftPanel();
        ~KickColorEditorLeftPanel() {}

        ui::KickToggleButton emptyPadMode{ "EMPTY PAD COLOR MODE", "DIM", "OFF" };
        ui::KickToggleButton padType{ "Single Pad", "All Pads", "Single Pad" };

        ui::KickMultiSelectTabBarButton padOff{ 2, getTabbedButtonBar(), "Dim" };
        ui::KickMultiSelectTabBarButton padOn{ 3, getTabbedButtonBar(), "Velocity" };

        ui::KickLabel emptyPadModeLabel{ "EMPTY PAD COLOR MODE" };
        ui::KickLabel padTypeLabel{ "PAD TYPE" };
        ui::KickLabel padOffLabel{ "Pad Off" };
        ui::KickLabel padOnLabel{ "Pad On" };
        ui::KickLabel assignedPadLabel{ "ASSIGNED PAD" };

        ui::KickColorButton                             padButtons[18];
        ui::KickColorButton*                            selectedColor;
        contracts::DeviceControlContracts::PadColourIds selectedColourId = contracts::DeviceControlContracts::PadColourIds::PadColourTrans;

    private:
        float x;
        float y;

        void resized() override;
        void paint( juce::Graphics& g ) override;
        void buttonClicked( juce::Button* ) override;
    };

    class KickColorEditorRightPanel : public juce::Component
    {
    public:
        KickColorEditorRightPanel();
        ~KickColorEditorRightPanel() {}

        std::unique_ptr<ui::KickPadButtonGroupComponent> pads;
        ui::KickToggleButton                             defaultBtn{ "Make Default", "Make Default", "Make Default" };

    private:
        void resized() override;
        void paint( juce::Graphics& g ) override;
        void visibilityChanged() override;
    };

    class KickPadColorMode
        : public kick_ui::IScreen
        , public juce::Component
        , public juce::Button::Listener
    {
    public:
        KickPadColorMode();

        bool requiresHandleCommand() { return true; }
        void handleCommand( CPktMgr::HWMsg_t& ) override;
        void refreshLEDs();

    private:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };

        const float         thickness{ 5.0f };
        ui::KickImageButton leftNav{ "Left Nav", "Icon_minus" };
        ui::KickImageButton rightNav{ "Right Nav", "Icon_plus" };
        ui::KickLabel       bankLabel{ "" };
        ui::KickLabel       warningLabel{ "Please Select Program" };
        ui::KickColorButton transparent{ ui::Theme::padButtonColour( contracts::DeviceControlContracts::PadColourIds::PadColourTrans ) };

        KickModeHeader           header{ "Color Editor", "Icon_ColourEdit" };
        HeaderBarProgramSelector hdrComp;

        KickColorEditorLeftPanel    left;
        KickColorEditorRightPanel   right;
        std::vector<mt::MTISample*> samples;

        int bankOffset{ 0 };

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void refresh();
        void sendColourModeToHardware( int noteOff, int noteOn );
        void setPadOffModeSelection( int selection );
        void setPadOnModeSelection( int selection );
        int  getPadOnColourMode( mt::PadColorMode );
        int  getPadOffColourMode( mt::PadColorMode );
        bool getEmptyColourMode( mt::PadColorMode );

        void buttonClicked( juce::Button* ) override{};
        void buttonClicked( int Id );
        void setBank( int, juce::String );
        void visibilityChanged() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickPadColorMode )
    };

}
