// SPDX-License-Identifier: AGPL-3.0-or-later
#include <juce_events/juce_events.h>
#include <contracts/dcl/dcl_signals.h>
#include <contracts/dcl/dcl_contracts.h>
#include <contracts/dcl/dcl_base_message_handler.h>
#include <thread>
#include <iostream>
static void check(bool ok, const char* why) { if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    using contracts::DclSignals;
    CPktMgr::HWMsg_t knob;
    knob.pktType = CPktMgr::EN_PKT_TYPE_KNOBS; knob.data = {1, 0, 1};
    CPktMgr::HWMsg_t button;
    int screenEdits = 0, firstEdits = 0, secondEdits = 0;
    bool firstActive = true;
    sigslot::scoped_connection screen = DclSignals::hardwareInputTriggered.connect([&](CPktMgr::HWMsg_t&) { ++screenEdits; });
    auto first = DclSignals::claimHardwareInput([&](CPktMgr::HWMsg_t& event) {
        check(juce::MessageManager::getInstance()->isThisTheMessageThread(), "owner runs on message thread");
        if (!firstActive || event.pktType != CPktMgr::EN_PKT_TYPE_KNOBS) return false;
        ++firstEdits; return true;
    });
    // The monolithic raw-input bridge and split GUI message handler both call
    // this contract. Neither emits the legacy screen signal directly anymore.
    DclSignals::dispatchHardwareInput(knob);
    check(firstEdits == 1 && screenEdits == 0, "owned knob cannot also edit ordinary screen");
    DclSignals::dispatchHardwareInput(button);
    check(screenEdits == 1, "unclaimed button reaches ordinary screen");
    firstActive = false; DclSignals::dispatchHardwareInput(knob);
    check(firstEdits == 1 && screenEdits == 2, "inactive dialog passes input through");
    firstActive = true;
    auto second = DclSignals::claimHardwareInput([&](CPktMgr::HWMsg_t&) { ++secondEdits; return true; });
    DclSignals::dispatchHardwareInput(knob);
    check(secondEdits == 1 && firstEdits == 1 && screenEdits == 2, "newest active dialog owns input exclusively");
    second.reset(); DclSignals::dispatchHardwareInput(knob);
    check(firstEdits == 2, "destroyed dialog releases ownership");
    std::thread worker([&] { DclSignals::dispatchHardwareInput(knob); }); worker.join();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    check(firstEdits == 3 && screenEdits == 2, "worker producer queues a copied message to common owner");
    contracts::DclBaseMessageHandler splitHandler;
    auto message = std::make_shared<ipc::MTIPCJson>();
    contracts::DclContractsHelper::hWMessageToIPCMessage(knob, message);
    splitHandler.handleEvent(contracts::DclIPCMessageType::DCL_IPC_MESSAGE_TYPE_HW_EVENT, message, {});
    check(firstEdits == 4 && screenEdits == 2, "actual split GUI IPC route reaches exclusive owner");
    contracts::KeyboardSignals::KeyboardEventTriggered(knob);
    check(firstEdits == 5 && screenEdits == 2, "actual split keyboard route reaches exclusive owner");
    first.reset(); DclSignals::dispatchHardwareInput(knob);
    check(screenEdits == 3, "closing owner restores ordinary screen route");
    std::cout << "PASS: common hardware routing, active-dialog ownership, no double edits, lifetime and producer-thread dispatch\n";
}
