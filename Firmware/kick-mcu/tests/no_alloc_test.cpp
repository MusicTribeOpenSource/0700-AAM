// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The receive path must not allocate. Two checks: the types involved are plain storage, and a
// run through the whole path with a counting operator new sees nothing.
#include <gtest/gtest.h>

#include <type_traits>

#include "alloc_hook.h"
#include "kick_mcu/app/firmware.h"
#include "kick_mcu/link/link.h"
#include "sim_board.h"
#include "sim_clock.h"
#include "sim_uart.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace kick_mcu::link;
using namespace testing_support;

// Fixed-buffer types: trivially destructible (nothing owns heap memory) and with no virtual
// members. A std::vector or std::function member would break the first assertion.
static_assert( std::is_trivially_destructible<RxParser>::value, "RxParser must not own heap memory" );
static_assert( std::is_trivially_destructible<Frame>::value, "Frame must not own heap memory" );
static_assert( std::is_trivially_destructible<TxQueue>::value, "TxQueue must not own heap memory" );
static_assert( std::is_trivially_destructible<Session>::value, "Session must not own heap memory" );
static_assert( std::is_trivially_destructible<SpscByteRing<256>>::value, "the receive ring must not own heap memory" );
static_assert( !std::is_polymorphic<RxParser>::value, "RxParser has no virtual functions" );
static_assert( sizeof( RxParser ) <= contract::kRxFrameBufferBytes + 64, "RxParser holds one frame buffer and a few counters" );

TEST( NoAlloc, ParsingFramesDoesNotAllocate )
{
    // Build the traffic first: the vectors here are the test's, not the firmware's.
    Bytes stream;
    for( int i = 0; i < 500; ++i )
    {
        const Bytes f = fromHost( contract::PKT_PAD, contract::PAD_SET_ALL_MULTI_COLOUR, Bytes( 1 + i % 33, (uint8_t)i ) );
        stream.insert( stream.end(), f.begin(), f.end() );
        stream.push_back( 0xEE ); // garbage
        stream.push_back( 'M' );  // a false start
    }

    RxParser parser;
    struct Count
    {
        size_t n{ 0 };
        static void on( void* c, const Frame& ) { ++static_cast<Count*>( c )->n; }
    } count;
    parser.setHandler( &Count::on, &count );

    alloc_hook::arm();
    for( uint8_t b : stream )
        parser.feed( b );
    parser.expire();
    const size_t allocations = alloc_hook::disarm();

    EXPECT_EQ( 0u, allocations );
    EXPECT_EQ( 500u, count.n );
}

namespace
{
// A UART that keeps no log, so the test counts only allocations made by firmware code.
class NullUart : public bsp::IUart
{
public:
    size_t write( bsp::ByteSpan bytes ) override
    {
        written += bytes.size;
        return bytes.size;
    }
    size_t writableBytes() const override { return 1024; }
    void   onByte( bsp::ByteHandler handler, void* context ) override
    {
        handler_ = handler;
        context_ = context;
    }
    void inject( const Bytes& b )
    {
        for( uint8_t x : b )
            handler_( context_, x );
    }

    size_t           written{ 0 };
    bsp::ByteHandler handler_{ nullptr };
    void*            context_{ nullptr };
};
} // namespace

TEST( NoAlloc, TheWholeLinkPathDoesNotAllocate )
{
    NullUart      uart;
    sim::SimClock clock;
    Link          link( uart, clock );

    const Bytes init1 = fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } );
    const Bytes init2 = fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } );
    const Bytes echo  = fromHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } );
    const Bytes led   = fromHost( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 4, 1 } );

    alloc_hook::arm();
    uart.inject( init1 );
    link.poll();
    uart.inject( init2 );
    link.poll();
    uart.inject( echo );
    link.poll();
    for( int i = 0; i < 2000; ++i )
    {
        uart.inject( led );
        const uint8_t p[2] = { (uint8_t)( i % 53 ), (uint8_t)( i & 1 ) };
        link.send( Lane::Button, contract::PKT_BUTTON, contract::BTN_STATUS, p, 2 );
        clock.advanceMs( 3 );
        link.poll();
    }
    const size_t allocations = alloc_hook::disarm();

    EXPECT_EQ( 0u, allocations );
    EXPECT_TRUE( link.session().streaming() );
    EXPECT_GT( uart.written, 2000u * 8 ) << "the loop really did transmit";
}

// ---- the input path ---------------------------------------------------------------------

static_assert( std::is_trivially_destructible<input::Debouncer>::value, "Debouncer must not own heap memory" );
static_assert( std::is_trivially_destructible<input::PadScanner>::value, "PadScanner must not own heap memory" );
static_assert( std::is_trivially_destructible<input::EncoderBank>::value, "EncoderBank must not own heap memory" );
static_assert( std::is_trivially_destructible<input::StripScanner>::value, "StripScanner must not own heap memory" );
static_assert( std::is_trivially_destructible<input::VelocityCurve>::value, "VelocityCurve must not own heap memory" );

