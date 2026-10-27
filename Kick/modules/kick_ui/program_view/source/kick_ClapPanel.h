// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <PluginPanel.h>
#include <input_events/input_events.h>

namespace kick_ui {
class ClapInstrumentPanel final : public kick::clap::PluginPanel {
public:
    explicit ClapInstrumentPanel(std::shared_ptr<kick::clap::PluginSession> session)
        : PluginPanel(std::move(session)) {
        juce::Component::SafePointer<ClapInstrumentPanel> safe(this);
        hardware = Controls::InputEvents::dispatchHardwareEvent.connect([safe](CPktMgr::HWMsg_t& message) {
            if (message.data.size() < 3 || !contracts::DclContractsHelper::isKnobMessage(message)) return;
            const auto code = contracts::DclContractsHelper::getKnobCode(message);
            const auto value = contracts::DclContractsHelper::getKnobValue(message);
            int index = -1;
            switch (code) {
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1: index = 0; break;
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_2: index = 1; break;
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_3: index = 2; break;
                case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_4: index = 3; break;
                default: return;
            }
            juce::MessageManager::callAsync([safe, index, value] {
                if (safe && safe->isShowing()) safe->adjustMacro(index, value / 127.0);
            });
        });
    }
private:
    sigslot::scoped_connection hardware;
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
