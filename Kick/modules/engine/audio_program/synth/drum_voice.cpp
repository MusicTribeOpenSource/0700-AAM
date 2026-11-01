/*
 * Copyright (c) 2024 MUSIC Tribe
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

#pragma once

#include "drum_voice.h"

namespace mt
{
    void DrumVoice::attachToAndReset( const juce::ValueTree& Tree, juce::UndoManager* Undo )
    {
        pLfo.attachTo( Tree.getChildWithName( IDs::LFO ) );
        pEnv.attachTo( Tree.getChildWithName( IDs::ENVELOPE ) );
    }

    void DrumVoice::noteOn( int Number, int Velocity )
    {
        // sample rates
        lfo.setSampleRate( sampleRate );
        filter.setSampleRate( sampleRate );

        ampEnv.setSampleRate( sampleRate );
        filtEnv.setSampleRate( sampleRate );
        pitchEnv.setSampleRate( sampleRate );

        // Setup LFO.
        lfo.setType( toEnum<LFOType>( pLfo.type() ) );
        lfo.setFrequency( pLfo.freq(), true );
        lfo.setEnabled( pLfo.isEnabled() );

        // Setup Envelopes.
        sampleLength = MIDI::SCALER * getLength();
        updateAmplitudeEnvelope( sampleLength );
        updateFilterEnvelope( sampleLength );
        updatePitchEnvelope( sampleLength );

        // Setup filter.
        updateFilter();

        ampEnv.gate( 1 );
        filtEnv.gate( 1 );
        pitchEnv.gate( 1 );

        // Tail off and reset.
        tailOff = 0.0;
        lfo.reset();
        filter.reset();

        ISynthVoice::noteOn( Number, Velocity );
    }

    void DrumVoice::noteOff()
    {
        ISynthVoice::noteOff();
        ampEnv.gate( 0 );
        filtEnv.gate( 0 );
        pitchEnv.gate( 0 );
    }

    void DrumVoice::updateLFO()
    {
        lfo.setType( toEnum<LFOType>( (int)pLfo.type() ) );
        lfo.setFrequency( pLfo.freq() );

        lfoToAmp    = pLfo.ampMod() * MIDI::SCALER;
        lfoToPan    = pLfo.panMod() * MIDI::SCALER;
        lfoToPitch  = pLfo.pitchMod() * MIDI::SCALER;
        lfoToFilter = pLfo.filterMod() * MIDI::SCALER;
    }

    void DrumVoice::updateFilter( double modulation )
    {
        filter.setFilterType( toEnum<FilterType>( pEnv.filter.type() ) );
        filter.setResonance( pEnv.filter.resonance() );

        auto cutoff = Filter::freqRange.convertTo0to1( pEnv.filter.cutoff() ) + modulation;
        filter.setCutoff( cutoff );
    }

    void DrumVoice::updateAmplitudeEnvelope( double Length )
    {
        auto type  = toEnum<ADSRType>( pEnv.amp.adsrType() );
        auto scale = MIDI::SCALER * Length;

        auto adF = scale * getAttackDecayScaler( type );
        auto hF  = scale * getHoldScaler( type );
        auto rF  = scale * getReleaseScaler( type );

        auto ampToAttack = juce::jmap<float>( pEnv.amp.ampMod(), 0.0, 127.0, -1.0, 1.0 );
        ampToVel         = juce::jmap<float>( pEnv.amp.velMod(), 0.0, 127.0, -1.0, 1.0 );
        ampToPan         = juce::jmap<float>( pEnv.amp.panMod(), 0.0, 127.0, -1.0, 1.0 );

        auto aa = ( pEnv.amp.a() * adF );
        auto ad = ( pEnv.amp.d() * adF );
        auto ah = ( pEnv.amp.h() * hF );
        auto ar = ( pEnv.amp.r() * rF );
        auto as = ( pEnv.amp.s() );

        auto mod = aa - ( ampToAttack * aa );
        aa -= mod;
        ah += mod;

        ampEnv.setParameters( aa, ah, ad, ar, as ); // In terms of samples...
    }

    void DrumVoice::updateFilterEnvelope( double Length )
    {
        auto type  = toEnum<ADSRType>( pEnv.filter.adsrType() );
        auto scale = MIDI::SCALER * Length;

        auto adF = scale * getAttackDecayScaler( type );
        auto hF  = scale * getHoldScaler( type );
        auto rF  = scale * getReleaseScaler( type );

        auto aa = ( pEnv.filter.a() * adF );
        auto ad = ( pEnv.filter.d() * adF );
        auto ah = ( pEnv.filter.h() * hF );
        auto ar = ( pEnv.filter.r() * rF );
        auto as = ( pEnv.filter.s() );
        filtEnv.setParameters( aa, ah, ad, ar, as ); // In terms of samples...
    }

    void DrumVoice::updatePitchEnvelope( double Length )
    {
        auto type  = toEnum<ADSRType>( pEnv.pitch.adsrType() );
        auto scale = MIDI::SCALER * Length;

        auto adF = scale * getAttackDecayScaler( type );
        auto hF  = scale * getHoldScaler( type );
        auto rF  = scale * getReleaseScaler( type );

        auto aa = ( pEnv.pitch.a() * adF );
        auto ad = ( pEnv.pitch.d() * adF );
        auto ah = ( pEnv.pitch.h() * hF );
        auto ar = ( pEnv.pitch.r() * rF );
        auto as = ( pEnv.pitch.s() );
        pitchEnv.setParameters( aa, ah, ad, ar, as ); // In terms of samples...
    }

    void DrumVoice::process( juce::AudioBuffer<float>& Buffer )
    {
        if( isRunning() )
        {
            update(); // Update parameters...

            const auto masterPitchRatio = master.pitchRatio.load();
            const auto masterGainL      = master.gainL.load();
            const auto masterGainR      = master.gainR.load();
            const auto numSamples       = Buffer.getNumSamples();

            auto out0 = tempBuffer.getWritePointer( 0 );
            auto out1 = tempBuffer.getWritePointer( 1 );

            // For each sample...
            for( auto i = 0; i < numSamples; ++i )
            {
                auto rGain     = gain.getNextValue();
                auto lfoOut    = lfo();
                auto ampAdsr   = ampEnv();
                auto filtAdsr  = filtEnv();
                auto pitchAdsr = pitchEnv();

                auto aMod   = rGain * ampAdsr * ( ( 1. - lfoToAmp ) + ( lfoToAmp * lfoOut ) ); // Amplitude modulation factor.
                auto panMod = ( lfoToPan * lfoOut ) + ( gain.getTargetValue() * ampToPan ); // Pan modulation factor.

                double pitchEnvFactor = 1.0;
                if( pEnv.pitch.isEnabled() && pEnv.pitch.depth() != 0 )
                {
                    pitchEnvFactor = std::pow( 2.0, ( ( pitchAdsr - 1.0 ) * pEnv.pitch.depth() ) / 12.0 );
                }
                auto pMod   = pitchEnvFactor * masterPitchRatio * ( 1.0 - ( lfoToPitch * lfoOut ) ); // Pitch modulation factor.
                auto fMod   = filtAdsr * pEnv.filter.depth() * ( ( lfoToFilter * lfoOut ) + ( lfoToPitch * lfoOut ) ); // Filter cutoff frequency modulation.

                // get next sample.
                updateFilter( fMod );
                auto& sample = getNext( pMod );
                filter.process( sample );

                // Copy to Bock.
                panner.setParameters( panMod, aMod );
                *out0++ = panner.getLeft() * sample.getSample( 0, 0 );
                *out1++ = panner.getRight() * sample.getSample( 1, 0 );
            }

            // Apply Master and Pad gain and pan, and add to the Buffer.
            const auto totalGainL = masterGainL * padGainL.load();
            const auto totalGainR = masterGainR * padGainR.load();
            Buffer.addFrom( 0, 0, tempBuffer, 0, 0, numSamples, totalGainL );
            Buffer.addFrom( 1, 0, tempBuffer, 1, 0, numSamples, totalGainR );

            if( hasFinished() )
                makeFree();
        }
    }

} // namespace mt
