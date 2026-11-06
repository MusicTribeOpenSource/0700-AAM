// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The system layer inside the whole firmware: version, tempo, shutdown, the boot-time self check,
// trial confirmation, and an update carried over the wire by the real link.
#include <gtest/gtest.h>

#include <memory>

#include "kick_mcu/app/firmware.h"
#include "kick_mcu/system/bpm.h"
#include "sim_board.h"
#include "sim_clock.h"
#include "sim_flash.h"
#include "sim_leds.h"
#include "sim_uart.h"
#include "system_support.h"
#include "test_support.h"

using namespace kick_mcu;
using namespace testing_support;
using system_support::makeFlash;
using system_support::makeImage;
using system_support::provision;
using system_support::testLayout;
using system_support::writeSlot;
namespace c = kick_mcu::contract;
namespace sys = kick_mcu::system;

namespace
{
struct SysRig : ::testing::Test
{
    sys::FlashLayout               layout{ testLayout() };
    std::unique_ptr<sim::SimFlash> flash{ makeFlash( layout ) };
    sim::SimPower                  power;
    sim::SimClock                  clock;
    sim::SimUart                   uart{ 4096 };
    sim::SimBoard                  board;
    sim::SimLedDriver              leds;
    std::unique_ptr<app::Firmware> fw;
    sys::BootDecision              boot;
    Bytes                          v1{ makeImage( { 1, 4, 2 }, 100, 1, 0x0102 ) };

    /// Boots whatever is in flash, the way a reset does: bootloader decision, then a new Firmware.
    void bootFirmware( bool withFlash = true, bool bpmBlink = false )
    {
        fw.reset();
        uart.clearWire();
        app::FirmwareConfig cfg;
        if( withFlash )
        {
            boot             = sys::selectBoot( *flash, layout );
            cfg.system.flash  = flash.get();
            cfg.system.layout = layout;
            cfg.system.boot   = boot;
        }
        cfg.system.power    = &power;
        cfg.blinkFollowsBpm = bpmBlink;
        fw = std::make_unique<app::Firmware>( uart, clock, app::InputHardware{ board.matrix, board.pads, board.encoders, board.strip }, app::OutputHardware{ leds }, cfg );
    }

    void host( uint8_t type, uint8_t cmd, const Bytes& payload ) { uart.injectRx( fromHost( type, cmd, payload ) ); }

    void step( int n )
    {
        for( int i = 0; i < n; ++i )
        {
            fw->step();
            clock.advanceUs( 250 );
            uart.drain();
        }
    }

    void handshake()
    {
        host( c::PKT_SYSTEM, c::SYS_COMM_INIT, { 1 } );
        step( 4 );
        host( c::PKT_SYSTEM, c::SYS_COMM_INIT, { 2 } );
        host( c::PKT_SYSTEM, c::SYS_CHECK_COMM_STATUS, { 2 } );
        step( 4 );
    }

    std::vector<Decoded> sent()
    {
        link::RxParser parser;
        Collector      got;
        got.attach( parser );
        feedAll( parser, uart.wire() );
        return got.frames;
    }

    std::vector<Decoded> sentOf( uint8_t type, uint8_t cmd )
    {
        std::vector<Decoded> out;
        for( auto& f : sent() )
            if( f.type == type && f.cmd == cmd )
                out.push_back( f );
        return out;
    }
};
} // namespace

TEST_F( SysRig, WithNoFlashTheHandshakeVersionIsTheBuildVersion )
{
    bootFirmware( false );
    host( c::PKT_SYSTEM, c::SYS_COMM_INIT, { 1 } );
    step( 4 );
    const auto v = sentOf( c::PKT_SYSTEM, c::SYS_GET_SW_VER );
    ASSERT_EQ( 1u, v.size() );
    EXPECT_EQ( ( Bytes{ 1, 0 } ), v[0].payload );
    EXPECT_EQ( sys::ImageStatus::NoFlash, fw->system().selfCheck().status );
}

TEST_F( SysRig, TheHandshakeReportsTheRunningImagesVersionAsTwoBytes )
{
    provision( *flash, layout, v1 );
    bootFirmware();
    host( c::PKT_SYSTEM, c::SYS_COMM_INIT, { 1 } );
    step( 4 );
    const auto v = sentOf( c::PKT_SYSTEM, c::SYS_GET_SW_VER );
    ASSERT_EQ( 1u, v.size() );
    EXPECT_EQ( ( Bytes{ 1, 4 } ), v[0].payload ) << "major, minor; bug does not travel in the handshake";
}

