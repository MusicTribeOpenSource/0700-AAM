// SPDX-License-Identifier: AGPL-3.0-or-later
#include <kick_clap/Host.h>
#include "Internal.h"
#include <clap/ext/audio-ports.h>
#include <clap/ext/note-ports.h>
#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/ext/latency.h>
#include <clap/ext/tail.h>
#include <clap/ext/thread-check.h>
#include <clap/ext/timer-support.h>
#include <clap/ext/posix-fd-support.h>
#include <clap/ext/log.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#ifndef _WIN32
#include <poll.h>
#endif

namespace kick::clap {
Event Event::midi(uint8_t status, uint8_t a, uint8_t b, uint32_t frame, uint16_t port) noexcept {
    clap_event_midi_t e{};
    e.header = {sizeof(e), frame, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0};
    e.port_index = port;
    e.data[0] = status; e.data[1] = a; e.data[2] = b;
    return from(e);
}
Event Event::note(bool on, int key, double velocity, uint32_t frame, int channel, int port, int noteId) noexcept {
    clap_event_note_t e{};
    e.header = {sizeof(e), frame, CLAP_CORE_EVENT_SPACE_ID,
                static_cast<uint16_t>(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0};
    e.note_id = noteId; e.port_index = static_cast<int16_t>(port);
    e.channel = static_cast<int16_t>(channel); e.key = static_cast<int16_t>(key);
    e.velocity = velocity;
    return from(e);
}

namespace {
thread_local const void* audioScope = nullptr;
struct AudioScope {
    const void* previous;
    explicit AudioScope(const void* owner) : previous(audioScope) { audioScope = owner; }
    ~AudioScope() { audioScope = previous; }
};
struct Gate {
    std::atomic_flag& flag;
    explicit Gate(std::atomic_flag& f) : flag(f) {
        while (flag.test_and_set(std::memory_order_acquire)) std::this_thread::yield();
    }
    ~Gate() { flag.clear(std::memory_order_release); }
};
struct Stream {
    std::vector<uint8_t> bytes;
    size_t offset = 0;
    bool failed = false;
    static int64_t CLAP_ABI write(const clap_ostream_t* s, const void* data, uint64_t size) {
        auto& self = *static_cast<Stream*>(s->ctx);
        if (size > Host::maxStateBytes - self.bytes.size()) { self.failed = true; return -1; }
        try {
            const auto* p = static_cast<const uint8_t*>(data);
            self.bytes.insert(self.bytes.end(), p, p + size);
            return static_cast<int64_t>(size);
        } catch (...) { self.failed = true; return -1; }
    }
    static int64_t CLAP_ABI read(const clap_istream_t* s, void* data, uint64_t size) {
        auto& self = *static_cast<Stream*>(s->ctx);
        const auto count = std::min<uint64_t>(size, self.bytes.size() - self.offset);
        if (count) std::memcpy(data, self.bytes.data() + self.offset, static_cast<size_t>(count));
        self.offset += static_cast<size_t>(count);
        return static_cast<int64_t>(count);
    }
};
bool validEvent(const clap_event_header_t* h) noexcept {
    if (!h || h->space_id != CLAP_CORE_EVENT_SPACE_ID || h->size > 128) return false;
    uint32_t size = 0;
    switch (h->type) {
        case CLAP_EVENT_NOTE_ON: case CLAP_EVENT_NOTE_OFF: case CLAP_EVENT_NOTE_CHOKE:
        case CLAP_EVENT_NOTE_END: size = sizeof(clap_event_note_t); break;
        case CLAP_EVENT_NOTE_EXPRESSION: size = sizeof(clap_event_note_expression_t); break;
        case CLAP_EVENT_PARAM_VALUE: size = sizeof(clap_event_param_value_t); break;
        case CLAP_EVENT_PARAM_MOD: size = sizeof(clap_event_param_mod_t); break;
        case CLAP_EVENT_PARAM_GESTURE_BEGIN: case CLAP_EVENT_PARAM_GESTURE_END:
            size = sizeof(clap_event_param_gesture_t); break;
        case CLAP_EVENT_MIDI: size = sizeof(clap_event_midi_t); break;
        case CLAP_EVENT_MIDI2: size = sizeof(clap_event_midi2_t); break;
        default: return false;
    }
    if (h->size != size) return false;
    if (h->type == CLAP_EVENT_NOTE_ON || h->type == CLAP_EVENT_NOTE_OFF || h->type == CLAP_EVENT_NOTE_CHOKE || h->type == CLAP_EVENT_NOTE_END) {
        const auto& n = *reinterpret_cast<const clap_event_note_t*>(h);
        if (!std::isfinite(n.velocity) || n.velocity < 0 || n.velocity > 1 || n.note_id < -1 ||
            n.channel < -1 || n.channel > 15 || n.key < -1 || n.key > 127 || n.port_index < -1) return false;
        if (h->type == CLAP_EVENT_NOTE_ON && (n.channel < 0 || n.key < 0 || n.port_index < 0)) return false;
    }
    if (h->type == CLAP_EVENT_PARAM_VALUE && !std::isfinite(reinterpret_cast<const clap_event_param_value_t*>(h)->value)) return false;
    if (h->type == CLAP_EVENT_PARAM_MOD && !std::isfinite(reinterpret_cast<const clap_event_param_mod_t*>(h)->amount)) return false;
    if (h->type == CLAP_EVENT_NOTE_EXPRESSION && !std::isfinite(reinterpret_cast<const clap_event_note_expression_t*>(h)->value)) return false;
    if (h->type == CLAP_EVENT_MIDI) {
        const auto& m = *reinterpret_cast<const clap_event_midi_t*>(h);
        if (m.data[0] < 0x80 || m.data[0] == 0xf0 || m.data[0] == 0xf7 || m.data[1] > 127 || m.data[2] > 127) return false;
    }
    return true;
}
}

struct Host::Impl {
    std::thread::id mainThread{};
    clap_host_t host{};
    std::shared_ptr<detail::Library> library;
    const clap_plugin_t* plugin = nullptr;
    const clap_plugin_params_t* params = nullptr;
    const clap_plugin_state_t* state = nullptr;
    const clap_plugin_latency_t* pluginLatency = nullptr;
    const clap_plugin_timer_support_t* timerSupport = nullptr;
    const clap_plugin_posix_fd_support_t* fdSupport = nullptr;
    Descriptor info;
    std::string message;
    std::vector<Parameter> paramInfo;
    uint64_t metadataVersion = 0;
    std::vector<void*> cookies;
    std::unique_ptr<std::atomic<double>[]> paramValues;
    std::vector<Port> inputs, outputs;
    std::vector<clap_note_port_info_t> noteInputs;
    std::vector<clap_audio_buffer_t> inputBuffers, outputBuffers;
    std::vector<float*> inputPointers, outputPointers;
    std::vector<float> scratch;
    double sampleRate = 48000;
    uint32_t maxFrames = 512;
    int64_t steadyTime = 0;
    bool active = false, processing = false, sleeping = false;
    bool transportObserved = false, hadTransport = false;
    clap_event_transport_t previousTransport{};
    uint32_t previousTransportFrames = 0;
    std::atomic_flag gate = ATOMIC_FLAG_INIT;
    std::atomic<bool> loaded{false}, failed{false}, dirty{false};
    std::atomic<bool> restart{false}, callback{false}, wake{false}, flush{false}, panic{false};
    std::atomic<uint32_t> rescan{0}, latencyFrames{0};
    std::atomic<uint64_t> blocks{0}, dropped{0}, errors{0};
    std::atomic<double> lastUs{0}, maxUs{0};
    detail::Queue<Event, maxEvents> commands, emitted;
    struct TimedEvent { Event event; uint32_t order = 0; };
    std::array<TimedEvent, maxEvents> ordered;
    std::array<Event, maxEvents> chunkEvents;
    std::array<Event, maxEvents> carryEvents;
    uint32_t carryCount = 0;
    uint32_t chunkCount = 0;
    uint32_t outputFrameOffset = 0, currentProcessFrames = 1;
    struct Timer { clap_id id; uint32_t period; std::chrono::steady_clock::time_point next; };
    std::vector<Timer> timers;
    clap_id nextTimer = 1;
    struct Fd { int fd; clap_posix_fd_flags_t flags; };
    std::vector<Fd> fds;
    static Impl& self(const clap_host_t* h) { return *static_cast<Impl*>(h->host_data); }
    bool main() {
        if (mainThread == std::thread::id{}) mainThread = std::this_thread::get_id();
        if (mainThread == std::this_thread::get_id()) return true;
        message = "CLAP control operation must run on its main thread";
        return false;
    }
    static bool CLAP_ABI isMain(const clap_host_t* h) { return self(h).mainThread == std::this_thread::get_id(); }
    static bool CLAP_ABI isAudio(const clap_host_t* h) { return audioScope == &self(h); }
    static void CLAP_ABI requestRestart(const clap_host_t* h) { self(h).restart.store(true); }
    static void CLAP_ABI requestCallback(const clap_host_t* h) { self(h).callback.store(true); }
    static void CLAP_ABI requestProcess(const clap_host_t* h) { self(h).wake.store(true); }
    static void CLAP_ABI requestFlush(const clap_host_t* h) { self(h).flush.store(true); self(h).wake.store(true); }
    static void CLAP_ABI rescanParams(const clap_host_t* h, clap_param_rescan_flags flags) { self(h).rescan.fetch_or(flags); }
    static void CLAP_ABI clearParams(const clap_host_t*, clap_id, clap_param_clear_flags) {}
    static void CLAP_ABI markDirty(const clap_host_t* h) { self(h).dirty.store(true); }
    static void CLAP_ABI latencyChanged(const clap_host_t* h) { self(h).restart.store(true); }
    static void CLAP_ABI tailChanged(const clap_host_t*) {}
    static bool CLAP_ABI supportedAudioRescan(const clap_host_t*, uint32_t flags) {
        return (flags & ~(CLAP_AUDIO_PORTS_RESCAN_NAMES | CLAP_AUDIO_PORTS_RESCAN_FLAGS |
                         CLAP_AUDIO_PORTS_RESCAN_CHANNEL_COUNT | CLAP_AUDIO_PORTS_RESCAN_PORT_TYPE |
                         CLAP_AUDIO_PORTS_RESCAN_IN_PLACE_PAIR | CLAP_AUDIO_PORTS_RESCAN_LIST)) == 0;
    }
    static void CLAP_ABI audioRescan(const clap_host_t* h, uint32_t) { self(h).restart.store(true); }
    static uint32_t CLAP_ABI dialects(const clap_host_t*) { return CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI_MPE; }
    static void CLAP_ABI noteRescan(const clap_host_t* h, uint32_t) { self(h).restart.store(true); }
    static void CLAP_ABI log(const clap_host_t* h, clap_log_severity severity, const char*) {
        // Logging from audio callbacks must not format strings, write files, or lock.
        if (severity == CLAP_LOG_FATAL || severity == CLAP_LOG_PLUGIN_MISBEHAVING) self(h).errors.fetch_add(1);
        if (severity == CLAP_LOG_FATAL) self(h).failed.store(true);
    }
    static bool CLAP_ABI registerTimer(const clap_host_t* h, uint32_t ms, clap_id* id) {
        auto& s = self(h);
        if (!isMain(h) || !id || s.timers.size() >= 128) return false;
        *id = s.nextTimer++;
        s.timers.push_back({*id, std::max(1u, ms), std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(1u, ms))});
        return true;
    }
    static bool CLAP_ABI unregisterTimer(const clap_host_t* h, clap_id id) {
        auto& s = self(h);
        if (!isMain(h)) return false;
        const auto before = s.timers.size();
        s.timers.erase(std::remove_if(s.timers.begin(), s.timers.end(), [=](const Timer& t) { return t.id == id; }), s.timers.end());
        return before != s.timers.size();
    }
    static bool CLAP_ABI registerFd(const clap_host_t* h, int fd, clap_posix_fd_flags_t flags) {
        auto& s = self(h);
        if (!isMain(h) || fd < 0 || s.fds.size() >= 128 ||
            (flags & ~(CLAP_POSIX_FD_READ | CLAP_POSIX_FD_WRITE | CLAP_POSIX_FD_ERROR))) return false;
        for (const auto& item : s.fds) if (item.fd == fd) return false;
        s.fds.push_back({fd, flags}); return true;
    }
    static bool CLAP_ABI modifyFd(const clap_host_t* h, int fd, clap_posix_fd_flags_t flags) {
        auto& s = self(h);
        if (!isMain(h)) return false;
        for (auto& item : s.fds) if (item.fd == fd) { item.flags = flags; return true; }
        return false;
    }
    static bool CLAP_ABI unregisterFd(const clap_host_t* h, int fd) {
        auto& s = self(h);
        if (!isMain(h)) return false;
        auto old = s.fds.size();
        s.fds.erase(std::remove_if(s.fds.begin(), s.fds.end(), [=](const Fd& item) { return item.fd == fd; }), s.fds.end());
        return old != s.fds.size();
    }
    static const void* CLAP_ABI extension(const clap_host_t*, const char* id) {
        static const clap_host_thread_check_t threads{isMain, isAudio};
        static const clap_host_params_t parameters{rescanParams, clearParams, requestFlush};
        static const clap_host_state_t state{markDirty};
        static const clap_host_latency_t latency{latencyChanged};
        static const clap_host_tail_t tail{tailChanged};
        static const clap_host_audio_ports_t audio{supportedAudioRescan, audioRescan};
        static const clap_host_note_ports_t notes{dialects, noteRescan};
        static const clap_host_log_t logging{log};
        static const clap_host_timer_support_t timer{registerTimer, unregisterTimer};
        static const clap_host_posix_fd_support_t descriptors{registerFd, modifyFd, unregisterFd};
        if (!id) return nullptr;
        if (!std::strcmp(id, CLAP_EXT_THREAD_CHECK)) return &threads;
        if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &parameters;
        if (!std::strcmp(id, CLAP_EXT_STATE)) return &state;
        if (!std::strcmp(id, CLAP_EXT_LATENCY)) return &latency;
        if (!std::strcmp(id, CLAP_EXT_TAIL)) return &tail;
        if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &audio;
        if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS)) return &notes;
        if (!std::strcmp(id, CLAP_EXT_LOG)) return &logging;
        if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT)) return &timer;
