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
#ifndef AUDIO_PAN_H_INCLUDE
    #define AUDIO_PAN_H_INCLUDE

namespace AudIO
{
    template<typename T>
    class Panner
    {
    public:
        enum class Type
        {
            BALANCED,
            LINEAR,
            LINEAR_4_5DB,
            EQUAL_POWER,
            LOG_3DB,
            LOG4_5dB,
            LOG_6DB
        };

        struct Params
        {
            double pan{ 0.0 }; // PAN value [ -1.0 (left) to 1.0 (right) ]
            double gain{ 1.0 }; // Gain value [ 0.0 (min) to 1.0 (max) ]
            Type   type{ Type::EQUAL_POWER }; // PAN algorithm to use
        };

        explicit Panner( const Params& P = {} ) { setParameters( P ); }
        explicit Panner( T Pan, T Gain = 1.f, Type Typ = Type::BALANCED )
            : Panner( Params{ Pan, Gain, Typ } ) {}

        void setType( Type Typ ) noexcept
        {
            type_ = Typ;
            recalculate();
        }

        void setParameters( T Pan, T Gain )
        {
            setPan( Pan, false );
            setGain( Gain );
        }

        void setParameters( const Params& P )
        {
            type_ = P.type;
            setPan( P.pan, false );
            setGain( P.gain );
        }

        void setPan( T Pan, bool Recalculate = true )
        {
            if( pan_ != Pan )
                pan_ = utils::limit<T>( -1.0, 1.0, Pan );

            if( Recalculate )
                recalculate();
        }

        void setGain( T Gain, bool Recalculate = true )
        {
            if( gain_ != Gain )
                gain_ = utils::max<T>( 0.0, Gain );

            if( Recalculate )
                recalculate();
        }

        ///
        /// Getters
        ///
        inline T         getGain() const noexcept { return ( gain_ ); }
        inline T         getPan() const noexcept { return ( pan_ ); }
        inline T         getLeft() const noexcept { return ( lr.l ); }
        inline T         getRight() const noexcept { return ( lr.r ); }
        std::tuple<T, T> getLeftRight() noexcept { return std::make_tuple<T, T>( lr.l, lr.r ); }
        const Gains<T>&  getGains() const noexcept { return lr; }

        /// //////////////////////////////////////////////////////////////
        /// IMPLEMENTATION DETIALS >>>>
        /// //////////////////////////////////////////////////////////////
        void reset() noexcept
        {
            lr.l  = 1.0;
            lr.r  = 1.0;
            gain_ = 1.0;
            pan_  = 0.0;
        }

        // This method is not really relevant for Pan.
        virtual T nextSample() noexcept
        {
            return pan_;
        }

        static std::pair<double, double> panSamples( double Left, double Right, Panner& pan )
        {
            auto lt = ( ( Left * std::sin( pan.getLeft() ) ) + ( Right * std::sin( pan.getRight() ) ) );
            auto rt = ( ( Left * std::cos( pan.getLeft() ) ) + ( Right * std::cos( pan.getRight() ) ) );
            return std::make_pair<T, T>( static_cast<T>( lt ), static_cast<T>( rt ) );
        }

        std::pair<T, T> panSamples( double Left, double Right )
        {
            auto lt = ( ( Left * std::sin( lr.l ) ) + ( Right * std::sin( lr.r ) ) );
            auto rt = ( ( Left * std::cos( lr.l ) ) + ( Right * std::cos( lr.r ) ) );
            return std::make_pair<T, T>( static_cast<T>( lt ), static_cast<T>( rt ) );
        }

        std::pair<T, T> panSamples( double Left, double Right, double Gain )
        {
            auto lt = Gain * ( ( Left * std::sin( lr.l ) ) + ( Right * std::sin( lr.r ) ) );
            auto rt = Gain * ( ( Left * std::cos( lr.l ) ) + ( Right * std::cos( lr.r ) ) );
            return std::make_pair<T, T>( static_cast<T>( lt ), static_cast<T>( rt ) );
        }