namespace
{
// A UART that takes everything and keeps nothing, so the test's own byte log cannot allocate.
class DiscardUart : public bsp::IUart
{
public:
    size_t write( bsp::ByteSpan bytes ) override { return bytes.size; }
    size_t writableBytes() const override { return 256; }
    void   onByte( bsp::ByteHandler handler, void* context ) override { handler_ = handler; context_ = context; }
    void   inject( const Bytes& bytes ) { for( uint8_t b : bytes ) handler_( context_, b ); }

private:
    bsp::ByteHandler handler_{ nullptr };
    void*            context_{ nullptr };
};
} // namespace

TEST( NoAlloc, ScanningInputsAndSendingThemDoesNotAllocate )
{
    sim::SimClock clock;
    DiscardUart   uart;
    sim::SimBoard board;
    app::FirmwareConfig cfg;
    cfg.scan.pads.aftertouchEnabled = true;
    app::Firmware fw( uart, clock, app::InputHardware{ board.matrix, board.pads, board.encoders, board.strip }, cfg );

    uart.inject( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } ) );
    uart.inject( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } ) );
    uart.inject( fromHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } ) );
    fw.step();
    ASSERT_TRUE( fw.link().session().streaming() );

    Rng rng( 5 );
    alloc_hook::arm();
    for( int t = 0; t < 8000; ++t )
    {
        if( t % 40 == 0 )
            board.matrix.press( rng.below( contract::kBtnCodeCount ) );
        if( t % 40 == 20 )
            for( unsigned k = 0; k < contract::kBtnCodeCount; ++k )
                board.matrix.release( k );
        for( unsigned p = 0; p < 16; ++p )
            board.pads.set( p, (uint16_t)( ( t / 50 ) % 2 ? rng.below( 4000 ) : 0 ) );
        board.encoders.step( rng.below( contract::kEncoderCount ), rng.below( 2 ) != 0 );
        if( t % 100 < 50 )
            board.strip.touch( (uint16_t)rng.below( 1024 ) );
        else
            board.strip.lift();
        fw.step();
        clock.advanceUs( 250 );
    }
    const size_t allocations = alloc_hook::disarm();

    EXPECT_EQ( 0u, allocations );
    EXPECT_GT( fw.link().tx().stats().queued[(size_t)link::Lane::Button], 10u ) << "the run exercised the path";
    EXPECT_GT( fw.link().tx().stats().queued[(size_t)link::Lane::PadKnob], 10u );
}

// ---- the output path --------------------------------------------------------------------

static_assert( std::is_trivially_destructible<output::LedFrame>::value, "LedFrame must not own heap memory" );
static_assert( std::is_trivially_destructible<output::ColourTable>::value, "ColourTable must not own heap memory" );
static_assert( std::is_trivially_destructible<output::GammaTable>::value, "GammaTable must not own heap memory" );
static_assert( std::is_trivially_destructible<output::RenderedFrame>::value, "RenderedFrame must not own heap memory" );

namespace
{
class CountingLeds : public bsp::ILedDriver
{
public:
    void setButton( unsigned, uint8_t ) override {}
    void setPad( unsigned, uint8_t, uint8_t, uint8_t ) override {}
    void setStrip( unsigned, uint8_t ) override {}
    void commit() override { ++commits; }
    size_t commits{ 0 };
};
} // namespace

TEST( NoAlloc, DecodingLedCommandsAndRefreshingDoesNotAllocate )
{
    sim::SimClock clock;
    DiscardUart   uart;
    sim::SimBoard board;
    CountingLeds  leds;
    app::Firmware fw( uart, clock, app::InputHardware{ board.matrix, board.pads, board.encoders, board.strip }, app::OutputHardware{ leds } );

    uart.inject( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 1 } ) );
    uart.inject( fromHost( contract::PKT_SYSTEM, contract::SYS_COMM_INIT, { 2 } ) );
    uart.inject( fromHost( contract::PKT_SYSTEM, contract::SYS_CHECK_COMM_STATUS, { 2 } ) );
    fw.step();

    Bytes multi{ 1 };
    for( uint8_t i = 0; i < 16; ++i )
        multi.push_back( i );
    Bytes stepAll{ 1 };
    for( uint8_t i = 0; i < 32; ++i )
        stepAll.push_back( i );
    const Bytes frames[] = {
        fromHost( contract::PKT_PAD, contract::PAD_SET_ALL_MULTI_COLOUR, multi ),
        fromHost( contract::PKT_PAD, contract::PAD_SET_STEP_ALL_PAD, stepAll ),
        fromHost( contract::PKT_PAD, contract::PAD_SET_BLINKING, { 3, 4, 12, 7 } ),
        fromHost( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 2, 1, 0 } ),
        fromHost( contract::PKT_LED, contract::LED_SET_ALL, { 1 } ),
        fromHost( contract::PKT_SLIDER, contract::SLIDER_BAR_LED, { 60 } ),
        fromHost( contract::PKT_LED, contract::LED_SET_SINGLE_LED, { 1 } ), // malformed
    };

    alloc_hook::arm();
    for( int i = 0; i < 4000; ++i )
    {
        uart.inject( frames[i % 7] );
        fw.step();
        clock.advanceMs( 3 );
    }
    const size_t allocations = alloc_hook::disarm();

    EXPECT_EQ( 0u, allocations );
    EXPECT_GT( leds.commits, 10u ) << "the run really drew";
    EXPECT_GT( fw.commands().stats().malformed, 100u );
}
