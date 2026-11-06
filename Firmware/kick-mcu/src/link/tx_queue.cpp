// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/link/tx_queue.h"

#include <cstring>

namespace kick_mcu
{
namespace link
{
    constexpr size_t TxQueue::kLaneDepth[kLaneCount];

    TxQueue::TxQueue()
    {
        lanes_[(size_t)Lane::System]     = { system_, kLaneDepth[0], 0, 0 };
        lanes_[(size_t)Lane::Button]     = { button_, kLaneDepth[1], 0, 0 };
        lanes_[(size_t)Lane::PadKnob]    = { padKnob_, kLaneDepth[2], 0, 0 };
        lanes_[(size_t)Lane::Aftertouch] = { aftertouch_, kLaneDepth[3], 0, 0 };
    }

    bool TxQueue::enqueue( Lane lane, uint8_t type, uint8_t dir, uint8_t cmd, const uint8_t* payload, size_t length )
    {
        const size_t index = (size_t)lane;
        LaneRing&    ring  = lanes_[index];

        uint8_t bytes[kMaxEncodedToHost];
        const size_t n = encodeFrame( bytes, sizeof( bytes ), Wire::McuToHost, type, dir, cmd, payload, length );
        if( n == 0 )
        {
            ++stats_.rejected;
            return false;
        }

        if( ring.count == ring.capacity )
        {
            if( lane != Lane::Aftertouch )
            {
                ++stats_.dropped[index];
                return false;
            }
            // Aftertouch: discard the oldest to make room for the newest.
            ring.head = ( ring.head + 1 ) % ring.capacity;
            --ring.count;
            ++stats_.dropped[index];
        }

        Entry& slot = ring.entries[( ring.head + ring.count ) % ring.capacity];
        slot.length = (uint8_t)n;
        std::memcpy( slot.bytes, bytes, n );
        ++ring.count;
        ++stats_.queued[index];
        return true;
    }

    size_t TxQueue::discard( Lane lane, uint8_t type, uint8_t firstByte )
    {
        LaneRing& ring = lanes_[(size_t)lane];

        // Compact in place. `write` never passes `read`, so a copy never overwrites an entry
        // that has not been looked at yet, and the order of the kept frames is unchanged.
        size_t write = 0;
        for( size_t read = 0; read < ring.count; ++read )
        {
            const Entry& e = ring.entries[( ring.head + read ) % ring.capacity];
            // Frame layout: 'M' 'T' type dir cmd size payload...; the first payload byte is at 6.
            const bool match = e.length > contract::kHeaderBytes && e.bytes[2] == type && e.bytes[contract::kHeaderBytes] == firstByte;
            if( match )
                continue;
            if( write != read )
                ring.entries[( ring.head + write ) % ring.capacity] = e;
            ++write;
        }
        const size_t removed = ring.count - write;
        ring.count           = write;
        return removed;
    }

    size_t TxQueue::pump( bsp::IUart& uart )
    {
        size_t sent = 0;
        for( ;; )
        {
            size_t lane = 0;
            while( lane < kLaneCount && lanes_[lane].count == 0 )
                ++lane;
            if( lane == kLaneCount )
                break;

            LaneRing&    ring  = lanes_[lane];
            const Entry& entry = ring.entries[ring.head];
            if( uart.writableBytes() < entry.length )
                break;

            const size_t accepted = uart.write( { entry.bytes, entry.length } );
            if( accepted != entry.length )
            {
                // The UART took part of a frame after reporting room for all of it: a BSP bug.
                // The accepted prefix is already on the wire and cannot be recalled, and sending
                // the whole frame again would repeat it, so the frame is dropped and counted.
                ++stats_.partialWrites;
                ring.head = ( ring.head + 1 ) % ring.capacity;
                --ring.count;
                continue;
            }
            ring.head = ( ring.head + 1 ) % ring.capacity;
            --ring.count;
            ++stats_.sent[lane];
            ++sent;
        }
        return sent;
    }

    bool TxQueue::empty() const
    {
        for( const LaneRing& ring : lanes_ )
            if( ring.count != 0 )
                return false;
        return true;
    }

} // namespace link
} // namespace kick_mcu
