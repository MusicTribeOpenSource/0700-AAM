// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mt_audio_clap_program.h"
#include "../synth/iSynthObject.h"

namespace mt {
class ClapProgram final : public ISynthObject {
public:
    ClapProgram(AudIO::Design& design, MTIProgram* program, std::shared_ptr<kick::clap::PluginSession> owner)
        : ISynthObject(design, 0), session(std::move(owner)) {
        ISynthObject::setProgram(program);
        setName(program->getName().toStdString());
        setBypass(false);
    }
    void initialize(uint32_t rate, uint32_t frames) override { session->prepare(rate, frames); }
    void doProcess(juce::AudioBuffer<float>& buffer) override {
        if (master.isMuted()) { session->panic(); buffer.clear(); return; }
        session->render(buffer);
    }
private:
    std::shared_ptr<kick::clap::PluginSession> session;
};
MTClapProgram::MTClapProgram(AudIO::DawEngine& audioEngine, juce::ValueTree& tree, juce::UndoManager* undo)
    : MTIProgram(tree, undo), engine(audioEngine), session(kick::clap::PluginSession::create(tree)) {
    auto object = std::make_unique<ClapProgram>(engine, this, session);
    synth = object.get(); engine.createObject(std::move(object)); synth->initAudioRouting();
    synth->connectOutputTo(engine.getBusManager()->getBusId(getOutput().toStdString()));
    synth->setGainsForSend(getSendGains());
    onNameChanged = [this] { synth->setName(getName().toStdString()); };
    onOutChanged = [this] { synth->connectOutputTo(engine.getBusManager()->getBusId(getOutput().toStdString())); };
    onSendGainChanged = [this](int channel, float gain) { synth->setGainForSend(static_cast<uint32_t>(channel), gain); };
    auto inserts = tree.getOrCreateChildWithName(IDs::INSERTS, nullptr);
    MTIInsertsInterface::setup(inserts, undo, synth);
}
MTClapProgram::~MTClapProgram() { session->panic(); if (synth) engine.removeObject(synth->getId()); }
void MTClapProgram::noteProcess(juce::Array<juce::MidiMessage>& messages) {
    for (const auto& message : messages) {
        // Releases still reach a muted program so unmuting cannot revive held gates.
        if (isProgramEffectivelyMuted() && message.isNoteOn()) { session->panic(); continue; }
        session->midi(message);
        if (message.isNoteOn()) reloadMidiMeter(message.getFloatVelocity());
    }
}
void MTClapProgram::noteOn(int note, int velocity) {
    session->midi(juce::MidiMessage::noteOn(1, juce::jlimit(0, 127, note), static_cast<juce::uint8>(juce::jlimit(0, 127, velocity))));
}
void MTClapProgram::noteOff(int note) { session->midi(juce::MidiMessage::noteOff(1, juce::jlimit(0, 127, note))); }
void MTClapProgram::enableOutputMetering(bool enable) { synth->enableOutputMetering(enable); }
const MTMeterData& MTClapProgram::readOutMeter() const { return synth->readOutMeter(); }
} // namespace mt
