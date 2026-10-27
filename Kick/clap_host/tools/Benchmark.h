// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <kick_clap/Host.h>
#include <atomic>
#include <chrono>

namespace kick::clap::bench {
struct Options {
    std::string path, id, output, driver, device;
    double sampleRate = 48000, seconds = 10, warmup = 1, automationHz = 30;
    uint32_t frames = 128, instances = 1, notes = 8, snapshotMs = 0;
    clap_id parameter = CLAP_INVALID_ID;
    bool audio = false, audible = false, listDevices = false, help = false;
};
Options parse(int argc, const char* const* argv);
const char* usage();
std::string quote(const std::string&);

// One render-thread writer; read only after the callback is removed. Histogram
// quantiles are upper bounds in 1 percentage-point load bins, not exact timings.
struct Timing {
    uint64_t calls = 0, frames = 0, deadlineMisses = 0;
    double totalUs = 0, maximumUs = 0, maximumLoad = 0;
    std::array<uint64_t, 1024> loadHistogram{};
    void record(double microseconds, uint32_t frameCount, double rate) noexcept;
    double percentileLoadUpper(double percentile) const noexcept;
};
struct DeviceInfo {
    std::string driver, name;
    int inputLatency = 0, outputLatency = 0;
    int nativeXruns = -1;
};
class Workload {
public:
    explicit Workload(Options options);
    bool load(); // main/control thread, exact saved descriptor ID
    void control(double elapsedSeconds); // service, notes, knob edits and snapshots
    void beginMeasurement(); // call once, after warmup
    void render(float* const* outputs, uint32_t channels, uint32_t frames) noexcept;
    uint64_t totalFrames() const noexcept { return progress.load(); }
    uint64_t measuredFrames() const noexcept { return captured.load(); }
    void fail(std::string reason) { message = std::move(reason); failed.store(true); }
    bool faulted() const noexcept { return failed.load(); }
    const std::string& error() const noexcept { return message; }
    const Options& options() const noexcept { return config; }
    std::string report(const DeviceInfo* = nullptr) const; // after stopping audio
    const Timing& timing() const noexcept { return measured; }
private:
    struct Instance {
        std::unique_ptr<Host> host;
        std::vector<float> buffer;
        std::array<float*, 2> channels{};
        Statistics baseline;
    };
    Options config;
    Descriptor descriptor;
    std::vector<Instance> instances;
    static constexpr uint32_t capacity = 32768;
    std::vector<float> mix;
    std::atomic<bool> recording{false}, failed{false};
    std::atomic<uint64_t> progress{0}, captured{0};
    Timing measured;
    std::string message;
    uint64_t noteChanges = 0, parameterChanges = 0, transportChanges = 0;
    uint64_t snapshots = 0, snapshotBytes = 0, snapshotFailures = 0;
    double snapshotMaximumUs = 0, snapshotTotalUs = 0, peak = 0, loadUs = 0;
    double nextNotes = 0, nextParameter = 0, nextSnapshot = 0, nextService = 0;
    uint64_t lastTransportStage = UINT64_MAX;
    double audioBeats = 0;
    uint64_t audioStage = UINT64_MAX;
    bool notesHeld = false;
    std::array<int, 32> heldNotes{};
};
void runOffline(Workload&);
void writeReport(const Options&, const std::string&);
} // namespace kick::clap::bench
