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

#include "gesture_decode.h"

#include "contracts/gesture/gesture_ipc_keys.h"
#include "contracts/gesture/gesture_ipc_message_types.h"

#include <juce_core/juce_core.h>

namespace kick_gesture
{
    bool decodeGestureMessage( const juce::var& root, DecodedGestureMessage& out )
    {
        out = DecodedGestureMessage{};
        if( !root.isObject() )
            return false;

        const int type = static_cast<int>( root.getProperty( "message_type", 0 ) );
        const juce::var ev = root.getProperty( contracts::GESTURE_IPC_KEY_EVENT, juce::var() );
        if( !ev.isObject() )
            return false;

        if( type == static_cast<int>( contracts::GestureIPCMessageType::GESTURE_IPC_MESSAGE_TYPE_EVENT ) )
        {
            const juce::String name = ev.getProperty( contracts::GESTURE_IPC_KEY_TYPE, juce::var() ).toString();
            contracts::GestureKind kind;
            if( !contracts::parseGestureKind( name.toRawUTF8(), kind ) )
                return false;

            out.event.kind = kind;
            if( ev.hasProperty( contracts::GESTURE_IPC_KEY_VALUE ) )
            {
                out.event.hasValue = true;
                out.event.value    = static_cast<float>( static_cast<double>( ev.getProperty( contracts::GESTURE_IPC_KEY_VALUE, 0.0 ) ) );
            }
            out.event.hand       = contracts::parseGestureHand( ev.getProperty( contracts::GESTURE_IPC_KEY_HAND, juce::var() ).toString().toRawUTF8() );
            out.event.confidence = static_cast<float>( static_cast<double>( ev.getProperty( contracts::GESTURE_IPC_KEY_CONFIDENCE, 0.0 ) ) );
            out.event.tMs        = static_cast<uint64_t>( static_cast<juce::int64>( ev.getProperty( contracts::GESTURE_IPC_KEY_T_MS, 0 ) ) );
            out.kind             = DecodedGestureMessage::Kind::Event;
            return true;
        }

        if( type == static_cast<int>( contracts::GestureIPCMessageType::GESTURE_IPC_MESSAGE_TYPE_STATUS ) )
        {
            out.status.connected = true;
            out.status.state     = ev.getProperty( contracts::GESTURE_IPC_KEY_STATE, juce::var() ).toString().toStdString();
            out.status.reason    = ev.getProperty( contracts::GESTURE_IPC_KEY_REASON, juce::var() ).toString().toStdString();
            out.status.rateHz    = static_cast<double>( ev.getProperty( contracts::GESTURE_IPC_KEY_RATE_HZ, 0.0 ) );
            out.kind             = DecodedGestureMessage::Kind::Status;
            return true;
        }
        return false;
    }
}
