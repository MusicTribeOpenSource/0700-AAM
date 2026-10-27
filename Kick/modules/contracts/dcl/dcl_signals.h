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
#include "juce_core/juce_core.h"
#include "PktMgr/PktMgr.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace juce
{
    class var;
}
namespace ipc
{
    class MTIPCJson;
}

namespace contracts
{
    class DclSignals
    {
    public:
        static sigslot::signal<std::shared_ptr<ipc::MTIPCJson>> sendCommandToDcl;
        static sigslot::signal<CPktMgr::HWMsg_t&>               createAndSendCommandToDcl;
        static sigslot::signal<CPktMgr::HWMsg_t&>               hardwareInputTriggered;
        // Main-thread owners get first refusal. Releasing the lease restores the
        // ordinary screen route; the newest active dialog has priority.
        struct HardwareInputOwner { std::function<bool(CPktMgr::HWMsg_t&)> consume; };
        using HardwareInputLease = std::shared_ptr<HardwareInputOwner>;
        static HardwareInputLease claimHardwareInput(std::function<bool(CPktMgr::HWMsg_t&)>);
        static void dispatchHardwareInput(CPktMgr::HWMsg_t&);
        // Todo: need to check whether the below signals should be defined here or in a UI module.
        static sigslot::signal<>    refreshPadLeds;
        static sigslot::signal<int> onMainModeTabSelected;
    };
}
