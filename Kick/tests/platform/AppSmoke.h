// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <audio_program/source/mt_audio_clap_program.h>
#include <kick_clap/Discovery.h>
#include <iostream>

// Runs on the application's message thread against its real manager and graph.
// The Python supervisor supplies an isolated data root and trusted test fixture.
inline juce::String runKickAppSmoke() {
    // Exercise lazy shared controls even when their dialogs were never opened.
    ui::KickVirtualKeyBoard::getWithText("Smoke");
    ui::IPv4Editor::Instance(); ui::NoteSelectorEditor::Instance();
    kick_ui::KSystemResources::Instance();
    { ui::KickNumPad<double> pad(0, 1, .5); }
    const auto path = juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_CLAP_PATH", {});
    if (path.isEmpty()) return {};
    const auto catalog = kick::clap::inspect(path.toStdString());
    const auto descriptor = std::find_if(catalog.plugins.begin(), catalog.plugins.end(), [](const auto& d) { return d.id == "org.kick.test.synth"; });
    if (descriptor == catalog.plugins.end()) return "Fixture instrument not found";
    auto& api = mt::getAudioInterface();
    auto file = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("smoke/Project.kickPrj");
    if (!file.getParentDirectory().createDirectory()) return "Cannot create smoke project directory";
    if (!api.loadProject(file)) return "Cannot create project";
    const auto baseline = api.getNumPrograms();
    const auto index = api.createProgram(mt::MTProgramType::Plugin, "SmokeCLAP");
    auto* program = dynamic_cast<mt::MTClapProgram*>(api.getProgram(index));
    if (!program) return "Cannot create real CLAP program";
    {
        auto session = program->getPluginSession();
        session->prepare(48000, 480); session->service(); session->select(*descriptor);
        if (!session->host().loaded()) return session->status();
        if (!session->host().setParameter(7, .7)) return "Cannot edit fixture gain";
        session->assignMacro(0, 7);
        // Flush parameter events through the actual program's MIDI entry point.
        juce::AudioBuffer<float> block(2, 480);
        program->noteOn(60, 100); session->render(block); program->noteOff(60);
        if (block.getMagnitude(0, 480) <= 0) return "Real CLAP program produced no audio";
    }
    // duplicateProgram must snapshot the live plugin before copying its tree.
    if (api.duplicateProgram(index) != 0 || api.getNumPrograms() != baseline + 2) return "Cannot duplicate CLAP program";
    if (api.getSelectedProgram() != api.getNumPrograms() - 1) return "Duplicate did not select the new program";
    const auto checkProgram = [&](const juce::String& name) -> bool {
        auto* p = dynamic_cast<mt::MTClapProgram*>(api.getProgram(name));
        if (!p) return false;
        auto session = p->getPluginSession();
        session->prepare(48000, 480); session->service();
        return session->host().loaded() && session->macro(0) == 7 && std::abs(session->host().parameterValue(7) - .7) < 1e-9;
    };
    if (!checkProgram("SmokeCLAP_copy")) return "Duplicate lost live plugin state";
    if (!api.saveProject() || !file.existsAsFile()) return "Cannot save project: " + api.getProjectSaveError();
    if (!api.closeProject() || !api.loadProject(file)) return "Cannot close/reopen project";
    if (api.getNumPrograms() != baseline + 2 || !checkProgram("SmokeCLAP") || !checkProgram("SmokeCLAP_copy")) return "Reopened project lost CLAP state";
    if (!api.saveProject() || !api.closeProject()) return "Cannot save/close restored project";
    std::cout << "PASS: full KICK app creates, plays, duplicates, saves and reopens CLAP programs with gain and macro state intact\n";
    return {};
}
