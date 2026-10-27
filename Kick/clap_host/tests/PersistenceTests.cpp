// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginSession.h"
#include "../../modules/engine/project/kick_xml_manager.h"
#include <kick_clap/Discovery.h>
#include <atomic>
#include <thread>
#include <iostream>
using namespace kick::clap;
static void check(bool ok, const char* why) { if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
static void pump(int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }
int main(int argc, char** argv) {
    check(argc == 2, "fixture path");
    juce::ScopedJuceInitialiser_GUI gui;
    const auto catalog = inspect(argv[1]); check(catalog.plugins.size() == 12, "fixtures");
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("kick-persistence", "", false);
    check(directory.createDirectory().wasOk(), "temporary project directory");
    const auto file = directory.getChildFile("project.xml");
    mt::KickXML document("ROOT", ".xml", "1");
    document.setAutoSave(true); document.setAutoSaveTime(20);
    std::shared_ptr<PluginSession> session;
    bool preparedOnMain = false; int preparationCalls = 0; int errors = 0; bool dirtyNotice = false;
    document.prepareSave = [&] {
        ++preparationCalls; preparedOnMain = juce::MessageManager::getInstance()->isThisTheMessageThread();
        return !session || session->save() ? juce::Result::ok() : juce::Result::fail("Program 'Lead' (fixture): " + session->status());
    };
    document.onSaveFailure = [&](const juce::String& reason) { ++errors; check(reason.contains("Lead"), "save error identifies program"); };
    document.onDirtyChanged = [&](bool dirty) { dirtyNotice = dirty; };
    auto tree = document.loadOrCreate(file); check(tree.isValid(), "create actual KickXML document");
    auto attach = [&](const Descriptor& plugin) {
        session = PluginSession::create(document.getState().getOrCreateChildWithName("PROGRAM", nullptr));
        session->prepare(48000, 128); session->service(); session->select(plugin);
        check(session->host().loaded(), "attach plugin");
    };
    auto checkDiskGain = [&](double expected) {
        const auto diskTree = juce::ValueTree::fromXml(file.loadFileAsString());
        auto diskSession = PluginSession::create(diskTree.getChildWithName("PROGRAM"));
        diskSession->prepare(48000, 128); diskSession->service();
        check(diskSession->host().loaded() && std::abs(diskSession->host().parameterValue(7) - expected) < 1e-9, "disk already contains current opaque state before any close/save/reopen");
    };
    auto reopen = [&] {
        session.reset();
        check(document.loadOrCreate(file).isValid(), "reopen saved project");
        session = PluginSession::create(document.getState().getChildWithName("PROGRAM"));
        session->prepare(48000, 128); session->service();
        check(session->host().loaded(), "restore plugin from disk");
    };
    juce::AudioBuffer<float> block(2, 128);
    attach(catalog.plugins[0]); check(document.save(), "initial save");
    check(session->host().setParameter(7, .7), "edit live sound before autosave"); session->render(block); session->service();
    // No explicit session/document save. Service's dirty marker starts KickXML's
    // real timer, and its shared barrier must snapshot the current opaque sound.
    pump(180); check(!document.isDirty() && preparedOnMain, "autosave completes on owning thread");
    checkDiskGain(.7);
    reopen(); check(std::abs(session->host().parameterValue(7) - .7) < 1e-9, "autosave then reopen retains live sound");
    session->host().setParameter(7, .6); session->render(block);
    check(document.close(), "close snapshots before programs are destroyed");
    checkDiskGain(.6);
    reopen(); check(std::abs(session->host().parameterValue(7) - .6) < 1e-9, "edit then close then reopen retains live sound");
    document.setAutoSave(false);
    session->host().setParameter(7, .55); session->render(block);
    pump(400);
    auto snapshotTree = juce::ValueTree::fromXml(document.getState().toXmlString());
    auto snapshot = PluginSession::create(snapshotTree.getChildWithName("PROGRAM"));
    snapshot->prepare(48000, 128); snapshot->service();
    check(std::abs(snapshot->host().parameterValue(7) - .55) < 1e-9, "bounded background snapshot updates opaque document state"); snapshot.reset();
    std::atomic<bool> done{false}; bool workerResult = false;
    std::thread worker([&] { workerResult = document.save(); done.store(true); });
    while (!done.load()) pump(10);
    worker.join(); check(workerResult && preparedOnMain, "worker save marshals preparation to message thread");
    session->remove(); session->select(catalog.plugins[10]); check(document.save(), "baseline state for failing fixture");
    const auto baselineDisk = file.loadFileAsString();
    const auto baselineOpaque = session->stateTree().getProperty("state");
    session->host().setParameter(7, .7); session->render(block);
    check(!document.save() && document.isDirty() && dirtyNotice && errors > 0, "failed explicit save stays visibly dirty");
    check(file.loadFileAsString() == baselineDisk && session->stateTree().getProperty("state") == baselineOpaque, "partial failed state save preserves previous disk and opaque snapshot");
    const auto beforeAutosave = preparationCalls;
    document.setAutoSave(true); session->service(); pump(100);
    check(preparationCalls > beforeAutosave, "failed autosave actually crossed the live-state barrier");
    check(document.isDirty() && file.loadFileAsString() == baselineDisk, "failed autosave preserves previous file");
    check(!document.close() && document.getDirectory().isNotEmpty(), "failed close retains active project");
    check(!document.loadOrCreate(directory.getChildFile("other.xml")).isValid() && !directory.getChildFile("other.xml").exists(), "failed close prevents project switch");
    session->host().setParameter(7, .6); session->render(block);
    check(document.save() && !document.isDirty() && document.getLastSaveError().isEmpty(), "retry can recover and clear dirty/error");
    checkDiskGain(.6);
    reopen(); check(std::abs(session->host().parameterValue(7) - .6) < 1e-9, "successful retry persisted live state");
    session->remove(); session->select(catalog.plugins[9]);
    const auto previousFile = file.loadFileAsString();
    check(!document.save() && document.getLastSaveError().contains("does not support"), "unsupported opaque state is explicit failure");
    check(file.loadFileAsString() == previousFile && document.isDirty(), "unsupported state preserves recoverable project");
    session->remove(); check(document.close(), "close after removing unsupported plugin"); session.reset();
    document.setAutoSave(false);
    // Force an I/O failure without relying on user/root permission behavior:
    // keep the previous document in a moved directory and block its old parent
    // path with a regular file. The failed transaction must leave it intact.
    document.onSaveFailure = [&](const juce::String&) { ++errors; };
    const auto ioDirectory = directory.getChildFile("io");
    const auto ioFile = ioDirectory.getChildFile("project.xml");
    check(document.loadOrCreate(ioFile).isValid(), "create document for disk failure");
    const auto originalXml = ioFile.loadFileAsString();
    document.getState().setProperty("edited", true, nullptr);
    const auto heldDirectory = directory.getChildFile("held");
    check(ioDirectory.moveFileTo(heldDirectory) && ioDirectory.replaceWithText("blocked parent"), "simulate unavailable project directory");
    check(!document.save() && document.isDirty() && document.getLastSaveError().contains("Cannot write"), "disk failure propagates and stays dirty");
    check(heldDirectory.getChildFile("project.xml").loadFileAsString() == originalXml, "failed write preserves previous document bytes");
    check(ioDirectory.deleteFile() && heldDirectory.moveFileTo(ioDirectory), "restore directory");
    check(document.save() && document.close(), "disk failure retry succeeds");
    directory.deleteRecursively();
    std::cout << "PASS: actual KickXML autosave, close, worker save, bounded snapshots, failed/unsupported state, dirty/error reporting and retry\n";
}