#ifndef _WIN32
        if (!std::strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT)) return &descriptors;
#else
        (void)descriptors;
#endif
        return nullptr;
    }
    Impl() {
        host.clap_version = CLAP_VERSION; host.host_data = this;
        host.name = "KICK"; host.vendor = "KICK"; host.url = "https://github.com/MusicTribeOpenSource/0700-AAM"; host.version = "0.1.0";
        host.get_extension = extension; host.request_restart = requestRestart;
        host.request_process = requestProcess; host.request_callback = requestCallback;
    }
    size_t paramIndex(clap_id id) const noexcept {
        const auto it = std::lower_bound(paramInfo.begin(), paramInfo.end(), id,
                                        [](const Parameter& p, clap_id target) { return p.id < target; });
        return it != paramInfo.end() && it->id == id ? static_cast<size_t>(it - paramInfo.begin()) : paramInfo.size();
    }
    void stop() {
        AudioScope scope(this);
        if (processing) plugin->stop_processing(plugin);
        processing = sleeping = false;
    }
    void deactivate() {
        stop();
        if (active) plugin->deactivate(plugin);
        active = false;
    }
    void close() {
        loaded.store(false);
        if (plugin) { deactivate(); if (plugin->destroy) plugin->destroy(plugin); }
        plugin = nullptr; params = nullptr; state = nullptr; pluginLatency = nullptr;
        timerSupport = nullptr; fdSupport = nullptr;
        timers.clear(); fds.clear();
        paramInfo.clear(); paramValues.reset(); cookies.clear(); inputs.clear(); outputs.clear(); noteInputs.clear();
        inputBuffers.clear(); outputBuffers.clear(); inputPointers.clear(); outputPointers.clear(); scratch.clear();
        library.reset();
        Event event; while (commands.pop(event)) {} while (emitted.pop(event)) {}
        failed.store(false); dirty.store(false); restart.store(false); callback.store(false);
        wake.store(false); flush.store(false); panic.store(false); rescan.store(0); latencyFrames.store(0);
        steadyTime = 0;
        transportObserved = hadTransport = false;
        carryCount = 0;
        ++metadataVersion;
    }
    void readParameters() {
        struct Item { Parameter info; void* cookie; double value; };
        std::vector<Item> items;
        if (params) {
            const auto count = params->count(plugin);
            if (count > 32768) throw std::runtime_error("Plugin advertises too many parameters");
            for (uint32_t i = 0; i < count; ++i) {
                clap_param_info_t p{};
                if (!params->get_info(plugin, i, &p) || p.id == CLAP_INVALID_ID ||
                    !std::isfinite(p.min_value) || !std::isfinite(p.max_value) ||
                    !std::isfinite(p.default_value) || p.min_value > p.max_value ||
                    p.default_value < p.min_value || p.default_value > p.max_value)
                    throw std::runtime_error("Invalid CLAP parameter metadata");
                double value = p.default_value;
                params->get_value(plugin, p.id, &value);
                if (!std::isfinite(value)) value = p.default_value;
                items.push_back({{p.id, std::string(p.name, strnlen(p.name, CLAP_NAME_SIZE)),
                                      std::string(p.module, strnlen(p.module, CLAP_PATH_SIZE)),
                                      p.min_value, p.max_value, p.default_value, p.flags}, p.cookie, value});
            }
        }
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.info.id < b.info.id; });
        paramInfo.clear(); cookies.clear();
        paramValues = std::make_unique<std::atomic<double>[]>(items.size());
        for (size_t i = 0; i < items.size(); ++i) {
            if (i && items[i-1].info.id == items[i].info.id) throw std::runtime_error("Duplicate CLAP parameter ID");
            paramInfo.push_back(std::move(items[i].info)); cookies.push_back(items[i].cookie);
            paramValues[i].store(items[i].value);
        }
        ++metadataVersion;
    }
    void readPorts() {
        inputs.clear(); outputs.clear(); noteInputs.clear();
        const auto* audio = static_cast<const clap_plugin_audio_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
        if (!audio || !audio->count || !audio->get) throw std::runtime_error("Plugin has no audio-ports extension");
        for (bool input : {true, false}) {
            auto& ports = input ? inputs : outputs;
            const auto count = audio->count(plugin, input);
            if (count > 16) throw std::runtime_error("More than 16 audio ports are unsupported");
            uint32_t channels = 0;
            for (uint32_t i = 0; i < count; ++i) {
                clap_audio_port_info_t p{};
                if (!audio->get(plugin, i, input, &p) || p.channel_count == 0 || p.channel_count > 64 ||
                    channels + p.channel_count > 64)
                    throw std::runtime_error("Invalid or unsupported audio channel layout");
                ports.push_back({p.id, std::string(p.name, strnlen(p.name, CLAP_NAME_SIZE)), p.channel_count, p.flags});
                channels += p.channel_count;
            }
        }
        if (outputs.empty()) throw std::runtime_error("Plugin has no audio outputs");
        const auto* notes = static_cast<const clap_plugin_note_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
        if (notes && notes->count && notes->get) {
            const auto count = notes->count(plugin, true);
            if (count > 16) throw std::runtime_error("More than 16 note ports are unsupported");
            for (uint32_t i = 0; i < count; ++i) {
                clap_note_port_info_t p{};
                if (!notes->get(plugin, i, true, &p)) throw std::runtime_error("Invalid CLAP note port");
                noteInputs.push_back(p);
            }
        }
        size_t ni = 0, no = 0;
        for (const auto& p : inputs) ni += p.channels;
        for (const auto& p : outputs) no += p.channels;
        inputPointers.resize(ni); outputPointers.resize(no);
        inputBuffers.resize(inputs.size()); outputBuffers.resize(outputs.size());
        scratch.assign((ni + no) * maxFrames, 0.f);
        size_t offset = 0;
        for (size_t i = 0; i < inputs.size(); ++i) {
            inputBuffers[i] = {}; inputBuffers[i].channel_count = inputs[i].channels;
            inputBuffers[i].data32 = inputPointers.data() + offset; offset += inputs[i].channels;
        }
        offset = 0;
        for (size_t i = 0; i < outputs.size(); ++i) {
            outputBuffers[i] = {}; outputBuffers[i].channel_count = outputs[i].channels;
            outputBuffers[i].data32 = outputPointers.data() + offset; offset += outputs[i].channels;
        }
    }
    bool activate() {
        if (!std::isfinite(sampleRate) || sampleRate < 8000 || sampleRate > 768000 || maxFrames == 0 || maxFrames > 65536)
            throw std::runtime_error("Invalid sample rate or maximum block size");
        readPorts(); readParameters();
        if (!plugin->activate(plugin, sampleRate, 1, maxFrames)) throw std::runtime_error("Plugin activation failed");
        active = true;
        transportObserved = false;
        latencyFrames.store(pluginLatency && pluginLatency->get ? pluginLatency->get(plugin) : 0);
        return true;
    }
    void fail(const std::string& why) { message = why; failed.store(true); }
    static uint32_t CLAP_ABI eventCount(const clap_input_events_t* list) { return static_cast<Impl*>(list->ctx)->chunkCount; }
    static const clap_event_header_t* CLAP_ABI eventGet(const clap_input_events_t* list, uint32_t i) {
        auto& s = *static_cast<Impl*>(list->ctx);
        return i < s.chunkCount ? s.chunkEvents[i].header() : nullptr;
    }
    static bool CLAP_ABI outputPush(const clap_output_events_t* list, const clap_event_header_t* event) {
        auto& s = *static_cast<Impl*>(list->ctx);
        if (!validEvent(event) || event->time >= s.currentProcessFrames) return false;
        if (event->type == CLAP_EVENT_PARAM_VALUE) {
            const auto* p = reinterpret_cast<const clap_event_param_value_t*>(event);
            auto i = s.paramIndex(p->param_id);
            if (i < s.paramInfo.size() && std::isfinite(p->value)) {
                if (s.paramValues[i].exchange(p->value) != p->value) s.dirty.store(true);
            }
        }
        Event copy; std::memcpy(copy.bytes.data(), event, event->size);
        reinterpret_cast<clap_event_header_t*>(copy.bytes.data())->time += s.outputFrameOffset;
        if (s.emitted.push(copy)) return true;
        s.dropped.fetch_add(1); return false;
    }
    void flushParameters() {
        if (!params || !params->flush) return;
        AudioScope scope(this);
        chunkCount = 0;
        outputFrameOffset = 0; currentProcessFrames = 1;
        // Only parameter events belong to flush(). Preserve musical events by
        // rendering them normally; service() never drains the command queue.
        clap_input_events_t in{this, eventCount, eventGet};
        clap_output_events_t out{this, outputPush};
        params->flush(plugin, &in, &out);
    }
    Event translate(Event event) const noexcept {
        auto* h = reinterpret_cast<clap_event_header_t*>(event.bytes.data());
        if (h->type == CLAP_EVENT_PARAM_VALUE) {
            auto* p = reinterpret_cast<clap_event_param_value_t*>(event.bytes.data());
            const auto i = paramIndex(p->param_id);
            if (i < cookies.size()) p->cookie = cookies[i];
        } else if (h->type == CLAP_EVENT_MIDI) {
            const auto& m = *reinterpret_cast<const clap_event_midi_t*>(event.bytes.data());
            if (m.port_index < noteInputs.size()) {
                const auto dialect = noteInputs[m.port_index].supported_dialects;
                const auto preferred = noteInputs[m.port_index].preferred_dialect;
                if ((dialect & CLAP_NOTE_DIALECT_CLAP) &&
                    (preferred == CLAP_NOTE_DIALECT_CLAP || !(dialect & CLAP_NOTE_DIALECT_MIDI))) {
                    const auto status = m.data[0] & 0xf0;
                    if (status == 0x90 || status == 0x80)
                        return Event::note(status == 0x90 && m.data[2] != 0, m.data[1], m.data[2] / 127.0,
                                           h->time, m.data[0] & 15, m.port_index);
                }
            }
        } else if (h->type == CLAP_EVENT_NOTE_ON || h->type == CLAP_EVENT_NOTE_OFF) {
            const auto& n = *reinterpret_cast<const clap_event_note_t*>(event.bytes.data());
            if (n.port_index >= 0 && static_cast<size_t>(n.port_index) < noteInputs.size() &&
                !(noteInputs[n.port_index].supported_dialects & CLAP_NOTE_DIALECT_CLAP))
                return Event::midi(static_cast<uint8_t>((h->type == CLAP_EVENT_NOTE_ON ? 0x90 : 0x80) | n.channel),
                                   static_cast<uint8_t>(n.key), static_cast<uint8_t>(std::clamp(n.velocity, 0., 1.) * 127), h->time,
                                   static_cast<uint16_t>(n.port_index));
        }
        return event;
    }
    bool transportChanged(const clap_event_transport_t* current, uint32_t frames) noexcept {
        bool changed = !transportObserved || hadTransport != (current != nullptr);
        if (current && hadTransport && transportObserved) {
            const auto& old = previousTransport;
            changed |= current->flags != old.flags;
            if (current->flags & CLAP_TRANSPORT_HAS_TEMPO)
                changed |= current->tempo != old.tempo || current->tempo_inc != old.tempo_inc;
            if (current->flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE)
                changed |= current->tsig_num != old.tsig_num || current->tsig_denom != old.tsig_denom;
            const auto elapsed = (old.flags & CLAP_TRANSPORT_IS_PLAYING) ? previousTransportFrames / sampleRate : 0.;
            if (current->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) {
                const auto expected = static_cast<long double>(old.song_pos_beats) +
                    elapsed * old.tempo / 60. * CLAP_BEATTIME_FACTOR;
                changed |= std::abs(static_cast<long double>(current->song_pos_beats) - expected) > 2.;
                changed |= current->loop_start_beats != old.loop_start_beats || current->loop_end_beats != old.loop_end_beats ||
                           current->bar_start != old.bar_start || current->bar_number != old.bar_number;
            }
            if (current->flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE) {
                const auto expected = static_cast<long double>(old.song_pos_seconds) + elapsed * CLAP_SECTIME_FACTOR;
                changed |= std::abs(static_cast<long double>(current->song_pos_seconds) - expected) > 2.;
                changed |= current->loop_start_seconds != old.loop_start_seconds || current->loop_end_seconds != old.loop_end_seconds;
            }
        }
        if (current) previousTransport = *current;
        hadTransport = current != nullptr; transportObserved = true; previousTransportFrames = frames;
        return changed;
    }
    bool accepts(const Event& event) const noexcept {
        const auto* h = event.header();
        if (h->type == CLAP_EVENT_NOTE_END) return false; // plugin-to-host only
        if (h->type == CLAP_EVENT_PARAM_VALUE || h->type == CLAP_EVENT_PARAM_MOD ||
            h->type == CLAP_EVENT_PARAM_GESTURE_BEGIN || h->type == CLAP_EVENT_PARAM_GESTURE_END) {
            const auto id = h->type == CLAP_EVENT_PARAM_VALUE ? reinterpret_cast<const clap_event_param_value_t*>(h)->param_id :
                            h->type == CLAP_EVENT_PARAM_MOD ? reinterpret_cast<const clap_event_param_mod_t*>(h)->param_id :
                            reinterpret_cast<const clap_event_param_gesture_t*>(h)->param_id;
            auto i = paramIndex(id);
            return i < paramInfo.size() && !(paramInfo[i].flags & CLAP_PARAM_IS_READONLY);
        }
        if (h->type == CLAP_EVENT_MIDI) {
            const auto& m = *reinterpret_cast<const clap_event_midi_t*>(h);
            return m.port_index < noteInputs.size() &&
                   (noteInputs[m.port_index].supported_dialects & (CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI_MPE));
        }
        if (h->type == CLAP_EVENT_MIDI2) return false; // not advertised by this host
        const int port = h->type == CLAP_EVENT_NOTE_EXPRESSION ? reinterpret_cast<const clap_event_note_expression_t*>(h)->port_index :
                         reinterpret_cast<const clap_event_note_t*>(h)->port_index;
        if (port == -1) return !noteInputs.empty();
        return port >= 0 && static_cast<size_t>(port) < noteInputs.size() &&
               (noteInputs[static_cast<size_t>(port)].supported_dialects & CLAP_NOTE_DIALECT_CLAP);
    }
};

