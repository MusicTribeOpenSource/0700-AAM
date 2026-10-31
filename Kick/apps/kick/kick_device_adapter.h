
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
// this is a adapter class for the monolith kick application.

#include "input_events/input_events.h"
#include "contracts/dcl/dcl_signals.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "PktMgr/PktMgr.hpp"
#include "contracts/utils/keyboard/keyboard_signals.h"

class KickDeviceAdapter
{
public:
    KickDeviceAdapter()
    {
        Controls::InputEvents::initialize();
        auto handler = [&]( CPktMgr::HWMsg_t& f_HwMsg ) {
            // Keep DclContractsHelper::isShiftPressed() in sync with the SHIFT button.
            if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && contracts::DclContractsHelper::getButtonCode( f_HwMsg ) == contracts::DeviceControlContracts::EN_SHIFT )
                contracts::shiftBtnState = contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED;
            contracts::DclSignals::hardwareInputTriggered( f_HwMsg );
        };

        Controls::InputEvents::dispatchHardwareEvent.connect( handler );

        contracts::KeyboardSignals::KeyboardEventTriggered.connect( handler );

        contracts::DclSignals::sendCommandToDcl.connect( [&]( std::shared_ptr<ipc::MTIPCJson> message ) {
            this->createAndPublishToDevice( message );
        } );

        contracts::DclSignals::createAndSendCommandToDcl.connect( [&]( const CPktMgr::HWMsg_t& message ) {
            this->sendToDevice( message );
        } );
    }

    void createAndPublishToDevice( std::shared_ptr<ipc::MTIPCJson> message )
    {
        if( !message ) return;
        CPktMgr::HWMsg_t msg;
        contracts::DclContractsHelper::ipcMessageToHWMessage( message, msg );
        /*juce::String log;
        log += "type " + juce::String( msg.pktType );
        log += " cmd " + juce::String( msg.pktCmd );
        log += " dir " + juce::String( msg.pktDir );
        log += " data ";
        for( auto i : msg.data )
        {
            log += juce::String( i ) + " ";
        }
        juce::Logger::writeToLog( log );
        juce::Logger::writeToLog( "" );*/
        Controls::InputEvents::sendMessageToHardware( msg );
    }

    void sendToDevice( const CPktMgr::HWMsg_t& hwMessage )
    {
        /*   juce::String log;
        log += "type " + juce::String( hwMessage.pktType );
        log += " cmd " + juce::String( hwMessage.pktCmd );
        log += " dir " + juce::String( hwMessage.pktDir );
        log += " data "; 
        for( auto i : hwMessage.data )
        {
            log += juce::String( i ) + " ";
        }
        juce::Logger::writeToLog( log );
        juce::Logger::writeToLog( "" );*/

        Controls::InputEvents::sendMessageToHardware( const_cast<CPktMgr::HWMsg_t&>( hwMessage ) );
    }
};
