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
    button.data = {contracts::DeviceControlContracts::EN_PLAY, contracts::DeviceControlContracts::BTN_PRESSED};
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
    const auto beforeInvalid = screenEdits;
    for (const auto type : {CPktMgr::EN_PKT_TYPE_SYSTEM, CPktMgr::EN_PKT_TYPE_BUTTON,
                            CPktMgr::EN_PKT_TYPE_KNOBS, CPktMgr::EN_PKT_TYPE_PAD, CPktMgr::EN_PKT_TYPE_SLIDER}) {
        CPktMgr::HWMsg_t truncated; truncated.pktType = type;
        DclSignals::dispatchHardwareInput(truncated);
    }
    auto truncated = knob; truncated.data.pop_back();
    DclSignals::dispatchHardwareInput(truncated);
    truncated.pktType = CPktMgr::EN_PKT_TYPE_SYSTEM;
    truncated.pktCmd = contracts::DeviceControlContracts::EN_SYS_GET_SW_VER; truncated.data = {1};
    DclSignals::dispatchHardwareInput(truncated);
    check(screenEdits == beforeInvalid, "truncated payloads never reach UI consumers");
    button.data = {contracts::DeviceControlContracts::EN_SHIFT, contracts::DeviceControlContracts::BTN_PRESSED};
    DclSignals::dispatchHardwareInput(button);
    check(contracts::DclContractsHelper::isShiftPressed(), "shared contract observes physical and split keyboard shift press");
    button.data[1] = contracts::DeviceControlContracts::BTN_RELEASED;
    contracts::KeyboardSignals::KeyboardEventTriggered(button);
    check(!contracts::DclContractsHelper::isShiftPressed(), "shared contract observes shift release");
    CPktMgr::HWMsg_t pad; pad.pktType = CPktMgr::EN_PKT_TYPE_PAD; pad.data = {0, 1, 100};
    DclSignals::dispatchHardwareInput(pad);
    check(screenEdits == beforeInvalid + 3 && contracts::DclContractsHelper::getPadAftertouch(pad) == 0,
          "three-byte pad events remain valid without optional aftertouch");
    message->setProperty("pkt_data", "invalid");
    splitHandler.handleEvent(contracts::DclIPCMessageType::DCL_IPC_MESSAGE_TYPE_HW_EVENT, message, {});
    splitHandler.handleEvent(contracts::DclIPCMessageType::DCL_IPC_MESSAGE_TYPE_HW_EVENT, {}, {});
    check(screenEdits == beforeInvalid + 3, "non-array IPC payload is rejected without dispatch");
    CPktMgr::HWMsg_t decoded = knob;
    check(!contracts::DclContractsHelper::ipcMessageToHWMessage(message, decoded) && decoded.data == knob.data,
          "failed conversion preserves the destination");
    check(contracts::DclContractsHelper::ipcMessageToHWMessage(contracts::DclContractsHelper::reconnectMessage(), decoded) && decoded.data.empty(),
          "outgoing system commands may have an empty payload");
    contracts::DclContractsHelper::hWMessageToIPCMessage(knob, message);
    check(contracts::DclContractsHelper::ipcMessageToHWMessage(message, decoded) && decoded.data == knob.data &&
          contracts::DclContractsHelper::ipcMessageToHWMessage(message, decoded) && decoded.data == knob.data,
          "reused destination replaces its previous payload");
    std::cout << "PASS: common hardware routing, active-dialog ownership, no double edits, lifetime and producer-thread dispatch\n";
}
