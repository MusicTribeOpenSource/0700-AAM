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

#include "../utils/contracts_marcros.h"

namespace contracts
{
    // Messages from the kick-gesture service to the app, over the app's IPC server (127.0.0.1:50501 by default).
    // 500 and 501 are far from the DCL types (100 to 102) on purpose.
    CONTRACTS_DEFINE_INT_ENUM_CLASS( GestureIPCMessageType,
                                     GESTURE_IPC_MESSAGE_TYPE_INVALID{ 0 },
                                     GESTURE_IPC_MESSAGE_TYPE_EVENT{ 500 }, // one gesture event, in the "event" object
                                     GESTURE_IPC_MESSAGE_TYPE_STATUS{ 501 } ); // one service status line, in the "event" object
}
