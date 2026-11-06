// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Desktop UART for tests and the simulator. Heap use is fine here: this is not firmware.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "kick_mcu/bsp/bsp.h"

namespace kick_mcu
{
namespace sim
{
    /// A UART with a bounded transmit FIFO. write() fills the FIFO, drain() empties it onto the
    /// "wire" (a byte log the test reads). injectRx() plays bytes in through the receive handler
    /// the way the interrupt would.
    class SimUart : public bsp::IUart
    {
    public:
        explicit SimUart( size_t txCapacity = 256 )
            : capacity_( txCapacity )
        {
        }

        size_t write( bsp::ByteSpan bytes ) override
        {
            const size_t room = capacity_ - fifo_.size();
            const size_t n    = bytes.size < room ? bytes.size : room;
            fifo_.insert( fifo_.end(), bytes.data, bytes.data + n );
            return n;
        }

        size_t writableBytes() const override { return capacity_ - fifo_.size(); }

        void onByte( bsp::ByteHandler handler, void* context ) override
        {
            handler_ = handler;
            context_ = context;
        }

        void injectRx( const uint8_t* data, size_t length )
        {
            for( size_t i = 0; i < length; ++i )
                if( handler_ != nullptr )
                    handler_( context_, data[i] );
        }

        void injectRx( const std::vector<uint8_t>& bytes ) { injectRx( bytes.data(), bytes.size() ); }

        /// Moves up to `count` bytes from the FIFO to the wire log.
        void drain( size_t count = (size_t)-1 )
        {
            const size_t n = count < fifo_.size() ? count : fifo_.size();
            wire_.insert( wire_.end(), fifo_.begin(), fifo_.begin() + (std::ptrdiff_t)n );
            fifo_.erase( fifo_.begin(), fifo_.begin() + (std::ptrdiff_t)n );
        }

        const std::vector<uint8_t>& wire() const { return wire_; }
        void                        clearWire() { wire_.clear(); }
        size_t                      fifoBytes() const { return fifo_.size(); }

    private:
        size_t               capacity_;
        std::vector<uint8_t> fifo_;
        std::vector<uint8_t> wire_;
        bsp::ByteHandler     handler_{ nullptr };
        void*                context_{ nullptr };
    };

} // namespace sim
} // namespace kick_mcu
