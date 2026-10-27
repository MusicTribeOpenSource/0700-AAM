/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.

 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 input_events
  vendor:             MUSIC Tribe, MY
  version:            0.1.0
  name:               MusicTribe Common definitions
  description:        Classes and interfaces containing common definitions
  website:
  license:            AGPLv3

  dependencies:       signal, PktMgr, serial_port

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

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

#pragma once

#ifndef INPUTEVENTS_H_INCLUDE
    #define INPUTEVENTS_H_INCLUDE

    // Dependent module includes.
    #include <3rdParty/signal/signal.hpp>
    #include <PktMgr/PktMgr.hpp>
    #include <serial_port/serial_port.h>

namespace Controls
{
    struct InputEvents
    {
        static void initialize();
        static void reconnect();
        static void shutdown();

        static void sendMessageToHardware( CPktMgr::HWMsg_t& f_HwMsg );
        static void messageReceived( CPktMgr::HWMsg_t& f_HwMsg );

        static sigslot::signal<CPktMgr::HWMsg_t&> dispatchHardwareEvent;
    };

}

#endif
