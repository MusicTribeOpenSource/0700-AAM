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

#include "keyboard_detector.h"
#include "keyboard_signals.h"

namespace contracts
{
    keyPressedDetector _keysDetector;
    // return global instance.
    keyPressedDetector& getKeypressDetector() { return _keysDetector; }

#if defined( _WIN32 ) || defined( _WIN64 )

    static const std::map<int, keyPressedDetector::cmdInfo> _keyMap = {
        { juce::KeyPress::upKey, { contracts::DeviceControlContracts::EN_KNOB_CENTER, false, CPktMgr::EN_PKT_TYPE_KNOBS, contracts::DeviceControlContracts::EN_KNOB_ROTARY } },
        { juce::KeyPress::downKey, { contracts::DeviceControlContracts::EN_KNOB_CENTER, false, CPktMgr::EN_PKT_TYPE_KNOBS, contracts::DeviceControlContracts::EN_KNOB_ROTARY } },
        { juce::KeyPress::rightKey, { contracts::DeviceControlContracts::EN_PLUS, false } },
        { juce::KeyPress::leftKey, { contracts::DeviceControlContracts::EN_MINUS, false } },
        //{ juce::KeyPress::returnKey, { contracts::DeviceControlContracts::EN_KNOB_ENTER, false } },//return key is there for 2 times

        { 'T', { contracts::DeviceControlContracts::EN_MAINMODE, false } },
        { 'Y', { contracts::DeviceControlContracts::EN_BROWSE, false } },
        { 'U', { contracts::DeviceControlContracts::EN_TRACK_MIX, false } },
        { 'I', { contracts::DeviceControlContracts::EN_TRACK_MUTE, false } },
        { 'O', { contracts::DeviceControlContracts::EN_NEXT_SEQ, false } },
        { '5', { contracts::DeviceControlContracts::EN_GRID, false } },
        { '6', { contracts::DeviceControlContracts::EN_SAVE, false } },
        { '7', { contracts::DeviceControlContracts::EN_PAD_MIX, false } },
        { '8', { contracts::DeviceControlContracts::EN_PAD_MUTE, false } },
        { '9', { contracts::DeviceControlContracts::EN_FX, false } },

        { 'Z', { contracts::DeviceControlContracts::EN_PAD1, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'X', { contracts::DeviceControlContracts::EN_PAD2, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'C', { contracts::DeviceControlContracts::EN_PAD3, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'V', { contracts::DeviceControlContracts::EN_PAD4, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'A', { contracts::DeviceControlContracts::EN_PAD5, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'S', { contracts::DeviceControlContracts::EN_PAD6, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'D', { contracts::DeviceControlContracts::EN_PAD7, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'F', { contracts::DeviceControlContracts::EN_PAD8, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'Q', { contracts::DeviceControlContracts::EN_PAD9, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'W', { contracts::DeviceControlContracts::EN_PAD10, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'E', { contracts::DeviceControlContracts::EN_PAD11, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { 'R', { contracts::DeviceControlContracts::EN_PAD12, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { '1', { contracts::DeviceControlContracts::EN_PAD13, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { '2', { contracts::DeviceControlContracts::EN_PAD14, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { '3', { contracts::DeviceControlContracts::EN_PAD15, true, CPktMgr::EN_PKT_TYPE_PAD } },
        { '4', { contracts::DeviceControlContracts::EN_PAD16, true, CPktMgr::EN_PKT_TYPE_PAD } },

        { 'G', { contracts::DeviceControlContracts::EN_STEP_SEQ, false } },
        { 'H', { contracts::DeviceControlContracts::EN_TC, false } },
        { 'J', { contracts::DeviceControlContracts::EN_SAMPLER, false } },
        { 'K', { contracts::DeviceControlContracts::EN_SAMPLE_EDIT, false } },
        { 'L', { contracts::DeviceControlContracts::EN_PROG_EDIT, false } },

        { 'B', { contracts::DeviceControlContracts::EN_AUTOMATION, false } },
        { 'N', { contracts::DeviceControlContracts::EN_ON_OFF, false } },
        { 'M', { contracts::DeviceControlContracts::EN_LOOPER, false } },
        { ',', { contracts::DeviceControlContracts::EN_LINK_EDIT, false } },
        { '.', { contracts::DeviceControlContracts::EN_MIDI_CONTROL, false } },

        { juce::KeyPress::F1Key, { contracts::DeviceControlContracts::EN_BANK_A, false } }, // F1
        { juce::KeyPress::F2Key, { contracts::DeviceControlContracts::EN_BANK_B, false } }, // F2
        { juce::KeyPress::F3Key, { contracts::DeviceControlContracts::EN_BANK_C, false } }, // F3
        { juce::KeyPress::F4Key, { contracts::DeviceControlContracts::EN_BANK_D, false } }, // F4
        //{ juce::KeyPress::F5Key, {contracts::DeviceControlContracts::EN_BANK_E, false } }, // F5
        { juce::KeyPress::F6Key, { contracts::DeviceControlContracts::EN_BANK_F, false } }, // F6
        { juce::KeyPress::F7Key, { contracts::DeviceControlContracts::EN_BANK_G, false } }, // F7
        { juce::KeyPress::F8Key, { contracts::DeviceControlContracts::EN_BANK_H, false } }, // F8

        { juce::KeyPress::F9Key, { contracts::DeviceControlContracts::EN_RECORD, false } },
        { juce::KeyPress::F10Key, { contracts::DeviceControlContracts::EN_OVERDUB, false } },
        { juce::KeyPress::F11Key, { contracts::DeviceControlContracts::EN_STOP, false } },
        { juce::KeyPress::spaceKey, { contracts::DeviceControlContracts::EN_PLAY, false } },
        { juce::KeyPress::F12Key, { contracts::DeviceControlContracts::EN_PLAYSTART, false } },
        { juce::KeyPress::playKey, { contracts::DeviceControlContracts::EN_PLAYSTART, false } },
        { juce::KeyPress::stopKey, { contracts::DeviceControlContracts::EN_STOP, false } },

        { juce::KeyPress::escapeKey, { contracts::DeviceControlContracts::EN_MENU, false } },
        { juce::KeyPress::insertKey, { contracts::DeviceControlContracts::EN_PREF, false } },
        { juce::KeyPress::deleteKey, { contracts::DeviceControlContracts::EN_ERASE, false } },

        { juce::KeyPress::tabKey, { contracts::DeviceControlContracts::EN_LINK, false } },
        { juce::KeyPress::returnKey, { contracts::DeviceControlContracts::EN_KNOB_ENTER, false } },

        { juce::KeyPress::pageUpKey, { contracts::DeviceControlContracts::EN_BACK, false } },
        { juce::KeyPress::pageDownKey, { contracts::DeviceControlContracts::EN_COPY, false } },

        { '0', { contracts::DeviceControlContracts::EN_FULL_LEVEL, false } },
        { '-', { contracts::DeviceControlContracts::EN_NOTE_REPEAT, false } },
        { '=', { contracts::DeviceControlContracts::EN_16LEVELS, false } },
        { 'P', { contracts::DeviceControlContracts::EN_HALF_LEVEL, false } },
        { '[', { contracts::DeviceControlContracts::EN_LATCH, false } },
        { ']', { contracts::DeviceControlContracts::EN_NOTES, false } }
    };

    void keyPressedDetector::detectForKeyCode( int KeyCode, const cmdInfo& Info )
    {
        // Compose message header ...
        CPktMgr::HWMsg_t hwMsg;
        hwMsg.pktType = Info.pktType;
        hwMsg.pktCmd  = Info.pktCmd;

        auto isKeyDown = juce::KeyPress::isKeyCurrentlyDown( KeyCode );

        // Check for Key Pressed...
        if( isKeyDown && !_keys.contains( KeyCode ) )
        {
            DBG( "Key Pressed: " << juce::KeyPress( KeyCode ).getTextDescription() );
            _keys.addIfNotAlreadyThere( KeyCode );

            if( isValidCmd( Info ) )
            {
                // Command payload...
                hwMsg.data.clear();
                hwMsg.data.push_back( (juce::int8)Info.id );

                if( CPktMgr::EN_PKT_TYPE_KNOBS == Info.pktType && contracts::DeviceControlContracts::EN_KNOB_ROTARY == Info.pktCmd )
                {
                    hwMsg.data.push_back( ( juce::int8 )( ( juce::KeyPress::upKey == KeyCode ) ? contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE : contracts::DeviceControlContracts::EN_KNOB_ANTICLOCKWISE ) );
                    hwMsg.data.push_back( ( juce::int8 )( 1 ) ); // Increment
                    hwMsg.data.push_back( ( juce::int8 )( 0 ) );
                }
                else
                {
                    hwMsg.data.push_back( ( juce::int8 )( contracts::DeviceControlContracts::BTN_PRESSED ) ); // Key Released/Pressed (in case of pad buttons)
                    hwMsg.data.push_back( ( juce::int8 )( 80 ) ); // Random Velocity
                    hwMsg.data.push_back( ( juce::int8 )( 100 ) ); // Aftertouch
                }
                contracts::KeyboardSignals::KeyboardEventTriggered( hwMsg );
            }
        }

        // Check for Key Released...
        if( !isKeyDown && _keys.contains( KeyCode ) )
        {
            DBG( "Key Released: " << juce::KeyPress( KeyCode ).getTextDescription() );
            _keys.removeAllInstancesOf( KeyCode );

            if( Info.releaseEventRequired )
            {
                // Command payload...
                hwMsg.data.clear();
                hwMsg.data.push_back( (juce::int8)Info.id );
                hwMsg.data.push_back( (juce::int8)contracts::DeviceControlContracts::BTN_RELEASED ); // Key Released/Pressed (in case of pad buttons)
                hwMsg.data.push_back( 80 ); // Random Velocity
                hwMsg.data.push_back( 100 ); // Aftertouch
                contracts::KeyboardSignals::KeyboardEventTriggered( hwMsg );
            }
        }
    }

    bool keyPressedDetector::keyPressed( const juce::KeyPress& key, juce::Component* originatingComponent )
    {
        return true;
    }

    bool keyPressedDetector::keyStateChanged( bool isKeyDown, juce::Component* originatingComponent )
    {
        auto mod = juce::ModifierKeys::getCurrentModifiers();
        if( shiftBtnState != mod.isShiftDown() )
        {
            shiftBtnState = mod.isShiftDown();
            CPktMgr::HWMsg_t hwMsg;
            hwMsg.data.push_back( (juce::int8)contracts::DeviceControlContracts::EN_SHIFT );
            hwMsg.data.push_back( shiftBtnState ? (juce::int8)contracts::DeviceControlContracts::BTN_PRESSED : (juce::int8)contracts::DeviceControlContracts::BTN_RELEASED );
            contracts::KeyboardSignals::KeyboardEventTriggered( hwMsg );
        }
        for( const auto& [keyCode, info] : _keyMap )
            detectForKeyCode( keyCode, info );
        return true;
    }
#endif // defined( _WIN32 ) || defined( _WIN64 )
} // namespace
