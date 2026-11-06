/*
 * Copyright (c) 2022 MUSIC Tribe
 *
 * This file is part of AudIO.
 *
 * AudIO is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * AudIO is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with AudIO. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#ifndef AUDIO_ADSR_H_INCLUDE
    #define AUDIO_ADSR_H_INCLUDE

    #include <cmath>

    #include "../Includes/idsp.h"
    #include "../Includes/Utils.h"

namespace AudIO
{
    template<typename T>
    class ADSR : public IDSP<T>
    {
    public:
        struct Params
        {
            double A; // Time in seconds...
            double H; // Time in seconds...
            double D; // Time in seconds...
            double S; // Sustain level ( 0.0 - 1.0 )
            double R; // Time in seconds...
        };

        enum State
        { //! ADSR envelope states.
            IDLE = 0,
            ATTACK,
            HOLD,
            DECAY,
            SUSTAIN,
            RELEASE,
            MAX
        };

        enum class Type
        { //! ADSR curve.
            LINEAR,
            EXPONENTIAL
        };

        explicit ADSR( Type Typ = Type::EXPONENTIAL ) noexcept
            : type_( Typ ){};

        explicit ADSR( const Params& P ) noexcept
        {
            setParameters( P );
        }

        bool         isActive() noexcept { return ( this->isEnabled() && currentStage_ != State::IDLE ); }
        inline State getCurrentState() const noexcept { return currentStage_; }
        Type         getType() noexcept { return type_; }

        void setType( Type Typ ) noexcept { type_ = Typ; }
        void setParameters( const Params& P ) noexcept
        {
            stageTime_[State::IDLE]    = (T)minimumLevel_;
            stageTime_[State::ATTACK]  = (T)utils::max( P.A, minimumLevel_ );
            stageTime_[State::HOLD]    = (T)P.H;
            stageTime_[State::DECAY]   = (T)P.D;
            stageTime_[State::SUSTAIN] = (T)P.S;
            stageTime_[State::RELEASE] = (T)utils::max( P.R, minimumLevel_ );
        }

        void gate( bool On ) noexcept
        {
            if( On )
                keyOn();
            else
                keyOff();
        }

        void keyOn() noexcept
        {
            if( this->isEnabled() )
                enterStage( State::ATTACK );
        }
        void keyOff() noexcept
        {
            if( this->isEnabled() )
                enterStage( State::RELEASE );
            else
                enterStage( State::IDLE );
        }

        void reset() noexcept override { enterStage( State::IDLE ); }

        T advance( unsigned int NumSamples ) noexcept override
        {
            if( this->skipProcess() )
                return 0.0;

            T val = 0.0;
            for( auto i = ( NumSamples - 1 ); 0 < i; --i )
                val = process_internal();
            return val;
        }

        T process() noexcept override
        {
            return static_cast<T>( this->skipProcess() ? 1.0 : process_internal() );
        }

        void process( Buffer<float>& buffer ) noexcept override
        {
            if( this->skipProcess() )
                return;

            auto   outs = buffer.getOutputs();
            float* outL = outs[0];
            float* outR = outs[1];

            if( !outL )
                return;

            const auto nSamples = (int)buffer.getBlockSize();
            for( auto sample = 0; sample < nSamples; ++sample )
            {
                auto adsrOut = static_cast<float>( this->process_internal() );
                // iterate over all available output channels
                *outL++ *= adsrOut;
                if( outR )
                    *outR++ *= adsrOut;
            }
        }

    private:
        void recalculate() noexcept override
        {
            enterStage( currentStage_ );
        }

        inline double process_internal()
        {
            switch( currentStage_ )
            {
                default: // IDLE or SUSTAIN...
                    return stageTime_[currentStage_];
                case State::ATTACK:
                case State::HOLD:
                case State::DECAY:
                case State::RELEASE:
                    if( ++numCurrentSamples >= numSamplesToNextStage )
                    {
                        enterStage( (State)( currentStage_ + 1 ) );
                    }
                    // LINEAR adds a fixed step per sample; EXPONENTIAL scales by a fixed ratio.
                    if( Type::LINEAR == type_ )
                        currentLevel_ += multiplier_;
                    else
                        currentLevel_ *= multiplier_;
                    return currentLevel_;
            }
        }

        inline unsigned long calcSamplesToState( State Stage ) noexcept
        {
            switch( Stage )
            {
                default:
                    return 0;
                case State::ATTACK:
                case State::HOLD:
                case State::DECAY:
                case State::RELEASE:
                    return ( stageTime_[Stage] * this->sampleRate() );
            }
        }

        inline double calculateCoefficient( double startLevel, double endLevel, unsigned long lengthInSamples, Type type )
        {
            // LINEAR returns the per-sample increment (0 for no change); EXPONENTIAL returns
            // the per-sample multiplier (1 for no change).
            double coeff = ( Type::LINEAR == type ) ? 0.0 : 1.0;
            if( 0 != lengthInSamples )
            {
                if( Type::LINEAR == type )
                    coeff = ( endLevel - startLevel ) / (double)( lengthInSamples );
                else // if( Mode::EXPONENTIAL == type_ )
                    coeff += ( ( std::log( endLevel ) - std::log( startLevel ) ) / (double)( lengthInSamples + 1 ) );
            }
            return coeff;
        }

        void enterStage( State newStage ) noexcept
        {
            double endLevel   = 0;
            numCurrentSamples = 0;

            numSamplesToNextStage = calcSamplesToState( newStage );
            currentStage_         = newStage;

            switch( currentStage_ )
            {
                default:
                    currentStage_ = State::IDLE;
                    currentLevel_ = endLevel = stageTime_[currentStage_];
                    break;
                case State::ATTACK:
                    currentLevel_ = minimumLevel_;
                    endLevel      = 1.0;
                    break;
                case State::HOLD:
                    endLevel = currentLevel_;
                    break;
                case State::DECAY:
                    endLevel = utils::max( stageTime_[State::SUSTAIN], minimumLevel_ );
                    break;
                case State::SUSTAIN:
                    currentLevel_ = endLevel = stageTime_[currentStage_];
                    if( utils::isEqual<double>( 0.0, currentLevel_ ) )
                    {
                        enterStage( State::RELEASE );
                        return;
                    } // fall through...
                    break;
                case State::RELEASE:
                    endLevel = minimumLevel_;
                    break;
            }

            multiplier_ = calculateCoefficient( currentLevel_, endLevel, numSamplesToNextStage, type_ );
            switch( currentStage_ )
            {
                case State::ATTACK:
                case State::HOLD:
                case State::DECAY:
                case State::RELEASE:
                    if( 0 == numSamplesToNextStage )
                        enterStage( State( currentStage_ + 1 ) );
                    break;
                case State::SUSTAIN:
                    if( utils::isEqual<double>( 0.0, currentLevel_ ) )
                        enterStage( State::RELEASE );
                    break;
            }
        }

    private:
        Type  type_{ Type::EXPONENTIAL };
        State currentStage_{ State::IDLE };
        // Internal...
        static constexpr double        minimumLevel_ = 0.0001;
        std::array<double, State::MAX> stageTime_{ 0.0, 0.01, 0.0, 0.5, 0.1, 1.0 };
        double                         currentLevel_{ minimumLevel_ };
        double                         multiplier_{ 1.0 }; // per-sample increment (LINEAR) or ratio (EXPONENTIAL)
        unsigned long                  numCurrentSamples{ 0 };
        unsigned long                  numSamplesToNextStage{ 0 };
    };

    //
}; // Namespace AudIO
#endif // AUDIO_ADSR_H_INCLUDE
