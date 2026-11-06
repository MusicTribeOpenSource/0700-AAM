// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/input/debounce.h"

namespace kick_mcu
{
namespace input
{
    void Debouncer::reset( uint32_t nowUs )
    {
        state_ = 0;
        for( uint32_t& c : counter_ )
            c = 0;
        lastUs_  = nowUs;
        started_ = true;
    }

    uint64_t Debouncer::update( uint64_t raw, uint32_t nowUs )
    {
        if( !started_ )
            reset( nowUs );

        // One reading never counts for more than half of the shorter window, so a single reading
        // after a stall cannot flip a key by itself; it takes at least two.
        const uint32_t shortest = config_.pressUs < config_.releaseUs ? config_.pressUs : config_.releaseUs;
        const uint32_t longest  = config_.pressUs > config_.releaseUs ? config_.pressUs : config_.releaseUs;
        uint32_t       maxStep  = ( shortest != 0 ? shortest : longest ) / 2;
        if( maxStep == 0 )
            maxStep = 1;
        uint32_t dt = nowUs - lastUs_;
        lastUs_     = nowUs;
        if( dt > maxStep )
            dt = maxStep;

        for( unsigned key = 0; key < 64; ++key )
        {
            const bool     closed  = ( raw >> key ) & 1u;
            const bool     pressed = ( state_ >> key ) & 1u;
            uint32_t&      counter = counter_[key];
            const uint32_t top     = pressed ? config_.releaseUs : config_.pressUs;

            if( closed )
            {
                counter = counter + dt > top ? top : counter + dt;
                if( !pressed && counter >= config_.pressUs )
                {
                    state_ |= uint64_t( 1 ) << key;
                    counter = config_.releaseUs;
                }
            }
            else
            {
                counter = counter > dt ? counter - dt : 0;
                if( pressed && counter == 0 )
                    state_ &= ~( uint64_t( 1 ) << key );
            }
        }
        return state_;
    }

} // namespace input
} // namespace kick_mcu
