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
#define PADS_FOR_BANK 16
#define ROW_HEIGHT    107

    struct PadMixerData
    {
        juce::String padName{ "" };
        juce::String sampleName{ "" };
        juce::String ioName{ "" };
        int          KeyNote{ 0 };
        double       level{ 0 };
        double       pan{ 0 };
        bool         solo{ false };
        bool         mute{ false };

        void clear()
        {
            padName    = "";
            sampleName = "";
            ioName     = "";
            KeyNote    = 0;
            level      = 0.0;
            pan        = 0.0;
            solo       = false;
            mute       = false;
        }
    };

    class PadMixerDisplay : public juce::Component
    {
    public:
        std::function<void( int )> onDataChanged     = []( int ) {};
        std::function<void()>      onSoloMuteChanged = []() {};

        PadMixerDisplay();
        ~PadMixerDisplay() = default;

        void                 setData( mt::MTIProgram*, PadMixerData* );
        int                  getKeyNote() { return pData ? pData->KeyNote : 0; };
        void                 linkRotated( int ID, int value );
        juce::Rectangle<int> getElementBounds( int state );
        void                 setMute( bool, bool );

    private:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };

        ui::KickImageButton    solo{ "SOLO", "Solo_normal", "Solo_active" };
        ui::KickImageButton    mute{ "MUTE", "Mute_normal", "Mute_active" };
        ui::KickSlider         levelSlider{ "LEVEL", juce::Slider::LinearBar, juce::Slider::TextBoxBelow };
        ui::KickRotary         panSlider{ "PAN", juce::Slider::TextBoxLeft, true };
        ui::Kick2LineNameLabel nameLabel{ "" };
        ui::KickLabel          padName{ "" };

        mt::MTIProgram* pProg{ nullptr };
        PadMixerData*   pData{ nullptr };

        //  Inserts insertsProg;
        bool mutedDueToNoteSolo{ false };
        int  keyNote{ -1 };
        void resized() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( PadMixerDisplay )
    };

    class PadMixerContent : public juce::Component
    {
    public:
        PadMixerContent();
        ~PadMixerContent() = default;

        void setPadEnable( bool Enable );
        void programChanged();
        void knobCommand( int knob, int value );
        void handleCommand( CPktMgr::HWMsg_t& );
        void meterUpdate();

    private:
        void resized() override;
        void visibilityChanged() override;
        void refreshPadData( int Pad );
        void refreshPadData();
        void refresh();
        void setDisplay( bool isProgramAvailable );
        void updateView();
        void refreshSoloMute();

    private:
        mt::MTAudioInterface&                            ae{ mt::getAudioInterface() };
        std::unique_ptr<ui::KickPadButtonGroupComponent> pads;
        std::unique_ptr<ui::KickTableView>               tableView;
        ui::KickTableViewProvider                        provider;
        bool                                             isPadEnabled{ true };
        int                                              bankIndex{ 0 };
        int                                              selectedPad{ 0 };
        std::vector<mt::MTISample*>                      sampleList;
        juce::Array<int>                                 rows;
        std::array<PadMixerData, _NUM_PADS>              padData;
        mt::MTIProgram*                                  pProg{ nullptr };
        ui::KickList                                     samples{ "Samples" };
        ui::KickLabel                                    infoLabel{ "PAD MIXER IS NOT APPLICABLE" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( PadMixerContent )
    };

    class PadMixer : public kick_ui::IScreen
        , public juce::Component
        , public juce::Timer
    {
    public:
        PadMixer();
        ~PadMixer() = default;

        bool canClose() override { return true; }
        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void meterUpdate();
        void handleCommand( CPktMgr::HWMsg_t& ) override;

    private:
        ui::KickLabel       programLabel{ "PROGRAM" };
        ui::KickLabel       trackLabel{ "TRACK" };
        ui::KickImageButton padEnabler{ "Icon", "PadMix_Enable", "PadMix_Enable_Active" };
        ui::KickButton      sampleAssign{ "Sample Assign" };

        KickModeHeader                   header{ "Pad Mixer", "Icon_PadMix" };
        HeaderBarProgramAndTrackSelector padHeaderComp;
        PadMixerContent                  padContent;

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged();
        void timerCallback() override;
    };
}
