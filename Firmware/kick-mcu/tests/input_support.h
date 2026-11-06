// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <vector>

#include "kick_mcu/input/event_sink.h"
#include "test_support.h"

namespace testing_support
{
/// An IEventSink that records what it is given. `refuse` makes send() fail, `accept` is what
/// accepting() reports.
struct RecordingSink : kick_mcu::input::IEventSink
{
    struct Event
    {
        kick_mcu::link::Lane lane;
        uint8_t              type, cmd;
        Bytes                payload;

        bool operator==( const Event& o ) const { return lane == o.lane && type == o.type && cmd == o.cmd && payload == o.payload; }
    };
    struct Discard
    {
        kick_mcu::link::Lane lane;
        uint8_t              type, firstByte;
    };

    std::vector<Event>   events;
    std::vector<Discard> discards;
    bool                 refuse{ false };
    bool                 accept{ true };
    int                  refuseNext{ 0 }; ///< refuse this many sends, then accept again

    bool accepting() const override { return accept; }

    bool send( kick_mcu::link::Lane lane, uint8_t type, uint8_t cmd, const uint8_t* payload, size_t length ) override
    {
        if( refuse )
            return false;
        if( refuseNext > 0 )
        {
            --refuseNext;
            return false;
        }
        events.push_back( { lane, type, cmd, Bytes( payload, payload + length ) } );
        return true;
    }

    void discardQueued( kick_mcu::link::Lane lane, uint8_t type, uint8_t firstByte ) override { discards.push_back( { lane, type, firstByte } ); }

    void clear() { events.clear(); discards.clear(); }

    /// Events of one packet type, in order.
    std::vector<Event> ofType( uint8_t type ) const
    {
        std::vector<Event> out;
        for( const auto& e : events )
            if( e.type == type )
                out.push_back( e );
        return out;
    }
};
} // namespace testing_support
