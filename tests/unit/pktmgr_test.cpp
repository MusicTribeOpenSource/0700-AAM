// Unit tests for the serial packet framer used between the KICK front panel
// MCU and the Linux host (ComProtocol/PktMgr).

#include <gtest/gtest.h>

#include "support/pktmgr_harness.h"

using kicktest::PktHarness;

namespace
{
    constexpr auto kButton = CPktMgr::EN_PKT_TYPE_BUTTON;
    constexpr auto kLed    = CPktMgr::EN_PKT_TYPE_LED;
    constexpr auto kReq    = CPktMgr::EN_PKT_REQUEST;
    constexpr auto kResp   = CPktMgr::EN_PKT_RESPONSE;
}

TEST( PktMgrCrc, EmptyBufferIsInitialValue )
{
    std::vector<char> empty;
    EXPECT_EQ( CPktMgr::CrcGen( empty ), 0xFF );
}

TEST( PktMgrCrc, MatchesCrc8Poly0x31Init0xFF )
{
    // CRC-8/NRSC-5 (poly 0x31, init 0xFF, no reflection, no xorout) check value for "123456789".
    std::vector<char> v{ '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    EXPECT_EQ( CPktMgr::CrcGen( v ), 0xF7 );
}

TEST( PktMgrCrc, DetectsEverySingleBitFlip )
{
    const std::vector<char> base{ 'M', 'T', 1, 0, 7, 3, 10, 20 };
    const uint8_t           ref = CPktMgr::CrcGen( const_cast<std::vector<char>&>( base ) );
    for( size_t byte = 0; byte < base.size(); ++byte )
        for( int bit = 0; bit < 8; ++bit )
        {
            auto flipped = base;
            flipped[byte] = static_cast<char>( flipped[byte] ^ ( 1 << bit ) );
            EXPECT_NE( CPktMgr::CrcGen( flipped ), ref ) << "byte " << byte << " bit " << bit;
        }
}

TEST( PktMgrEncode, LayoutIsMagicTypeDirCmdSizeDataCrc )
{
    PktHarness h;
    auto       wire = h.encode( kLed, kResp, 9, { 0x11, 0x22 } );

    ASSERT_EQ( wire.size(), 9u );
    EXPECT_EQ( wire[0], 'M' );
    EXPECT_EQ( wire[1], 'T' );
    EXPECT_EQ( wire[2], CPktMgr::EN_PKT_TYPE_LED );
    EXPECT_EQ( wire[3], CPktMgr::EN_PKT_RESPONSE );
    EXPECT_EQ( wire[4], 9 );
    EXPECT_EQ( wire[5], 3 ) << "size byte counts payload plus CRC";
    EXPECT_EQ( wire[6], 0x11 );
    EXPECT_EQ( wire[7], 0x22 );

    std::vector<char> body( wire.begin(), wire.end() - 1 );
    EXPECT_EQ( static_cast<uint8_t>( wire.back() ), CPktMgr::CrcGen( body ) );
}

TEST( PktMgrEncode, EmptyPayloadHasSizeOne )
{
    PktHarness h;
    auto       wire = h.encode( kButton, kReq, 1 );
    ASSERT_EQ( wire.size(), 7u );
    EXPECT_EQ( wire[5], 1 );
}

TEST( PktMgrDecode, RoundTripDeliversTypeDirCmdAndPayload )
{
    PktHarness h;
    h.feed( h.encode( kLed, kResp, 42, { 1, 2, 3, 4 } ) );

    ASSERT_TRUE( h.mgr.IsFrameRcvComplete() );
    EXPECT_EQ( h.mgr.GetRxPktType(), CPktMgr::EN_PKT_TYPE_LED );
    EXPECT_EQ( h.mgr.GetPktDir(), CPktMgr::EN_PKT_RESPONSE );
    EXPECT_EQ( h.mgr.GetRxCommand(), 42 );
    EXPECT_EQ( h.drainPayload(), ( std::vector<uint8_t>{ 1, 2, 3, 4 } ) );
}

TEST( PktMgrDecode, FrameIsNotCompleteBeforeCrcByteArrives )
{
    PktHarness h;
    auto       wire = h.encode( kButton, kReq, 5, { 9, 8 } );
    wire.pop_back();
    h.feed( wire );
    EXPECT_FALSE( h.mgr.IsFrameRcvComplete() );
}

TEST( PktMgrDecode, CorruptedCrcIsRejected )
{
    PktHarness h;
    auto       wire = h.encode( kButton, kReq, 5, { 9, 8 } );
    wire.back() ^= 0x01;
    h.feed( wire );
    EXPECT_FALSE( h.mgr.IsFrameRcvComplete() );
}

TEST( PktMgrDecode, CorruptedPayloadIsRejected )
{
    PktHarness h;
    auto       wire = h.encode( kButton, kReq, 5, { 9, 8 } );
    wire[6] ^= 0x40;
    h.feed( wire );
    EXPECT_FALSE( h.mgr.IsFrameRcvComplete() );
}

TEST( PktMgrDecode, ClearReceiverAllowsBackToBackFrames )
{
    PktHarness h;
    for( int i = 0; i < 5; ++i )
    {
        h.feed( h.encode( kButton, kReq, i, { i, i + 1 } ) );
        ASSERT_TRUE( h.mgr.IsFrameRcvComplete() ) << "frame " << i;
        EXPECT_EQ( h.mgr.GetRxCommand(), i );
        EXPECT_EQ( h.drainPayload(), ( std::vector<uint8_t>{ (uint8_t)i, (uint8_t)( i + 1 ) } ) );
        h.mgr.ClearReceiver();
        EXPECT_FALSE( h.mgr.IsFrameRcvComplete() );
    }
}

TEST( PktMgrDecode, InitResetsReceiveState )
{
    PktHarness h;
    h.feed( h.encode( kButton, kReq, 1, { 1 } ) );
    ASSERT_TRUE( h.mgr.IsFrameRcvComplete() );
    h.mgr.Init();
    EXPECT_FALSE( h.mgr.IsFrameRcvComplete() );
    EXPECT_EQ( h.mgr.GetRxCommand(), 0 );
}

TEST( PktMgrDecode, AccessorsReflectFifoPositions )
{
    PktHarness h;
    h.feed( h.encode( kButton, kReq, 3, { 5, 6, 7 } ) );
    EXPECT_EQ( h.mgr.GetRxWritePoint(), 3 );
    EXPECT_EQ( h.mgr.GetRxReadPoint(), 0 );
    h.mgr.PopUint8FromRxFifo();
    EXPECT_EQ( h.mgr.GetRxReadPoint(), 1 );
}

TEST( PktMgrDecode, SetFrameRcvCompleteOverridesFlag )
{
    PktHarness h;
    h.mgr.SetFrameRcvComplete( true );
    EXPECT_TRUE( h.mgr.IsFrameRcvComplete() );
    h.mgr.SetFrameRcvComplete( false );
    EXPECT_FALSE( h.mgr.IsFrameRcvComplete() );
}

TEST( PktMgrDecode, AllPacketTypesRoundTrip )
{
    const CPktMgr::enumPacketType types[] = {
        CPktMgr::EN_PKT_TYPE_SYSTEM, CPktMgr::EN_PKT_TYPE_BUTTON, CPktMgr::EN_PKT_TYPE_LED,   CPktMgr::EN_PKT_TYPE_PAD,
        CPktMgr::EN_PKT_TYPE_KNOBS,  CPktMgr::EN_PKT_TYPE_MIDI,   CPktMgr::EN_PKT_TYPE_CV,    CPktMgr::EN_PKT_TYPE_SLIDER };
    for( auto t : types )
    {
        PktHarness h;
        h.feed( h.encode( t, kReq, 1, { 0x7F } ) );
        ASSERT_TRUE( h.mgr.IsFrameRcvComplete() ) << "type " << t;
        EXPECT_EQ( h.mgr.GetRxPktType(), t );
    }
}

TEST( PktMgrDecode, PayloadBytesAboveSignedRangeSurvive )
{
    PktHarness h;
    h.feed( h.encode( kLed, kReq, 1, { 0x80, 0xFF, 0xA5 } ) );
    ASSERT_TRUE( h.mgr.IsFrameRcvComplete() );
    EXPECT_EQ( h.drainPayload(), ( std::vector<uint8_t>{ 0x80, 0xFF, 0xA5 } ) );
}

TEST( PktMgrHwMsg, ResetZeroesMessage )
{
    PktHarness          h;
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::EN_PKT_TYPE_CV;
    msg.pktDir  = CPktMgr::EN_PKT_RESPONSE;
    msg.pktCmd  = 77;
    msg.data    = { 1, 2, 3 };
    h.mgr.Reset( msg );
    EXPECT_EQ( msg.pktType, CPktMgr::EN_PKT_TYPE_SYSTEM );
    EXPECT_EQ( msg.pktDir, CPktMgr::EN_PKT_REQUEST );
    EXPECT_EQ( msg.pktCmd, 0 );
    EXPECT_TRUE( msg.data.empty() );
}

// ---------------------------------------------------------------------------
// Recovery: the receiver must resynchronize after noise, bad frames and
// oversize frames, and must never overwrite a frame the consumer has not read.
// ---------------------------------------------------------------------------

TEST( PktMgrRecovery, EmptyPayloadFrameCompletes )
{
    PktHarness h;
    h.feed( h.encode( kButton, kReq, 1 ) );
    EXPECT_TRUE( h.mgr.IsFrameRcvComplete() );
    EXPECT_EQ( h.mgr.GetRxCommand(), 1 );
    EXPECT_TRUE( h.drainPayload().empty() );
}

TEST( PktMgrRecovery, LeadingNoiseIsSkipped )
{
    PktHarness h;
    h.feed( { 0x00, 0x13, 0x37 } );
    h.feed( h.encode( kButton, kReq, 4, { 1, 2 } ) );
    ASSERT_TRUE( h.mgr.IsFrameRcvComplete() );
    EXPECT_EQ( h.drainPayload(), ( std::vector<uint8_t>{ 1, 2 } ) );
}

TEST( PktMgrRecovery, StrayStartByteBeforeRealFrameIsSkipped )
{
    PktHarness h;
    h.feed( { 'M', 'M' } ); // false start, then the real 'M' of the frame
    auto frame = h.encode( kButton, kReq, 4, { 7 } );
    h.feed( std::vector<char>( frame.begin() + 1, frame.end() ) );
    EXPECT_TRUE( h.mgr.IsFrameRcvComplete() );
}

TEST( PktMgrRecovery, RejectedFrameDoesNotWedgeReceiver )
{
    PktHarness h;
    auto       bad = h.encode( kButton, kReq, 5, { 1, 2 } );
    bad.back() ^= 0xFF;
    h.feed( bad );
    EXPECT_FALSE( h.mgr.IsFrameRcvComplete() );

    h.feed( h.encode( kButton, kReq, 6, { 3, 4 } ) );
    ASSERT_TRUE( h.mgr.IsFrameRcvComplete() );
    EXPECT_EQ( h.mgr.GetRxCommand(), 6 );
    EXPECT_EQ( h.drainPayload(), ( std::vector<uint8_t>{ 3, 4 } ) );
}

TEST( PktMgrRecovery, OversizedLengthIsRejectedWithoutOverrunningTheFifo )
{
    PktHarness h;
    auto       frame = h.encode( kButton, kReq, 5, { 1, 2 } );
    frame[5]         = 200; // claims far more payload than the 30 byte RX FIFO holds
    h.feed( frame );
    EXPECT_FALSE( h.mgr.IsFrameRcvComplete() );
    h.feed( h.encode( kButton, kReq, 6, { 3 } ) );
    EXPECT_TRUE( h.mgr.IsFrameRcvComplete() );
}

TEST( PktMgrRecovery, PayloadFillingTheRxFifoIsAccepted )
{
    PktHarness h;
    h.feed( h.encode( kButton, kReq, 8, std::vector<int>( 30, 7 ) ) ); // harness RX FIFO is 30 bytes
    EXPECT_TRUE( h.mgr.IsFrameRcvComplete() );
    EXPECT_EQ( h.drainPayload().size(), 30u );
}

TEST( PktMgrRecovery, PayloadOneByteOverTheRxFifoIsRejectedAndResyncs )
{
    PktHarness h;
    h.feed( h.encode( kButton, kReq, 8, std::vector<int>( 31, 7 ) ) );
    EXPECT_FALSE( h.mgr.IsFrameRcvComplete() );
    h.mgr.ClearReceiver();
    h.feed( h.encode( kButton, kReq, 9, { 1 } ) );
    EXPECT_TRUE( h.mgr.IsFrameRcvComplete() );
    EXPECT_EQ( h.mgr.GetRxCommand(), 9 );
}

TEST( PktMgrRecovery, UnconsumedFrameIsNotOverwrittenByTheNextOne )
{
    PktHarness h;
    h.feed( h.encode( kButton, kReq, 1, { 11 } ) );
    ASSERT_TRUE( h.mgr.IsFrameRcvComplete() );

    h.feed( h.encode( kButton, kReq, 2, { 22 } ) ); // arrives before the consumer reads frame 1

    EXPECT_EQ( h.mgr.GetRxCommand(), 1 );
    EXPECT_EQ( h.drainPayload(), std::vector<uint8_t>{ 11 } );
}

TEST( PktMgrRecovery, NextFrameIsReceivedOnceTheConsumerClearsTheReceiver )
{
    PktHarness h;
    h.feed( h.encode( kButton, kReq, 1, { 11 } ) );
    h.mgr.ClearReceiver();
    h.feed( h.encode( kButton, kReq, 2, { 22 } ) );
    ASSERT_TRUE( h.mgr.IsFrameRcvComplete() );
    EXPECT_EQ( h.mgr.GetRxCommand(), 2 );
    EXPECT_EQ( h.drainPayload(), std::vector<uint8_t>{ 22 } );
}

TEST( PktMgrTx, FifoKeepsOrder )
{
    PktHarness h;
    h.mgr.PushUint8ToTxFifo( 0xAA );
    h.mgr.PushUint8ToTxFifo( 0xBB );
    EXPECT_EQ( h.mgr.PopUint8FromTxFifo(), 0xAA );
    EXPECT_EQ( h.mgr.PopUint8FromTxFifo(), 0xBB );
}
