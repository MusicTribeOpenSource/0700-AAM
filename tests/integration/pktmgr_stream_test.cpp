// Integration tests: the packet codec used as the host does, a continuous byte
// stream in, whole messages out, with the receiver reset after each frame.

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

#include "support/pktmgr_harness.h"

using kicktest::PktHarness;

namespace
{
    struct Msg
    {
        CPktMgr::enumPacketType type;
        CPktMgr::enumPacketDir  dir;
        int                     cmd;
        std::vector<int>        data;
    };

    Msg randomMsg( std::mt19937& rng )
    {
        Msg m;
        m.type = static_cast<CPktMgr::enumPacketType>( rng() % 8 );
        m.dir  = static_cast<CPktMgr::enumPacketDir>( rng() % 2 );
        m.cmd  = rng() % 256;
        // RX FIFO is 30 bytes and PktMgr does not bounds-check it, so stay inside.
        const int n = 1 + rng() % 28;
        for( int i = 0; i < n; ++i )
            m.data.push_back( rng() % 256 );
        return m;
    }

    // Feed a stream; after each completed frame, harvest and reset like input_events.cpp does.
    std::vector<Msg> decodeStream( PktHarness& h, const std::vector<char>& stream, size_t chunk = 1 )
    {
        std::vector<Msg> out;
        for( size_t i = 0; i < stream.size(); i += chunk )
        {
            const size_t end = std::min( stream.size(), i + chunk );
            for( size_t j = i; j < end; ++j )
            {
                h.mgr.PushByteToRxFifo( static_cast<uint8_t>( stream[j] ) );
                if( h.mgr.IsFrameRcvComplete() )
                {
                    Msg m;
                    m.type = h.mgr.GetRxPktType();
                    m.dir  = h.mgr.GetPktDir();
                    m.cmd  = h.mgr.GetRxCommand();
                    for( uint8_t b : h.drainPayload() )
                        m.data.push_back( b );
                    out.push_back( std::move( m ) );
                    h.mgr.ClearReceiver();
                }
            }
        }
        return out;
    }
}

TEST( PktMgrStream, ManyRandomMessagesArriveIntactAndInOrder )
{
    std::mt19937 rng( 0xC0FFEE );
    PktHarness   enc;
    std::vector<Msg>  sent;
    std::vector<char> stream;
    for( int i = 0; i < 500; ++i )
    {
        sent.push_back( randomMsg( rng ) );
        const auto& m    = sent.back();
        auto        wire = enc.encode( m.type, m.dir, m.cmd, m.data );
        stream.insert( stream.end(), wire.begin(), wire.end() );
    }

    PktHarness rx;
    auto       got = decodeStream( rx, stream );

    ASSERT_EQ( got.size(), sent.size() );
    for( size_t i = 0; i < sent.size(); ++i )
    {
        ASSERT_EQ( got[i].type, sent[i].type ) << "msg " << i;
        ASSERT_EQ( got[i].dir, sent[i].dir ) << "msg " << i;
        ASSERT_EQ( got[i].cmd, sent[i].cmd ) << "msg " << i;
        ASSERT_EQ( got[i].data.size(), sent[i].data.size() ) << "msg " << i;
        for( size_t k = 0; k < sent[i].data.size(); ++k )
            ASSERT_EQ( got[i].data[k], sent[i].data[k] ) << "msg " << i << " byte " << k;
    }
}

TEST( PktMgrStream, ChunkSizeDoesNotChangeTheResult )
{
    std::mt19937      rng( 42 );
    PktHarness        enc;
    std::vector<char> stream;
    size_t            count = 0;
    for( int i = 0; i < 60; ++i, ++count )
    {
        auto m    = randomMsg( rng );
        auto wire = enc.encode( m.type, m.dir, m.cmd, m.data );
        stream.insert( stream.end(), wire.begin(), wire.end() );
    }

    PktHarness base;
    auto       ref = decodeStream( base, stream, 1 );
    ASSERT_EQ( ref.size(), count );

    for( size_t chunk : { 2u, 3u, 7u, 64u, 4096u } )
    {
        PktHarness rx;
        auto       got = decodeStream( rx, stream, chunk );
        ASSERT_EQ( got.size(), ref.size() ) << "chunk " << chunk;
        for( size_t i = 0; i < ref.size(); ++i )
            ASSERT_EQ( got[i].data, ref[i].data ) << "chunk " << chunk << " msg " << i;
    }
}

