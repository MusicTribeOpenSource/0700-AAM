/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "kick_xml_manager.h"

namespace {
// Never hold the document lock while waiting for the message thread. Project
// loading runs on a worker, but CLAP snapshots must run on the owning UI thread.
void onPersistenceThread(std::function<void()> action) {
    auto* manager = juce::MessageManager::getInstanceWithoutCreating();
    if (!manager || manager->isThisTheMessageThread()) action();
    else manager->callFunctionOnMessageThread([](void* context) -> void* {
        (*static_cast<std::function<void()>*>(context))();
        return nullptr;
    }, &action);
}
}

namespace mt {
KickXML::KickXML(juce::Identifier ID, const juce::String& FileExtension, const juce::String& Version)
    : id(ID), extn(FileExtension), version(Version) {}
KickXML::~KickXML() {
    // Owners with live-state hooks must close explicitly while their programs
    // still exist. Preferences retain their original destructor-save behavior.
    timer.stopTimer(); cancelPendingUpdate();
    if (!prepareSave) save();
    vt.removeListener(this);
}
bool KickXML::autoSave{true};
int KickXML::autoSaveTimeOutInMS{1500};
void KickXML::valueTreeChanged() {
    dirty.store(true);
    if (!saving.load()) triggerAsyncUpdate();
}
void KickXML::handleAsyncUpdate() {
    if (autosaveDue.exchange(false)) {
        save(SaveReason::Autosave);
        return;
    }
    if (onDirtyChanged) onDirtyChanged(dirty.load());
    if (!dirty.load() || saving.load()) return;
    const bool preferences = vt.hasType("PREFERENCES");
    if ((preferences || autoSave) && !timer.isTimerRunning())
        timer.startTimer(preferences ? prefAutoSaveTimeOutInMS : juce::jmax(1, autoSaveTimeOutInMS));
}
juce::ValueTree KickXML::loadOrCreate(const juce::File& requested) {
    juce::ValueTree result;
    onPersistenceThread([&] {
        if (!close()) return;
        const juce::ScopedLock guard(lock);
        const bool isFile = requested.existsAsFile() || requested.hasFileExtension(extn);
        const auto directory = isFile ? requested.getParentDirectory() : requested;
        const auto file = isFile ? requested : directory.getChildFile(directory.getFileName() + extn);
        juce::ValueTree loaded;
        const bool existing = file.existsAsFile() && file.getSize() != 0;
        if (existing) {
            loaded = juce::ValueTree::fromXml(file.loadFileAsString());
            if (!loaded.isValid()) { failSave("Project XML could not be read", SaveReason::Explicit); return; }
        } else {
            if (directory.createDirectory().failed()) { failSave("Cannot create project directory", SaveReason::Explicit); return; }
            loaded = juce::ValueTree{id};
            loaded.setProperty("manufacturer", __manufacturer, nullptr);
            loaded.setProperty("version", version, nullptr);
        }
        vt.removeListener(this);
        vt = loaded;
        xmlFileDir = directory; xmlFile = file; directoryText = directory.getFullPathName();
        dirty.store(!existing);
        vt.addListener(this);
        if (dirty.load() && !saveOnMessageThread(SaveReason::Explicit)) return;
        onLoad(); result = vt;
    });
    return result;
}
bool KickXML::failSave(const juce::String& message, SaveReason reason) {
    const bool changed = lastSaveError != message;
    lastSaveError = message; dirty.store(true);
    if (onDirtyChanged) onDirtyChanged(true);
    // Do not repeatedly open the same error dialog on every autosave retry.
    if (onSaveFailure && (changed || reason != SaveReason::Autosave)) onSaveFailure(message);
    // saveOnMessageThread stops the timer before attempting the transaction.
    // Keep retrying a failed autosave even if no further edit changes the tree.
    if (reason == SaveReason::Autosave) triggerAsyncUpdate();
    return false;
}
bool KickXML::saveOnMessageThread(SaveReason reason) {
    if (xmlFile == juce::File{}) return true;
    if (saving.exchange(true)) return failSave("Project save is already in progress", reason);
    struct Transaction { std::atomic<bool>& flag; ~Transaction() { flag.store(false); } } transaction{saving};
    timer.stopTimer(); cancelPendingUpdate(); autosaveDue.store(false);
    // Snapshot first; neither a failed snapshot nor a failed disk write may
    // replace the previous recoverable project document.
    if (prepareSave) {
        const auto prepared = prepareSave();
        if (prepared.failed()) return failSave(prepared.getErrorMessage(), reason);
    }
    const juce::ScopedLock guard(lock);
    if (!vt.isValid()) return failSave("No valid project document to save", reason);
    const auto xml = vt.toXmlString();
    if (xmlFile.loadFileAsString() != xml) {
        juce::TemporaryFile temporary(xmlFile);
        bool written = false;
        {
            // JUCE's buffered flush can discard a short write without setting
            // its status. Write directly so writeText reports incomplete data.
            auto output = temporary.getFile().createOutputStream(0);
            if (output && output->openedOk()) {
                written = output->writeText(xml, false, false, nullptr);
                output->flush();
                written = written && output->getStatus().wasOk();
            }
        }
        if (!written || !temporary.overwriteTargetFileWithTemporary())
            return failSave("Cannot write project: " + xmlFile.getFullPathName(), reason);
    }
    lastSaveError.clear(); dirty.store(false);
    if (onDirtyChanged) onDirtyChanged(false);
    return true;
}
bool KickXML::save(SaveReason reason) {
    bool result = false;
    onPersistenceThread([&] { result = saveOnMessageThread(reason); });
    return result;
}
bool KickXML::close() {
    bool result = false;
    onPersistenceThread([&] {
        if (!saveOnMessageThread(SaveReason::Close)) return;
        detachWithoutSaving(); result = true;
    });
    return result;
}
void KickXML::detachWithoutSaving() {
    timer.stopTimer(); cancelPendingUpdate(); autosaveDue.store(false);
    const juce::ScopedLock guard(lock);
    xmlFile = juce::File{}; xmlFileDir = juce::File{}; directoryText.clear();
}
} // namespace mt
