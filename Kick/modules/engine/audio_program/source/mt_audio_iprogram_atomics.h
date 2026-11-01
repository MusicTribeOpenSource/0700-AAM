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

#pragma once

namespace mt
{

    struct MTIAtomicProgramMaster
    {
        explicit MTIAtomicProgramMaster( juce::ValueTree Tree = juce::ValueTree{ IDs::PROGRAM } )
        {
            auto CalculatePitchRatio = [&]() {
                pitchRatio = AudIO::utils::calculatePitchFactor( semi(), fine() );
            };
            semi.onValueChanged = CalculatePitchRatio;
            fine.onValueChanged = CalculatePitchRatio;

            auto CalculateLRGains = [&]() {
                panner.setParameters( pan(), gain() );
                gainL = isMuted() ? 0.f : panner.getLeft();
                gainR = isMuted() ? 0.f : panner.getRight();
            };
            isMuted.onValueChanged = CalculateLRGains;
            gain.onValueChanged    = CalculateLRGains;
            pan.onValueChanged     = CalculateLRGains;

            attachTo( Tree );
        }

        void attachTo( juce::ValueTree Tree )
        {
            auto Undo = nullptr;
            isMuted.attachTo( Tree, IDs::mute, Undo, false );
            isSolo.attachTo( Tree, IDs::solo, Undo, false );
            semi.attachTo( Tree, IDs::semi, Undo, SEMI::DEFAULT, SEMI::MIN, SEMI::MAX );
            fine.attachTo( Tree, IDs::fine, Undo, FINE::DEFAULT, FINE::MIN, FINE::MAX );
            poly.attachTo( Tree, IDs::poly, Undo, POLYPHONY::DEFAULT, POLYPHONY::MIN, POLYPHONY::MAX );
            gain.attachTo( Tree, IDs::gain, Undo, GAIN::DEFAULT, GAIN::MIN, GAIN::MAX );
            pan.attachTo( Tree, IDs::pan, Undo, PAN::DEFAULT, PAN::MIN, PAN::MAX );
        }

        Atomic<bool>  isMuted;
        Atomic<bool>  isSolo;
        Atomic<int>   semi;
        Atomic<int>   fine;
        Atomic<int>   poly;
        Atomic<float> gain;
        Atomic<float> pan;

        std::atomic<float> pitchRatio;
        std::atomic<float> gainL;
        std::atomic<float> gainR;
        AudIO::PannerFloat panner;
    };

    struct MTIAtomicLFO
    {
        explicit MTIAtomicLFO( juce::ValueTree Tree = juce::ValueTree{ IDs::LFO } ) { attachTo( Tree ); }
        void attachTo( juce::ValueTree Tree )
        {
            auto Undo = nullptr;
            isEnabled.attachTo( Tree, IDs::isEnabled, Undo, false );
            sync.attachTo( Tree, IDs::sync, Undo, 0 );
            type.attachTo( Tree, IDs::type, Undo, 0 );
            freq.attachTo( Tree, IDs::freq, Undo, 0 );
            ampMod.attachTo( Tree, IDs::level, Undo, 0 );
            panMod.attachTo( Tree, IDs::pan, Undo, 0 );
            pitchMod.attachTo( Tree, IDs::pitch, Undo, 0 );
            filterMod.attachTo( Tree, IDs::filter, Undo, 0 );
        }

        Atomic<bool>  isEnabled;
        Atomic<int>   sync;
        Atomic<int>   type;
        Atomic<float> freq;
        Atomic<int>   ampMod;
        Atomic<int>   panMod;
        Atomic<int>   pitchMod;
        Atomic<int>   filterMod;

        juce::ValueTree vTree;
    };

    struct MTIAtomicADSR
    {
        explicit MTIAtomicADSR( juce::ValueTree Tree = juce::ValueTree{ IDs::ADSR } ) { attachTo( Tree ); }
        void attachTo( juce::ValueTree Tree )
        {
            auto Undo = nullptr;
            adsrType.attachTo( Tree, IDs::type, Undo, 0 );
            a.attachTo( Tree, IDs::attack, Undo, 0.f );
            h.attachTo( Tree, IDs::hold, Undo, 0.f );
            d.attachTo( Tree, IDs::decay, Undo, 0.f );
            s.attachTo( Tree, IDs::sustain, Undo, 127.f );
            r.attachTo( Tree, IDs::release, Undo, 0.f );
        }

