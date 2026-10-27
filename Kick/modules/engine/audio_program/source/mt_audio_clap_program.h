// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "mt_audio_iprogram.h"
#include <PluginSession.h>

namespace mt {
class ClapProgram;
class MTClapProgram final : public MTIProgram {
public:
    MTClapProgram(AudIO::DawEngine&, juce::ValueTree&, juce::UndoManager* = nullptr);
    ~MTClapProgram() override;
    std::shared_ptr<kick::clap::PluginSession> getPluginSession() const { return session; }
    void noteProcess(juce::Array<juce::MidiMessage>&) override;
    void noteOn(int note, int velocity) override;
    void noteOff(int note) override;
    void noteOffAll() override { session->panic(); }
    bool prepareToSave() override { return session->save(); }
    juce::String saveError() const override { return session->status(); }
    void updatePluginTransport(double bpm, double beats, bool playing) override { session->setTransport(bpm, beats, playing); }
    void enableOutputMetering(bool) override;
    const MTMeterData& readOutMeter() const override;
private:
    AudIO::DawEngine& engine;
    std::shared_ptr<kick::clap::PluginSession> session;
    ClapProgram* synth = nullptr;
};
} // namespace mt
