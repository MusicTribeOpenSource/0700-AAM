// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace kick_mcu
{
namespace link
{
    /// Single-producer single-consumer ring of bytes, safe between an interrupt handler (the
    /// producer) and the main loop (the consumer) with no lock. Capacity must be a power of two.
    /// One slot stays empty to tell full from empty, so it holds N - 1 bytes.
    template<size_t N>
    class SpscByteRing
    {
        static_assert( N >= 2 && ( N & ( N - 1 ) ) == 0, "capacity must be a power of two" );

    public:
        /// Producer side. Returns false, and stores nothing, when the ring is full.
        bool push( uint8_t byte )
        {
            const size_t head = head_.load( std::memory_order_relaxed );
            const size_t next = ( head + 1 ) & ( N - 1 );
            if( next == tail_.load( std::memory_order_acquire ) )
                return false;
            data_[head] = byte;
            head_.store( next, std::memory_order_release );
            return true;
        }

        /// Consumer side. Returns false when the ring is empty.
        bool pop( uint8_t& byte )
        {
            const size_t tail = tail_.load( std::memory_order_relaxed );
            if( tail == head_.load( std::memory_order_acquire ) )
                return false;
            byte = data_[tail];
            tail_.store( ( tail + 1 ) & ( N - 1 ), std::memory_order_release );
            return true;
        }

        static constexpr size_t capacity() { return N - 1; }

    private:
        uint8_t             data_[N]{};
        std::atomic<size_t> head_{ 0 };
        std::atomic<size_t> tail_{ 0 };
    };

} // namespace link
} // namespace kick_mcu
