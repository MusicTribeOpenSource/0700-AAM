/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.

 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 common_defs
  vendor:             MUSIC Tribe, MY
  version:            0.1.0
  name:               MusicTribe Common definitions
  description:        Classes and interfaces containing common definitions
  website:
  license:            AGPLv3

  dependencies:       kick_ui_header, kick_ui_selectors, audio_program, kick_ui_header

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/


#pragma once

// Dependent module includes.
#include <kick_ui_header/kick_ui_header.h>
#include <kick_ui_selectors/kick_ui_selectors.h>
#include <audio_program/audio_program.h>

// Module include files.
//
// Mixer common.
#include "kick_ui/row_view/kick_IRowView.h"
// Mixer files...
#include "source/kick_master_route_mix.h"
#include "source/kick_program_mix.h"
#include "source/kick_sends_mix.h"
#include "source/kick_returns_mix.h"
#include "source/kick_submixes_mix.h"
#include "source/kick_track_midi_mix.h"
#include "source/kick_audio_track_mix.h"
#include "source/kick_output_track_mix.h"

namespace kick_ui
{
    class ChannelMixerHeaderComponent : public juce::Component
    {
    public:
        ChannelMixerHeaderComponent();
        ui::KickListBox type{ ui::KickListBox::BoxType::withTitleBar, "MIXER", {} };

    private:
        void resized() override;
    };

    class ChannelMixer : public kick_ui::IScreen
        , public juce::Component
        , private juce::Timer
    {
    public:
        ChannelMixer();
        ~ChannelMixer();

        bool canClose() override { return true; }
        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void knobCommand( int knob, int value );
        void handleCommand( CPktMgr::HWMsg_t& ) override;

    private:
        KickModeHeader                         header{ "Track Mixer", "Icon_Mixer" };
        ChannelMixerHeaderComponent            headerComp;
        std::unique_ptr<juce::Component>       pMaster;
        juce::Rectangle<int>                   bCont;
        std::vector<std::unique_ptr<IRowView>> pViews;
        IRowView*                              pView{ nullptr };
        int                                    index{ -1 };

        void showView( int Index );
        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;
        void timerCallback() override;
    };
}
