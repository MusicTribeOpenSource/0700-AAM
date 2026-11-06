// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A UART that shifts bytes out at a fixed baud rate, for latency measurement. Heap use is fine
// here: this is not firmware.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

#include "kick_mcu/bsp/bsp.h"

namespace kick_mcu
{
namespace sim
{
    /// Models the transmit side of a UART with a bounded FIFO. write() puts bytes in the FIFO;
    /// each byte then takes ten bit times (start, eight data, stop) to leave, one after another.
    /// service() releases the bytes that have finished by the clock's current time and reports
    /// each with the time its last bit left, in microseconds.
    ///
    /// The FIFO size is a real design input, not a convenience. A button frame that arrives while
    /// a pad storm is draining waits for whatever is already in the FIFO, so its worst-case
    /// latency is the FIFO's drain time plus the frame's own time on the wire. Keep it short.
    ///
    /// Time comes from the IClock so that a SimClock makes the whole thing deterministic.
    class PacedUart : public bsp::IUart
    {
    public:
        using ByteSink = std::function<void( uint8_t byte, double completedUs )>;

        PacedUart( const bsp::IClock& clock, size_t fifoBytes = 24, uint32_t baud = 230400 )
            : clock_( clock )
            , capacity_( fifoBytes )
            , byteUs_( 10.0e6 / (double)baud )
        {
            lastClockUs_ = clock_.nowUs();
            extended_    = lastClockUs_;
        }

        void setSink( ByteSink sink ) { sink_ = std::move( sink ); }

        /// Microseconds one byte takes on the line.
        double byteTimeUs() const { return byteUs_; }

        size_t write( bsp::ByteSpan bytes ) override
        {
            service();
            const size_t room = capacity_ - fifo_.size();
            const size_t n    = bytes.size < room ? bytes.size : room;
            const double now  = extendedNowUs();
            for( size_t i = 0; i < n; ++i )
            {
                const double start = lineFreeAtUs_ > now ? lineFreeAtUs_ : now;
                lineFreeAtUs_      = start + byteUs_;
                fifo_.push_back( { bytes.data[i], lineFreeAtUs_ } );
            }
            return n;
        }

        size_t writableBytes() const override
        {
            const_cast<PacedUart*>( this )->service();
            return capacity_ - fifo_.size();
        }

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

        /// Releases every byte that has finished by now.
        void service()
        {
            const double now = extendedNowUs();
            while( !fifo_.empty() && fifo_.front().completedUs <= now )
            {
                if( sink_ )
                    sink_( fifo_.front().byte, fifo_.front().completedUs );
                fifo_.pop_front();
            }
        }

        size_t fifoBytes() const { return fifo_.size(); }

    private:
        struct Slot
        {
            uint8_t byte;
            double  completedUs;
        };

        /// The clock is 32-bit microseconds and wraps every 71 minutes; this keeps a running total.
        double extendedNowUs()
        {
            const uint32_t now = clock_.nowUs();
            extended_ += (uint32_t)( now - lastClockUs_ );
            lastClockUs_ = now;
            return (double)extended_;
        }

        const bsp::IClock& clock_;
        size_t             capacity_;
        double             byteUs_;
        std::deque<Slot>   fifo_;
        double             lineFreeAtUs_{ 0 };
        uint64_t           extended_{ 0 };
        uint32_t           lastClockUs_{ 0 };
        ByteSink           sink_;
        bsp::ByteHandler   handler_{ nullptr };
        void*              context_{ nullptr };
    };

} // namespace sim
} // namespace kick_mcu
