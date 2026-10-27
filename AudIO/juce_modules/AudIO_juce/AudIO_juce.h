/*
 BEGIN_JUCE_MODULE_DECLARATION
  ID: AudIO_juce
  vendor: KICK
  version: 0.1.0
  name: AudIO JUCE device adapter
  description: Released AudIO engine and JUCE audio device integration
  license: AGPL-3.0-or-later
  dependencies: AudIO_constants, juce_audio_devices, juce_audio_formats, juce_dsp, juce_audio_utils
 END_JUCE_MODULE_DECLARATION
*/
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <AudIO_constants/AudIO_constants.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <AudIO/AudIO.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Recorder.h"
#include "DspAdapters.h"

namespace mt {
// The engine pool has a fixed block size. The adapter splits device callbacks
// into that size using preallocated storage, independent of driver buffer size.
class AudioDevice final : public AudIO::IAudioDevice, private juce::AudioIODeviceCallback {
public:
    AudioDevice(uint32_t inputs, uint32_t outputs, const juce::String& preferred);
    ~AudioDevice() override;
    void start(uint32_t rate, uint32_t frames) override;
    void resume() override;
    void pause() override;
    void stop() override;
    juce::AudioDeviceManager& getJuceAudioDeviceMgr() { return devices; }
    const juce::String& lastError() const noexcept { return error; }
private:
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                          const juce::AudioIODeviceCallbackContext&) override;
    juce::AudioDeviceManager devices;
    juce::String preferred, error;
    uint32_t inputs, outputs, engineFrames = 0;
    juce::AudioBuffer<float> inputBlock, outputBlock;
    uint32_t filled = 0;
    std::atomic<bool> resetPipeline{false};
};
}
