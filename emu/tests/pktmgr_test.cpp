// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Standalone checks for the CPktMgr receive parser. No JUCE, no gtest.
//   c++ -std=c++17 -I ComProtocol/PktMgr emu/tests/pktmgr_test.cpp ComProtocol/PktMgr/PktMgr.cpp -o /tmp/pktmgr_test && /tmp/pktmgr_test
#include "PktMgr.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
    int failures = 0;

#define CHECK( cond )                                                                 \
    do                                                                                \
    {                                                                                 \
        if( !( cond ) )                                                               \
        {                                                                             \
            std::printf( "FAIL line %d: %s\n", __LINE__, #cond );                     \
            ++failures;                                                               \
        }                                                                             \
    } while( 0 )

    struct Parser
    {
        uint8_t rx[20]{};
        uint8_t tx[40]{};
        CPktMgr mgr{ CPktMgr::EN_COM_CHANNEL_USB, rx, sizeof( rx ), tx, sizeof( tx ), 10 };
        Parser() { mgr.Init(); }

        // Feeds bytes and returns how many complete frames were reported. Mirrors what input_events.cpp
        // does: take the frame, then ClearReceiver().
        int feed( const std::vector<uint8_t>& bytes )
        {
            int frames = 0;
            for( uint8_t b : bytes )
            {
                mgr.PushByteToRxFifo( b );
                if( mgr.IsFrameRcvComplete() )
                {
                    ++frames;
                    mgr.SetFrameRcvComplete( false );
                    mgr.ClearReceiver();
                }
            }
            return frames;
        }
    };

    // 'M' 'T' type dir cmd size payload... crc
    std::vector<uint8_t> frame( uint8_t type, uint8_t dir, uint8_t cmd, const std::vector<uint8_t>& payload )
    {
        std::vector<char> raw{ 'M', 'T', (char)type, (char)dir, (char)cmd, (char)( payload.size() + 1 ) };
        for( uint8_t b : payload )
            raw.push_back( (char)b );
        const uint8_t crc = CPktMgr::CrcGen( raw );
        std::vector<uint8_t> out( raw.begin(), raw.end() );
        out.push_back( crc );
        return out;
    }

    std::vector<uint8_t> cat( std::vector<uint8_t> a, const std::vector<uint8_t>& b )
    {
        a.insert( a.end(), b.begin(), b.end() );
        return a;
    }
}

int main()
{
    const auto good = frame( 1, 0, 3, { 7, 1 } );

    { // valid frame
        Parser p;
        CHECK( p.feed( good ) == 1 );
    }
    { // line noise before the frame must not corrupt its CRC
        Parser p;
        CHECK( p.feed( cat( { 0x00, 0xFF, 'T', 'M', 0x13 }, good ) ) == 1 );
    }
    { // 'M' 'M' 'T': the first M was noise
        Parser p;
        CHECK( p.feed( cat( { 'M' }, good ) ) == 1 );
    }
    { // a frame with no payload (size byte 1) is delivered, not stuck
        Parser p;
        CHECK( p.feed( frame( 1, 0, 9, {} ) ) == 1 );
        CHECK( p.feed( good ) == 1 );
    }
    { // bad CRC is dropped and the next good frame still parses
        Parser p;
        auto bad = good;
        bad.back() ^= 0x55;
        CHECK( p.feed( bad ) == 0 );
        CHECK( p.feed( good ) == 1 );
    }
    { // payload larger than the RX fifo must not be accepted, and must not overrun it
        Parser p;
        CHECK( p.feed( frame( 1, 0, 5, std::vector<uint8_t>( 40, 0xAA ) ) ) == 0 );
        CHECK( p.feed( good ) == 1 );
    }
    { // two instances do not share a partial frame
        Parser a, b;
        const std::vector<uint8_t> head( good.begin(), good.begin() + 4 );
        const std::vector<uint8_t> tail( good.begin() + 4, good.end() );
        a.feed( head );
        CHECK( b.feed( good ) == 1 ); // would be corrupted by a's half-frame if the buffer were static
        CHECK( a.feed( tail ) == 1 );
    }

    std::printf( failures ? "%d FAILED\n" : "all passed\n", failures );
    return failures ? 1 : 0;
}
