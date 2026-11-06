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

  dependencies:       kick_ui_header, kick_ui_selectors, audio_program, kick_ui_header, project

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/


#pragma once

// Dependent module includes.
#include <kick_ui_header/kick_ui_header.h>
#include <kick_ui_selectors/kick_ui_selectors.h>
#include <audio_program/audio_program.h>
#include <project/project.h>

// Module include files.
//
// Mixer common.
// Mixer files...

namespace kick_ui
{

    struct TextValue : public juce::Component
    {
        ui::KickLabel title;
        ui::KickLabel value;

        TextValue( const juce::String& T, const juce::String& V = "" );
        void resized() override;
        void paint( juce::Graphics& g ) override;
    };

    struct SettingsInterface : public ui::KickGrid
    {
        juce::ValueTree state;

        // Virtual functions
        virtual void refresh()                                   = 0;
        virtual void setupHeader( kick_ui::KickModeHeader& Hdr ) = 0;
        virtual void handleCommand( CPktMgr::HWMsg_t& ) {}

        explicit SettingsInterface( int NRows, int NCols, bool RowTitlesRequired = false, juce::ValueTree VT = {} )
            : ui::KickGrid( NRows, NCols, RowTitlesRequired ), state( VT ){};
    };

    class Settings : public kick_ui::IScreen
        , public juce::Component
        , private juce::Timer
    {
    public:
        Settings();
        ~Settings() = default;

        bool canClose() override { return true; }
        bool requiresStatusBar() override { return false; }
        bool requiresHandleCommand() override { return true; }

        void handleCommand( CPktMgr::HWMsg_t& ) override;

    private:
        kick_ui::KickModeHeader header{ "Preferences", "Icon_SettingsScreen" };
        juce::Rectangle<int>    bCont;

        std::array<juce::String, 8>                     selectorStrings{ "Info", "Project Defaults", "Hardware", "MIDI", "CV", "Ethernet", "WiFi", "Camera" };
        std::array<ui::KickToggleButton, 8>             selectors;
        std::vector<std::unique_ptr<SettingsInterface>> pViews;
        SettingsInterface*                              pView{ nullptr };

        void showView( int Index );
        void resized() override;
        void visibilityChanged() override;
        void timerCallback() override;
    };
}
