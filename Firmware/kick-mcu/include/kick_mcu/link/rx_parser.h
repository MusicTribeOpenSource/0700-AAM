// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Byte-at-a-time frame receiver.
#pragma once

#include <cstddef>
#include <cstdint>

#include "kick_mcu/contract.h"
#include "kick_mcu/link/frame_codec.h"

namespace kick_mcu
{
namespace link
{
    struct RxStats
    {
        uint32_t frames{ 0 };       ///< frames delivered
        uint32_t crcErrors{ 0 };    ///< complete frames whose CRC did not match
        uint32_t sizeErrors{ 0 };   ///< size byte of 0, or a payload larger than the receive buffer
        uint32_t garbageBytes{ 0 }; ///< bytes dropped while hunting for 'M' 'T'
        uint32_t resyncs{ 0 };      ///< times a rejected or timed-out frame was re-scanned for a later 'M' 'T'
        uint32_t timeouts{ 0 };     ///< partial frames abandoned by expire()
    };

    /// Parses the stream into frames. All storage is inside the object: one 64-byte frame buffer,
    /// plus a stack scratch of the same size while re-scanning. No heap, no blocking, and each
    /// feed() call does work bounded by one frame buffer.
    ///
    /// Recovery rules:
    ///  - Bytes outside a frame are dropped until 'M' 'T'.
    ///  - A frame with size 0, a payload over kMaxPayloadFromHost, or a bad CRC is rejected, and
    ///    every byte of it after the first is scanned again for a new 'M' 'T'. A real frame that
    ///    began inside the rejected one is therefore not lost.
    ///  - size 1 (no payload) is a valid frame. The host's PktMgr cannot receive one; this parser can.
    ///  - expire() abandons a partial frame the same way. The owner calls it when the line has
    ///    gone quiet, which flushes a frame stuck waiting for bytes that will never arrive.
    ///  - The parser does not look at type, dir or cmd. Unknown values are the caller's problem.
    ///  - A byte after the CRC (the trailer MCU-to-host frames carry) is outside a frame and is
    ///    dropped as garbage, so this parser reads both directions. It counts those bytes in
    ///    garbageBytes.
    class RxParser
    {
    public:
        /// Called from inside feed() or expire() for each valid frame. The frame is only valid for
        /// the duration of the call.
        using FrameHandler = void ( * )( void* context, const Frame& frame );

        void setHandler( FrameHandler handler, void* context )
        {
            handler_ = handler;
            context_ = context;
        }

        void feed( uint8_t byte );

        /// True while a frame has been started and not finished.
        bool inFrame() const { return state_ != State::HuntM; }

        /// Abandons the partial frame in progress, if any, and re-scans its tail for a later start.
        void expire();

        const RxStats& stats() const { return stats_; }

    private:
        enum class State : uint8_t
        {
            HuntM,
            HuntT,
            Type,
            Dir,
            Cmd,
            Size,
            Data,
            Crc,
        };

        /// Returns false when the byte completed a rejected frame; raw_ then holds that frame.
        bool step( uint8_t byte );
        void drain( uint8_t* queue, size_t count );
        void resetFrame();

        State        state_{ State::HuntM };
        uint8_t      raw_[contract::kRxFrameBufferBytes]{};
        size_t       rawLength_{ 0 };
        size_t       payloadExpected_{ 0 };
        FrameHandler handler_{ nullptr };
        void*        context_{ nullptr };
        RxStats      stats_{};
    };

} // namespace link
} // namespace kick_mcu
