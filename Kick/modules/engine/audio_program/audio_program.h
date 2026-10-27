/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.

 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 common
  vendor:             MUSIC Tribe, MY
  version:            0.1.0
  name:               MusicTribe Common definitions
  description:        Classes and interfaces containing common definitions
  website:
  license:            AGPLv3

  dependencies:       audio_file_manager

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

/*
 * Copyright (c) 2022 MUSIC Tribe
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
#ifndef AUDIO_PROGRAM_INCLUDE
    #define AUDIO_PROGRAM_INCLUDE
    // Dependent module includes...
    #include <audio_file_manager/audio_file_manager.h>

    // Module include files...
    #include "source/mt_audio_iprogram.h"

    #include "synth/audioPreview.h"
    #include "synth/metronome.h"
    #include "synth/audioProgram.h"
    #include "synth/clipProgram.h"
    #include "synth/drumProgram.h"
    #include "synth/keygroupProgram.h"

    #include "source/mt_audio_common_program.h"
    #include "source/mt_audio_drum_program.h"
    #include "source/mt_audio_clip_program.h"
    #include "source/mt_audio_keygroup_program.h"
    #include "source/mt_audio_midi_program.h"
    #include "source/mt_audio_cv_program.h"
    #include "source/mt_audio_audio_program.h"
    #include "source/mt_audio_clap_program.h"

#endif // Header include guard
