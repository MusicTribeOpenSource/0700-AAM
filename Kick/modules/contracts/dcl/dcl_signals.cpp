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

#include "dcl_signals.h"
#include "dcl_contracts_helper.h"
#include <juce_events/juce_events.h>
#include <algorithm>

namespace contracts
{
    sigslot::signal<std::shared_ptr<ipc::MTIPCJson>> DclSignals::sendCommandToDcl          = {};
    sigslot::signal<CPktMgr::HWMsg_t&>               DclSignals::createAndSendCommandToDcl = {};
    sigslot::signal<CPktMgr::HWMsg_t&>               DclSignals::hardwareInputTriggered    = {};
    sigslot::signal<>                                DclSignals::refreshPadLeds            = {};
    sigslot::signal<int>                             DclSignals::onMainModeTabSelected     = {};
}

namespace contracts {
namespace { std::vector<std::weak_ptr<DclSignals::HardwareInputOwner>> inputOwners; }
DclSignals::HardwareInputLease DclSignals::claimHardwareInput(std::function<bool(CPktMgr::HWMsg_t&)> consume) {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    auto owner = std::make_shared<HardwareInputOwner>(); owner->consume = std::move(consume);
    inputOwners.erase(std::remove_if(inputOwners.begin(), inputOwners.end(), [](const auto& p) { return p.expired(); }), inputOwners.end());
    inputOwners.push_back(owner);
    return owner;
}
void DclSignals::dispatchHardwareInput(CPktMgr::HWMsg_t& message) {
    if (!DclContractsHelper::isValidHardwareInput(message)) return;
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
        juce::MessageManager::callAsync([copy = message]() mutable { dispatchHardwareInput(copy); });
        return;
    }
    if (DclContractsHelper::isButtonMessage(message) && DclContractsHelper::getButtonCode(message) == DeviceControlContracts::EN_SHIFT)
        shiftBtnState.store(DclContractsHelper::getButtonState(message) != DeviceControlContracts::BTN_RELEASED);
    // Copy weak leases: callbacks may open/close dialogs while dispatching.
    const auto owners = inputOwners;
    for (auto it = owners.rbegin(); it != owners.rend(); ++it)
        if (auto owner = it->lock()) if (owner->consume && owner->consume(message)) return;
    hardwareInputTriggered(message);
}
}
