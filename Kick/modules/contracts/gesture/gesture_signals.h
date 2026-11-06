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

#include "3rdParty/signal/signal.hpp"
#include "gesture_event.h"

namespace contracts
{
    // Emitted on the message thread.
    class GestureSignals
    {
    public:
        // A gesture event from kick-gesture. Fired for every event, before any mapping, gating or enable switch.
        static sigslot::signal<const GestureEvent&> gestureReceived;
        // The service connected, disconnected, or sent a status line.
        static sigslot::signal<const GestureServiceStatus&> serviceStatusChanged;
        // The camera service state changed. The on-screen camera indicator follows this.
        static sigslot::signal<const CameraStatus&> cameraStatusChanged;
    };
}
