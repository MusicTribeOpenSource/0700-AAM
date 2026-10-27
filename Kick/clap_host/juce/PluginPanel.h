// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "PluginSession.h"
#include <kick_clap/Discovery.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <thread>

namespace kick::clap {
class PluginPanel : public juce::Component, private juce::Timer, private juce::ListBoxModel {
public:
    explicit PluginPanel(std::shared_ptr<PluginSession>, bool automaticallyScan = true);
    ~PluginPanel() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void adjustMacro(int index, double delta) { session->adjustMacro(index, delta); }
    static std::filesystem::path scannerPath();
private:
    struct ParameterRow;
    struct ScanJob {
        std::atomic<bool> cancelled{false}, complete{false};
        ScanResult result;
    };
    void scanPlugins();
    void rebuildParameters();
    void timerCallback() override;
    void chooseFolder();
    int getNumRows() override { return static_cast<int>(visibleParameters.size()); }
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow(int, bool, juce::Component*) override;
    std::shared_ptr<PluginSession> session;
    std::shared_ptr<ScanJob> scanJob;
    std::vector<Descriptor> catalog;
    std::vector<std::filesystem::path> roots;
    juce::TextButton scanButton{"Scan"}, folderButton{"Add folder"}, loadButton{"Load"}, removeButton{"Remove"}, panicButton{"Panic"}, errorsButton{"Scan errors"};
    juce::ComboBox pluginList;
    juce::TextEditor search;
    juce::Label statusLabel, title;
    std::array<juce::ComboBox, 4> macros;
    juce::ListBox parameterList{"Parameters", this};
    std::vector<Parameter> visibleParameters;
    std::unique_ptr<juce::FileChooser> chooser;
    uint64_t parameterRevision = 0;
    juce::String scanErrors;
};
} // namespace kick::clap
