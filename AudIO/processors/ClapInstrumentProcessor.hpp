// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * File: AudIO/processors/ClapInstrumentProcessor.hpp
 * Project: Behringer KICK (0700-AAM) — Standalone CLAP Synth Workstation Host
 * Author: JJ Simmons / Dream Maker 5 (@FYD5IVE)
 * Copyright (C) 2026 Dream Maker 5 (@FYD5IVE). All Rights Reserved.
 *
 * OPEN-SOURCE & COMMERCIAL DUAL-LICENSING NOTICE:
 * Layer 1 open interop foundation is licensed under GNU AGPL-3.0-or-later.
 * Closed-source OEM firmware integration, proprietary multi-SKU hardware bundling,
 * or Layer 2 commercial distribution requires a separate commercial OEM license
 * from Dream Maker 5 (@FYD5IVE): https://github.com/FYD5IVE
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
 * - Layer 1 AudiomParamEvent 256-slot SPSC queue & .mancer.json v0.1 interop hooks
 * - Hybrid Analog Loop (Stereo Out + 4x CV/Gate -> External Analog VCF/VCA -> Stereo Line In)
 */

#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <dlfcn.h>

// MIT-Licensed CLAP C ABI headers (AudIO/third_party/clap/include/clap/clap.h)
#include "clap/clap.h"

namespace AudIO {

// Fixed-capacity SPSC Lock-Free Ring Buffer for Real-Time Event Bridging
// Capacity = 256 yields deterministic FIFO delivery up to 255 pending events
// with non-blocking overflow protection and 0-byte heap allocation in RT thread.
template <typename T, size_t Capacity>
class LockFreeEventRingBuffer {
public:
    bool push(const T& item) noexcept {
        const size_t currentTail = tail_.load(std::memory_order_relaxed);
        const size_t nextTail = (currentTail + 1) % Capacity;
        if (nextTail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer full (Capacity - 1 pending events), drop safely without blocking RT thread
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

    // Standard non-blocking aliases for Layer 1 / Layer 2 interop callers
    bool tryPush(const T& item) noexcept { return push(item); }
    bool tryPop(T& item) noexcept { return pop(item); }

    void clear() noexcept {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

private:
    std::array<T, Capacity> buffer_{};
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
};

// Layer 1 Open Interop Source Bitmask Flags (The Audiom Matrix)
enum AudiomSourceFlags : uint8_t {
    SourceFlag_None       = 0x00,
    SourceFlag_Encoder    = 0x01, // Endless Encoders (Bank A: X1..Y2 / Bank B: X3..Y4)
    SourceFlag_PadPolyAT  = 0x02, // 16 RGB Pads & Polyphonic Aftertouch
    SourceFlag_TouchStrip = 0x04, // Capacitive Touch Strip / Expressive Ribbon Bus
    SourceFlag_UI_Layer2  = 0x08  // Non-RT Layer 2 UI & ComProtocol Dispatcher
};

// Layer 1 Normalized Parameter & Modulation Event Payload (POD, 0-byte RT heap)
struct alignas(8) AudiomParamEvent {
    clap_id  paramId{CLAP_INVALID_ID}; // Target CLAP parameter ID
    float    normalizedValue{0.0f};    // Normalized [0.0, 1.0] value or [-1.0, 1.0] modulation offset
    uint32_t sampleOffset{0};          // Sample-accurate offset within current audio block
    int16_t  key{-1};                  // Target MIDI key for polyphonic modulation (-1 = global)
    int16_t  channel{-1};              // Target MIDI channel (-1 = all)
    uint8_t  sourceFlags{SourceFlag_None}; // Bitwise OR of AudiomSourceFlags
    uint8_t  isModulation{0};          // 0 = CLAP_EVENT_PARAM_VALUE, 1 = CLAP_EVENT_PARAM_MOD
    uint8_t  macroSlot{0xFF};          // Optional physical macro slot 0..7 (0xFF = direct paramId)
    uint8_t  cvTargetMask{0};          // Bit 0: 0-5V Cutoff CV1, Bit 1: 0-5V Resonance CV2
};

static_assert(std::is_trivially_copyable_v<AudiomParamEvent>,
              "AudiomParamEvent must remain trivially copyable for lock-free SPSC transport.");

// Unified Hardware / MIDI Control Event passed from ComProtocol & External MIDI threads
struct KickHardwareSynthEvent {
    enum class Type : uint8_t {
        NoteOn,
        NoteOff,
        MacroEncoderChange, // Encoders 1-4 (Bank A: 0-3, Bank B: 4-7)
        TouchStripMod,      // Pitch Bend or Spline Morph
        OpticalGestureXY    // AI Camera 2D X/Y Polyphonic Modulation
    };

    Type type;
    uint8_t channel{0};
    uint8_t keyOrMacroIndex{0};  // MIDI Note (0-127) or Macro Index (0-7)
    float normalizedValue{0.0f}; // Velocity (0.0-1.0) or Parameter Value
    float secondaryValue{0.0f};  // Used for Y-axis on OpticalGestureXY
};

class ClapInstrumentProcessor {
public:
    static constexpr size_t kMaxBlockSize = 1024;
    static constexpr size_t kEventQueueCapacity = 512;
    static constexpr size_t kParamEventQueueCapacity = 256;
    static constexpr size_t kMaxCachedParams = 512;
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

    // Layer 1 Non-RT Message Thread Accessors (Lock-Free Descriptor Queries & Event Push)
    uint32_t getPluginParamCount() const noexcept {
        return cachedParamCount_.load(std::memory_order_acquire);
    }

    bool queryPluginParamInfo(uint32_t paramIndex, clap_param_info_t* outInfo) const noexcept {
        if (!outInfo) return false;
        const uint32_t count = cachedParamCount_.load(std::memory_order_acquire);
        if (paramIndex >= count || paramIndex >= kMaxCachedParams) {
            return false;
        }
        *outInfo = cachedParamDescriptors_[paramIndex];
        return true;
    }

    bool pushNormalizedParamEvent(const AudiomParamEvent& ev) noexcept {
        return m_paramEventQueue.push(ev);
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
    double denormalizeParamValue(clap_id paramId, float normalized) const noexcept;
    double scaleParamModAmount(clap_id paramId, float normalizedOffset) const noexcept;

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

    // Pre-cached CLAP parameter descriptor table populated during initialization/activate()
    std::array<clap_param_info_t, kMaxCachedParams> cachedParamDescriptors_{};
    std::atomic<uint32_t> cachedParamCount_{0};

    // Pre-allocated CLAP parameter IDs bound to the 8 hardware macros
    std::array<clap_id, kNumMacros> macroParamIds_{};

    // Persistent 0-5V analog CV Cutoff & Resonance states across audio blocks
    float lastCvCutoffNormalized_{0.0f};
    float lastCvResonanceNormalized_{0.0f};

    // Lock-free SPSC queues from ComProtocol / MIDI / Layer 2 UI -> AudIO RT thread
    LockFreeEventRingBuffer<KickHardwareSynthEvent, kEventQueueCapacity> incomingEvents_;
    LockFreeEventRingBuffer<AudiomParamEvent, kParamEventQueueCapacity> m_paramEventQueue;

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
