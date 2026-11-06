// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Flash for the desktop: a byte array with NOR behaviour and power-cut injection.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "kick_mcu/bsp/bsp.h"

namespace kick_mcu
{
namespace sim
{
    /// NOR flash as the update path has to treat it:
    ///
    ///  - erased bytes read 0xFF, and eraseBlock() is the only way back to 0xFF
    ///  - write() can only clear bits. A write that would need a 0 bit to become 1 is refused as
    ///    a whole (illegalWrites++), the way a real controller flags a program error
    ///
    /// Power cuts. cutPowerAfter(n) lets exactly n more single-byte operations happen, then the
    /// device is dead: every later read, write and erase fails until restorePower(). One byte
    /// programmed and one byte erased each count as one operation, so a cut can land on any
    /// byte boundary of any write or erase, which is the property the update tests sweep.
    /// An erase that is cut leaves the bytes erased so far at 0xFF and the rest as they were,
    /// and a write that is cut leaves the prefix programmed. Real parts can also leave a byte
    /// half-programmed; that is not modelled, and the CRC on every record is what covers it.
    class SimFlash : public bsp::IFlash
    {
    public:
        SimFlash( size_t sizeBytes, size_t blockBytes )
            : mem_( sizeBytes, 0xFF )
            , blockBytes_( blockBytes )
        {
        }

        size_t sizeBytes() const override { return mem_.size(); }
        size_t eraseBlockBytes() const override { return blockBytes_; }

        bool read( size_t address, uint8_t* out, size_t length ) override
        {
            if( dead_ || address > mem_.size() || length > mem_.size() - address )
                return false;
            std::memcpy( out, mem_.data() + address, length );
            return true;
        }

        bool write( size_t address, const uint8_t* data, size_t length ) override
        {
            if( dead_ || address > mem_.size() || length > mem_.size() - address )
                return false;
            for( size_t i = 0; i < length; ++i )
            {
                if( ( data[i] & ~mem_[address + i] ) != 0 )
                {
                    ++illegalWrites_;
                    return false;
                }
            }
            for( size_t i = 0; i < length; ++i )
            {
                if( !step() )
                    return false;
                mem_[address + i] &= data[i];
            }
            return true;
        }

        bool eraseBlock( size_t address ) override
        {
            if( dead_ || address % blockBytes_ != 0 || address >= mem_.size() )
                return false;
            for( size_t i = 0; i < blockBytes_; ++i )
            {
                if( !step() )
                    return false;
                mem_[address + i] = 0xFF;
            }
            return true;
        }

        // ---- test controls -------------------------------------------------------------------

        /// Dead after `operations` more byte operations. 0 kills it before the next one.
        void cutPowerAfter( uint64_t operations )
        {
            armed_     = true;
            remaining_ = operations;
        }
        void disarm() { armed_ = false; }

        /// Power returns: the contents stay as the cut left them.
        void restorePower()
        {
            dead_  = false;
            armed_ = false;
        }

        bool     powerLost() const { return dead_; }
        uint64_t operations() const { return operations_; } ///< byte operations performed so far
        uint32_t illegalWrites() const { return illegalWrites_; }

        /// Direct access for tests and the sim's flash file, bypassing power state and NOR rules.
        std::vector<uint8_t>&       contents() { return mem_; }
        const std::vector<uint8_t>& contents() const { return mem_; }

    private:
        bool step()
        {
            if( armed_ )
            {
                if( remaining_ == 0 )
                {
                    dead_ = true;
                    return false;
                }
                --remaining_;
            }
            ++operations_;
            return true;
        }

        std::vector<uint8_t> mem_;
        size_t               blockBytes_;
        bool                 armed_{ false };
        bool                 dead_{ false };
        uint64_t             remaining_{ 0 };
        uint64_t             operations_{ 0 };
        uint32_t             illegalWrites_{ 0 };
    };

    /// Records what the firmware asked of the power rail and the reset line.
    class SimPower : public bsp::IPower
    {
    public:
        void requestOff() override { ++offRequests_; }
        void requestReset() override { ++resetRequests_; }

        uint32_t offRequests() const { return offRequests_; }
        uint32_t resetRequests() const { return resetRequests_; }

    private:
        uint32_t offRequests_{ 0 };
        uint32_t resetRequests_{ 0 };
    };

} // namespace sim
} // namespace kick_mcu
