/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

// module include files...

namespace mt
{
    constexpr int NumPreviewPolyphony = 1;

    /// @brief
    AudioPreview::AudioPreview( AudIO::Design& D, const juce::String& Name )
        //: ISynthObject( D, NumPreviewPolyphony )
        : AudIO::Object( 0, 2, 0 )
        , AudIO::DawRouting( D, AudIO::BusType::Object, AudIO::BusWidth::Stereo )
    {
        setName( Name.toStdString() );
        AudIO::DawRouting::setID( getId() );
    }

    AudioPreview::~AudioPreview()
    {
        previewOff();
        player.removeSound();
    }

    void AudioPreview::previewOn( const juce::String& Source )
    {
        auto cGain = gain.getTargetValue();
        gain.setCurrentAndTargetValue( 0.0 );
        gain.setTargetValue( cGain );
        player.setSoundToPlay( Source, -1.0 );
        // Start
        if( player.isValidSource() )
        {
            setBypass( false );
            player.start();
        }
    }

    void AudioPreview::previewOff()
    {
        // Stop
        player.stop();
        setBypass( true );
    };

    void AudioPreview::setGain( double Gain, bool Force )
    {
        auto g = juce::jlimit( 0.0, 1.0, Gain );
        if( Force )
            gain.setCurrentAndTargetValue( g );
        else
            gain.setTargetValue( g );
    }

    void AudioPreview::setPan( double Pan, bool Force )
    {
        auto p = juce::jlimit( -1.0, 1.0, Pan );
        if( Force )
            pan.setCurrentAndTargetValue( p );
        else
            pan.setTargetValue( p );
    }

    void AudioPreview::setGainAndPan( double Gain, double Pan, bool Force )
    {
        setGain( Gain, Force );
        setPan( Pan, Force );
    }

    void AudioPreview::prepareToPlay( uint32_t samplerate, uint32_t blocksize )
    {
        AudIO::Object::prepareToPlay( samplerate, blocksize );
        player.setSampleRate( samplerate );
        // previewOn() fades in from 0: keep it a short de-click ramp, a long one swallows the attack.
        gain.reset( (double)samplerate, 0.01 );
        pan.reset( (double)samplerate, 0.01 );
    }

    void AudioPreview::teardown()
    {
        AudIO::Object::teardown();
    }

    void AudioPreview::process( AudIO::Buffer<float>& buffer )
    {
        if( !isBypassed() && !isMuted() && isRunning() )
        {
            auto numSamples = buffer.getBlockSize();

            if( isMuted() )
            {
                buffer = 0;
                player.advance( 1.0, numSamples );
            }
            else
            {
                juce::AudioSampleBuffer buf( (float* const*)buffer.getOutputs(), buffer.getNumOutputChannels(), numSamples );
                player.getNextBlock( buf, 1.0 );
                gain.applyGain( buf, numSamples );
            }
        }
        else
            buffer = 0;

        if( outMeterEnabled )
            meter.process( buffer );
    }

    MTMeterData& AudioPreview::getMeter()
    {
        return meter.getMeter( meterData );
    }

};
