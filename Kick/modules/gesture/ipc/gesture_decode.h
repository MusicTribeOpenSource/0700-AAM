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

#include "contracts/gesture/gesture_event.h"

namespace juce
{
    class var;
}

namespace kick_gesture
{
    // What one IPC message from kick-gesture holds, after decoding. Plain data, so it can be built on the IPC thread
    // and handed to the message thread.
    struct DecodedGestureMessage
    {
        enum class Kind
        {
            None,
            Event,
            Status
        };

        Kind                            kind{ Kind::None };
        contracts::GestureEvent         event;
        contracts::GestureServiceStatus status;
    };

    // Reads the parsed JSON of an IPC message: message_type 500 (event) or 501 (status), with the service's own JSON
    // line under the key "event" (docs/gesture-events.md). False for anything else. A reader of this function never
    // sees an exception: a payload that is not an object, a missing key or an unknown gesture name is just false.
    bool decodeGestureMessage( const juce::var& root, DecodedGestureMessage& out );
}
