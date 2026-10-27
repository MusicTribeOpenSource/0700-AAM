// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <PluginPanel.h>
#include <contracts/dcl/dcl_signals.h>
#include <contracts/dcl/dcl_contracts_helper.h>

namespace kick_ui {
class ClapInstrumentPanel final : public kick::clap::PluginPanel {
public:
    explicit ClapInstrumentPanel(std::shared_ptr<kick::clap::PluginSession> session)
        : PluginPanel(std::move(session)) {
        juce::Component::SafePointer<ClapInstrumentPanel> safe(this);
        hardware = contracts::DclSignals::claimHardwareInput([safe](CPktMgr::HWMsg_t& message) {
            if (!safe || !safe->isShowing() || safe->getTopLevelComponent() != juce::Component::getCurrentlyModalComponent()) return false;
            if (message.data.size() < 3 || !contracts::DclContractsHelper::isKnobMessage(message)) return false;
            const auto code = contracts::DclContractsHelper::getKnobCode(message);
            const auto value = contracts::DclContractsHelper::getKnobValue(message);
            int index = -1;
            switch (code) {
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1: index = 0; break;
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_2: index = 1; break;
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_3: index = 2; break;
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_4: index = 3; break;
                default: return false;
            }
            safe->adjustMacro(index, value / 127.0);
            return true;
        });
    }
private:
    contracts::DclSignals::HardwareInputLease hardware;
};
inline void openClapInstrument(mt::MTIProgram* program) {
    if (auto* plugin = dynamic_cast<mt::MTClapProgram*>(program)) {
        juce::DialogWindow::LaunchOptions dialog;
        dialog.content.setOwned(new ClapInstrumentPanel(plugin->getPluginSession()));
        dialog.dialogTitle = "CLAP - " + plugin->getName();
        dialog.dialogBackgroundColour = juce::Colour(0xff15191e);
        dialog.escapeKeyTriggersCloseButton = true;
        dialog.useNativeTitleBar = false;
        dialog.resizable = true;
        dialog.launchAsync();
    }
}
} // namespace kick_ui
