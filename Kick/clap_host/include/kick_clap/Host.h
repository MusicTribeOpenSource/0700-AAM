// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <clap/clap.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace kick::clap {

struct Descriptor {
    std::string path, id, name, vendor, version, description;
    std::vector<std::string> features;
};

struct Parameter {
    clap_id id = CLAP_INVALID_ID;
    std::string name, module;
    double minimum = 0, maximum = 1, defaultValue = 0;
    uint32_t flags = 0;
};

struct Port {
    clap_id id = CLAP_INVALID_ID;
    std::string name;
    uint32_t channels = 0, flags = 0;
};

// Owning, fixed-capacity event. Pointer-bearing events (SysEx) are deliberately
// rejected: their data cannot safely outlive the caller in the command queue.
struct Event {
    alignas(8) std::array<uint8_t, 128> bytes{};
    const clap_event_header_t* header() const noexcept {
        return reinterpret_cast<const clap_event_header_t*>(bytes.data());
    }
    template<class T> static Event from(const T& source) noexcept {
        static_assert(sizeof(T) <= 128, "event too large");
        Event result;
        std::memcpy(result.bytes.data(), &source, sizeof(T));
        return result;
    }
    static Event midi(uint8_t status, uint8_t a, uint8_t b, uint32_t frame = 0,
                      uint16_t port = 0) noexcept;
    static Event note(bool on, int key, double velocity, uint32_t frame = 0,
                      int channel = 0, int port = 0, int noteId = -1) noexcept;
};

struct Statistics {
    uint64_t blocks = 0, droppedEvents = 0, processErrors = 0;
    double lastMicroseconds = 0, maximumMicroseconds = 0;
    // Cumulative per host. A missed DSP deadline is not an audio-device xrun.
    uint64_t deadlineMisses = 0, controlSilences = 0, idleBlocks = 0;
};

// Control API belongs to ONE main/UI thread. process(), enqueue() and atomic
// telemetry are the only cross-thread API. Control operations exclude rendering
// with a gate: the callback NEVER waits and returns silence during a change.
// CLAP permits the exclusively held main thread to act as the symbolic audio
// thread for stop/reset/flush; thread-check reports this scope accurately.
class Host {
public:
    static constexpr uint32_t maxEvents = 2048;
    static constexpr uint32_t maxStateBytes = 64 * 1024 * 1024;
    Host();
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    bool load(const Descriptor&, double sampleRate, uint32_t maxFrames,
              const std::vector<uint8_t>& state = {});
    void unload();
    bool reconfigure(double sampleRate, uint32_t maxFrames);
    void service(); // main-loop callbacks, restart, timers, fd polling, rescans
    bool saveState(std::vector<uint8_t>& destination);
    bool loadState(const std::vector<uint8_t>& source);
    bool supportsState() const noexcept;
    bool loaded() const noexcept;
    bool faulted() const noexcept;
    const std::string& error() const noexcept;
    const Descriptor& descriptor() const noexcept;
    const std::vector<Parameter>& parameters() const noexcept;
    uint64_t metadataRevision() const noexcept;
    const std::vector<Port>& inputPorts() const noexcept;
    const std::vector<Port>& outputPorts() const noexcept;
    double parameterValue(clap_id) const noexcept;
    std::string parameterText(clap_id, double value) const;
    bool setParameter(clap_id, double plainValue) noexcept;
    bool parameterGesture(clap_id, bool begin) noexcept;
    bool stateDirty() const noexcept;
    uint32_t latency() const noexcept;
    Statistics statistics() const noexcept;

    bool enqueue(const Event&) noexcept; // bounded multi-producer queue
    bool popOutput(Event&) noexcept; // main/control-thread consumer
    void panic() noexcept;
    void reset(); // main-thread, exclusive audio scope

    // Planar channels are flattened in port order. Missing inputs are silent;
    // surplus plugin outputs render into preallocated scratch storage. All
    // supplied outputs are overwritten. Frames may exceed negotiated maxFrames:
    // the host splits them and rebases event and transport timestamps.
    bool process(const float* const* inputs, uint32_t inputChannels,
                 float* const* outputs, uint32_t outputChannels, uint32_t frames,
                 const Event* events = nullptr, uint32_t count = 0,
                 const clap_event_transport_t* transport = nullptr) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace kick::clap
