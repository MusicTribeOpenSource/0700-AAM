
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
#include "contracts/dcl/dcl_ipc_message_types.h"
#include <PktMgr/PktMgr.hpp>

#include "contracts/dcl/dcl_contracts_helper.h"
#include "ipc/utils/mt_ipc_json.h"

namespace kick
{
    namespace dcl
    {
        class KickDeviceMessageHandler : public ipc::MTIIPCMessageHandler
        {
        public:
            KickDeviceMessageHandler() {}

            ~KickDeviceMessageHandler(){}

            virtual bool                    isDefaultHandler() { return true; }
            virtual const std::vector<int>& messageTypes() { return types; };


            virtual void handleRequest( const int                                   messageType,
                                        std::shared_ptr<ipc::MTIPCJson>              message,
                                        std::weak_ptr<ipc::MTIPCMessageDispatcher> dispatcher )
            {
                CPktMgr::HWMsg_t hwPacket;
                if (!contracts::DclContractsHelper::ipcMessageToHWMessage( message, hwPacket )) return;
                Controls::InputEvents::sendMessageToHardware( hwPacket );

            };

        private:
            std::vector<int> types{contracts::DclIPCMessageType::DCL_IPC_MESSAGE_TYPE_HW_COMMAND };
        };
    }
}