TEST( PktMgrStream, EverySingleBitErrorInAFrameIsDetected )
{
    PktHarness       enc;
    const auto       good = enc.encode( CPktMgr::EN_PKT_TYPE_PAD, CPktMgr::EN_PKT_REQUEST, 17, { 10, 20, 30, 40 } );
    const std::vector<uint8_t> goodPayload{ 10, 20, 30, 40 };

    // The unflipped frame must be accepted, or the loop below proves nothing.
    {
        PktHarness rx;
        rx.feed( good );
        ASSERT_TRUE( rx.mgr.IsFrameRcvComplete() );
        ASSERT_EQ( rx.drainPayload(), goodPayload );
    }

    // Flip bits in every byte after the 'MT' sync and before the CRC: the receiver
    // must never hand back a frame whose payload differs from what was sent. Each
    // iteration uses a fresh receiver, so no state leaks between them.
    for( size_t byte = 2; byte < good.size(); ++byte )
        for( int bit = 0; bit < 8; ++bit )
        {
            auto bad = good;
            bad[byte] = static_cast<char>( bad[byte] ^ ( 1 << bit ) );
            PktHarness rx;
            rx.mgr.ClearReceiver();
            rx.feed( bad );
            if( rx.mgr.IsFrameRcvComplete() )
                ASSERT_EQ( rx.drainPayload(), goodPayload ) << "byte " << byte << " bit " << bit << " delivered corrupt data";
        }
}

TEST( PktMgrStream, BidirectionalExchangeRequestThenResponse )
{
    // Host sends a request; device answers with a response carrying the same command id.
    PktHarness host, device;
    auto       request = host.encode( CPktMgr::EN_PKT_TYPE_LED, CPktMgr::EN_PKT_REQUEST, 0x21, { 5, 255, 0, 0 } );

    auto atDevice = decodeStream( device, request );
    ASSERT_EQ( atDevice.size(), 1u );
    EXPECT_EQ( atDevice[0].dir, CPktMgr::EN_PKT_REQUEST );

    auto response = device.encode( atDevice[0].type, CPktMgr::EN_PKT_RESPONSE, atDevice[0].cmd, { 1 } );
    auto atHost   = decodeStream( host, response );
    ASSERT_EQ( atHost.size(), 1u );
    EXPECT_EQ( atHost[0].dir, CPktMgr::EN_PKT_RESPONSE );
    EXPECT_EQ( atHost[0].cmd, 0x21 );
    EXPECT_EQ( atHost[0].type, CPktMgr::EN_PKT_TYPE_LED );
}

TEST( PktMgrStream, MaximumPayloadFitsTheRxFifo )
{
    PktHarness enc, rx;
    std::vector<int> payload( 28, 0x5A );
    auto             got = decodeStream( rx, enc.encode( CPktMgr::EN_PKT_TYPE_KNOBS, CPktMgr::EN_PKT_REQUEST, 3, payload ) );
    ASSERT_EQ( got.size(), 1u );
    EXPECT_EQ( got[0].data, payload );
}

// The CRC accumulator used to be a function-local static shared by every CPktMgr, so a
// truncated frame on one link poisoned the next frame on another. It is per instance now.
TEST( PktMgrStream, AbandonedFrameOnOneInstanceDoesNotAffectAnother )
{
    PktHarness a;
    auto       truncated = a.encode( CPktMgr::EN_PKT_TYPE_BUTTON, CPktMgr::EN_PKT_REQUEST, 1, { 1, 2, 3 } );
    truncated.resize( 7 ); // header plus one payload byte, then the link drops
    a.feed( truncated );

    PktHarness b;
    b.feed( b.encode( CPktMgr::EN_PKT_TYPE_BUTTON, CPktMgr::EN_PKT_REQUEST, 2, { 9, 9 } ) );
    EXPECT_TRUE( b.mgr.IsFrameRcvComplete() );
}
