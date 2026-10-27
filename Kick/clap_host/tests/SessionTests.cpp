// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginSession.h"
#include "PluginPanel.h"
#include <kick_clap/Discovery.h>
#include <cstdlib>
#include <iostream>
using namespace kick::clap;
static void check(bool value, const char* message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
int main(int argc, char** argv) {
    check(argc >= 2, "plugin path");
    juce::ScopedJuceInitialiser_GUI juce;
    const auto catalog = inspect(argv[1]);
    check(!catalog.plugins.empty(), "discover instrument");
    auto descriptor = catalog.plugins.front();
    if (argc > 2) {
        bool found = false;
        for (const auto& d : catalog.plugins) if (d.id == argv[2]) { descriptor = d; found = true; }
        check(found, "requested plugin ID exists");
    }
    juce::ValueTree tree("PROGRAM");
    auto session = PluginSession::create(tree);
    session->prepare(48000, 128); session->service(); session->select(descriptor);
    check(session->host().loaded(), session->status().toRawUTF8());
    if (const auto* imagePath = std::getenv("KICK_CLAP_PANEL_SNAPSHOT")) {
        PluginPanel panel(session, false);
        auto image = panel.createComponentSnapshot(panel.getLocalBounds());
        juce::FileOutputStream stream{juce::File(imagePath)};
        check(stream.openedOk(), "open panel snapshot");
        juce::PNGImageFormat png; check(png.writeImageToStream(image, stream), "render panel snapshot");
    }
    if (argc == 2) {
        check(session->host().setParameter(7, .3), "parameter edit");
        session->assignMacro(0, 7); session->adjustMacro(0, .1);
    }
    session->midi(juce::MidiMessage::noteOn(1, 60, .8f));
    session->setTransport(120, 8, true);
    juce::AudioBuffer<float> block(2, 128);
    double energy = 0;
    for (int i = 0; i < 375; ++i) {
        session->render(block);
        for (int c = 0; c < 2; ++c) for (int s = 0; s < block.getNumSamples(); ++s) {
            const auto value = block.getSample(c, s); check(std::isfinite(value), "finite rendered audio"); energy += value * value;
        }
        if ((i % 16) == 0) session->service();
    }
    check(energy > 1e-6, "plugin produces audible audio");
    session->midi(juce::MidiMessage::noteOff(1, 60)); session->render(block);
    check(session->save(), "save session state");
    auto xml = tree.createXml(); check(xml != nullptr, "serialize project tree");
    auto restoredTree = juce::ValueTree::fromXml(*xml);
    auto restored = PluginSession::create(restoredTree);
    restored->prepare(48000, 128); restored->service();
    check(restored->host().loaded(), "restore plugin from serialized project");
    if (argc == 2) {
        check(restored->macro(0) == 7, "restore macro assignment");
        check(std::abs(restored->host().parameterValue(7) - .4) < 1e-9, "restore edited parameter");
    }
    if (argc == 2) {
        auto cloneTree = juce::ValueTree::fromXml(tree.toXmlString());
        auto clone = PluginSession::create(cloneTree);
        clone->prepare(48000, 128); clone->service();
        check(std::abs(clone->host().parameterValue(7) - .4) < 1e-9, "duplicate retains opaque state");
        clone.reset();
        auto missingTree = juce::ValueTree::fromXml(tree.toXmlString());
        auto missingState = missingTree.getChildWithName("CLAP");
        const auto opaque = missingState.getProperty("state");
        missingState.setProperty("path", "/missing/instrument.clap", nullptr);
        auto missing = PluginSession::create(missingTree);
        missing->prepare(48000, 128); missing->service();
        check(!missing->host().loaded() && missing->save() && missingState.getProperty("state") == opaque, "missing plugin preserves saved state");
        missing->select(descriptor);
        check(missing->host().loaded() && std::abs(missing->host().parameterValue(7) - .4) < 1e-9, "relink restores saved state for exact ID");
        missing.reset();
        session->remove(); session->select(catalog.plugins[8]);
        session->assignMacro(0, 8); session->assignMacro(1, 9);
        session->adjustMacro(0, 1. / 127);
        check(session->host().parameterValue(8) == 1, "one clockwise tick toggles on");
        for (int i = 0; i < 200; ++i) session->adjustMacro(0, 1. / 127);
        check(session->host().parameterValue(8) == 1, "toggle upper endpoint");
        session->adjustMacro(0, -1. / 127);
        check(session->host().parameterValue(8) == 0, "one counterclockwise tick toggles off");
        session->adjustMacro(0, -1. / 127);
        check(session->host().parameterValue(8) == 0, "toggle lower endpoint");
        for (int i = 1; i <= 4; ++i) {
            session->adjustMacro(1, 1. / 127);
            check(session->host().parameterValue(9) == i, "enum moves upward one step per tick");
        }
        session->adjustMacro(1, 1. / 127); check(session->host().parameterValue(9) == 4, "enum upper endpoint");
        for (int i = 3; i >= 0; --i) {
            session->adjustMacro(1, -1. / 127);
            check(session->host().parameterValue(9) == i, "enum moves downward one step per tick");
        }
        session->adjustMacro(1, -1. / 127); check(session->host().parameterValue(9) == 0, "enum lower endpoint");
        session->remove(); session->select(catalog.plugins[11]);
        session->setTransport(120, 0, false); session->render(block);
        check(block.getMagnitude(0, 128) == 0, "transport instrument stopped");
        session->setTransport(120, 0, true); session->render(block);
        check(block.getMagnitude(0, 128) > 0, "session Play wakes instrument without MIDI");
        session->setTransport(120, 8, false); session->render(block);
        check(block.getMagnitude(0, 128) == 0, "session Stop silences transport instrument");
    }
    session.reset(); restored.reset();
    std::cout << "PASS: JUCE session MIDI/audio, transport, parameter/macro edits, project XML and opaque plugin-state recall\n";
}
