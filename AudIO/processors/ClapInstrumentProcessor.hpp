// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * File: AudIO/processors/ClapInstrumentProcessor.hpp
 * Project: Behringer KICK (0700-AAM) — Standalone CLAP Synth Workstation Host
 * Author: JJ Simmons / Dream Maker 5 (@FYD5IVE)
 * Copyright (C) 2026 Dream Maker 5 (@FYD5IVE). All Rights Reserved.
 *
 * Description:
 * Real-time safe, lock-free CLAP (free-audio/clap, MIT License) Instrument
 * & Effect Host Processor for the 0700-AAM AudIO engine on Rockchip RK3588
 * (aarch64 Linux). Dynamically loads native ARM64 .clap synthesizer binaries
 * (e.g., u-he Zebra 3 / Zebralette 3, Surge XT, Vital) from user storage
 * (/mnt/sdcard/Plugins/CLAP/) and bridges ComProtocol hardware controls:
 * - External 5-Pin DIN / USB-A Host MIDI Keyboards + 16 RGB Pads
 * - 4 Endless Encoders (Bank A: Macros 1-4 / X1,Y1,X2,Y2; Bank B: Macros 5-8)
 * - Capacitive Touch Strip & AI Optical Gesture Camera (2D X/Y Modulation)
 * - Hybrid Analog Loop (Stereo Out + 4x CV/Gate -> External Analog VCF/VCA -> Stereo Line In)
 */

#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <dlfcn.h>

// MIT-Licensed CLAP C ABI headers (AudIO/third_party/clap/include/clap/clap.h)
#include "clap/clap.h"

namespace AudIO {

// Fixed-capacity SPSC Lock-Free Ring Buffer for Real-Time Event Bridging
template <typename T, size_t Capacity>
class LockFreeEventRingBuffer {
public:
    bool push(const T& item) noexcept {
        const size_t currentTail = tail_.load(std::memory_order_relaxed);
        const size_t nextTail = (currentTail + 1) % Capacity;
        if (nextTail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer full, drop event safely without blocking RT thread
        }
        buffer_[currentTail] = item;
        tail_.store(nextTail, std::memory_order_release);
        return true;
    }

    bool pop(T& item) noexcept {
        const size_t currentHead = head_.load(std::memory_order_relaxed);
        if (currentHead == tail_.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }
        item = buffer_[currentHead];
        head_.store((currentHead + 1) % Capacity, std::memory_order_release);
        return true;
    }

    void clear() noexcept {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

private:
    std::array<T, Capacity> buffer_{};
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};

// Unified Hardware / MIDI Control Event passed from ComProtocol & External MIDI threads
struct KickHardwareSynthEvent {
    enum class Type : uint8_t {
        NoteOn,
        NoteOff,
        MacroEncoderChange, // Encoders 1-4 (Bank A: 0-3, Bank B: 4-7)
        TouchStripMod, // Pitch Bend or Spline Morph
        OpticalGestureXY // AI Camera 2D X/Y Polyphonic Modulation
    };

    Type type;
    uint8_t channel{0};
    uint8_t keyOrMacroIndex{0}; // MIDI Note (0-127) or Macro Index (0-7)
    float normalizedValue{0.0f}; // Velocity (0.0-1.0) or Parameter Value
    float secondaryValue{0.0f}; // Used for Y-axis on OpticalGestureXY
};

class ClapInstrumentProcessor {
public:
    static constexpr size_t kMaxBlockSize = 1024;
    static constexpr size_t kEventQueueCapacity = 512;
    static constexpr size_t kNumMacros = 8; // Maps 1:1 to u-he X1/Y1 .. X4/Y4

    ClapInstrumentProcessor() noexcept;
    ~ClapInstrumentProcessor() noexcept;

    // Non-RT Lifecycle: Dynamic loading of native aarch64 .clap binary from SD card
    bool loadPluginFromSdCard(const char* clapBinaryPath, const char* pluginId,
                              double sampleRate, uint32_t maxBlockSize) noexcept;
    void unloadPlugin() noexcept;

    // Thread-Safe Hardware & MIDI Ingestion (Called by ComProtocol / MIDI I/O threads)
    bool enqueueHardwareEvent(const KickHardwareSynthEvent& evt) noexcept {
        return incomingEvents_.push(evt);
    }

    void setEncoderBank(uint8_t bankIndex) noexcept {
        activeEncoderBank_.store(bankIndex & 0x01, std::memory_order_relaxed);
    }

    void setHybridExternalFilterLoop(bool enabled) noexcept {
        hybridAnalogLoopEnabled_.store(enabled, std::memory_order_relaxed);
    }

    // Real-Time Audio Thread Entrypoint (Zero allocations, zero locks)
    void processAudioBlock(const float* inLeft, const float* inRight,
                           float* outLeft, float* outRight,
                           float* cvOut1, float* cvOut2,
                           float* cvOut3, float* cvOut4,
                           uint32_t numFrames) noexcept;

private:
    void bindMacroParameters() noexcept;
    static uint32_t clapInputEventsSize(const struct clap_input_events* list) noexcept;
    static const clap_event_header_t* clapInputEventsGet(const struct clap_input_events* list, uint32_t index) noexcept;

    void* dsoHandle_{nullptr};
    const clap_plugin_entry_t* pluginEntry_{nullptr};
    const clap_plugin_t* plugin_{nullptr};
    const clap_plugin_params_t* paramsExt_{nullptr};
    clap_host_t hostDescriptor_{};

    std::atomic<bool> isActive_{false};
    std::atomic<uint8_t> activeEncoderBank_{0}; // 0 = Macros 1-4, 1 = Macros 5-8
    std::atomic<bool> hybridAnalogLoopEnabled_{false};

    // Pre-allocated CLAP parameter IDs bound to the 8 hardware macros
    std::array<clap_id, kNumMacros> macroParamIds_{};

    // Lock-free SPSC queue from ComProtocol / MIDI threads -> AudIO RT thread
    LockFreeEventRingBuffer<KickHardwareSynthEvent, kEventQueueCapacity> incomingEvents_;

    // Pre-allocated RT CLAP event union list for the current audio block
    union ClapEventSlot {
        clap_event_header_t header;
        clap_event_note_t note;
        clap_event_param_value_t paramValue;
        clap_event_param_mod_t paramMod;
    };
    std::array<ClapEventSlot, kEventQueueCapacity> rtBlockEvents_{};
    uint32_t rtBlockEventCount_{0};

    // Pre-allocated scratch buffers for CLAP stereo synthesis before hybrid return
    std::array<float, kMaxBlockSize> synthScratchL_{};
    std::array<float, kMaxBlockSize> synthScratchR_{};

    clap_input_events_t clapInEvents_{};
    clap_output_events_t clapOutEvents_{};
};

} // namespace AudIO
