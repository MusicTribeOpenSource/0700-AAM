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

#ifdef PROCESSING_OBJECTS_H_INCLUDE
/* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
    #error "Incorrect use of JUCE cpp file"
#endif

// Module include files...
#include "audio_program.h"

// Module source files...
#include "synth/drum_voice.cpp"
#include "synth/iSynthObject.cpp"
#include "synth/iSynthVoice.cpp"

#include "synth/audioPreview.cpp"
#include "synth/metronome.cpp"
#include "synth/audioProgram.cpp"
#include "synth/clipProgram.cpp"
#include "synth/DrumProgram.cpp"
#include "synth/KeygroupProgram.cpp"

#include "source/mt_audio_iprogram.cpp"
#include "source/mt_audio_common_program.cpp"

#include "source/mt_audio_drum_program.cpp"
#include "source/mt_audio_clip_program.cpp"
#include "source/mt_audio_keygroup_program.cpp"
#include "source/mt_audio_midi_program.cpp"
#include "source/mt_audio_cv_program.cpp"
#include "source/mt_audio_audio_program.cpp"
#include "source/mt_audio_clap_program.cpp"
