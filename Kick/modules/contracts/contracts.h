/*
 * Copyright (c) 2024 MUSIC Tribe
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
#ifndef CONTRACTS_H_INCLUDE
    #define CONTRACTS_H_INCLUDE

#include "config/dcl_ipc_config.h"
#include "config/engine_ipc_config.h"
#include "config/gui_ipc_config.h"
#include "config/main_ipc_config.h"
#include "config/ipc_config_inteface.h"
#include "config/ipc_config_helpers.h"
#include "config/ipc_config_constants.h"
#include "dcl/dcl_ipc_interface.h"
#include "dcl/dcl_signals.h"
#include "dcl/dcl_contracts_helper.h"
#include "config/connection_id_store.h"
#include "utils/keyboard/keyboard_detector.h"
#include "utils/keyboard/keyboard_signals.h"
#include "utils/app/ipc_application.h"
#include "audioengine/audio_engine_contracts_helpers.h"
#include "audioengine/audio_engine_signals.h"
#include "logger/logger.h"
#include "main/system_ipc_helpers.h"
#include "main/system_signals.h"
#include "gesture/gesture_event.h"
#include "gesture/gesture_ipc_keys.h"
#include "gesture/gesture_ipc_message_types.h"
#include "gesture/gesture_signals.h"

#endif