Host::Host() : impl(std::make_unique<Impl>()) {}
Host::~Host() { Gate guard(impl->gate); impl->close(); }
bool Host::load(const Descriptor& descriptor, double sampleRate, uint32_t frames, const std::vector<uint8_t>& bytes) {
    auto& s = *impl;
    if (!s.main()) return false;
    Gate guard(s.gate);
    s.close(); s.message.clear(); s.info = descriptor;
    try {
        s.library = detail::openLibrary(descriptor.path);
        auto* factory = s.library->factory();
        if (!factory) throw std::runtime_error("No CLAP plugin factory");
        const clap_plugin_descriptor_t* chosen = nullptr;
        for (uint32_t i = 0, count = std::min(4096u, factory->get_plugin_count(factory)); i < count; ++i) {
            const auto* d = factory->get_plugin_descriptor(factory, i);
            if (d && d->id && d->name && descriptor.id == d->id && clap_version_is_compatible(d->clap_version)) { chosen = d; break; }
        }
        if (!chosen) throw std::runtime_error("Saved plugin ID is absent from this binary");
        s.info = detail::describe(s.library->path, *chosen);
        s.plugin = factory->create_plugin(factory, &s.host, chosen->id);
        if (!s.plugin || !s.plugin->init || !s.plugin->destroy || !s.plugin->activate || !s.plugin->deactivate ||
            !s.plugin->start_processing || !s.plugin->stop_processing || !s.plugin->reset ||
            !s.plugin->process || !s.plugin->get_extension || !s.plugin->on_main_thread)
            throw std::runtime_error("Incomplete CLAP plugin interface");
        if (!s.plugin->init(s.plugin)) throw std::runtime_error("Plugin initialization failed");
        s.params = static_cast<const clap_plugin_params_t*>(s.plugin->get_extension(s.plugin, CLAP_EXT_PARAMS));
        if (s.params && (!s.params->count || !s.params->get_info || !s.params->get_value || !s.params->flush))
            throw std::runtime_error("Incomplete CLAP parameter interface");
        s.state = static_cast<const clap_plugin_state_t*>(s.plugin->get_extension(s.plugin, CLAP_EXT_STATE));
        s.pluginLatency = static_cast<const clap_plugin_latency_t*>(s.plugin->get_extension(s.plugin, CLAP_EXT_LATENCY));
        s.timerSupport = static_cast<const clap_plugin_timer_support_t*>(s.plugin->get_extension(s.plugin, CLAP_EXT_TIMER_SUPPORT));
        s.fdSupport = static_cast<const clap_plugin_posix_fd_support_t*>(s.plugin->get_extension(s.plugin, CLAP_EXT_POSIX_FD_SUPPORT));
        if (!bytes.empty()) {
            if (bytes.size() > maxStateBytes || !s.state || !s.state->load) throw std::runtime_error("Plugin cannot restore saved state");
            Stream stream; stream.bytes = bytes;
            clap_istream_t input{&stream, Stream::read};
            if (!s.state->load(s.plugin, &input)) throw std::runtime_error("Plugin rejected saved state");
        }
        s.sampleRate = sampleRate; s.maxFrames = frames;
        s.activate(); s.loaded.store(true); s.dirty.store(false);
        return true;
    } catch (const std::exception& e) { s.message = e.what(); }
      catch (...) { s.message = "Plugin threw during initialization"; }
    s.close(); s.failed.store(true); return false;
}
void Host::unload() { auto& s = *impl; if (!s.main()) return; Gate guard(s.gate); s.close(); }
bool Host::reconfigure(double rate, uint32_t frames) {
    auto& s = *impl;
    if (!s.main() || !loaded()) return false;
    Gate guard(s.gate);
    try { s.deactivate(); s.sampleRate = rate; s.maxFrames = frames; s.activate(); s.failed.store(false); return true; }
    catch (const std::exception& e) { s.fail(e.what()); return false; }
}
void Host::service() {
    auto& s = *impl;
    if (!s.main() || !s.plugin) return;
    Gate guard(s.gate);
    try {
        if (s.callback.exchange(false)) s.plugin->on_main_thread(s.plugin);
        const auto now = std::chrono::steady_clock::now();
        // Callbacks may register/unregister timers and fds, so iterate snapshots.
        auto timers = s.timers;
        if (s.timerSupport && s.timerSupport->on_timer) for (const auto& timer : timers) if (now >= timer.next) {
            auto it = std::find_if(s.timers.begin(), s.timers.end(), [&](const Impl::Timer& t) { return t.id == timer.id; });
            if (it != s.timers.end()) { it->next = now + std::chrono::milliseconds(timer.period); s.timerSupport->on_timer(s.plugin, timer.id); }
        }
#ifndef _WIN32
        auto fds = s.fds;
        if (s.fdSupport && s.fdSupport->on_fd) for (const auto& item : fds) {
            pollfd fd{item.fd, static_cast<short>(((item.flags & CLAP_POSIX_FD_READ) ? POLLIN : 0) |
                                               ((item.flags & CLAP_POSIX_FD_WRITE) ? POLLOUT : 0)), 0};
            if (::poll(&fd, 1, 0) > 0) {
                auto flags = static_cast<clap_posix_fd_flags_t>(((fd.revents & POLLIN) ? CLAP_POSIX_FD_READ : 0) |
                       ((fd.revents & POLLOUT) ? CLAP_POSIX_FD_WRITE : 0) |
                       ((fd.revents & (POLLERR | POLLHUP | POLLNVAL)) ? CLAP_POSIX_FD_ERROR : 0));
                if (flags && std::any_of(s.fds.begin(), s.fds.end(), [&](const Impl::Fd& f) { return f.fd == item.fd; }))
                    s.fdSupport->on_fd(s.plugin, item.fd, flags);
            }
        }
#endif
        const auto flags = s.rescan.exchange(0);
        if (s.restart.exchange(false) || (flags & (CLAP_PARAM_RESCAN_ALL | CLAP_PARAM_RESCAN_INFO))) {
            s.deactivate(); s.activate();
        } else if (flags & CLAP_PARAM_RESCAN_VALUES) {
            for (size_t i = 0; i < s.paramInfo.size(); ++i) { double value; if (s.params->get_value(s.plugin, s.paramInfo[i].id, &value) && std::isfinite(value)) s.paramValues[i].store(value); }
        }
        if (s.flush.exchange(false)) s.flushParameters();
        if (s.failed.load()) s.stop();
    } catch (const std::exception& e) { s.fail(e.what()); }
      catch (...) { s.fail("Plugin threw during main-thread service"); }
}
bool Host::saveState(std::vector<uint8_t>& destination) {
    auto& s = *impl;
    if (!s.main() || !s.plugin || !s.state || !s.state->save) { s.message = "Plugin does not support state saving"; return false; }
    Gate guard(s.gate);
    try {
        // Keep audio-thread parameter changes in the plugin's saved state even
        // when the user saves immediately after moving a slider.
        s.stop();
        s.chunkCount = 0;
        s.outputFrameOffset = 0; s.currentProcessFrames = 1;
        Event e;
        while (s.commands.pop(e)) {
            const auto type = e.header()->type;
            if (type == CLAP_EVENT_PARAM_VALUE || type == CLAP_EVENT_PARAM_GESTURE_BEGIN || type == CLAP_EVENT_PARAM_GESTURE_END)
                if (s.chunkCount < maxEvents) s.chunkEvents[s.chunkCount++] = s.translate(e);
            if (type != CLAP_EVENT_PARAM_VALUE && type != CLAP_EVENT_PARAM_GESTURE_BEGIN && type != CLAP_EVENT_PARAM_GESTURE_END) {
                if (s.carryCount < maxEvents) s.carryEvents[s.carryCount++] = e;
                else { s.panic.store(true); s.dropped.fetch_add(1); }
            }
        }
        if (s.params && s.params->flush) {
            AudioScope scope(&s);
            clap_input_events_t in{&s, Impl::eventCount, Impl::eventGet};
            clap_output_events_t out{&s, Impl::outputPush};
            s.params->flush(s.plugin, &in, &out);
        }
        Stream stream; clap_ostream_t output{&stream, Stream::write};
        if (!s.state->save(s.plugin, &output) || stream.failed) { s.message = "Plugin state saving failed"; return false; }
        destination = std::move(stream.bytes); s.dirty.store(false); return true;
    } catch (...) { s.message = "Plugin threw during state saving"; return false; }
}
bool Host::loadState(const std::vector<uint8_t>& bytes) {
    auto& s = *impl;
    if (!s.main() || !s.plugin || !s.state || !s.state->load || bytes.size() > maxStateBytes) return false;
    Gate guard(s.gate);
    try {
        s.deactivate();
        Stream stream; stream.bytes = bytes; clap_istream_t input{&stream, Stream::read};
        if (!s.state->load(s.plugin, &input)) { s.activate(); s.message = "Plugin rejected state"; return false; }
        Event e; while (s.commands.pop(e)) {}
        s.carryCount = 0;
        s.activate(); s.dirty.store(false); s.failed.store(false); return true;
    } catch (...) { s.fail("Plugin state restoration failed"); return false; }
}
bool Host::supportsState() const noexcept { return impl->state && impl->state->save && impl->state->load; }
bool Host::loaded() const noexcept { return impl->loaded.load(); }
bool Host::faulted() const noexcept { return impl->failed.load(); }
const std::string& Host::error() const noexcept { return impl->message; }
const Descriptor& Host::descriptor() const noexcept { return impl->info; }
const std::vector<Parameter>& Host::parameters() const noexcept { return impl->paramInfo; }
uint64_t Host::metadataRevision() const noexcept { return impl->metadataVersion; }
const std::vector<Port>& Host::inputPorts() const noexcept { return impl->inputs; }
const std::vector<Port>& Host::outputPorts() const noexcept { return impl->outputs; }
double Host::parameterValue(clap_id id) const noexcept {
    auto& s = *impl; const auto i = s.paramIndex(id);
    return i < s.paramInfo.size() ? s.paramValues[i].load() : 0;
}
std::string Host::parameterText(clap_id id, double value) const {
    char text[256]{};
    if (impl->params && impl->params->value_to_text && impl->params->value_to_text(impl->plugin, id, value, text, sizeof(text)))
        return {text, strnlen(text, sizeof(text))};
    return std::to_string(value);
}
bool Host::setParameter(clap_id id, double value) noexcept {
    auto& s = *impl; const auto index = s.paramIndex(id);
    if (index == s.paramInfo.size() || !std::isfinite(value) || (s.paramInfo[index].flags & CLAP_PARAM_IS_READONLY)) return false;
    value = std::clamp(value, s.paramInfo[index].minimum, s.paramInfo[index].maximum);
    if (s.paramInfo[index].flags & CLAP_PARAM_IS_STEPPED) value = std::trunc(value);
    clap_event_param_value_t event{};
    event.header = {sizeof(event), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, CLAP_EVENT_IS_LIVE};
    event.param_id = id; event.note_id = -1; event.port_index = event.channel = event.key = -1; event.value = value;
    if (!enqueue(Event::from(event))) return false;
    s.paramValues[index].store(value); s.dirty.store(true); return true;
}
bool Host::parameterGesture(clap_id id, bool begin) noexcept {
    clap_event_param_gesture_t event{};
    event.header = {sizeof(event), 0, CLAP_CORE_EVENT_SPACE_ID,
                    static_cast<uint16_t>(begin ? CLAP_EVENT_PARAM_GESTURE_BEGIN : CLAP_EVENT_PARAM_GESTURE_END), CLAP_EVENT_IS_LIVE};
    event.param_id = id; return enqueue(Event::from(event));
}
bool Host::stateDirty() const noexcept { return impl->dirty.load(); }
uint32_t Host::latency() const noexcept { return impl->latencyFrames.load(); }
Statistics Host::statistics() const noexcept { auto& s = *impl; return {s.blocks.load(), s.dropped.load(), s.errors.load(), s.lastUs.load(), s.maxUs.load()}; }
bool Host::enqueue(const Event& e) noexcept {
    auto& s = *impl;
    if (!validEvent(e.header())) return false;
    if (s.commands.push(e)) { s.wake.store(true); return true; }
    s.dropped.fetch_add(1); s.panic.store(true); return false;
}
bool Host::popOutput(Event& e) noexcept { return impl->emitted.pop(e); }
void Host::panic() noexcept { impl->panic.store(true); impl->wake.store(true); }
void Host::reset() { auto& s = *impl; if (!s.main() || !s.plugin) return; Gate guard(s.gate); AudioScope scope(&s); s.plugin->reset(s.plugin); s.sleeping = false; }

