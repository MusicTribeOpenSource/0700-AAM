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

#include "iSynthVoice.h"

namespace mt
{
    class DrumVoice : public ISynthVoice
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<DrumVoice>;

        explicit DrumVoice( MTIAtomicProgramMaster& Master, ASoundSlice* Slice = nullptr )
            : ISynthVoice( Master, Slice ) {}
        virtual ~DrumVoice() = default;

        void attachToAndReset( const juce::ValueTree& Tree, juce::UndoManager* Undo );
        void noteOn( int Number, int Velocity ) final;
        void noteOff() final;

        inline bool isTailingOff() const { return !juce::approximatelyEqual( tailOff, 0.0 ); }
        void setPadGains( float gl, float gr ) { padGainL.store( gl ); padGainR.store( gr ); }

    private:
        inline void update()
        {
            updateLFO();
            updateEnvelopes();
        }

        inline void updateEnvelopes()
        {
            updateAmplitudeEnvelope( sampleLength );
            updateFilterEnvelope( sampleLength );
            updatePitchEnvelope( sampleLength );
        }

        void updateLFO();
        void updateFilter( double modulation = 0.0 );
        void updateAmplitudeEnvelope( double Length );
        void updateFilterEnvelope( double Length );
        void updatePitchEnvelope( double Length );

        void process( juce::AudioBuffer<float>& Buffer );

    private:
        MTIAtomicLFO       pLfo;
        MTIAtomicEnvelopes pEnv;

        AudIO::Panner<float> panner;
        AudIO::LFO           lfo;
        Filter               filter;

        ADSR   ampEnv;
        ADSR   filtEnv;
        ADSR   pitchEnv;
        double sampleLength{ 0 };

        std::atomic<float> padGainL{ 1.0f };
        std::atomic<float> padGainR{ 1.0f };

        float lfoToAmp{ 0 };
        float lfoToPitch{ 0 };
        float lfoToFilter{ 0 };
        float lfoToPan{ 0 };

        float ampToVel{ 0 };
        float ampToPan{ 0 };

        double tailOff{ 0 };
    };

} // namespace mt
