// SPDX-License-Identifier: AGPL-3.0-or-later
#include <kick_clap/Host.h>
#include <kick_clap/Discovery.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <thread>
#ifndef _WIN32
#include <dlfcn.h>
#endif
using namespace kick::clap;
static thread_local bool forbidAllocation = false;
void* operator new(std::size_t size) {
    if (forbidAllocation) { std::fputs("Heap allocation during rendering\n", stderr); std::abort(); }
    if (auto* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
static void check(bool ok, const char* why) { if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
static bool render(Host& h, float* const* out, uint32_t n, const Event* e = nullptr, uint32_t count = 0,
                   const float* const* in = nullptr, uint32_t ni = 0) {
    forbidAllocation = true;
    auto ok = h.process(in, ni, out, 2, n, e, count);
    forbidAllocation = false;
    return ok;
}
int main(int argc, char** argv) {
    check(argc == 5, "fixture paths");
#ifndef _WIN32
    auto diagnostics = dlopen(argv[1], RTLD_NOW);
    check(diagnostics != nullptr, "keep fixture diagnostics alive through all unloads");
#endif
    const auto catalog = inspect(argv[1]);
    check(catalog.failures.empty() && catalog.plugins.size() == 8, "enumerate every descriptor in one binary");
    ScanResult decoded;
    check(decodeScan(encodeScan(catalog), decoded) && decoded.plugins[2].id == catalog.plugins[2].id, "catalog wire roundtrip");
    check(!decodeScan("broken", decoded) && !decodeScan(encodeScan(catalog).substr(0, 20), decoded), "reject malformed scanner output");
    auto isolated = scan({argv[1]}, argv[2]);
    check(isolated.plugins.size() == 8 && isolated.failures.empty(), "isolated scanner enumeration");
    check(scan({argv[3]}, argv[2], 1000).failures.size() == 1, "scanner crash isolation");
    check(scan({argv[4]}, argv[2], 100).failures.size() == 1, "scanner timeout isolation");

    Host h;
    check(h.load(catalog.plugins[0], 48000, 128), "load exact instrument ID");
    check(h.parameters().size() == 2 && h.latency() == 13 && h.supportsState(), "parameters, latency, state");
    h.service();
    std::this_thread::sleep_for(std::chrono::milliseconds(2)); h.service();
    std::array<float, 1025> left{}, right{};
    float* out[]{left.data(), right.data()};
    Event events[]{Event::midi(0x90, 60, 100, 17), Event::midi(0x80, 60, 0, 700)};
    check(render(h, out, 1025, events, 2), "split oversized blocks and translate MIDI to native CLAP notes");
    for (int i = 0; i < 1025; ++i) check(left[i] == ((i >= 17 && i < 700) ? .5f : 0.f) && left[i] == right[i], "sample offsets and mono-to-stereo routing");
    check(!h.setParameter(99, .2) && !h.setParameter(7, std::numeric_limits<double>::quiet_NaN()), "reject read-only and nonfinite parameters");
    check(h.setParameter(7, .25), "queue parameter");
    std::vector<uint8_t> saved;
    check(h.saveState(saved) && saved.size() == sizeof(double), "save queued parameters before next callback");
    Host second;
    check(second.load(catalog.plugins[0], 48000, 64, saved), "restore opaque state on another instance");
    check(std::abs(second.parameterValue(7) - .25) < 1e-9, "parameter restored from plugin state");
    check(second.parameterText(7, .25) == "0.25", "plugin value formatting");
    check(second.enqueue(Event::note(true, 60, .8)), "enqueue native note");
    std::thread audio([&] { check(render(second, out, 1025), "render on separate symbolic audio thread"); });
    audio.join();
    check(left[0] == .25f, "restored synth audible");
    second.panic(); check(render(second, out, 128) && left[0] == 0.f, "panic kills held notes");
    check(second.loadState(saved), "restore on activated plugin");
    check(second.reconfigure(44100, 256), "sample-rate and buffer reconfiguration");

    // Buffers may alias when the host is an insert effect.
    check(h.load(catalog.plugins[1], 48000, 128), "load effect descriptor");
    left.fill(.4f); right.fill(-.8f);
    const float* input[]{left.data(), right.data()};
    check(render(h, out, 1025, nullptr, 0, input, 2), "effect audio input and aliased output");
    for (size_t i = 0; i < left.size(); ++i) check(left[i] == .2f && right[i] == -.4f, "effect channel mapping");
    check(h.load(catalog.plugins[2], 48000, 128), "MIDI-only plugin");
    Event note = Event::note(true, 64, .8);
    check(render(h, out, 128, &note, 1) && left[0] == .5f, "native notes translated for MIDI-only ports");
    check(h.setParameter(7, .95), "request restart parameter");
    check(render(h, out, 128), "plugin requests restart from processing");
    h.service();
    check(render(h, out, 128) && !h.faulted(), "restart callback deactivates and reactivates correctly");

    std::atomic<bool> running{true};
    std::thread reader([&] { while (running.load()) render(h, out, 128); });
    for (int i = 0; i < 20; ++i) { h.setParameter(7, .1 + i * .01); h.service(); check(h.saveState(saved), "state while callback running"); }
    running.store(false); reader.join();
    check(h.statistics().processErrors == 0, "concurrent control/render remains valid");
    std::array<std::thread, 4> producers;
    std::atomic<int> rejected{0};
    for (auto& producer : producers) producer = std::thread([&] {
        for (int i = 0; i < 200; ++i) {
            if (!h.enqueue(Event::note(true, 60, .8))) ++rejected;
            if (!h.enqueue(Event::note(false, 60, 0))) ++rejected;
        }
    });
    for (auto& producer : producers) producer.join();
    check(rejected.load() == 0 && render(h, out, 128) && left[0] == 0.f, "bounded multi-producer event publication");
    check(!h.enqueue(Event::note(true, 200, .8)), "reject invalid note key");
    // Overflow is bounded and forces a reset rather than losing note-off forever.
    for (uint32_t i = 0; i < Host::maxEvents + 10; ++i) h.enqueue(note);
    check(render(h, out, 128) && left[0] == 0.f && h.statistics().droppedEvents > 0, "queue overflow panic");
    check(h.load(catalog.plugins[3], 48000, 128), "load NaN fixture");
    check(!render(h, out, 128) && h.faulted(), "nonfinite audio faults and silences instance");
    for (size_t i = 0; i < 128; ++i) check(left[i] == 0.f && right[i] == 0.f, "fault output cleared");
    check(h.load(catalog.plugins[4], 48000, 128), "load error fixture");
    check(!render(h, out, 128) && h.faulted(), "process status error handled");
    check(h.load(catalog.plugins[5], 48000, 128), "load sleep fixture");
    check(render(h, out, 128), "sleeping plugin first block");
    auto before = h.statistics().blocks;
    check(render(h, out, 128) && h.statistics().blocks == before, "sleep skips idle DSP");
    check(h.enqueue(note) && render(h, out, 128) && left[0] == .5f, "event wakes sleeping plugin");
    check(!h.load(catalog.plugins[6], 48000, 128) && !h.loaded(), "failed activation cleans up");
    check(h.load(catalog.plugins[7], 48000, 128) && !render(h, out, 128), "failed start faults safely");
    auto missing = catalog.plugins[0]; missing.id = "missing.id";
    check(!h.load(missing, 48000, 128), "do not substitute another descriptor for missing saved ID");
    second.unload(); h.unload();
#ifndef _WIN32
    auto errors = reinterpret_cast<int(*)()>(dlsym(diagnostics, "fixture_thread_errors"));
    check(errors && errors() == 0, "all lifecycle and event calls satisfy fixture contracts");
    auto users = reinterpret_cast<int(*)()>(dlsym(diagnostics, "fixture_entry_users"));
    check(users && users() == 0, "balanced per-binary entry init/deinit across instances");
    dlclose(diagnostics);
#endif
    std::cout << "PASS: discovery isolation, descriptors, ports, note dialects, event offsets, state, parameters, lifecycle, concurrent control, overflow, sleep, errors, allocation-free rendering\n";
}
