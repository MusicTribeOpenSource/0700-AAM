// SPDX-License-Identifier: AGPL-3.0-or-later
#include <clap/clap.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/note-ports.h>
#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/ext/thread-check.h>
#include <clap/ext/timer-support.h>
#include <clap/ext/latency.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>

namespace {
std::atomic<int> threadErrors{0}, entryUsers{0}, processCalls{0}, callbackCalls{0};
constexpr const char* ids[]{"org.kick.test.synth", "org.kick.test.effect", "org.kick.test.midi",
                            "org.kick.test.nan", "org.kick.test.error", "org.kick.test.sleep",
                            "org.kick.test.activate-failure", "org.kick.test.start-failure"};
const char* instrument[]{CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, nullptr};
const char* effect[]{CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, nullptr};
const clap_plugin_descriptor_t descriptors[]{
    {CLAP_VERSION, ids[0], "KICK fixture", "KICK", "", "", "", "1", "Test instrument", instrument},
    {CLAP_VERSION, ids[1], "KICK effect", "KICK", "", "", "", "1", "Test effect", effect},
    {CLAP_VERSION, ids[2], "KICK MIDI", "KICK", "", "", "", "1", "MIDI-only instrument", instrument},
    {CLAP_VERSION, ids[3], "KICK NaN", "KICK", "", "", "", "1", "Fault fixture", instrument},
    {CLAP_VERSION, ids[4], "KICK error", "KICK", "", "", "", "1", "Fault fixture", instrument},
    {CLAP_VERSION, ids[5], "KICK sleep", "KICK", "", "", "", "1", "Sleep fixture", instrument},
    {CLAP_VERSION, ids[6], "KICK activation failure", "KICK", "", "", "", "1", "Failure fixture", instrument},
    {CLAP_VERSION, ids[7], "KICK start failure", "KICK", "", "", "", "1", "Failure fixture", instrument}};
struct Plugin {
    clap_plugin_t api{};
    const clap_host_t* host;
    const clap_host_thread_check_t* check;
    int kind;
    bool active = false, processing = false, gate = false;
    double gain = .5;
    clap_id timerId = CLAP_INVALID_ID;
    uint32_t maxFrames = 0;
    static Plugin& get(const clap_plugin_t* p) { return *static_cast<Plugin*>(p->plugin_data); }
    void main() const { if (!check || !check->is_main_thread(host)) ++threadErrors; }
    void audio() const { if (!check || !check->is_audio_thread(host)) ++threadErrors; }
    Plugin(const clap_host_t* h, int k) : host(h), check(nullptr), kind(k) {
        api.desc = &descriptors[k]; api.plugin_data = this;
        api.init = [](const clap_plugin_t* p) {
            auto& s = get(p);
            s.check = static_cast<const clap_host_thread_check_t*>(s.host->get_extension(s.host, CLAP_EXT_THREAD_CHECK));
            s.main(); s.host->request_callback(s.host);
            if (auto* timer = static_cast<const clap_host_timer_support_t*>(s.host->get_extension(s.host, CLAP_EXT_TIMER_SUPPORT)))
                timer->register_timer(s.host, 1, &s.timerId);
            return true;
        };
        api.destroy = [](const clap_plugin_t* p) { auto& s = get(p); s.main(); if (s.active || s.processing) ++threadErrors; delete &s; };
        api.activate = [](const clap_plugin_t* p, double, uint32_t, uint32_t max) {
            auto& s = get(p); s.main(); if (s.kind == 6) return false; s.active = true; s.maxFrames = max; return true;
        };
        api.deactivate = [](const clap_plugin_t* p) { auto& s = get(p); s.main(); if (s.processing) ++threadErrors; s.active = false; };
        api.start_processing = [](const clap_plugin_t* p) { auto& s = get(p); s.audio(); if (!s.active) ++threadErrors; if (s.kind == 7) return false; s.processing = true; return true; };
        api.stop_processing = [](const clap_plugin_t* p) { auto& s = get(p); s.audio(); if (!s.processing) ++threadErrors; s.processing = false; };
        api.reset = [](const clap_plugin_t* p) { auto& s = get(p); s.audio(); s.gate = false; };
        api.process = process;
        api.get_extension = extension;
        api.on_main_thread = [](const clap_plugin_t* p) {
            auto& s = get(p); s.main(); ++callbackCalls;
            if (auto* params = static_cast<const clap_host_params_t*>(s.host->get_extension(s.host, CLAP_EXT_PARAMS))) params->request_flush(s.host);
        };
    }
    void event(const clap_event_header_t* e) {
        if (!e) { ++threadErrors; return; }
        if (e->type == CLAP_EVENT_NOTE_ON) { if (kind == 2) ++threadErrors; gate = true; }
        if (e->type == CLAP_EVENT_NOTE_OFF || e->type == CLAP_EVENT_NOTE_CHOKE) gate = false;
        if (e->type == CLAP_EVENT_MIDI) {
            const auto* m = reinterpret_cast<const clap_event_midi_t*>(e);
            if ((m->data[0] & 0xf0) == 0x90) gate = m->data[2] > 0;
            if ((m->data[0] & 0xf0) == 0x80) gate = false;
        }
        if (e->type == CLAP_EVENT_PARAM_VALUE) {
            const auto* value = reinterpret_cast<const clap_event_param_value_t*>(e);
            if (value->param_id == 7) {
                gain = value->value; if (value->cookie != this) ++threadErrors;
                if (gain > .9) { host->request_restart(host); host->request_callback(host); }
            }
        }
    }
    static clap_process_status process(const clap_plugin_t* p, const clap_process_t* b) {
        auto& s = get(p); s.audio(); ++processCalls;
        if (!s.active || !s.processing || !b->frames_count || b->frames_count > s.maxFrames) ++threadErrors;
        if (s.kind == 4) return CLAP_PROCESS_ERROR;
        uint32_t event = 0;
        const auto count = b->in_events->size(b->in_events);
        for (uint32_t i = 0; i < b->frames_count; ++i) {
            while (event < count) {
                const auto* e = b->in_events->get(b->in_events, event);
                if (e->time < i || e->time >= b->frames_count) ++threadErrors;
                if (e->time > i) break;
                s.event(e); ++event;
            }
            for (uint32_t c = 0; c < b->audio_outputs[0].channel_count; ++c) {
                float value = s.gate ? static_cast<float>(s.gain) : 0.f;
                if (s.kind == 1) value = b->audio_inputs[0].data32[c][i] * static_cast<float>(s.gain);
                if (s.kind == 3) value = std::numeric_limits<float>::quiet_NaN();
                b->audio_outputs[0].data32[c][i] = value;
            }
        }
        clap_event_param_value_t value{};
        value.header = {sizeof(value), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
        value.param_id = 7; value.value = s.gain;
        b->out_events->try_push(b->out_events, &value.header);
        return !s.gate && s.kind == 5 ? CLAP_PROCESS_SLEEP : CLAP_PROCESS_CONTINUE;
    }
    static const void* extension(const clap_plugin_t*, const char* id) {
        static const clap_plugin_audio_ports_t audio{
            [](const clap_plugin_t* p, bool input) -> uint32_t { get(p).main(); return input ? (get(p).kind == 1 ? 1 : 0) : 1; },
            [](const clap_plugin_t* p, uint32_t i, bool input, clap_audio_port_info_t* out) {
                auto& s = get(p); s.main(); if (i || (input && s.kind != 1)) return false;
                *out = {}; out->id = 0; std::strcpy(out->name, "Main"); out->flags = CLAP_AUDIO_PORT_IS_MAIN;
                out->channel_count = s.kind == 0 ? 1 : 2; out->port_type = s.kind == 0 ? CLAP_PORT_MONO : CLAP_PORT_STEREO;
                out->in_place_pair = CLAP_INVALID_ID; return true;
            }};
        static const clap_plugin_note_ports_t notes{
            [](const clap_plugin_t*, bool input) -> uint32_t { return input ? 1 : 0; },
            [](const clap_plugin_t* p, uint32_t i, bool input, clap_note_port_info_t* out) {
                if (i || !input) return false;
                *out = {}; out->id = 0;
                out->supported_dialects = get(p).kind == 2 ? CLAP_NOTE_DIALECT_MIDI : CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
                out->preferred_dialect = get(p).kind == 2 ? CLAP_NOTE_DIALECT_MIDI : CLAP_NOTE_DIALECT_CLAP;
                return true;
            }};
        static const clap_plugin_params_t params{
            [](const clap_plugin_t*) -> uint32_t { return 2; },
            [](const clap_plugin_t* p, uint32_t index, clap_param_info_t* out) {
                get(p).main(); if (index > 1) return false;
                *out = {}; out->id = index ? 99 : 7; out->cookie = &get(p);
                std::strcpy(out->name, index ? "Read only" : "Gain");
                out->min_value = 0; out->max_value = 1; out->default_value = .5;
                out->flags = index ? CLAP_PARAM_IS_READONLY : CLAP_PARAM_IS_AUTOMATABLE;
                return true;
            },
            [](const clap_plugin_t* p, clap_id id, double* value) { get(p).main(); if (id != 7 && id != 99) return false; *value = get(p).gain; return true; },
            [](const clap_plugin_t*, clap_id, double value, char* out, uint32_t size) { std::snprintf(out, size, "%.2f", value); return true; },
            [](const clap_plugin_t*, clap_id, const char* text, double* value) { *value = std::atof(text); return true; },
            [](const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t*) {
                get(p).audio(); for (uint32_t i = 0; i < in->size(in); ++i) get(p).event(in->get(in, i));
            }};
        static const clap_plugin_state_t state{
            [](const clap_plugin_t* p, const clap_ostream_t* out) {
                get(p).main();
                // Exercise short writes and a genuinely opaque payload.
                const double value = get(p).gain;
                const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
                for (size_t i = 0; i < sizeof(value); ++i) if (out->write(out, bytes + i, 1) != 1) return false;
                return true;
            },
            [](const clap_plugin_t* p, const clap_istream_t* in) {
                auto& s = get(p); s.main(); if (s.active) ++threadErrors;
                double value = 0;
                if (in->read(in, &value, sizeof(value)) != sizeof(value) || !std::isfinite(value)) return false;
                s.gain = value; return true;
            }};
        static const clap_plugin_latency_t latency{[](const clap_plugin_t*) -> uint32_t { return 13; }};
        static const clap_plugin_timer_support_t timers{[](const clap_plugin_t* p, clap_id id) {
            auto& s = get(p); s.main();
            auto* hostTimers = static_cast<const clap_host_timer_support_t*>(s.host->get_extension(s.host, CLAP_EXT_TIMER_SUPPORT));
            if (!hostTimers->unregister_timer(s.host, id)) ++threadErrors;
            ++callbackCalls;
        }};
        if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &audio;
        if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS)) return &notes;
        if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &params;
        if (!std::strcmp(id, CLAP_EXT_STATE)) return &state;
        if (!std::strcmp(id, CLAP_EXT_LATENCY)) return &latency;
        if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT)) return &timers;
        return nullptr;
    }
};
const clap_plugin_factory_t factory{
    [](const clap_plugin_factory_t*) -> uint32_t { return 8; },
    [](const clap_plugin_factory_t*, uint32_t index) -> const clap_plugin_descriptor_t* { return index < 8 ? &descriptors[index] : nullptr; },
    [](const clap_plugin_factory_t*, const clap_host_t* host, const char* id) -> const clap_plugin_t* {
        for (int i = 0; i < 8; ++i) if (!std::strcmp(id, ids[i])) return &(new Plugin(host, i))->api;
        return nullptr;
    }};
}
extern "C" {
CLAP_EXPORT int fixture_thread_errors() { return threadErrors.load(); }
CLAP_EXPORT int fixture_entry_users() { return entryUsers.load(); }
CLAP_EXPORT int fixture_process_calls() { return processCalls.load(); }
CLAP_EXPORT int fixture_callback_calls() { return callbackCalls.load(); }
CLAP_EXPORT const clap_plugin_entry_t clap_entry{
    CLAP_VERSION,
    [](const char*) {
#ifdef FIXTURE_crash
        std::abort();
#elif defined(FIXTURE_hang)
        std::this_thread::sleep_for(std::chrono::seconds(20));
#endif
        ++entryUsers; return true;
    },
    [] { --entryUsers; },
    [](const char* id) -> const void* { return !std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? &factory : nullptr; }};
}
