// SPDX-License-Identifier: AGPL-3.0-or-later
#include <AudIO_juce/AudIO_juce.h>
#include <AudIO_juce/Recorder.cpp>
#include <AudIO_juce/DeviceBlocks.h>

namespace mt {
AudioDevice::AudioDevice(uint32_t in, uint32_t out, const juce::String& name)
    : IAudioDevice(in, out), preferred(name), inputs(in), outputs(out) {}
AudioDevice::~AudioDevice() { stop(); }
void AudioDevice::start(uint32_t rate, uint32_t frames) {
    stop();
    error.clear();
    if (juce::SystemStats::getEnvironmentVariable("KICK_NO_AUDIO_DEVICE", {}) == "1") return;
    // AudIO's graph memory is allocated by DawEngine before device creation.
    engineFrames = AudIO::MemoryPool<float>::get().getBlockSize();
    if (!engineFrames) { error = "AudIO memory pool is not initialized"; return; }
    inputBlock.setSize(static_cast<int>(inputs), static_cast<int>(engineFrames));
    outputBlock.setSize(static_cast<int>(outputs), static_cast<int>(engineFrames));
    inputBlock.clear(); outputBlock.clear(); filled = 0;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.sampleRate = rate; setup.bufferSize = static_cast<int>(frames); setup.outputDeviceName = preferred;
    error = devices.initialise(static_cast<int>(inputs), static_cast<int>(outputs), nullptr, false, {}, &setup);
    if (error.isNotEmpty()) { setState(AudIO::AudioDeviceState::stopped); return; }
    devices.addAudioCallback(this); resume();
}
void AudioDevice::resume() {
    if (!devices.getCurrentAudioDevice() || error.isNotEmpty()) return;
    resetPipeline.store(true); setState(AudIO::AudioDeviceState::playing);
}
void AudioDevice::pause() { setState(AudIO::AudioDeviceState::paused); }
void AudioDevice::stop() {
    devices.removeAudioCallback(this); devices.closeAudioDevice();
    setState(AudIO::AudioDeviceState::stopped);
}
void AudioDevice::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    filled = 0; inputBlock.clear(); outputBlock.clear();
    prepareToPlay(static_cast<uint32_t>(device->getCurrentSampleRate()), engineFrames);
}
void AudioDevice::audioDeviceStopped() { teardown(); }
void AudioDevice::audioDeviceIOCallbackWithContext(const float* const* in, int ni, float* const* out, int no, int frames,
                                                  const juce::AudioIODeviceCallbackContext&) {
    for (int c = 0; c < no; ++c) if (out[c]) std::fill_n(out[c], frames, 0.f);
    if (!isPlaying() || !engineFrames) return;
    if (resetPipeline.exchange(false)) { filled = 0; inputBlock.clear(); outputBlock.clear(); }
    // One engine-block pipeline: complete input/output chunks cross arbitrary
    // callback boundaries without resizing or writing past graph pool buffers.
    detail::deviceBlocks(in, ni, out, no, frames, inputBlock, outputBlock, filled,
        [this](const auto& inputBlock, auto& outputBlock) noexcept {
            for (uint32_t c = 0; c < inputs; ++c) if (auto* dst = _buffer.getInputBuffer(c)) std::copy_n(inputBlock.getReadPointer(static_cast<int>(c)), engineFrames, dst);
            _buffer.clearOutputs(); process(_buffer);
            for (uint32_t c = 0; c < outputs; ++c) {
                auto* dst = outputBlock.getWritePointer(static_cast<int>(c));
                if (auto* src = _buffer.getOutputBuffer(c)) std::copy_n(src, engineFrames, dst); else std::fill_n(dst, engineFrames, 0.f);
            }
        });
}
}
