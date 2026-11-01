/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "AudIO_juce.h"
#include "source/mt_recorder.cpp"

// AudIO_airwindows.cpp sits at the AudIO module root, so JUCE already
// compiles it next to AudIO.cpp. Nothing extra to pull in here.

namespace mt
{
    AudioDevice::AudioDevice( int numInputs, int numOutputs, juce::String deviceName )
        : AudIO::IAudioDevice( (uint32_t)numInputs, (uint32_t)numOutputs )
        , requestedDevice( deviceName )
        , numIns( numInputs )
        , numOuts( numOutputs )
    {
        if( requestedDevice.isEmpty() )
            requestedDevice = juce::SystemStats::getEnvironmentVariable( "KICK_AUDIO_DEVICE", {} );
    }

    AudioDevice::~AudioDevice()
    {
        stop();
    }

    void AudioDevice::start( uint32_t samplerate, uint32_t blocksize )
    {
        blockSize = blocksize;

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        auto error = deviceManager.initialise( numIns, numOuts, nullptr, true, requestedDevice, nullptr );
        if( error.isNotEmpty() )
            juce::Logger::writeToLog( "[emu] audio device init: " + error );

        deviceManager.getAudioDeviceSetup( setup );
        setup.sampleRate = (double)samplerate;
        setup.bufferSize = (int)blocksize;
        error            = deviceManager.setAudioDeviceSetup( setup, true );
        if( error.isNotEmpty() )
            juce::Logger::writeToLog( "[emu] audio device setup: " + error );

        if( auto* dev = deviceManager.getCurrentAudioDevice() )
            juce::Logger::writeToLog( "[emu] audio device: " + dev->getName() + " @ "
                                      + juce::String( dev->getCurrentSampleRate() ) + " Hz, "
                                      + juce::String( dev->getCurrentBufferSizeSamples() ) + " samples" );

        prepareToPlay( samplerate, blocksize );
        setState( AudIO::AudioDeviceState::playing );
        deviceManager.addAudioCallback( this );
    }

    void AudioDevice::resume()
    {
        setState( AudIO::AudioDeviceState::playing );
    }

    void AudioDevice::pause()
    {
        setState( AudIO::AudioDeviceState::paused );
    }

    void AudioDevice::stop()
    {
        if( !isInitialized() )
            return;

        deviceManager.removeAudioCallback( this );
        deviceManager.closeAudioDevice();
        setState( AudIO::AudioDeviceState::stopped );
        _buffer.clearOutputs();
        teardown();
    }

    void AudioDevice::audioDeviceAboutToStart( juce::AudioIODevice* ) {}
    void AudioDevice::audioDeviceStopped() {}

    void AudioDevice::audioDeviceIOCallbackWithContext( const float* const* inputChannelData,
                                                        int                 numInputChannels,
                                                        float* const*       outputChannelData,
                                                        int                 numOutputChannels,
                                                        int                 numSamples,
                                                        const juce::AudioIODeviceCallbackContext& )
    {
        for( int ch = 0; ch < numOutputChannels; ++ch )
            if( outputChannelData[ch] != nullptr )
                juce::FloatVectorOperations::clear( outputChannelData[ch], numSamples );

        if( !isPlaying() )
            return;

        const int engineIns  = (int)_buffer.getNumInputChannels();
        const int engineOuts = (int)_buffer.getNumOutputChannels();

        for( int offset = 0; offset < numSamples; )
        {
            const int n = juce::jmin( (int)blockSize, numSamples - offset );

            for( int ch = 0; ch < engineIns; ++ch )
                if( auto* dst = _buffer.getInputBuffer( (uint32_t)ch ) )
                {
                    if( ch < numInputChannels && inputChannelData[ch] != nullptr )
                        juce::FloatVectorOperations::copy( dst, inputChannelData[ch] + offset, n );
                    else
                        juce::FloatVectorOperations::clear( dst, n );
                }

            process( _buffer );

            // Engine outputs beyond what the host device has are folded onto the
            // first stereo pair, so a laptop still hears the second output.
            for( int ch = 0; ch < engineOuts; ++ch )
                if( auto* src = _buffer.getOutputBuffer( (uint32_t)ch ) )
                {
                    const int dstCh = ch < numOutputChannels ? ch : ( ch % juce::jmax( 1, juce::jmin( 2, numOutputChannels ) ) );
                    if( dstCh < numOutputChannels && outputChannelData[dstCh] != nullptr )
                        juce::FloatVectorOperations::add( outputChannelData[dstCh] + offset, src, n );
                }

            offset += n;
        }
    }
}
