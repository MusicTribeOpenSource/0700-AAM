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

namespace contracts
{
    // The object under this key is the JSON line kick-gesture writes, unchanged (docs/gesture-events.md).
    const char* const GESTURE_IPC_KEY_EVENT( "event" );

    // Fields inside that object.
    const char* const GESTURE_IPC_KEY_TYPE( "type" );
    const char* const GESTURE_IPC_KEY_VALUE( "value" );
    const char* const GESTURE_IPC_KEY_HAND( "hand" );
    const char* const GESTURE_IPC_KEY_CONFIDENCE( "confidence" );
    const char* const GESTURE_IPC_KEY_T_MS( "t_ms" );

    // Status lines.
    const char* const GESTURE_IPC_KEY_STATE( "state" );
    const char* const GESTURE_IPC_KEY_REASON( "reason" );
    const char* const GESTURE_IPC_KEY_RATE_HZ( "rate_hz" );

    // Connection id the service registers with. Any value works while client validation is off; a fixed one keeps logs readable.
    const int GESTURE_IPC_CONNECTION_ID( 500 );
    const char* const GESTURE_IPC_CONNECTION_NAME( "kick-gesture" );
    const char* const GESTURE_IPC_DEFAULT_ADDRESS( "127.0.0.1" );
    const int         GESTURE_IPC_DEFAULT_PORT( 50501 );
}