TEST_F( SysRig, SelfCheckPassesForAGoodImageAndReportsFlashValidAndTheCrc )
{
    provision( *flash, layout, v1 );
    bootFirmware();
    const auto& r = fw->system().selfCheck();
    EXPECT_TRUE( r.ok() );
    EXPECT_EQ( sys::kFlashValid, r.flashState );
    EXPECT_EQ( r.storedCrc32, r.computedCrc32 );
    EXPECT_EQ( ( sys::VersionNo{ 1, 4, 2 } ), r.version );
    EXPECT_EQ( 0x0102, r.swRevision );
}

TEST_F( SysRig, AGetSwVerRequestReturnsTheExtendedReportWithoutTheUpdatePath )
{
    provision( *flash, layout, v1 );
    bootFirmware();
    host( c::PKT_SYSTEM, c::SYS_GET_SW_VER, {} );
    step( 4 );
    const auto v = sentOf( c::PKT_SYSTEM, c::SYS_GET_SW_VER );
    ASSERT_EQ( 1u, v.size() );
    ASSERT_EQ( sys::kVersionReportBytes, v[0].payload.size() );
    const Bytes& p = v[0].payload;
    EXPECT_EQ( 1, p[0] );
    EXPECT_EQ( 4, p[1] );
    EXPECT_EQ( 2, p[2] );
    EXPECT_EQ( 0, p[3] ) << "status ok";
    EXPECT_EQ( ( Bytes{ 0x12, 0x34, 0x56, 0x78 } ), Bytes( p.begin() + 4, p.begin() + 8 ) ) << "FLASH_VALID";
    const uint32_t crc = ( (uint32_t)p[8] << 24 ) | ( p[9] << 16 ) | ( p[10] << 8 ) | p[11];
    EXPECT_EQ( fw->system().selfCheck().computedCrc32, crc );
    EXPECT_EQ( 0x01, p[12] );
    EXPECT_EQ( 0x02, p[13] );
    EXPECT_EQ( 0, p[14] );
}

TEST_F( SysRig, ADamagedRunningImageIsReportedByTheSelfCheck )
{
    provision( *flash, layout, v1 );
    bootFirmware();                                              // sees a good image
    flash->contents()[layout.baseOf( sys::Slot::A ) + 90] ^= 1;  // bit rot after boot
    // Re-run the check the way boot would, with the same decision.
    const auto r = sys::runSelfCheck( flash.get(), layout, boot, sys::kBuildVersion );
    EXPECT_EQ( sys::ImageStatus::BadPayloadCrc, r.status );
    EXPECT_NE( r.storedCrc32, r.computedCrc32 );
    EXPECT_EQ( sys::kBuildVersion, r.version ) << "an image that fails its check does not get to name its version";
}

TEST_F( SysRig, BpmIsStoredBigEndianAndTheUnsetValueMeansNoTempo )
{
    bootFirmware( false );
    EXPECT_FALSE( fw->system().bpm().valid() );
    host( c::PKT_SYSTEM, c::SYS_BPM, { 0x00, 120 } );
    step( 2 );
    EXPECT_TRUE( fw->system().bpm().valid() );
    EXPECT_EQ( 120, fw->system().bpm().raw() );
    EXPECT_EQ( 500u, fw->system().bpm().beatPeriodMs() );
    host( c::PKT_SYSTEM, c::SYS_BPM, { 0x01, 0x2C } ); // 300
    step( 2 );
    EXPECT_EQ( 300, fw->system().bpm().raw() );
    EXPECT_EQ( 200u, fw->system().bpm().beatPeriodMs() );
    host( c::PKT_SYSTEM, c::SYS_BPM, { 0xFF, 0xFF } ); // -1
    step( 2 );
    EXPECT_FALSE( fw->system().bpm().valid() );
    EXPECT_EQ( 0u, fw->system().bpm().beatPeriodMs() );
    host( c::PKT_SYSTEM, c::SYS_BPM, { 0x42 } ); // truncated
    step( 2 );
    EXPECT_EQ( 1u, fw->system().bpm().malformed() );
    EXPECT_EQ( 0u, fw->hostFramesIgnored() ) << "handled, even when malformed";
}

TEST_F( SysRig, ABpmPeriodIsClampedSoAWildValueCannotSpinTheBlinkClock )
{
    sys::Bpm bpm;
    const uint8_t huge[2] = { 0x7F, 0xFF };
    bpm.onPayload( huge, 2 );
    EXPECT_EQ( 60u, bpm.beatPeriodMs() );
    const uint8_t slow[2] = { 0, 1 };
    bpm.onPayload( slow, 2 );
    EXPECT_EQ( 4000u, bpm.beatPeriodMs() );
}