    protected:
    private:
        Type     type_{ Type::BALANCED };
        Gains<T> lr{ 1 };
        T        pan_{ 0.0 };
        T        gain_{ 1.0 };

        void recalculate() noexcept
        {
            double p = 0.5 * ( pan_ + 1.0 );
            switch( type_ )
            {
                case Type::BALANCED:
                {
                    lr.l = static_cast<T>( gain_ * 2.0 * utils::min( 0.5, ( 1.0 - p ) ) );
                    lr.r = static_cast<T>( gain_ * 2.0 * utils::min( 0.5, p ) );
                }
                break;
                case Type::LINEAR:
                {
                    lr.l = static_cast<T>( gain_ * 2.0 * ( 1.0 - p ) );
                    lr.r = static_cast<T>( gain_ * 2.0 * p );
                }
                break;
                case Type::LINEAR_4_5DB:
                {
                    // Mirror image curves, so a centred signal is equal on both sides (-4.5 dB).
                    auto pl = 1.0 - p;
                    lr.l    = static_cast<T>( gain_ * ( ( 0.5 * pl * pl ) + 0.4775 ) );
                    lr.r    = static_cast<T>( gain_ * ( ( 0.5 * p * p ) + 0.4775 ) );
                }
                break;
                case Type::EQUAL_POWER:
                {
                    p    = ( p * Math<T>::pi_2 ) - Math<T>::pi_4;
                    lr.l = static_cast<T>( gain_ * ( ( std::cos( p ) - std::sin( p ) ) * Math<T>::sqrt2_2 ) );
                    lr.r = static_cast<T>( gain_ * ( ( std::cos( p ) + std::sin( p ) ) * Math<T>::sqrt2_2 ) );
                }
                break;
                case Type::LOG_3DB:
                {
                    lr.l = static_cast<T>( gain_ * ( std::pow( 1 - p, log( Math<T>::gain3dB ) / log( 0.5 ) ) ) );
                    lr.r = static_cast<T>( gain_ * ( std::pow( p, log( Math<T>::gain3dB ) / log( 0.5 ) ) ) );
                }
                break;
                case Type::LOG4_5dB:
                {
                    lr.l = static_cast<T>( gain_ * ( std::pow( 1 - p, log( Math<T>::gain4_5dB ) / log( 0.5 ) ) ) );
                    lr.r = static_cast<T>( gain_ * ( std::pow( p, log( Math<T>::gain4_5dB ) / log( 0.5 ) ) ) );
                }
                break;
                case Type::LOG_6DB:
                {
                    lr.l = static_cast<T>( gain_ * ( std::pow( 1 - p, log( Math<T>::gain6dB ) / log( 0.5 ) ) ) );
                    lr.r = static_cast<T>( gain_ * ( std::pow( p, log( Math<T>::gain6dB ) / log( 0.5 ) ) ) );
                }
                break;
            }
        }
    };

    template<typename T>
    static std::pair<T, T> panSamples( T Left, T Right, T ltPan, T rtPan )
    {
        auto lt = ( Left * ltPan );
        auto rt = ( Right * rtPan );
        return std::make_pair<T, T>( static_cast<T>( lt ), static_cast<T>( rt ) );
    }

    template<typename T>
    static std::pair<T, T> panSamples( T Left, T Right, Panner<T>& pan )
    {
        auto lt = ( Left * pan.getLeft() );
        auto rt = ( Right * pan.getRight() );
        return std::make_pair<T, T>( static_cast<T>( lt ), static_cast<T>( rt ) );
    }

    ///
    /// Typedef Helpers.
    ///
    using PannerFloat  = Panner<float>;
    using PannerDouble = Panner<double>;
} // namespace AudIO

#endif // AUDIO_PAN_H_INCLUDE