        Atomic<int>   adsrType;
        Atomic<float> a;
        Atomic<float> h;
        Atomic<float> d;
        Atomic<float> s;
        Atomic<float> r;

        juce::ValueTree vTree;
    };

    struct MTIAtomicAmplitudeEnvelope : public MTIAtomicADSR
    {
        explicit MTIAtomicAmplitudeEnvelope( juce::ValueTree Tree = juce::ValueTree{ IDs::AMPENV } ) { attachTo( Tree ); }
        void attachTo( juce::ValueTree Tree )
        {
            auto Undo = nullptr;
            MTIAtomicADSR::attachTo( Tree );
            isEnabled.attachTo( Tree, IDs::isEnabled, Undo, 0.f );
            ampMod.attachTo( Tree, IDs::level, Undo, 0 );
            panMod.attachTo( Tree, IDs::pan, Undo, 1.f );
            velMod.attachTo( Tree, IDs::velocity, Undo, 1.f );
        }

        Atomic<bool> isEnabled;
        Atomic<int>  ampMod;
        Atomic<int>  panMod;
        Atomic<int>  velMod;
    };

    struct MTIAtomicFilterEnvelope : public MTIAtomicADSR
    {
        explicit MTIAtomicFilterEnvelope( juce::ValueTree Tree = juce::ValueTree{ IDs::FILTERENV } ) { attachTo( Tree ); }
        void attachTo( juce::ValueTree Tree )
        {
            auto Undo = nullptr;
            MTIAtomicADSR::attachTo( Tree );
            isEnabled.attachTo( Tree, IDs::isEnabled, Undo, 0.f );
            type.attachTo( Tree, IDs::type, Undo, 0 );
            cutoff.attachTo( Tree, IDs::freq, Undo, 20000 );
            resonance.attachTo( Tree, IDs::resonance, Undo, 10 );
            depth.attachTo( Tree, IDs::depth, Undo, 0 );
            cutoffMod.attachTo( Tree, IDs::cutoffMod, Undo, 0 );
            attackMod.attachTo( Tree, IDs::attackMod, Undo, 1.f );
            depthMod.attachTo( Tree, IDs::depthMod, Undo, 1.f );
        }

        Atomic<bool> isEnabled;
        Atomic<int>  type;
        Atomic<int>  cutoff;
        Atomic<int>  resonance;

        Atomic<int> depth;
        Atomic<int> cutoffMod;
        Atomic<int> attackMod;
        Atomic<int> depthMod;
    };

    struct MTIAtomicPitchEnvelope : public MTIAtomicADSR
    {
        explicit MTIAtomicPitchEnvelope( juce::ValueTree Tree = juce::ValueTree{ IDs::PITCHENV } ) { attachTo( Tree ); }
        void attachTo( juce::ValueTree Tree )
        {
            auto Undo = nullptr;
            MTIAtomicADSR::attachTo( Tree );
            isEnabled.attachTo( Tree, IDs::isEnabled, Undo, 0.f );
            depth.attachTo( Tree, IDs::depth, Undo, 0 );
            pitchMod.attachTo( Tree, IDs::pitchMod, Undo, 1.f );
        }

        Atomic<bool> isEnabled;
        Atomic<int>  depth;
        Atomic<int>  pitchMod;
    };

    struct MTIAtomicEnvelopes
    {
        explicit MTIAtomicEnvelopes( juce::ValueTree Tree = juce::ValueTree{ IDs::ENVELOPE } ) { attachTo( Tree ); }
        void attachTo( juce::ValueTree Tree )
        {
            amp.attachTo( Tree.getChildWithName( IDs::AMPENV ) );
            filter.attachTo( Tree.getChildWithName( IDs::FILTERENV ) );
            pitch.attachTo( Tree.getChildWithName( IDs::PITCHENV ) );
        }

        MTIAtomicAmplitudeEnvelope amp;
        MTIAtomicFilterEnvelope    filter;
        MTIAtomicPitchEnvelope     pitch;
    };

} // namespace MT
