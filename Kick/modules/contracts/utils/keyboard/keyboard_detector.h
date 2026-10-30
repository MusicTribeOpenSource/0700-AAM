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

#include <juce_gui_basics/juce_gui_basics.h>
#include "contracts/dcl/dcl_contracts.h"
#include "PktMgr/PktMgr.hpp"



// The keyboard front panel (PC keys press the pads and buttons) is always built on Windows. On other platforms it is
// opt-in with cmake -DKICK_DESKTOP_KEYBOARD=ON, so a keyboard plugged into the device itself cannot press its buttons.
#if defined( _WIN32 ) || defined( _WIN64 ) || defined( KICK_DESKTOP_KEYBOARD )
    #define KICK_KEYBOARD_PANEL 1
#endif

namespace contracts
{
    

#if defined( KICK_KEYBOARD_PANEL )
    struct keyPressedDetector : public juce::KeyListener
    {
        juce::Array<int> _keys;

        struct cmdInfo
        {
            int                     id{ contracts::DeviceControlContracts::EN_INVALID_ID };
            bool                    releaseEventRequired{ false };
            CPktMgr::enumPacketType pktType{ CPktMgr::EN_PKT_TYPE_BUTTON };
            int                     pktCmd{ 0 };
        };

        bool isValidCmd( const cmdInfo& Cmd ) { return contracts::DeviceControlContracts::EN_INVALID_ID != Cmd.id; }
        void detectForKeyCode( int KeyCode, const cmdInfo& Info );

        bool              keyPressed( const juce::KeyPress& key, juce::Component* originatingComponent ) override;
        bool              keyStateChanged( bool isKeyDown, juce::Component* originatingComponent ) override;
        std::atomic<bool> shiftBtnState;

    };
#else // keyboard front panel not enabled in this build.
    struct keyPressedDetector : public juce::KeyListener
    {
        bool keyPressed( const juce::KeyPress& key, juce::Component* originatingComponent ) override { return false;}
        bool keyStateChanged( bool isKeyDown, juce::Component* originatingComponent ) override { return false; };
    };
#endif // defined( KICK_KEYBOARD_PANEL )
    keyPressedDetector& getKeypressDetector();
}
