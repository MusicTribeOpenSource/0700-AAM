// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include "kick_mcu/bsp/bsp.h"

namespace kick_mcu
{
namespace sim
{
    /// A clock the test advances by hand, so timing tests do not sleep and cannot flake.
    class SimClock : public bsp::IClock
    {
    public:
        uint32_t nowMs() const override { return (uint32_t)( micros_ / 1000 ); }
        uint32_t nowUs() const override { return (uint32_t)micros_; }

        void advanceMs( uint32_t ms ) { micros_ += (uint64_t)ms * 1000; }
        void advanceUs( uint32_t us ) { micros_ += us; }
        void set( uint64_t micros ) { micros_ = micros; }

    private:
        uint64_t micros_{ 0 };
    };

} // namespace sim
} // namespace kick_mcu
