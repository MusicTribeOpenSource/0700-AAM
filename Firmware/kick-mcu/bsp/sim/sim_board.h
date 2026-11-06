// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Fake input hardware for tests and the simulator: a state the caller sets, read back through
// the BSP scanner interfaces. Stands in for the matrix, pad sensors, encoders and touch strip.
#pragma once

#include <cstdint>

#include "kick_mcu/bsp/bsp.h"

namespace kick_mcu
{
namespace sim
{
    class SimMatrix : public bsp::IMatrixScanner
    {
    public:
        uint64_t scan() override { return keys_ ^ glitch_; }

        void press( unsigned code ) { keys_ |= uint64_t( 1 ) << code; }
        void release( unsigned code ) { keys_ &= ~( uint64_t( 1 ) << code ); }

        /// Flips a key for the next scan only: contact bounce, or a noise spike.
        void glitchOnce( unsigned code ) { glitch_ = uint64_t( 1 ) << code; }
        void clearGlitch() { glitch_ = 0; }

    private:
        uint64_t keys_{ 0 };
        uint64_t glitch_{ 0 };
    };

    class SimPads : public bsp::IPadSensor
    {
    public:
        void sample( uint16_t ( &pressure )[16] ) override
        {
            for( int i = 0; i < 16; ++i )
                pressure[i] = raw_[i];
        }

        void set( unsigned pad, uint16_t raw ) { raw_[pad] = raw; }

    private:
        uint16_t raw_[16]{};
    };

    /// Quadrature outputs. step() moves one encoder one transition, in the order a clockwise
    /// turn produces: bit pairs (A + 2B) 0, 1, 3, 2, 0, ...
    class SimEncoders : public bsp::IEncoderBank
    {
    public:
        uint64_t readPhases() override { return phases_; }

        void step( unsigned encoder, bool clockwise )
        {
            static constexpr uint8_t kClockwise[4] = { 1, 3, 0, 2 };         // next state after 0, 1, 2, 3
            static constexpr uint8_t kCounter[4]   = { 2, 0, 3, 1 };         // previous state
            const unsigned shift = 2 * encoder;
            const uint8_t  now   = (uint8_t)( ( phases_ >> shift ) & 3u );
            const uint8_t  next  = clockwise ? kClockwise[now] : kCounter[now];
            phases_ = ( phases_ & ~( uint64_t( 3 ) << shift ) ) | ( uint64_t( next ) << shift );
        }

        void setRaw( uint64_t phases ) { phases_ = phases; }

    private:
        uint64_t phases_{ 0 };
    };

    class SimStrip : public bsp::IStrip
    {
    public:
        bool read( uint16_t& position ) override
        {
            position = position_;
            return touched_;
        }

        void touch( uint16_t position )
        {
            touched_  = true;
            position_ = position;
        }
        void lift() { touched_ = false; }

    private:
        bool     touched_{ false };
        uint16_t position_{ 0 };
    };

    struct SimBoard
    {
        SimMatrix   matrix;
        SimPads     pads;
        SimEncoders encoders;
        SimStrip    strip;
    };

} // namespace sim
} // namespace kick_mcu
