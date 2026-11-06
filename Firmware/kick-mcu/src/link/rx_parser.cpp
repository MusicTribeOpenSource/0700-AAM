// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/link/rx_parser.h"

#include <cstring>

namespace kick_mcu
{
namespace link
{
    void RxParser::resetFrame()
    {
        state_           = State::HuntM;
        rawLength_       = 0;
        payloadExpected_ = 0;
    }

    bool RxParser::step( uint8_t byte )
    {
        switch( state_ )
        {
            case State::HuntM:
                if( byte == contract::kStartByte1 )
                {
                    raw_[0]    = byte;
                    rawLength_ = 1;
                    state_     = State::HuntT;
                }
                else
                    ++stats_.garbageBytes;
                return true;

            case State::HuntT:
                if( byte == contract::kStartByte2 )
                {
                    raw_[rawLength_++] = byte;
                    state_             = State::Type;
                }
                else if( byte == contract::kStartByte1 )
                {
                    // "M M T ...": the first M was noise, this one may start a frame.
                    ++stats_.garbageBytes;
                }
                else
                {
                    stats_.garbageBytes += 2;
                    resetFrame();
                }
                return true;

            case State::Type:
                raw_[rawLength_++] = byte;
                state_             = State::Dir;
                return true;

            case State::Dir:
                raw_[rawLength_++] = byte;
                state_             = State::Cmd;
                return true;

            case State::Cmd:
                raw_[rawLength_++] = byte;
                state_             = State::Size;
                return true;

            case State::Size:
                raw_[rawLength_++] = byte;
                if( byte == 0 || (size_t)( byte - contract::kCrcBytes ) > contract::kMaxPayloadFromHost )
                {
                    ++stats_.sizeErrors;
                    return false;
                }
                payloadExpected_ = (size_t)( byte - contract::kCrcBytes );
                state_           = payloadExpected_ == 0 ? State::Crc : State::Data;
                return true;

            case State::Data:
                raw_[rawLength_++] = byte;
                if( rawLength_ == contract::kHeaderBytes + payloadExpected_ )
                    state_ = State::Crc;
                return true;

            case State::Crc:
            {
                const uint8_t expected = crc8( raw_, rawLength_ );
                raw_[rawLength_++]     = byte;
                if( expected != byte )
                {
                    ++stats_.crcErrors;
                    return false;
                }

                Frame frame;
                frame.type   = raw_[2];
                frame.dir    = raw_[3];
                frame.cmd    = raw_[4];
                frame.length = (uint8_t)payloadExpected_;
                if( payloadExpected_ > 0 )
                    std::memcpy( frame.payload, raw_ + contract::kHeaderBytes, payloadExpected_ );
                resetFrame();
                ++stats_.frames;
                if( handler_ != nullptr )
                    handler_( context_, frame );
                return true;
            }
        }
        return true;
    }

    // `queue` holds `count` bytes still to be fed, in order. When a frame is rejected, its bytes
    // after the first go back to the front of the queue and the first is dropped. Each rejection
    // removes at least one byte from the stream, so the loop ends, and the queue never holds more
    // than one frame buffer: it only ever contains bytes that were already inside raw_.
    void RxParser::drain( uint8_t* queue, size_t count )
    {
        size_t next = 0;
        while( next < count )
        {
            const uint8_t byte = queue[next++];
            if( step( byte ) )
                continue;

            ++stats_.resyncs;
            const size_t replay    = rawLength_ - 1;
            const size_t remaining = count - next;
            std::memmove( queue + replay, queue + next, remaining );
            std::memcpy( queue, raw_ + 1, replay );
            count = replay + remaining;
            next  = 0;
            resetFrame();
            stats_.garbageBytes += 1; // the 'M' that started the rejected frame
        }
    }

    void RxParser::feed( uint8_t byte )
    {
        uint8_t queue[contract::kRxFrameBufferBytes];
        queue[0] = byte;
        drain( queue, 1 );
    }

    void RxParser::expire()
    {
        if( state_ == State::HuntM )
            return;

        ++stats_.timeouts;
        ++stats_.resyncs;
        uint8_t      queue[contract::kRxFrameBufferBytes];
        const size_t replay = rawLength_ - 1;
        std::memcpy( queue, raw_ + 1, replay );
        resetFrame();
        stats_.garbageBytes += 1;
        drain( queue, replay );
    }

} // namespace link
} // namespace kick_mcu
