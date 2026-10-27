// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Benchmark.h"
#include <kick_clap/Discovery.h>
#include <clap/ext/params.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#ifndef KICK_BENCH_REVISION
#define KICK_BENCH_REVISION "unknown"
#endif
#ifndef KICK_BENCH_BUILD_TYPE
#define KICK_BENCH_BUILD_TYPE "unknown"
#endif
namespace kick::clap::bench {
namespace {
double real(const std::string& value, double minimum, double maximum, const std::string& key) {
    size_t end = 0; double number = 0;
    try { number = std::stod(value, &end); } catch (...) { throw std::runtime_error("Invalid " + key); }
    if (end != value.size() || !std::isfinite(number) || number < minimum || number > maximum)
        throw std::runtime_error("Invalid " + key);
    return number;
}
uint32_t integer(const std::string& value, uint32_t minimum, uint32_t maximum, const std::string& key) {
    const auto number = real(value, minimum, maximum, key);
    if (number != std::trunc(number)) throw std::runtime_error("Expected integer " + key);
    return static_cast<uint32_t>(number);
}
const char* architecture() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__arm__)
    return "arm";
#else
    return "other";
#endif
}
const char* system() {
#ifdef __linux__
    return "linux";
#elif defined(__APPLE__)
    return "macos";
#elif defined(_WIN32)
    return "windows";
#else
    return "other";
#endif
}
std::string finiteNumber(double number) {
    if (!std::isfinite(number)) return "null";
    std::ostringstream out; out << std::setprecision(12) << number; return out.str();
}
}
const char* usage() {
    return "KICK CLAP benchmark\n"
           "  kick-clap-bench --plugin PATH --id ID [options]\n"
           "  --rate 48000 --frames 128 --instances 1 --notes 8\n"
           "  --seconds 10 --warmup 1 --parameter ID --automation-hz 30\n"
           "  --snapshot-ms 250 --output report.json\n"
           "Optional kick-clap-device-bench additionally supports:\n"
           "  --audio --driver ALSA --device NAME [--audible]\n"
           "  --list-devices\n"
           "Offline mode runs as fast as possible; it does not measure device xruns.\n"
           "Device output is muted unless --audible is specified.\n";
}
Options parse(int argc, const char* const* argv) {
    Options out;
    for (int i = 1; i < argc; ++i) {
        const std::string key(argv[i]);
        if (key == "--help") { out.help = true; continue; }
        if (key == "--audio") { out.audio = true; continue; }
        if (key == "--audible") { out.audible = true; continue; }
        if (key == "--list-devices") { out.listDevices = true; continue; }
        if (i + 1 >= argc) throw std::runtime_error("Missing value for " + key);
        const std::string value(argv[++i]);
        if (key == "--plugin") out.path = value;
        else if (key == "--id") out.id = value;
        else if (key == "--output") out.output = value;
        else if (key == "--driver") out.driver = value;
        else if (key == "--device") out.device = value;
        else if (key == "--rate") out.sampleRate = real(value, 8000, 384000, key);
        else if (key == "--frames") out.frames = integer(value, 16, 32768, key);
        else if (key == "--instances") out.instances = integer(value, 1, 64, key);
        else if (key == "--notes") out.notes = integer(value, 1, 32, key);
        else if (key == "--seconds") out.seconds = real(value, .01, 3600, key);
        else if (key == "--warmup") out.warmup = real(value, 0, 60, key);
        else if (key == "--parameter") out.parameter = integer(value, 0, CLAP_INVALID_ID - 1, key);
        else if (key == "--automation-hz") out.automationHz = real(value, .1, 1000, key);
        else if (key == "--snapshot-ms") out.snapshotMs = integer(value, 0, 60000, key);
        else throw std::runtime_error("Unknown option " + key);
    }
    if (!out.help && !out.listDevices && (out.path.empty() || out.id.empty())) throw std::runtime_error("Provide --plugin and --id");
    if (!out.audio && (out.audible || !out.driver.empty() || !out.device.empty())) throw std::runtime_error("Device options require --audio");
    return out;
}
std::string quote(const std::string& text) {
    std::string result = "\"";
    constexpr const char* digits = "0123456789abcdef";
    for (const auto character : text) {
        const auto c = static_cast<unsigned char>(character);
        if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
        else if (c < 32) { result += "\\u00"; result += digits[c >> 4]; result += digits[c & 15]; }
        else result += static_cast<char>(c);
    }
    return result + '"';
}
void Timing::record(double us, uint32_t count, double rate) noexcept {
    ++calls; frames += count; totalUs += us; maximumUs = std::max(maximumUs, us);
    const auto load = us / (count / rate * 1e6);
    maximumLoad = std::max(maximumLoad, load); if (load > 1) ++deadlineMisses;
    const auto bin = load >= 10.23 ? size_t(1023) : static_cast<size_t>(std::max(0., std::floor(load * 100)));
    ++loadHistogram[bin];
}
double Timing::percentileLoadUpper(double q) const noexcept {
    if (!calls) return 0;
    const auto target = static_cast<uint64_t>(std::ceil(std::clamp(q, 0., 1.) * calls));
    uint64_t accumulated = 0;
    for (size_t i = 0; i < loadHistogram.size(); ++i) {
        accumulated += loadHistogram[i];
        if (accumulated >= std::max(uint64_t(1), target))
            return i == 1023 ? std::numeric_limits<double>::infinity() : (i + 1) / 100.;
    }
    return std::numeric_limits<double>::infinity();
}
Workload::Workload(Options options) : config(std::move(options)), mix(capacity * 2) {}
bool Workload::load() {
    struct LoadTiming {
        double& us; std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        ~LoadTiming() { us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count(); }
    } loadTiming{loadUs};
    const auto catalog = inspect(config.path);
    for (const auto& failure : catalog.failures) message += failure.message + "\n";
    if (!message.empty()) return false;
    auto found = std::find_if(catalog.plugins.begin(), catalog.plugins.end(), [&](const auto& d) { return d.id == config.id; });
    if (found == catalog.plugins.end()) { message = "Exact plugin ID was not found"; return false; }
    descriptor = *found;
    if (std::find(descriptor.features.begin(), descriptor.features.end(), CLAP_PLUGIN_FEATURE_INSTRUMENT) == descriptor.features.end()) {
        message = "This workload requires an instrument"; return false;
    }
    instances.reserve(config.instances);
    for (uint32_t i = 0; i < config.instances; ++i) {
        Instance instance;
        instance.host = std::make_unique<Host>();
        if (!instance.host->load(descriptor, config.sampleRate, config.frames)) { message = instance.host->error(); return false; }
        if (config.snapshotMs && !instance.host->supportsState()) { message = "Snapshot workload requires opaque state support"; return false; }
        if (config.parameter != CLAP_INVALID_ID) {
            const auto& params = instance.host->parameters();
            const auto parameter = std::find_if(params.begin(), params.end(), [&](const auto& p) { return p.id == config.parameter; });
            if (parameter == params.end() || (parameter->flags & CLAP_PARAM_IS_READONLY)) { message = "Automation parameter is missing or read-only"; return false; }
        }
        instance.buffer.resize(capacity * 2);
        instance.channels = {instance.buffer.data(), instance.buffer.data() + capacity};
        instances.push_back(std::move(instance));
    }
    nextSnapshot = config.snapshotMs ? config.snapshotMs / 1000. : std::numeric_limits<double>::infinity();
    return true;
}
void Workload::beginMeasurement() {
    for (auto& instance : instances) instance.baseline = instance.host->statistics();
    noteChanges = parameterChanges = transportChanges = snapshots = snapshotBytes = snapshotFailures = 0;
    snapshotMaximumUs = snapshotTotalUs = 0;
    recording.store(true);
}
void Workload::control(double elapsed) {
    const bool measuring = recording.load();
    if (elapsed >= nextService) {
        for (auto& instance : instances) { instance.host->service(); Event event; while (instance.host->popOutput(event)) {} }
        nextService = elapsed + 1. / 200;
    }
    if (elapsed >= nextNotes) {
        for (auto& instance : instances) {
            for (uint32_t n = 0; n < config.notes; ++n) {
                if (notesHeld) instance.host->enqueue(Event::note(false, heldNotes[n], 0));
                const auto key = 36 + static_cast<int>(n) + (static_cast<int>(elapsed * 2) % 12);
                instance.host->enqueue(Event::note(true, key, .7));
            }
        }
        for (uint32_t n = 0; n < config.notes; ++n) heldNotes[n] = 36 + static_cast<int>(n) + (static_cast<int>(elapsed * 2) % 12);
        notesHeld = true; nextNotes = elapsed + .5; if (measuring) ++noteChanges;
    }
    if (config.parameter != CLAP_INVALID_ID && elapsed >= nextParameter) {
        for (auto& instance : instances) for (const auto& parameter : instance.host->parameters()) if (parameter.id == config.parameter) {
            const auto normalized = .5 + .25 * std::sin(elapsed * 3);
            auto value = parameter.minimum + normalized * (parameter.maximum - parameter.minimum);
            if (parameter.flags & CLAP_PARAM_IS_STEPPED) value = std::round(value);
            if (!instance.host->setParameter(parameter.id, value)) { message = "Parameter event could not be queued"; failed.store(true); }
            break;
        }
        nextParameter = elapsed + 1. / config.automationHz; if (measuring) ++parameterChanges;
    }
    const auto stage = static_cast<uint64_t>(elapsed * 2);
    if (stage != lastTransportStage) { if (measuring) ++transportChanges; lastTransportStage = stage; }
    if (elapsed >= nextSnapshot) {
        for (auto& instance : instances) {
            std::vector<uint8_t> state;
            const auto start = std::chrono::steady_clock::now();
            const auto ok = instance.host->saveState(state);
            const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
            if (measuring) {
                ++snapshots; snapshotBytes += state.size(); snapshotTotalUs += us; snapshotMaximumUs = std::max(snapshotMaximumUs, us);
                if (!ok) ++snapshotFailures;
            }
            if (!ok) { message = instance.host->error(); failed.store(true); }
        }
        nextSnapshot = elapsed + config.snapshotMs / 1000.;
    }
}
void Workload::render(float* const* outputs, uint32_t channels, uint32_t count) noexcept {
    const auto started = std::chrono::steady_clock::now();
    const bool measure = recording.load();
    if (outputs) for (uint32_t c = 0; c < channels; ++c) if (outputs[c]) std::fill_n(outputs[c], count, 0.f);
    if (!count) return;
    if (count > capacity || instances.size() != config.instances) { failed.store(true); return; }
    std::fill_n(mix.data(), count, 0.f); std::fill_n(mix.data() + capacity, count, 0.f);
    const auto position = progress.load() / config.sampleRate;
    const auto stage = static_cast<uint64_t>(position * 2);
    clap_event_transport_t transport{};
    transport.header = {sizeof(transport), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0};
    transport.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE;
    // Periodic tempo/seek and stopped intervals exercise the host sleep/wake
    // path. This is a synthetic stress timeline, not an automation recorder.
    transport.tempo = (stage / 2) % 2 ? 135 : 120;
    if (stage % 8 != 7) transport.flags |= CLAP_TRANSPORT_IS_PLAYING;
    if (stage != audioStage && stage % 32 == 0) audioBeats = 0;
    audioStage = stage;
    transport.song_pos_beats = static_cast<clap_beattime>(audioBeats * CLAP_BEATTIME_FACTOR);
    if (transport.flags & CLAP_TRANSPORT_IS_PLAYING) audioBeats += count / config.sampleRate * transport.tempo / 60.;
    for (auto& instance : instances) {
        instance.host->process(nullptr, 0, instance.channels.data(), 2, count, nullptr, 0, &transport);
        if (instance.host->faulted()) failed.store(true);
        for (uint32_t c = 0; c < 2; ++c) for (uint32_t n = 0; n < count; ++n)
            mix[c * capacity + n] += instance.channels[c][n] / config.instances;
    }
    for (uint32_t c = 0; c < 2; ++c) for (uint32_t n = 0; n < count; ++n) {
        const auto value = mix[c * capacity + n];
        if (!std::isfinite(value)) { failed.store(true); continue; }
        if (measure) peak = std::max(peak, static_cast<double>(std::abs(value)));
        if (outputs && c < channels && outputs[c] && config.audible) outputs[c][n] = value * .1f;
    }
    if (failed.load() && outputs) for (uint32_t c = 0; c < channels; ++c) if (outputs[c]) std::fill_n(outputs[c], count, 0.f);
    if (measure) {
        measured.record(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count(), count, config.sampleRate);
        captured.fetch_add(count);
    }
    progress.fetch_add(count);
}
std::string Workload::report(const DeviceInfo* device) const {
    uint64_t drops = 0, errors = 0, busy = 0, idle = 0, dspMisses = 0;
    for (const auto& instance : instances) {
        const auto s = instance.host->statistics(); const auto& b = instance.baseline;
        drops += s.droppedEvents - b.droppedEvents; errors += s.processErrors - b.processErrors;
        busy += s.controlSilences - b.controlSilences; idle += s.idleBlocks - b.idleBlocks; dspMisses += s.deadlineMisses - b.deadlineMisses;
    }
    auto reason = message;
    if (failed.load() && reason.empty()) reason = "Plugin processing failed or the device callback exceeded buffer capacity";
    const bool complete = reason.empty() && measured.frames >= static_cast<uint64_t>(std::ceil(config.seconds * config.sampleRate));
    std::ostringstream out;
    out << "{\n\"schema\":1,\"mode\":" << quote(config.audio ? "audio-device" : "offline")
        << ",\"completed\":" << (complete ? "true" : "false") << ",\"error\":" << quote(reason)
        << ",\"revision\":" << quote(KICK_BENCH_REVISION) << ",\"build_type\":" << quote(KICK_BENCH_BUILD_TYPE)
        << ",\"load_us\":" << finiteNumber(loadUs) << ",\"architecture\":" << quote(architecture()) << ",\"system\":" << quote(system())
#ifdef __VERSION__
        << ",\"compiler\":" << quote(__VERSION__)
#endif
        << ",\n\"plugin\":{\"path\":" << quote(config.path) << ",\"id\":" << quote(config.id)
        << ",\"name\":" << quote(descriptor.name) << ",\"vendor\":" << quote(descriptor.vendor) << ",\"version\":" << quote(descriptor.version) << "},\n"
        << "\"settings\":{\"rate\":" << finiteNumber(config.sampleRate) << ",\"frames\":" << config.frames
        << ",\"instances\":" << config.instances << ",\"notes_per_instance\":" << config.notes
        << ",\"seconds\":" << finiteNumber(config.seconds) << ",\"warmup_seconds\":" << finiteNumber(config.warmup)
        << ",\"parameter\":" << (config.parameter == CLAP_INVALID_ID ? "null" : std::to_string(config.parameter))
        << ",\"automation_hz\":" << finiteNumber(config.parameter == CLAP_INVALID_ID ? 0 : config.automationHz)
        << ",\"snapshot_ms\":" << config.snapshotMs << ",\"audible\":" << (config.audible ? "true" : "false") << "},\n"
        << "\"render\":{\"callbacks\":" << measured.calls << ",\"frames\":" << measured.frames
        << ",\"audio_seconds\":" << finiteNumber(measured.frames / config.sampleRate)
        << ",\"mean_us\":" << finiteNumber(measured.calls ? measured.totalUs / measured.calls : 0)
        << ",\"maximum_us\":" << finiteNumber(measured.maximumUs)
        << ",\"mean_load_ratio\":" << finiteNumber(measured.frames ? measured.totalUs / (measured.frames / config.sampleRate * 1e6) : 0)
        << ",\"maximum_load_ratio\":" << finiteNumber(measured.maximumLoad)
        << ",\"p50_load_upper_bound\":" << finiteNumber(measured.percentileLoadUpper(.5))
        << ",\"p95_load_upper_bound\":" << finiteNumber(measured.percentileLoadUpper(.95))
        << ",\"p99_load_upper_bound\":" << finiteNumber(measured.percentileLoadUpper(.99))
        << ",\"deadline_misses\":" << measured.deadlineMisses << ",\"plugin_deadline_misses\":" << dspMisses
        << ",\"control_silences\":" << busy << ",\"idle_blocks\":" << idle << ",\"dropped_events\":" << drops
        << ",\"process_errors\":" << errors << ",\"peak_amplitude\":" << finiteNumber(peak) << "},\n"
        << "\"control\":{\"note_changes\":" << noteChanges << ",\"parameter_changes\":" << parameterChanges
        << ",\"transport_changes\":" << transportChanges << ",\"snapshots\":" << snapshots
        << ",\"snapshot_bytes\":" << snapshotBytes << ",\"snapshot_failures\":" << snapshotFailures
        << ",\"snapshot_mean_us\":" << finiteNumber(snapshots ? snapshotTotalUs / snapshots : 0)
        << ",\"snapshot_maximum_us\":" << finiteNumber(snapshotMaximumUs) << "},\n\"device\":";
    if (device) out << "{\"driver\":" << quote(device->driver) << ",\"name\":" << quote(device->name)
        << ",\"input_latency_samples\":" << device->inputLatency << ",\"output_latency_samples\":" << device->outputLatency
        << ",\"native_xruns\":" << (device->nativeXruns < 0 ? "null" : std::to_string(device->nativeXruns)) << "}";
    else out << "null";
    out << "\n}\n"; return out.str();
}
void runOffline(Workload& workload) {
    const auto& config = workload.options();
    const auto warmupFrames = static_cast<uint64_t>(std::ceil(config.warmup * config.sampleRate));
    const auto target = static_cast<uint64_t>(std::ceil(config.seconds * config.sampleRate));
    while (workload.totalFrames() < warmupFrames && !workload.faulted()) {
        workload.control(workload.totalFrames() / config.sampleRate);
        workload.render(nullptr, 0, static_cast<uint32_t>(std::min(uint64_t(config.frames), warmupFrames - workload.totalFrames())));
    }
    workload.beginMeasurement();
    while (workload.measuredFrames() < target && !workload.faulted()) {
        workload.control(workload.totalFrames() / config.sampleRate);
        if (workload.faulted()) break;
        workload.render(nullptr, 0, static_cast<uint32_t>(std::min(uint64_t(config.frames), target - workload.measuredFrames())));
    }
}
void writeReport(const Options& config, const std::string& report) {
    if (config.output.empty()) { std::cout << report; return; }
    std::ofstream file(config.output, std::ios::binary | std::ios::trunc);
    file << report; file.close();
    if (!file) throw std::runtime_error("Cannot write benchmark report: " + config.output);
}
} // namespace kick::clap::bench
