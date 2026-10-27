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

#include "ipc/processor/mti_ipc_message_handler.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/dcl/dcl_signals.h"
#include "contracts/dcl/dcl_ipc_message_types.h"
#include "contracts/utils/keyboard/keyboard_signals.h"
#include "ipc/utils/mt_ipc_json.h"

namespace contracts
{
    class DclBaseMessageHandler : public ipc::MTIIPCMessageHandler
    {
    public:
        DclBaseMessageHandler()
        {
            contracts::KeyboardSignals::KeyboardEventTriggered.connect( [&]( CPktMgr::HWMsg_t& f_HwMsg ) {
                contracts::DclSignals::dispatchHardwareInput( f_HwMsg );
            } );
        }

        const std::vector<int>& messageTypes() override { return supportedMessageTypes; };

        void handleEvent( const int                                  messageType,
                          std::shared_ptr<ipc::MTIPCJson>            message,
                          std::weak_ptr<ipc::MTIPCMessageDispatcher> dispatcher ) override
        {
            CPktMgr::HWMsg_t hwMessage;
            contracts::DclContractsHelper::ipcMessageToHWMessage( message, hwMessage );
            contracts::DclSignals::dispatchHardwareInput( hwMessage );
            /*juce::Logger::writeToLog( "Received Message" );
            juce::Logger::writeToLog( juce::JSON::toString( message->getRoot() ) );*/
        };

    private:
        std::vector<int> supportedMessageTypes{ contracts::DclIPCMessageType::DCL_IPC_MESSAGE_TYPE_HW_EVENT };
    };
}
