// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "Benchmark.h"
#include <algorithm>
#include <cmath>
#include <juce_audio_devices/juce_audio_devices.h>
namespace kick::clap::bench {
// No allocation, logging, device queries or locks in our render callback.
class DeviceRender final : public juce::AudioIODeviceCallback {
public:
    explicit DeviceRender(Workload& target) : workload(target) {}
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* output, int channels, int frames,
                                          const juce::AudioIODeviceCallbackContext&) override {
        if (channels < 0 || frames < 0) { deviceError.store(true); return; }
        if (deviceError.load()) {
            if (output) for (int c = 0; c < channels; ++c) if (output[c]) std::fill_n(output[c], frames, 0.f);
            return;
        }
        workload.render(output, static_cast<uint32_t>(channels), static_cast<uint32_t>(frames));
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
        stopped.store(false);
        if (!device || std::abs(device->getCurrentSampleRate() - workload.options().sampleRate) > .1 ||
            device->getCurrentBufferSizeSamples() != static_cast<int>(workload.options().frames)) deviceError.store(true);
    }
    void audioDeviceStopped() override { stopped.store(true); }
    void audioDeviceError(const juce::String&) override { deviceError.store(true); }
    std::atomic<bool> deviceError{false}, stopped{false};
private:
    Workload& workload;
};
} // namespace kick::clap::bench
