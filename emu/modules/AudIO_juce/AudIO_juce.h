/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 AudIO_juce
  vendor:             Bombest
  version:            0.1.0
  name:               AudIO JUCE bridge (emulator stand-in)
  description:        Wraps the public AudIO engine and drives it from a JUCE audio device
  website:
  license:            AGPLv3

  dependencies:       AudIO, AudIO_constants, juce_audio_devices, juce_audio_formats, juce_audio_utils

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

/*
 * Stand-in for the private AudIO_juce module. Kick includes this header to get
 * the whole AudIO:: namespace plus mt::AudioDevice, the IAudioDevice that
 * mt_audio_manager.cpp hands to DawEngine::addAudioDevice().
 *
 * On macOS the device is CoreAudio, on Linux it is ALSA, whatever
 * juce::AudioDeviceManager picks. Set KICK_AUDIO_DEVICE to choose an output by
 * name.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#ifndef AUDIO_JUCE_H_INCLUDE
    #define AUDIO_JUCE_H_INCLUDE

    #include <AudIO/AudIO.h>
    #include <AudIO_constants/AudIO_constants.h>
    #include <juce_audio_devices/juce_audio_devices.h>
    #include <juce_audio_formats/juce_audio_formats.h>
    #include <juce_audio_utils/juce_audio_utils.h>

    #include <array>
    #include <thread>

    #include "source/mt_voice_dsp.h"
    #include "source/mt_recorder.h"

namespace mt
{
    /// IAudioDevice backed by a juce::AudioDeviceManager.
    ///
    /// JUCE may call back with any buffer size. AudIO's memory pool holds blocks
    /// of a fixed size, so the callback is cut into chunks of at most the block
    /// size passed to start().
    class AudioDevice
        : public AudIO::IAudioDevice
        , private juce::AudioIODeviceCallback
    {
    public:
        AudioDevice( int numInputs, int numOutputs, juce::String deviceName );
        ~AudioDevice() override;

        void start( uint32_t samplerate, uint32_t blocksize ) override;
        void resume() override;
        void pause() override;
        void stop() override;

        juce::AudioDeviceManager& getJuceAudioDeviceMgr() noexcept { return deviceManager; }

    private:
        void audioDeviceIOCallbackWithContext( const float* const* inputChannelData,
                                               int                 numInputChannels,
                                               float* const*       outputChannelData,
                                               int                 numOutputChannels,
                                               int                 numSamples,
                                               const juce::AudioIODeviceCallbackContext& context ) override;
        void audioDeviceAboutToStart( juce::AudioIODevice* device ) override;
        void audioDeviceStopped() override;

        juce::AudioDeviceManager deviceManager;
        juce::String             requestedDevice;
        const int                numIns;
        const int                numOuts;
        uint32_t                 blockSize = AUDIO_BLOCK_SIZE;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AudioDevice )
    };
}

#endif // AUDIO_JUCE_H_INCLUDE
