// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginPanel.h"
#include <clap/ext/params.h>

namespace kick::clap {
struct PluginPanel::ParameterRow : juce::Component {
    std::shared_ptr<PluginSession> session;
    Parameter parameter;
    juce::Label label, value;
    juce::Slider slider;
    ParameterRow(std::shared_ptr<PluginSession> owner, Parameter p) : session(std::move(owner)), parameter(std::move(p)) {
        addAndMakeVisible(label); addAndMakeVisible(value); addAndMakeVisible(slider);
        label.setText(parameter.module.empty() ? parameter.name : parameter.module + " / " + parameter.name, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        value.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(parameter.minimum, parameter.maximum > parameter.minimum ? parameter.maximum : parameter.minimum + 1,
                        (parameter.flags & CLAP_PARAM_IS_STEPPED) ? 1. : 0.);
        slider.setEnabled(!(parameter.flags & CLAP_PARAM_IS_READONLY) && parameter.maximum > parameter.minimum);
        slider.onValueChange = [this] { session->host().setParameter(parameter.id, slider.getValue()); updateText(); };
        slider.onDragStart = [this] { session->host().parameterGesture(parameter.id, true); };
        slider.onDragEnd = [this] { session->host().parameterGesture(parameter.id, false); };
        refresh();
    }
    void refresh() {
        if (!slider.isMouseButtonDown()) slider.setValue(session->host().parameterValue(parameter.id), juce::dontSendNotification);
        updateText();
    }
    void updateText() { value.setText(session->host().parameterText(parameter.id, slider.getValue()), juce::dontSendNotification); }
    void resized() override {
        auto bounds = getLocalBounds().reduced(8, 2);
        label.setBounds(bounds.removeFromLeft(getWidth() * 35 / 100));
        value.setBounds(bounds.removeFromRight(130)); slider.setBounds(bounds);
    }
};
std::filesystem::path PluginPanel::scannerPath() {
    if (const auto* overridePath = std::getenv("KICK_CLAP_SCANNER")) return overridePath;
    auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto sibling = executable.getSiblingFile("kick-clap-scan");
    if (sibling.existsAsFile()) return sibling.getFullPathName().toStdString();
    if (juce::File("/usr/bin/kick-clap-scan").existsAsFile()) return "/usr/bin/kick-clap-scan";
    return "/usr/local/bin/kick-clap-scan";
}
PluginPanel::PluginPanel(std::shared_ptr<PluginSession> owner, bool automaticallyScan) : session(std::move(owner)), roots(defaultSearchPaths()) {
    for (auto* component : std::initializer_list<juce::Component*>{&scanButton, &folderButton, &loadButton, &removeButton,
                                                                  &panicButton, &errorsButton, &pluginList, &search, &statusLabel, &title, &parameterList}) addAndMakeVisible(component);
    title.setText("CLAP instruments", juce::dontSendNotification);
#if JUCE_MAJOR_VERSION >= 8
    title.setFont(juce::Font(juce::FontOptions(22.f)));
#else
    title.setFont(juce::Font(22.f));
#endif
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setText(session->status(), juce::dontSendNotification);
    pluginList.setTextWhenNothingSelected("Scan and choose an instrument");
    search.setTextToShowWhenEmpty("Filter parameters...", juce::Colours::grey);
    search.onTextChange = [this] { rebuildParameters(); };
    parameterList.setRowHeight(54);
    parameterList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff15191e));
    errorsButton.setEnabled(false);
    errorsButton.onClick = [this] { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Plugin scan errors", scanErrors); };
    scanButton.onClick = [this] { scanPlugins(); };
    folderButton.onClick = [this] { chooseFolder(); };
    loadButton.onClick = [this] {
        auto selected = pluginList.getSelectedId() - 1;
        if (selected >= 0 && selected < static_cast<int>(catalog.size())) { session->select(catalog[static_cast<size_t>(selected)]); rebuildParameters(); }
    };
    removeButton.onClick = [this] { session->remove(); rebuildParameters(); };
    panicButton.onClick = [this] { session->panic(); };
    for (int i = 0; i < 4; ++i) {
        auto& macro = macros[static_cast<size_t>(i)]; addAndMakeVisible(macro);
        macro.setTextWhenNothingSelected("Knob " + juce::String(i + 1));
        macro.onChange = [this, i] {
            const auto selected = macros[static_cast<size_t>(i)].getSelectedId() - 2;
            const auto& parameters = session->host().parameters();
            session->assignMacro(i, selected >= 0 && selected < static_cast<int>(parameters.size()) ? parameters[static_cast<size_t>(selected)].id : CLAP_INVALID_ID);
        };
    }
    auto extraPaths = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("KICK/clap-paths.txt");
    juce::StringArray lines; lines.addLines(extraPaths.loadFileAsString());
    for (const auto& line : lines) if (line.isNotEmpty()) roots.emplace_back(line.toStdString());
    setSize(960, 540); rebuildParameters(); startTimerHz(10);
    if (automaticallyScan) scanPlugins();
}
PluginPanel::~PluginPanel() { stopTimer(); parameterList.setModel(nullptr); if (scanJob) scanJob->cancelled.store(true); }
void PluginPanel::scanPlugins() {
    if (scanJob && !scanJob->complete.load()) return;
    scanJob = std::make_shared<ScanJob>();
    auto job = scanJob; auto paths = roots; auto scanner = scannerPath();
    statusLabel.setText("Scanning instruments...", juce::dontSendNotification); scanButton.setEnabled(false);
    // The worker owns only shared job data. Closing the panel cancels the child
    // process and cannot leave a callback pointing at a deleted component.
    std::thread([job, paths, scanner] {
        try { job->result = scan(paths, scanner, 5000, &job->cancelled); }
        catch (const std::exception& e) { job->result.failures.push_back({scanner.string(), e.what()}); }
        job->complete.store(true);
    }).detach();
}
void PluginPanel::chooseFolder() {
    chooser = std::make_unique<juce::FileChooser>("Choose a folder containing Linux CLAP instruments");
    juce::Component::SafePointer<PluginPanel> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [safe](const juce::FileChooser& selection) {
        if (!safe || selection.getResult() == juce::File{}) return;
        const auto path = selection.getResult().getFullPathName();
        safe->roots.emplace_back(path.toStdString());
        auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("KICK/clap-paths.txt");
        file.getParentDirectory().createDirectory(); file.appendText(path + "\n");
        safe->scanPlugins();
    });
}
void PluginPanel::rebuildParameters() {
    visibleParameters.clear();
    auto& host = session->host();
    const auto filter = search.getText();
    for (const auto& p : host.parameters()) {
        if (!(p.flags & CLAP_PARAM_IS_HIDDEN) && (filter.isEmpty() || juce::String(p.name + " " + p.module).containsIgnoreCase(filter))) {
            visibleParameters.push_back(p);
        }
    }
    std::sort(visibleParameters.begin(), visibleParameters.end(), [](const Parameter& a, const Parameter& b) {
        return juce::String(a.module + "/" + a.name).compareNatural(juce::String(b.module + "/" + b.name)) < 0;
    });
    parameterRevision = host.metadataRevision();
    parameterList.updateContent();
    title.setText(host.loaded() ? "CLAP - " + juce::String(host.descriptor().name) : "CLAP instruments", juce::dontSendNotification);
    std::vector<size_t> order;
    for (size_t i = 0; i < host.parameters().size(); ++i) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const auto& lhs = host.parameters()[a]; const auto& rhs = host.parameters()[b];
        return juce::String(lhs.module + "/" + lhs.name).compareNatural(juce::String(rhs.module + "/" + rhs.name)) < 0;
    });
    for (int i = 0; i < 4; ++i) {
        auto& macro = macros[static_cast<size_t>(i)]; macro.clear(juce::dontSendNotification); macro.addItem("Knob " + juce::String(i+1) + ": unassigned", 1);
        int selected = 1;
        for (const auto index : order) {
            const auto& p = host.parameters()[index];
            const auto item = static_cast<int>(index + 2);
            if (!(p.flags & (CLAP_PARAM_IS_HIDDEN | CLAP_PARAM_IS_READONLY))) macro.addItem(p.name, item);
            if (session->macro(i) == p.id) selected = item;
        }
        macro.setSelectedId(selected, juce::dontSendNotification);
    }
    resized();
}
juce::Component* PluginPanel::refreshComponentForRow(int row, bool, juce::Component* existing) {
    delete existing;
    if (row < 0 || row >= static_cast<int>(visibleParameters.size())) return nullptr;
    return new ParameterRow(session, visibleParameters[static_cast<size_t>(row)]);
}
void PluginPanel::timerCallback() {
    if (scanJob && scanJob->complete.load()) {
        catalog.clear();
        for (auto& p : scanJob->result.plugins)
            if (std::find(p.features.begin(), p.features.end(), CLAP_PLUGIN_FEATURE_INSTRUMENT) != p.features.end()) catalog.push_back(std::move(p));
        pluginList.clear(juce::dontSendNotification);
        for (size_t i = 0; i < catalog.size(); ++i) {
            pluginList.addItem(catalog[i].name + " - " + catalog[i].vendor, static_cast<int>(i + 1));
            if (catalog[i].id == session->host().descriptor().id) pluginList.setSelectedId(static_cast<int>(i + 1), juce::dontSendNotification);
        }
        juce::String failures;
        for (const auto& f : scanJob->result.failures) failures += juce::String(f.path) + ": " + f.message + "\n";
        scanButton.setTooltip(failures);
        statusLabel.setTooltip(failures);
        scanErrors = failures; errorsButton.setEnabled(failures.isNotEmpty());
        title.setText("CLAP instruments - " + juce::String(catalog.size()) + " found, " + juce::String(scanJob->result.failures.size()) + " scan errors", juce::dontSendNotification);
        scanJob.reset(); scanButton.setEnabled(true);
    }
    auto& host = session->host();
    if (host.metadataRevision() != parameterRevision) rebuildParameters();
    const auto* viewport = parameterList.getViewport();
    const auto first = viewport ? viewport->getViewPositionY() / 54 : 0;
    const auto last = std::min(getNumRows(), first + parameterList.getHeight() / 54 + 2);
    for (int i = std::max(0, first); i < last; ++i)
        if (auto* row = dynamic_cast<ParameterRow*>(parameterList.getComponentForRowNumber(i))) row->refresh();
    const auto stats = host.statistics();
    if (!scanJob) statusLabel.setText(session->status() + "  |  latency " + juce::String(host.latency()) + " samples  |  peak " +
                                     juce::String(stats.maximumMicroseconds, 0) + " us  |  missed " + juce::String(stats.deadlineMisses) +
                                     "  |  paused " + juce::String(stats.controlSilences) + "  |  dropped " + juce::String(stats.droppedEvents), juce::dontSendNotification);
}
void PluginPanel::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff15191e)); }
void PluginPanel::resized() {
    auto bounds = getLocalBounds().reduced(12);
    title.setBounds(bounds.removeFromTop(30));
    auto toolbar = bounds.removeFromTop(46);
    scanButton.setBounds(toolbar.removeFromLeft(72).reduced(2)); folderButton.setBounds(toolbar.removeFromLeft(104).reduced(2));
    errorsButton.setBounds(toolbar.removeFromLeft(96).reduced(2));
    panicButton.setBounds(toolbar.removeFromRight(72).reduced(2)); removeButton.setBounds(toolbar.removeFromRight(82).reduced(2));
    loadButton.setBounds(toolbar.removeFromRight(70).reduced(2)); pluginList.setBounds(toolbar.reduced(2));
    auto macroRow = bounds.removeFromTop(44);
    const auto width = macroRow.getWidth() / 4;
    for (auto& macro : macros) macro.setBounds(macroRow.removeFromLeft(width).reduced(3));
    search.setBounds(bounds.removeFromTop(34).reduced(2));
    statusLabel.setBounds(bounds.removeFromBottom(36)); parameterList.setBounds(bounds);
}
} // namespace kick::clap
