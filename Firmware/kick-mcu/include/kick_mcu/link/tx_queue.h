// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Transmit queue with priority lanes.
#pragma once

#include <cstddef>
#include <cstdint>

#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/link/frame_codec.h"

namespace kick_mcu
{
namespace link
{
    /// Lanes, highest priority first. The numeric order is the drain order.
    enum class Lane : uint8_t
    {
        System = 0, ///< handshake, version, heartbeat
        Button,     ///< button press and release
        PadKnob,    ///< pad down and up, encoder steps, touch strip
        Aftertouch, ///< continuous pad pressure; the first thing to give way
        Count,
    };

    constexpr size_t kLaneCount = (size_t)Lane::Count;

    struct TxStats
    {
        uint32_t queued[kLaneCount]{};  ///< frames accepted into the lane
        uint32_t sent[kLaneCount]{};    ///< frames handed to the UART
        uint32_t dropped[kLaneCount]{}; ///< frames lost because the lane was full (see enqueue())
        uint32_t rejected{ 0 };         ///< frames refused because the payload is too long for the wire
        uint32_t partialWrites{ 0 };    ///< frames the UART only partly accepted despite reporting room (a BSP bug)
    };

    /// Fixed-size queue of already-encoded MCU-to-host frames. Lane depths are set at compile
    /// time. Not thread-safe: enqueue() and pump() belong to one context, the main loop.
    ///
    /// Why lanes: a pad storm (16 pads, velocity plus aftertouch) can queue faster than the UART
    /// drains. With one FIFO, a button press would wait behind it or be dropped when it filled.
    /// With lanes, pump() always sends the highest-priority lane that has a frame, so a button
    /// waits for at most the frame already in flight, and the storm loses its own lowest lane first.
    ///
    /// Ordering holds inside a lane only. A button frame can overtake an earlier pad frame, and
    /// aftertouch for a pad can arrive after that pad's release. The host copes with the first; the
    /// second is the input layer's to avoid by not queueing aftertouch for a released pad.
    ///
    /// Overflow: when a lane is full the new frame is dropped and dropped[lane] counts it, except
    /// in the Aftertouch lane, where the oldest frame is dropped instead because the newest
    /// pressure value is the one worth sending.
    class TxQueue
    {
    public:
        TxQueue();
        TxQueue( const TxQueue& )            = delete;
        TxQueue& operator=( const TxQueue& ) = delete;

        /// Queues one frame for the host. Returns false when it was not queued: the payload is too
        /// long for the wire (rejected++), or the lane was full and the new frame dropped
        /// (dropped++). In the Aftertouch lane a full lane makes room and the call returns true.
        bool enqueue( Lane lane, uint8_t type, uint8_t dir, uint8_t cmd, const uint8_t* payload, size_t length );

        /// Sends frames from the highest-priority non-empty lane for as long as the UART has room
        /// for the next whole frame. Returns the number of frames sent. Never blocks. If the
        /// frame at the head of the best lane does not fit, it stops there instead of sending a
        /// smaller frame from a lower lane, so priority order is never inverted.
        size_t pump( bsp::IUart& uart );

        /// Removes queued frames of `type` whose first payload byte is `firstByte` from a lane,
        /// keeping the order of the rest, and returns how many it removed. They are not counted as
        /// dropped: the caller asked for them to go. A frame already handed to the UART is out of
        /// reach. The input layer uses it to pull a released pad's aftertouch out of the queue.
        size_t discard( Lane lane, uint8_t type, uint8_t firstByte );

        /// Discards everything queued in a lane without counting it as dropped.
        void clear( Lane lane ) { lanes_[(size_t)lane].count = 0; lanes_[(size_t)lane].head = 0; }

        size_t depth( Lane lane ) const { return lanes_[(size_t)lane].count; }
        bool   empty() const;

        const TxStats& stats() const { return stats_; }

        static constexpr size_t kLaneDepth[kLaneCount] = { 8, 32, 64, 16 };

    private:
        struct Entry
        {
            uint8_t length;
            uint8_t bytes[kMaxEncodedToHost];
        };

        struct LaneRing
        {
            Entry* entries{ nullptr };
            size_t capacity{ 0 };
            size_t head{ 0 };
            size_t count{ 0 };
        };

        Entry    system_[kLaneDepth[0]];
        Entry    button_[kLaneDepth[1]];
        Entry    padKnob_[kLaneDepth[2]];
        Entry    aftertouch_[kLaneDepth[3]];
        LaneRing lanes_[kLaneCount];
        TxStats  stats_{};
    };

} // namespace link
} // namespace kick_mcu
