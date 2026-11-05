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
    enum class AudioGridToolType
    {
        GridPencilTool,
        GridSelectTool,
        GridEraseTool,
        GridCopyTool,
        GridDragTool,
        GridCutTool,
        GridMagnifyTool
    };

    class KeyGroupInterface
    {
    public:
        virtual void refresh()                          = 0;
        virtual void handleCommand( CPktMgr::HWMsg_t& ) = 0;
    };

    class DrumInterface
    {
    public:
        virtual void refresh()                          = 0;
        virtual void handleCommand( CPktMgr::HWMsg_t& ) = 0;
    };

    class ProgramEditHeaderComponent : public juce::Component
    {
    public:
        KickProgramSelectorWithDisplay programs;
        ui::KickImageButton            icon;

        ProgramEditHeaderComponent();
        void setHeaderSubComponent( juce::Component* Comp );
        void setIcon( const juce::String& icon );

    private:
        void resized();
        void visibilityChanged() override;

    private:
        juce::Component* progHeader{ nullptr };
    };

    class AudioHeaderTools : public juce::Component
    {
    public:
        AudioHeaderTools();

        SIGNALS : sigslot::signal<AudioGridToolType> toolSelectionChanged;

        ui::KickNumber      bars{ "BARS", BAR::MIN, BAR::MAX, BAR::MIN, BAR::STEP, true };
        ui::KickImageButton selectAll{ "Select", "Select_Normal", "Select_Active" };
        ui::KickImageButton selectSample{ "Select", "Select_Normal", "Select_Active" };
        ui::KickImageButton pencil{ "Pencil", "Pencil_Normal", "Pencil_Active" };
        ui::KickImageButton erase{ "Erase", "Erase_Normal", "Erase_Active" };
        ui::KickImageButton cut{ "Cut", "Cut_Normal", "Cut_Active" };
        ui::KickImageButton copy{ "Copy", "Copy Button", "Copy Button_Active" };
        ui::KickImageButton paste{ "Paste Button", "Paste Button", "Paste Button_Active", "", "Paste Disabled" };
        ui::KickImageButton settings{ "Settings", "Settings_Header" };
        ui::KickImageButton magnify{ "Magnify" };
        void                resized() override;
        void                paint( juce::Graphics& );
    };

    class ProgramEdit : public kick_ui::IScreen
        , public juce::Component
    {
    public:
        ProgramEdit();

        void loadProgram( mt::MTProgramType Type, int Index );

        bool canClose() { return true; }
        bool requiresControlBar() override { return false; }
        bool requiresHandleCommand() override { return true; }
        void handleCommand( CPktMgr::HWMsg_t& ) override;

    private:
        kick_ui::KickModeHeader header{ "Program Edit", "Icon_pgmEdit" };
        kick_ui::KickModeHeader audioheader{ "Grid View", "Icon_Grid" };
        ui::KickListBox         tracks{ ui::KickListBox::withoutTitleBar, "TRACK", {} };

        ProgramEditHeaderComponent hdrComp;
        AudioHeaderTools           audioHdrTools;

        std::vector<std::unique_ptr<ProgEditInterface>> programs;
        ProgEditInterface*                              selectedProgram{ nullptr };
        juce::Component*                                comp{ nullptr };
        ui::KickLabel                                   infoLabel{ "" };

        void          paint( juce::Graphics& g ) override;
        void          resized() override;
        void          visibilityChanged() override;
        ui::KickLabel selectedPadLabel{ "" };
    };
}