TEST_F( SysRig, BlinkFollowsTheTempoOnlyWhenAskedTo )
{
    bootFirmware( false, false );
    host( c::PKT_SYSTEM, c::SYS_BPM, { 0, 120 } );
    step( 4 );
    EXPECT_EQ( 500u, fw->blink().periodMs() ) << "default is 2 Hz; 120 bpm also happens to be 500";
    host( c::PKT_SYSTEM, c::SYS_BPM, { 0, 60 } );
    step( 4 );
    EXPECT_EQ( 500u, fw->blink().periodMs() );

    bootFirmware( false, true );
    host( c::PKT_SYSTEM, c::SYS_BPM, { 0, 60 } );
    step( 4 );
    EXPECT_EQ( 1000u, fw->blink().periodMs() );
    host( c::PKT_SYSTEM, c::SYS_BPM, { 0, 150 } );
    step( 4 );
    EXPECT_EQ( 400u, fw->blink().periodMs() );
}

TEST_F( SysRig, ShutdownAcknowledgesGoesDarkStaysDarkAndStopsReportingInput )
{
    bootFirmware( false );
    handshake();
    host( c::PKT_LED, c::LED_SET_SINGLE_LED, { 13, 1, 0 } );
    step( 80 );
    ASSERT_EQ( 255, leds.visible().buttons[13] );
    uart.clearWire();

    host( c::PKT_SYSTEM, c::SYS_SHUTDOWN, {} );
    step( 4 );
    EXPECT_EQ( 0, leds.visible().buttons[13] ) << "LEDs are quiesced at once";
    const auto ack = sentOf( c::PKT_SYSTEM, c::SYS_SHUTDOWN );
    ASSERT_EQ( 1u, ack.size() );
    EXPECT_EQ( ( Bytes{ 1 } ), ack[0].payload );

    EXPECT_EQ( 0u, power.offRequests() ) << "not before the ack has had time to leave";
    step( 400 ); // 100 ms
    EXPECT_EQ( 1u, power.offRequests() );
    step( 400 );
    EXPECT_EQ( 1u, power.offRequests() ) << "asked once";

    host( c::PKT_LED, c::LED_SET_SINGLE_LED, { 13, 1, 0 } );
    step( 80 );
    EXPECT_EQ( 0, leds.visible().buttons[13] ) << "it stays dark";

    uart.clearWire();
    board.matrix.press( 3 );
    step( 80 );
    EXPECT_TRUE( sentOf( c::PKT_BUTTON, c::BTN_STATUS ).empty() ) << "no more input events";
}

TEST_F( SysRig, ATrialImageConfirmsItselfOnlyAfterTheHandshake )
{
    provision( *flash, layout, v1 );
    const Bytes v2 = makeImage( { 1, 5, 0 }, 80, 2 );
    ASSERT_TRUE( writeSlot( *flash, layout, sys::Slot::B, v2 ) );
    ASSERT_TRUE( sys::BootControl( *flash, layout ).commitTrial( sys::Slot::A, sys::Slot::B ) );

    bootFirmware();
    ASSERT_TRUE( boot.trial );
    step( 100 );
    EXPECT_FALSE( fw->system().confirmed() ) << "no host, no evidence";
    sys::BootRecord r;
    ASSERT_TRUE( sys::BootControl( *flash, layout ).load( r ) );
    EXPECT_EQ( sys::BootState::TrialBooted, r.state );

    handshake();
    EXPECT_TRUE( fw->system().confirmed() );
    ASSERT_TRUE( sys::BootControl( *flash, layout ).load( r ) );
    EXPECT_EQ( sys::BootState::Confirmed, r.state );
    EXPECT_EQ( sys::Slot::B, r.good );

    bootFirmware(); // another reset
    EXPECT_FALSE( boot.trial );
    EXPECT_EQ( sys::Slot::B, boot.slot );
}

TEST_F( SysRig, ATrialImageThatNeverShakesHandsIsReplacedByTheOldOneOnTheNextReset )
{
    provision( *flash, layout, v1 );
    ASSERT_TRUE( writeSlot( *flash, layout, sys::Slot::B, makeImage( { 1, 5, 0 }, 80, 2 ) ) );
    ASSERT_TRUE( sys::BootControl( *flash, layout ).commitTrial( sys::Slot::A, sys::Slot::B ) );
    bootFirmware();
    step( 200 );
    bootFirmware(); // reset with no confirm
    EXPECT_EQ( sys::Slot::A, boot.slot );
    EXPECT_TRUE( boot.reverted );
    EXPECT_EQ( ( sys::VersionNo{ 1, 4, 2 } ), fw->system().selfCheck().version );
}