bool Host::process(const float* const* input, uint32_t ni, float* const* output, uint32_t no, uint32_t frames,
                   const Event* incoming, uint32_t count, const clap_event_transport_t* transport) noexcept {
    auto& s = *impl;
    auto silence = [&] { if (output) for (uint32_t c = 0; c < no; ++c) if (output[c]) std::fill_n(output[c], frames, 0.f); };
    if (!frames) return true;
    if (!output || (count && !incoming) || s.gate.test_and_set(std::memory_order_acquire)) { silence(); return false; }
    struct Release { std::atomic_flag& flag; ~Release() { flag.clear(std::memory_order_release); } } release{s.gate};
    AudioScope scope(&s);
    if (!s.plugin || !s.active || s.failed.load()) { silence(); return false; }
    const auto start = std::chrono::steady_clock::now();
    try {
        uint32_t total = 0;
        auto append = [&](Event e) {
            if (!validEvent(e.header()) || e.header()->time >= frames) { s.dropped.fetch_add(1); s.panic.store(true); return; }
            if (total == maxEvents) { s.dropped.fetch_add(1); s.panic.store(true); return; }
            auto translated = s.translate(e);
            if (!s.accepts(translated)) { s.dropped.fetch_add(1); return; }
            s.ordered[total] = {translated, total}; ++total;
        };
        Event event;
        for (uint32_t i = 0; i < s.carryCount; ++i) append(s.carryEvents[i]);
        s.carryCount = 0;
        for (uint32_t i = 0; i < maxEvents && s.commands.pop(event); ++i) append(event);
        for (uint32_t i = 0; i < std::min(count, maxEvents); ++i) append(incoming[i]);
        if (count > maxEvents) { s.dropped.fetch_add(count - maxEvents); s.panic.store(true); }
        if (s.panic.exchange(false)) {
            // Never leave a stuck gate after an event overflow. Reset is an
            // audio-thread operation and covers every note channel and port.
            s.plugin->reset(s.plugin); total = 0; s.sleeping = false;
        }
        std::sort(s.ordered.begin(), s.ordered.begin() + total, [](const Impl::TimedEvent& a, const Impl::TimedEvent& b) {
            return a.event.header()->time < b.event.header()->time ||
                (a.event.header()->time == b.event.header()->time && a.order < b.order);
        });
        bool hasInput = false;
        if (input) for (uint32_t c = 0; c < ni && !hasInput; ++c) if (input[c])
            for (uint32_t i = 0; i < frames; ++i) if (input[c][i] != 0.f) { hasInput = true; break; }
        const bool transportWake = s.transportChanged(transport, frames);
        if (s.sleeping && !total && !hasInput && !transportWake && !s.wake.exchange(false) && !s.flush.load()) { s.steadyTime += frames; silence(); return true; }
        s.wake.store(false); s.sleeping = false;
        if (!s.processing) {
            if (!s.plugin->start_processing(s.plugin)) { s.failed.store(true); s.errors.fetch_add(1); silence(); return false; }
            s.processing = true;
        }
        if (s.flush.exchange(false)) s.flushParameters();
        uint32_t next = 0;
        for (uint32_t done = 0; done < frames;) {
            const auto n = std::min(s.maxFrames, frames - done);
            s.chunkCount = 0;
            s.outputFrameOffset = done; s.currentProcessFrames = n;
            while (next < total && s.ordered[next].event.header()->time < done + n) {
                auto e = s.ordered[next++].event;
                reinterpret_cast<clap_event_header_t*>(e.bytes.data())->time -= done;
                s.chunkEvents[s.chunkCount++] = e;
            }
            for (size_t c = 0; c < s.inputPointers.size(); ++c) {
                auto* target = s.scratch.data() + c * s.maxFrames;
                if (input && c < ni && input[c]) std::copy_n(input[c] + done, n, target);
                else std::fill_n(target, n, 0.f);
                s.inputPointers[c] = target;
            }
            for (size_t c = 0; c < s.outputPointers.size(); ++c) {
                auto* target = s.scratch.data() + (s.inputPointers.size() + c) * s.maxFrames;
                std::fill_n(target, n, 0.f); s.outputPointers[c] = target;
            }
            clap_input_events_t inEvents{&s, Impl::eventCount, Impl::eventGet};
            clap_output_events_t outEvents{&s, Impl::outputPush};
            clap_process_t block{};
            block.steady_time = s.steadyTime; block.frames_count = n;
            block.audio_inputs = s.inputBuffers.data(); block.audio_inputs_count = static_cast<uint32_t>(s.inputBuffers.size());
            block.audio_outputs = s.outputBuffers.data(); block.audio_outputs_count = static_cast<uint32_t>(s.outputBuffers.size());
            block.in_events = &inEvents; block.out_events = &outEvents;
            clap_event_transport_t position{};
            if (transport) {
                position = *transport;
                if ((position.flags & CLAP_TRANSPORT_IS_PLAYING) && (position.flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE))
                    position.song_pos_seconds += static_cast<clap_sectime>((static_cast<double>(done) / s.sampleRate) * CLAP_SECTIME_FACTOR);
                if ((position.flags & CLAP_TRANSPORT_IS_PLAYING) && (position.flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) && (position.flags & CLAP_TRANSPORT_HAS_TEMPO))
                    position.song_pos_beats += static_cast<clap_beattime>((static_cast<double>(done) / s.sampleRate) * position.tempo / 60. * CLAP_BEATTIME_FACTOR);
                position.header.time = 0; block.transport = &position;
            }
            const auto status = s.plugin->process(s.plugin, &block);
            if (status < CLAP_PROCESS_CONTINUE || status > CLAP_PROCESS_SLEEP) { s.failed.store(true); s.errors.fetch_add(1); silence(); return false; }
            for (uint32_t c = 0; c < no; ++c) if (output[c]) {
                auto* destination = output[c] + done;
                if (c < s.outputPointers.size()) {
                    for (uint32_t i = 0; i < n; ++i) {
                        const auto value = s.outputPointers[c][i];
                        if (!std::isfinite(value)) { s.failed.store(true); s.errors.fetch_add(1); silence(); return false; }
                        destination[i] = value;
                    }
                } else std::fill_n(destination, n, 0.f);
            }
            // A mono instrument feeds both sides of KICK's stereo program bus.
            if (s.outputPointers.size() == 1 && no == 2 && output[0] && output[1]) std::copy_n(output[0] + done, n, output[1] + done);
            s.sleeping = status == CLAP_PROCESS_SLEEP;
            s.steadyTime += n; done += n;
        }
        s.blocks.fetch_add(1);
        const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
        s.lastUs.store(us); if (us > s.maxUs.load()) s.maxUs.store(us);
        return true;
    } catch (...) { s.failed.store(true); s.errors.fetch_add(1); silence(); return false; }
}
} // namespace kick::clap
