// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * File: AudIO/processors/ClapInstrumentProcessor.cpp
 * Project: Behringer KICK (0700-AAM) — Standalone CLAP Synth Workstation Host
 * Author: JJ Simmons / Dream Maker 5 (@FYD5IVE)
 * Copyright (C) 2026 Dream Maker 5 (@FYD5IVE). All Rights Reserved.
 */

#include "ClapInstrumentProcessor.hpp"
#include <algorithm>

namespace AudIO {

static const void* getHostExtension(const clap_host_t*, const char*) noexcept { return nullptr; }
static void requestRestart(const clap_host_t*) noexcept {}
static void requestProcess(const clap_host_t*) noexcept {}
static void requestCallback(const clap_host_t*) noexcept {}

ClapInstrumentProcessor::ClapInstrumentProcessor() noexcept {
    hostDescriptor_.clap_version = CLAP_VERSION_INIT;
    hostDescriptor_.host_data = this;
    hostDescriptor_.name = "Behringer KICK 0700-AAM AudIO CLAP Host";
    hostDescriptor_.vendor = "Music Tribe / Dream Maker 5 (@FYD5IVE)";
    hostDescriptor_.url = "https://github.com/MusicTribeOpenSource/0700-AAM";
    hostDescriptor_.version = "1.0.0";
    hostDescriptor_.get_extension = getHostExtension;
    hostDescriptor_.request_restart = requestRestart;
    hostDescriptor_.request_process = requestProcess;
    hostDescriptor_.request_callback = requestCallback;

    clapInEvents_.ctx = this;
    clapInEvents_.size = &ClapInstrumentProcessor::clapInputEventsSize;
    clapInEvents_.get = &ClapInstrumentProcessor::clapInputEventsGet;

    clapOutEvents_.ctx = this;
    clapOutEvents_.try_push = [](const struct clap_output_events*, const clap_event_header_t*) noexcept {
        return true;
    };
}

ClapInstrumentProcessor::~ClapInstrumentProcessor() noexcept {
    unloadPlugin();
}

bool ClapInstrumentProcessor::loadPluginFromSdCard(const char* clapBinaryPath,
                                                   const char* pluginId,
                                                   double sampleRate,
                                                   uint32_t maxBlockSize) noexcept {
    unloadPlugin();

    // Load native Linux aarch64 .clap shared object from /mnt/sdcard/Plugins/CLAP/
    dsoHandle_ = dlopen(clapBinaryPath, RTLD_LOCAL | RTLD_NOW);
    if (!dsoHandle_) return false;

    pluginEntry_ = reinterpret_cast<const clap_plugin_entry_t*>(dlsym(dsoHandle_, "clap_entry"));
    if (!pluginEntry_ || !pluginEntry_->init(clapBinaryPath)) {
        unloadPlugin();
        return false;
    }

    const auto* factory = static_cast<const clap_plugin_factory_t*>(
        pluginEntry_->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory) {
        unloadPlugin();
        return false;
    }

    plugin_ = factory->create_plugin(factory, &hostDescriptor_, pluginId);
    if (!plugin_ || !plugin_->init(plugin_)) {
        unloadPlugin();
        return false;
    }

    bindMacroParameters();

    if (!plugin_->activate(plugin_, sampleRate, 1, std::min<uint32_t>(maxBlockSize, kMaxBlockSize))) {
        unloadPlugin();
        return false;
    }

    isActive_.store(true, std::memory_order_release);
    return true;
}

void ClapInstrumentProcessor::unloadPlugin() noexcept {
    isActive_.store(false, std::memory_order_release);
    if (plugin_) {
        plugin_->deactivate(plugin_);
        plugin_->destroy(plugin_);
        plugin_ = nullptr;
    }
    if (pluginEntry_) {
        pluginEntry_->deinit();
        pluginEntry_ = nullptr;
    }
    if (dsoHandle_) {
        dlclose(dsoHandle_);
        dsoHandle_ = nullptr;
    }
    paramsExt_ = nullptr;
}

void ClapInstrumentProcessor::bindMacroParameters() noexcept {
    macroParamIds_.fill(CLAP_INVALID_ID);
    paramsExt_ = static_cast<const clap_plugin_params_t*>(
        plugin_->get_extension(plugin_, CLAP_EXT_PARAMS));
    if (!paramsExt_) return;

    const uint32_t count = paramsExt_->count(plugin_);
    const uint32_t bindCount = std::min<uint32_t>(count, kNumMacros);
    for (uint32_t i = 0; i < bindCount; ++i) {
        clap_param_info_t info{};
        if (paramsExt_->get_info(plugin_, i, &info)) {
            macroParamIds_[i] = info.id;
        }
    }
}

uint32_t ClapInstrumentProcessor::clapInputEventsSize(const struct clap_input_events* list) noexcept {
    const auto* self = static_cast<const ClapInstrumentProcessor*>(list->ctx);
    return self->rtBlockEventCount_;
}

const clap_event_header_t* ClapInstrumentProcessor::clapInputEventsGet(
    const struct clap_input_events* list, uint32_t index) noexcept {
    const auto* self = static_cast<const ClapInstrumentProcessor*>(list->ctx);
    return &self->rtBlockEvents_[index].header;
}

void ClapInstrumentProcessor::processAudioBlock(const float* inLeft, const float* inRight,
                                                float* outLeft, float* outRight,
                                                float* cvOut1, float* cvOut2,
                                                float* cvOut3, float* cvOut4,
                                                uint32_t numFrames) noexcept {
    const uint32_t frames = std::min<uint32_t>(numFrames, kMaxBlockSize);

    if (!isActive_.load(std::memory_order_acquire) || !plugin_) {
        std::memset(outLeft, 0, frames * sizeof(float));
        std::memset(outRight, 0, frames * sizeof(float));
        return;
    }

    // 1. Drain lock-free ComProtocol / External MIDI queue into pre-allocated CLAP event list
    rtBlockEventCount_ = 0;
    KickHardwareSynthEvent hwEvt{};
    float latestCvCutoff = 0.0f;
    float latestCvResonance = 0.0f;

    while (rtBlockEventCount_ < kEventQueueCapacity && incomingEvents_.pop(hwEvt)) {
        auto& slot = rtBlockEvents_[rtBlockEventCount_];
        switch (hwEvt.type) {
            case KickHardwareSynthEvent::Type::NoteOn:
            case KickHardwareSynthEvent::Type::NoteOff: {
                slot.note.header.size = sizeof(clap_event_note_t);
                slot.note.header.time = 0;
                slot.note.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                slot.note.header.type = (hwEvt.type == KickHardwareSynthEvent::Type::NoteOn)
                                            ? CLAP_EVENT_NOTE_ON
                                            : CLAP_EVENT_NOTE_OFF;
                slot.note.header.flags = 0;
                slot.note.note_id = -1;
                slot.note.port_index = 0;
                slot.note.channel = hwEvt.channel;
                slot.note.key = hwEvt.keyOrMacroIndex;
                slot.note.velocity = static_cast<double>(hwEvt.normalizedValue);
                ++rtBlockEventCount_;
                break;
            }
            case KickHardwareSynthEvent::Type::MacroEncoderChange: {
                const uint8_t bankOffset = activeEncoderBank_.load(std::memory_order_relaxed) * 4;
                const uint8_t macroIdx = (hwEvt.keyOrMacroIndex & 0x03) + bankOffset;
                const clap_id paramId = macroParamIds_[macroIdx];
                if (paramId != CLAP_INVALID_ID) {
                    slot.paramValue.header.size = sizeof(clap_event_param_value_t);
                    slot.paramValue.header.time = 0;
                    slot.paramValue.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                    slot.paramValue.header.type = CLAP_EVENT_PARAM_VALUE;
                    slot.paramValue.header.flags = 0;
                    slot.paramValue.param_id = paramId;
                    slot.paramValue.cookie = nullptr;
                    slot.paramValue.note_id = -1;
                    slot.paramValue.port_index = 0;
                    slot.paramValue.channel = -1;
                    slot.paramValue.key = -1;
                    slot.paramValue.value = static_cast<double>(hwEvt.normalizedValue);
                    ++rtBlockEventCount_;
                }
                if (macroIdx == 0) latestCvCutoff = hwEvt.normalizedValue;
                if (macroIdx == 1) latestCvResonance = hwEvt.normalizedValue;
                break;
            }
            case KickHardwareSynthEvent::Type::TouchStripMod:
            case KickHardwareSynthEvent::Type::OpticalGestureXY: {
                const clap_id targetParam = macroParamIds_[0];
                if (targetParam != CLAP_INVALID_ID) {
                    slot.paramMod.header.size = sizeof(clap_event_param_mod_t);
                    slot.paramMod.header.time = 0;
                    slot.paramMod.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                    slot.paramMod.header.type = CLAP_EVENT_PARAM_MOD;
                    slot.paramMod.header.flags = 0;
                    slot.paramMod.param_id = targetParam;
                    slot.paramMod.cookie = nullptr;
                    slot.paramMod.note_id = -1;
                    slot.paramMod.port_index = 0;
                    slot.paramMod.channel = -1;
                    slot.paramMod.key = -1;
                    slot.paramMod.amount = static_cast<double>(hwEvt.normalizedValue);
                    ++rtBlockEventCount_;
                }
                break;
            }
        }
    }

    // 2. Execute CLAP DSP Process Block
    float* outChannels[2] = { synthScratchL_.data(), synthScratchR_.data() };
    clap_audio_buffer_t clapOutBuf{};
    clapOutBuf.data32 = outChannels;
    clapOutBuf.channel_count = 2;
    clapOutBuf.latency = 0;
    clapOutBuf.constant_mask = 0;

    clap_process_t processData{};
    processData.steady_time = -1;
    processData.frames_count = frames;
    processData.transport = nullptr;
    processData.audio_inputs = nullptr;
    processData.audio_inputs_count = 0;
    processData.audio_outputs = &clapOutBuf;
    processData.audio_outputs_count = 1;
    processData.in_events = &clapInEvents_;
    processData.out_events = &clapOutEvents_;

    plugin_->process(plugin_, &processData);

    // 3. Drive 4x Hardware CV/Gate Outputs & Hybrid Analog VCF/VCA Return Loop
    if (cvOut1 && cvOut2) {
        std::fill_n(cvOut1, frames, latestCvCutoff * 5.0f); // 0-5V Filter Cutoff CV
        std::fill_n(cvOut2, frames, latestCvResonance * 5.0f); // 0-5V Filter Resonance CV
    }
    if (cvOut3 && cvOut4) {
        std::memset(cvOut3, 0, frames * sizeof(float));
        std::memset(cvOut4, 0, frames * sizeof(float));
    }

    if (hybridAnalogLoopEnabled_.load(std::memory_order_relaxed) && inLeft && inRight) {
        // Route digital synth out to external analog filter & sum returned Stereo Line In
        for (uint32_t i = 0; i < frames; ++i) {
            outLeft[i] = synthScratchL_[i] + inLeft[i];
            outRight[i] = synthScratchR_[i] + inRight[i];
        }
    } else {
        std::memcpy(outLeft, synthScratchL_.data(), frames * sizeof(float));
        std::memcpy(outRight, synthScratchR_.data(), frames * sizeof(float));
    }
}

} // namespace AudIO
