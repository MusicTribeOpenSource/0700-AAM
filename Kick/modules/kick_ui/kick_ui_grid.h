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

/*******************************************************************************
  The block below describes the properties of this module, and is read by
  the Projucer to automatically generate project code that uses it.
  For details about the syntax and how to create or use a module, see the
  JUCE Module Format.txt file.

  BEGIN_JUCE_MODULE_DECLARATION

   ID:                 kick_ui
   vendor:             MusicTribe
   version:            0.1.0
   name:               MusicTribe ui grid
   description:        Classes aand interfaces for DAW
   website:            http://www.musictribe.com/
   license:            AGPLv3

   dependencies:       kick_ui_components, kick_ui_header, interface

  END_JUCE_MODULE_DECLARATION

 *******************************************************************************/

#pragma once
#ifndef KICK_UI_GRID_H_INCLUDE
    #define KICK_UI_GRID_H_INCLUDE

    #include <kick_ui_components/kick_ui_components.h>
    #include <kick_ui_header/kick_ui_header.h>
    #include <interface/interface.h>

    // grid common
    #include "audio_components/gridview/kick_GridData.h"

    // grid view
    #include "audio_components/gridview/kick_Locate.h"
    #include "audio_components/gridview/kick_GridEditOptions.h"
    #include "audio_components/gridview/kick_TimeSigSetting.h"
    #include "audio_components/gridview/kick_ParameterView.h"
    #include "audio_components/gridview/kick_AppGridHeader.h"
    #include "audio_components/gridview/kick_GridComponent.h"
    #include "audio_components/gridview/kick_AppGrid.h"

    // Grid tools.
    #include "audio_components/gridview/kick_IGridTool.h"
    #include "audio_components/gridview/kick_GridEraseTool.h"
    #include "audio_components/gridview/kick_GridSelectTool.h"
    #include "audio_components/gridview/kick_GridPencilTool.h"
    #include "audio_components/gridview/kick_GridCopyTool.h"
    #include "audio_components/gridview/kick_GridMagnifyTool.h"

    // Step sequencer grid
    #include "audio_components/step_sequencer/kick_StepSequencerDataType.h"
    #include "audio_components/step_sequencer/kick_StepSequencerControl.h"
    #include "audio_components/step_sequencer/kick_StepPresets.h"
    #include "audio_components/step_sequencer/kick_DrumGrid.h"
    #include "audio_components/step_sequencer/kick_StepSequencer.h"

#endif // KICK_UI_GRID_H_INCLUDE
