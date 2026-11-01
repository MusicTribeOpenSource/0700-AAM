/*
 * Envelope and filter used by mt::DrumVoice (Kick/modules/engine/audio_program/synth/drum_voice.*).
 *
 * Clean-room stand-ins written from the calls drum_voice.cpp makes. The
 * originals lived in the private AudIO_juce module.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

namespace mt
{
    /// Attack-hold-decay-sustain-release envelope. Times are in samples, as the
    /// "In terms of samples" comments in drum_voice.cpp say; sustain is a 0..1 level.
    class ADSR
    {
    public:
        void setSampleRate( double SampleRate ) noexcept { sampleRate = SampleRate; }

        void setParameters( double Attack, double Hold, double Decay, double Release, double Sustain ) noexcept
        {
            attack  = juce::jmax( 1.0, Attack );
            hold    = juce::jmax( 0.0, Hold );
            decay   = juce::jmax( 1.0, Decay );
            release = juce::jmax( 1.0, Release );
            sustain = juce::jlimit( 0.0, 1.0, Sustain );
        }

        void gate( bool On ) noexcept
        {
            if( On )
            {
                stage   = Stage::Attack;
                counter = 0.0;
            }
            else if( stage != Stage::Idle )
            {
                releaseStart = level;
                stage        = Stage::Release;
                counter      = 0.0;
            }
        }

        void reset() noexcept
        {
            stage = Stage::Idle;
            level = 0.0;
        }

        bool isActive() const noexcept { return stage != Stage::Idle; }

        double operator()() noexcept
        {
            switch( stage )
            {
                case Stage::Idle: level = 0.0; break;
                case Stage::Attack:
                    level = juce::jmin( 1.0, level + 1.0 / attack );
                    if( level >= 1.0 )
                        advance( hold > 0.0 ? Stage::Hold : Stage::Decay );
                    break;
                case Stage::Hold:
                    if( ++counter >= hold )
                        advance( Stage::Decay );
                    break;
                case Stage::Decay:
                    level -= ( 1.0 - sustain ) / decay;
                    if( level <= sustain )
                    {
                        level = sustain;
                        advance( Stage::Sustain );
                    }
                    break;
                case Stage::Sustain: level = sustain; break;
                case Stage::Release:
                    level -= releaseStart / release;
                    if( level <= 0.0 )
                    {
                        level = 0.0;
                        stage = Stage::Idle;
                    }
                    break;
            }
            return level;
        }

    private:
        enum class Stage
        {
            Idle,
            Attack,
            Hold,
            Decay,
            Sustain,
            Release
        };

        void advance( Stage S ) noexcept
        {
            stage   = S;
            counter = 0.0;
        }

        Stage  stage{ Stage::Idle };
        double sampleRate{ 48000.0 };
        double attack{ 1.0 }, hold{ 0.0 }, decay{ 1.0 }, release{ 1.0 }, sustain{ 1.0 };
        double level{ 0.0 }, releaseStart{ 0.0 }, counter{ 0.0 };
    };

    /// Filter modes, indexed by the drum program's filter "type" value.
    enum class FilterType
    {
        LowPass = 0,
        HighPass,
        BandPass,
        Notch,
        Off,
    };

    /// Stereo state-variable filter (topology-preserving transform).
    /// setCutoff() takes the 0..1 position in freqRange, which is how drum_voice.cpp
    /// adds its envelope modulation. Resonance is the program's 0..127 value.
    class Filter
    {
    public:
        static inline const juce::NormalisableRange<double> freqRange{ 20.0, 20000.0, 0.0, 0.25 };

        void setSampleRate( double SampleRate ) noexcept
        {
            sampleRate = SampleRate;
            recalc();
        }

        void setFilterType( FilterType Type ) noexcept { type = Type; }

        void setResonance( double Resonance ) noexcept
        {
            // Map 0..127 onto Q 0.5..12.
            q = 0.5 + 11.5 * juce::jlimit( 0.0, 1.0, Resonance / 127.0 );
            recalc();
        }

        void setCutoff( double Normalised ) noexcept
        {
            cutoffHz = freqRange.convertFrom0to1( juce::jlimit( 0.0, 1.0, Normalised ) );
            recalc();
        }

        void reset() noexcept
        {
            for( auto& s : state )
                s = {};
        }

        /// Filters every sample of a (usually 2-channel, 1-sample) buffer in place.
        void process( juce::AudioBuffer<float>& Buffer ) noexcept
        {
            if( type == FilterType::Off )
                return;

            const int nCh = juce::jmin( 2, Buffer.getNumChannels() );
            for( int ch = 0; ch < nCh; ++ch )
            {
                auto* d = Buffer.getWritePointer( ch );
                for( int i = 0; i < Buffer.getNumSamples(); ++i )
                    d[i] = processSample( ch, d[i] );
            }
        }

        float processSample( int Channel, float x ) noexcept
        {
            auto& s  = state[(size_t)Channel];
            auto  hp = ( x - ( 2.0 * r + g ) * s.ic1 - s.ic2 ) / ( 1.0 + 2.0 * r * g + g * g );
            auto  bp = g * hp + s.ic1;
            auto  lp = g * bp + s.ic2;
            s.ic1    = 2.0 * bp - s.ic1;
            s.ic2    = 2.0 * lp - s.ic2;

            switch( type )
            {
                case FilterType::HighPass: return (float)hp;
                case FilterType::BandPass: return (float)( 2.0 * r * bp );
                case FilterType::Notch: return (float)( x - 2.0 * r * bp );
                case FilterType::LowPass:
                default: return (float)lp;
            }
        }

    private:
        void recalc() noexcept
        {
            const auto fc = juce::jmin( cutoffHz, sampleRate * 0.49 );
            g             = std::tan( juce::MathConstants<double>::pi * fc / sampleRate );
            r             = 1.0 / ( 2.0 * q );
        }

        struct ChannelState
        {
            double ic1{ 0.0 }, ic2{ 0.0 };
        };

        FilterType                  type{ FilterType::LowPass };
        double                      sampleRate{ 48000.0 };
        double                      cutoffHz{ 20000.0 };
        double                      q{ 0.707 };
        double                      g{ 1.0 }, r{ 0.707 };
        std::array<ChannelState, 2> state{};
    };
}
