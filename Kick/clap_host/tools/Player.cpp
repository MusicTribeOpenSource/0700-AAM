// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginPanel.h"

namespace {
class Player final : public juce::AudioAppComponent, private juce::MidiInputCallback,
                     private juce::MidiKeyboardStateListener {
public:
    Player() : session(kick::clap::PluginSession::create(project)), panel(session), keyboard(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) {
        addAndMakeVisible(panel); addAndMakeVisible(keyboard); addAndMakeVisible(settings);
        keyboardState.addListener(this);
        settings.onClick = [this] {
            juce::DialogWindow::LaunchOptions dialog;
            auto* selector = new juce::AudioDeviceSelectorComponent(deviceManager, 0, 0, 1, 2, true, true, true, false);
            selector->setSize(600, 420); dialog.content.setOwned(selector);
            dialog.dialogTitle = "Audio and MIDI"; dialog.dialogBackgroundColour = juce::Colours::darkgrey;
            dialog.escapeKeyTriggersCloseButton = true; dialog.useNativeTitleBar = true; dialog.launchAsync();
        };
        deviceManager.addMidiInputDeviceCallback({}, this);
        setSize(1000, 680); setAudioChannels(0, 2);
    }
    ~Player() override {
        deviceManager.removeMidiInputDeviceCallback({}, this); keyboardState.removeListener(this); shutdownAudio();
    }
    void prepareToPlay(int frames, double rate) override { session->prepare(rate, static_cast<uint32_t>(frames)); }
    void releaseResources() override { session->panic(); }
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        std::array<float*, 2> channels{};
        const auto count = std::min(2, info.buffer->getNumChannels());
        for (int c = 0; c < count; ++c) channels[static_cast<size_t>(c)] = info.buffer->getWritePointer(c, info.startSample);
        juce::AudioBuffer<float> block(channels.data(), count, info.numSamples); session->render(block);
    }
    void resized() override {
        auto bounds = getLocalBounds(); settings.setBounds(bounds.removeFromBottom(34).reduced(6));
        keyboard.setBounds(bounds.removeFromBottom(84)); panel.setBounds(bounds);
    }
private:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) override { session->midi(message); }
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override { session->midi(juce::MidiMessage::noteOn(channel, note, velocity)); }
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float) override { session->midi(juce::MidiMessage::noteOff(channel, note)); }
    juce::ValueTree project{"PROGRAM"};
    std::shared_ptr<kick::clap::PluginSession> session;
    kick::clap::PluginPanel panel;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton settings{"Audio and MIDI settings"};
};
class Application final : public juce::JUCEApplication {
    class Window final : public juce::DocumentWindow {
    public:
        Window() : DocumentWindow("KICK CLAP Host", juce::Colours::darkgrey, allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(new Player, true); setResizable(true, false);
            centreWithSize(getWidth(), getHeight()); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
public:
    const juce::String getApplicationName() override { return "KICK CLAP Host"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    void initialise(const juce::String&) override { window = std::make_unique<Window>(); }
    void shutdown() override { window.reset(); }
private:
    std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(Application)
