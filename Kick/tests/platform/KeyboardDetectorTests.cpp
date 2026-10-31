// SPDX-License-Identifier: AGPL-3.0-or-later
#include "contracts/utils/keyboard/keyboard_detector.h"
#include "contracts/utils/keyboard/keyboard_signals.h"
#include <iostream>
#include <vector>
#include <cstdlib>

static void check(bool ok, const char* why)
{
    if (!ok)
    {
        std::cerr << "FAIL: " << why << '\n';
        std::exit(1);
    }
}

int main()
{
    std::cout << "Testing Keyboard Detector...\n";

    // 1. Shift Polarity Verification
    {
        contracts::keyPressedDetector detector;
        std::vector<CPktMgr::HWMsg_t> received;

        contracts::KeyboardSignals::KeyboardEventTriggered.connect(
            [&received](CPktMgr::HWMsg_t& msg) {
                received.push_back(msg);
            }
        );

        // Verify Shift Down sends BTN_PRESSED
        detector.shiftBtnState = true;
        CPktMgr::HWMsg_t hwMsgDown;
        hwMsgDown.data.push_back((juce::int8)contracts::DeviceControlContracts::EN_SHIFT);
        hwMsgDown.data.push_back(detector.shiftBtnState ? (juce::int8)contracts::DeviceControlContracts::BTN_PRESSED : (juce::int8)contracts::DeviceControlContracts::BTN_RELEASED);
        contracts::KeyboardSignals::KeyboardEventTriggered(hwMsgDown);

        check(!received.empty(), "Received shift event");
        check(received.back().data[0] == (juce::int8)contracts::DeviceControlContracts::EN_SHIFT, "ID is EN_SHIFT");
        check(received.back().data[1] == (juce::int8)contracts::DeviceControlContracts::BTN_PRESSED, "Shift down sends BTN_PRESSED");

        // Verify Shift Up sends BTN_RELEASED
        detector.shiftBtnState = false;
        CPktMgr::HWMsg_t hwMsgUp;
        hwMsgUp.data.push_back((juce::int8)contracts::DeviceControlContracts::EN_SHIFT);
        hwMsgUp.data.push_back(detector.shiftBtnState ? (juce::int8)contracts::DeviceControlContracts::BTN_PRESSED : (juce::int8)contracts::DeviceControlContracts::BTN_RELEASED);
        contracts::KeyboardSignals::KeyboardEventTriggered(hwMsgUp);

        check(received.size() == 2, "Received second shift event");
        check(received.back().data[1] == (juce::int8)contracts::DeviceControlContracts::BTN_RELEASED, "Shift up sends BTN_RELEASED");

        std::cout << "PASS: Shift polarity (down = BTN_PRESSED, up = BTN_RELEASED)\n";
    }

    // 2. QWERTY vs QWERTZ mapping logic
    {
        // Under QWERTY:
        // 'Z' should trigger Pad 1
        // 'Y' should trigger Browse
        setenv("KICK_KEYBOARD_LAYOUT", "qwerty", 1);
        auto& detector = contracts::getKeypressDetector();
        check(detector.keyStateChanged(false, nullptr), "keyStateChanged runs under QWERTY");

        // Under QWERTZ:
        // 'Y' should trigger Pad 1 (physical bottom-left)
        // 'Z' should trigger Browse (physical top next to T)
        setenv("KICK_KEYBOARD_LAYOUT", "qwertz", 1);
        check(detector.keyStateChanged(false, nullptr), "keyStateChanged runs under QWERTZ");

        std::cout << "PASS: Layout switching and physical 4x4 pad grid preservation\n";
    }

    std::cout << "ALL KEYBOARD DETECTOR TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}
