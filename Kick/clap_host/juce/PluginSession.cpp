// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginSession.h"
#include <clap/ext/params.h>
#include <cmath>

namespace kick::clap {
std::shared_ptr<PluginSession> PluginSession::create(juce::ValueTree program) {
    return {new PluginSession(program), [](PluginSession* session) {
        // Never destroy a plugin or its dynamic library in the audio callback.
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) delete session;
        else juce::MessageManager::callAsync([session] { delete session; });
    }};
}
PluginSession::PluginSession(juce::ValueTree program)
    : state(program.getOrCreateChildWithName("CLAP", nullptr)) {
    state.addListener(this);
    startTimerHz(30);
}
PluginSession::~PluginSession() { stopTimer(); state.removeListener(this); processor.unload(); }
void PluginSession::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& key) {
    if (tree == state && (key == juce::Identifier("path") || key == juce::Identifier("pluginId"))) reload.store(true);
}
void PluginSession::select(const Descriptor& descriptor) {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (processor.loaded() && !save()) return;
    // A failed replacement retains the old saved identity and opaque state.
    auto rate = requestedRate.load(); auto frames = requestedFrames.load();
    if (rate <= 0) { statusMessage = "Audio engine is not prepared"; return; }
    const bool samePlugin = state.getProperty("pluginId").toString() == juce::String(descriptor.id);
    std::vector<uint8_t> restore;
    if (samePlugin) {
        juce::MemoryBlock snapshot;
        const auto text = state.getProperty("state").toString();
        if (text.isNotEmpty() && (!snapshot.fromBase64Encoding(text) || snapshot.getSize() > Host::maxStateBytes)) {
            statusMessage = "Saved plugin state is invalid"; return;
        }
        restore.resize(snapshot.getSize());
        if (!restore.empty()) std::memcpy(restore.data(), snapshot.getData(), restore.size());
    }
    if (!processor.load(descriptor, rate, frames, restore)) { statusMessage = processor.error(); reload.store(true); return; }
    if (!samePlugin) state.removeProperty("state", nullptr);
    state.setProperty("path", juce::String(descriptor.path), nullptr);
    state.setProperty("pluginId", juce::String(descriptor.id), nullptr);
    state.setProperty("name", juce::String(descriptor.name), nullptr);
    state.setProperty("vendor", juce::String(descriptor.vendor), nullptr);
    if (!samePlugin) for (int i = 0; i < 4; ++i) state.removeProperty("macro" + juce::String(i), nullptr);
    loadedPath = descriptor.path; loadedId = descriptor.id;
    preparedRate = rate; preparedFrames = frames; renderRate.store(rate); reload.store(false);
    statusMessage = processor.supportsState() ? "Ready" : "Ready - this plugin cannot save its state";
    save();
}
void PluginSession::remove() {
    processor.unload();
    state.removeAllProperties(nullptr);
    loadedPath.clear(); loadedId.clear(); renderRate.store(0); reload.store(false);
    statusMessage = "Choose a CLAP instrument";
}
void PluginSession::prepare(double rate, uint32_t frames) noexcept {
    requestedFrames.store(std::max(1u, frames)); requestedRate.store(rate);
}
void PluginSession::service() {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    const auto rate = requestedRate.load(); const auto frames = requestedFrames.load();
    if (rate <= 0 || !std::isfinite(rate)) return;
    if (reload.exchange(false)) {
        const auto path = state.getProperty("path").toString();
        const auto id = state.getProperty("pluginId").toString();
        if (path.isNotEmpty() && id.isNotEmpty()) {
            juce::MemoryBlock bytes;
            const auto encoded = state.getProperty("state").toString();
            if (encoded.length() > static_cast<int>(Host::maxStateBytes * 2) ||
                (encoded.isNotEmpty() && !bytes.fromBase64Encoding(encoded))) {
                statusMessage = "Saved plugin state is invalid"; processor.unload(); return;
            }
            Descriptor descriptor; descriptor.path = path.toStdString(); descriptor.id = id.toStdString();
            std::vector<uint8_t> snapshot(bytes.getSize());
            if (!snapshot.empty()) std::memcpy(snapshot.data(), bytes.getData(), snapshot.size());
            if (!processor.load(descriptor, rate, frames, snapshot)) {
                statusMessage = "Missing or unavailable plugin: " + juce::String(processor.error()); renderRate.store(0);
            } else {
                loadedPath = path; loadedId = id; preparedRate = rate; preparedFrames = frames; renderRate.store(rate);
                statusMessage = processor.supportsState() ? "Ready" : "Ready - this plugin cannot save its state";
            }
        }
    }
    if (processor.loaded() && (preparedRate != rate || preparedFrames != frames)) {
        renderRate.store(0);
        if (processor.reconfigure(rate, frames)) { preparedRate = rate; preparedFrames = frames; renderRate.store(rate); }
        else statusMessage = processor.error();
    }
    processor.service();
    // Output is bounded in the core. Drain it even when the editor is closed,
    // while retaining parameter telemetry and dirty state already applied there.
    Event event; while (processor.popOutput(event)) {}
    if (processor.faulted()) statusMessage = "Plugin stopped after a processing error";
    // Coalesce edits into a bounded 250 ms window on the owning message thread.
    // Mark the document dirty before the opaque snapshot is available so an
    // autosave/close barrier cannot serialize the previous sound unnoticed.
    const auto now = std::chrono::steady_clock::now();
    if (processor.loaded() && processor.stateDirty()) {
        state.setProperty("snapshotPending", true, nullptr);
        if (snapshotDue == std::chrono::steady_clock::time_point{}) snapshotDue = now + std::chrono::milliseconds(250);
        if (now >= snapshotDue) {
            if (save()) snapshotDue = {};
            else snapshotDue = now + std::chrono::seconds(2);
        }
    } else snapshotDue = {};
}
bool PluginSession::save() {
    if (!processor.loaded()) return true; // Preserve missing plugin state verbatim.
    if (!processor.supportsState()) { statusMessage = "Plugin does not support project state saving"; return false; }
    std::vector<uint8_t> bytes;
    if (!processor.saveState(bytes)) { statusMessage = processor.error(); return false; }
    juce::MemoryBlock snapshot(bytes.data(), bytes.size());
    state.setProperty("state", snapshot.toBase64Encoding(), nullptr);
    state.setProperty("path", loadedPath, nullptr); state.setProperty("pluginId", loadedId, nullptr);
    state.removeProperty("snapshotPending", nullptr);
    snapshotDue = {};
    statusMessage = processor.faulted() ? "Plugin stopped after a processing error" : "Ready";
    return true;
}
void PluginSession::render(juce::AudioBuffer<float>& buffer) noexcept {
    const auto rate = renderRate.load();
    if (rate == 0 || rate != requestedRate.load()) { buffer.clear(); return; }
    clap_event_transport_t transport{};
    transport.header = {sizeof(transport), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0};
    transport.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE;
    // Bounded seqlock read: keep the previous coherent snapshot if a producer
    // is publishing. The audio callback neither waits nor reads a mixed tuple.
    for (int attempt = 0; attempt < 3; ++attempt) {
        const auto revision = transportRevision.load();
        if (revision & 1u) continue;
        const auto bpm = tempo.load();
        const auto position = beats.load();
        const auto running = playing.load();
        if (revision != transportRevision.load()) continue;
        if (revision != audioTransportRevision) {
            audioBeats = position; audioTempo = bpm; audioPlaying = running; audioTransportRevision = revision;
        }
        break;
    }
    transport.tempo = audioTempo;
    if (audioPlaying) transport.flags |= CLAP_TRANSPORT_IS_PLAYING;
    transport.song_pos_beats = static_cast<clap_beattime>(audioBeats * CLAP_BEATTIME_FACTOR);
    processor.process(nullptr, 0, buffer.getArrayOfWritePointers(), static_cast<uint32_t>(buffer.getNumChannels()),
                      static_cast<uint32_t>(buffer.getNumSamples()), nullptr, 0, &transport);
    if (audioPlaying) audioBeats += buffer.getNumSamples() / rate * transport.tempo / 60.;
}
void PluginSession::midi(const juce::MidiMessage& message) noexcept {
    if (message.isAllNotesOff() || message.isAllSoundOff()) { processor.panic(); return; }
    const auto count = message.getRawDataSize();
    if (count < 1 || count > 3 || message.isSysEx()) return;
    const auto* data = message.getRawData();
    processor.enqueue(Event::midi(data[0], count > 1 ? data[1] : 0, count > 2 ? data[2] : 0));
}
void PluginSession::setTransport(double bpm, double position, bool isPlaying) noexcept {
    if (!std::isfinite(bpm) || bpm <= 0 || !std::isfinite(position)) return;
    const std::lock_guard<std::mutex> writer(transportWriters);
    transportRevision.fetch_add(1);
    tempo.store(bpm); beats.store(position); playing.store(isPlaying);
    transportRevision.fetch_add(1);
}
clap_id PluginSession::macro(int index) const {
    if (index < 0 || index >= 4) return CLAP_INVALID_ID;
    return static_cast<clap_id>(static_cast<juce::int64>(state.getProperty("macro" + juce::String(index), static_cast<juce::int64>(CLAP_INVALID_ID))));
}
void PluginSession::assignMacro(int index, clap_id id) {
    if (index >= 0 && index < 4) state.setProperty("macro" + juce::String(index), static_cast<juce::int64>(id), nullptr);
}
void PluginSession::adjustMacro(int index, double delta) {
    if (!std::isfinite(delta) || delta == 0) return;
    const auto id = macro(index);
    for (const auto& parameter : processor.parameters()) if (parameter.id == id) {
        const auto amount = (parameter.flags & CLAP_PARAM_IS_STEPPED)
            ? std::copysign(std::max(1., std::round(std::abs(delta) * 127.)), delta)
            : delta * (parameter.maximum - parameter.minimum);
        processor.setParameter(id, processor.parameterValue(id) + amount); break;
    }
}
} // namespace kick::clap