TEST_F( SysRig, AFailedConfirmIsRetriedLaterNotEveryPass )
{
    provision( *flash, layout, v1 );
    ASSERT_TRUE( writeSlot( *flash, layout, sys::Slot::B, makeImage( { 1, 5, 0 }, 80, 2 ) ) );
    ASSERT_TRUE( sys::BootControl( *flash, layout ).commitTrial( sys::Slot::A, sys::Slot::B ) );
    bootFirmware();
    flash->cutPowerAfter( 0 ); // every flash operation fails
    handshake();
    EXPECT_FALSE( fw->system().confirmed() );
    EXPECT_EQ( 1u, fw->system().confirmFailures() ) << "one attempt, not one per pass";
    flash->restorePower();
    step( 4400 ); // past the 1 s retry interval
    EXPECT_TRUE( fw->system().confirmed() );
}

TEST_F( SysRig, UpdateOverTheWireInstallsAndTheNextBootRunsTheNewImageAfterConfirming )
{
    provision( *flash, layout, v1 );
    bootFirmware();
    handshake();
    uart.clearWire();

    const Bytes v2 = makeImage( { 2, 0, 1 }, 140, 3 );

    auto exchange = [&]( const Bytes& payload ) -> Bytes {
        uart.clearWire();
        host( c::PKT_SYSTEM, c::SYS_SW_UPDATE, payload );
        for( int i = 0; i < 600; ++i ) // up to 150 ms
        {
            step( 1 );
            auto replies = sentOf( c::PKT_SYSTEM, c::SYS_SW_UPDATE );
            if( !replies.empty() )
                return replies.back().payload;
        }
        return {};
    };

    ASSERT_EQ( ( Bytes{ c::SW_UPDATE_BEGIN_TRANSFER } ), exchange( { c::SW_UPDATE_INITIATE } ) );
    ASSERT_EQ( ( Bytes{ c::SW_UPDATE_ACK } ), exchange( { c::SW_UPDATE_DATABASE, 0, 0 } ) );
    for( size_t off = 0; off < v2.size(); off += 16 )
    {
        const size_t n = std::min<size_t>( 16, v2.size() - off );
        Bytes        p{ c::SW_UPDATE_DATA, (uint8_t)( off >> 8 ), (uint8_t)off };
        p.insert( p.end(), v2.begin() + (std::ptrdiff_t)off, v2.begin() + (std::ptrdiff_t)( off + n ) );
        p.push_back( 0x99 );
        ASSERT_EQ( ( Bytes{ c::SW_UPDATE_ACK } ), exchange( p ) ) << "record at " << off;
    }
    ASSERT_EQ( ( Bytes{ c::SW_UPDATE_ACK } ), exchange( { c::SW_UPDATE_END } ) );

    EXPECT_EQ( 0u, power.resetRequests() );
    step( 2200 ); // 550 ms
    EXPECT_EQ( 1u, power.resetRequests() );

    bootFirmware(); // the reset
    EXPECT_TRUE( boot.trial );
    EXPECT_EQ( sys::Slot::B, boot.slot );
    host( c::PKT_SYSTEM, c::SYS_COMM_INIT, { 1 } );
    step( 4 );
    const auto v = sentOf( c::PKT_SYSTEM, c::SYS_GET_SW_VER );
    ASSERT_EQ( 1u, v.size() );
    EXPECT_EQ( ( Bytes{ 2, 0 } ), v[0].payload ) << "the host sees the new version";
    host( c::PKT_SYSTEM, c::SYS_COMM_INIT, { 2 } );
    host( c::PKT_SYSTEM, c::SYS_CHECK_COMM_STATUS, { 2 } );
    step( 4 );
    EXPECT_TRUE( fw->system().confirmed() );
    bootFirmware();
    EXPECT_EQ( sys::Slot::B, boot.slot );
    EXPECT_FALSE( boot.trial );
}

TEST_F( SysRig, UpdateWithNoFlashAnswersFailureInsteadOfHanging )
{
    bootFirmware( false );
    host( c::PKT_SYSTEM, c::SYS_SW_UPDATE, { c::SW_UPDATE_INITIATE } );
    step( 4 );
    const auto r = sentOf( c::PKT_SYSTEM, c::SYS_SW_UPDATE );
    ASSERT_EQ( 1u, r.size() );
    EXPECT_EQ( ( Bytes{ c::SW_UPDATE_FAILURE } ), r[0].payload );
}
