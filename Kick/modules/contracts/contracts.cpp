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

#ifdef CONTRACTS_H_INCLUDE
/* When you add this cpp file to your project, you mustn't include it in a file where you've
     already included any other headers - just put it inside a file on its own, possibly with your config
     flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
     header files that the compiler may be using.
  */
    #error "Incorrect use of JUCE cpp file"
#endif

#include "contracts.h"

#include "dcl/dcl_ipc_interface.cpp"
#include "dcl/dcl_signals.cpp"
#include "dcl/dcl_contracts_helper.cpp"
#include "config/connection_id_store.cpp"
#include "utils/keyboard/keyboard_detector.cpp"
#include "utils/keyboard/keyboard_signals.cpp"
#include "utils/app/ipc_application.cpp"
#include "audioengine/audio_engine_contracts_helpers.cpp"
#include "audioengine/audio_engine_signals.cpp"
#include "logger/logger.cpp"
#include "main/system_ipc_helpers.cpp"
#include "main/system_signals.cpp"
#include "gesture/gesture_signals.cpp"

