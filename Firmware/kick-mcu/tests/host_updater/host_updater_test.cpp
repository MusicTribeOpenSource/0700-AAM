// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The host's real MCUUpdate state machine (kick_FirmwareUpdate.cpp, unmodified) against the real
// firmware update path, over frames the host's own PktMgr encodes and decodes.
//
// The host updater is built from static state it never resets: boDataTransferPending goes false
// when a run ends and nothing sets it true again, and MCUUpdate keeps its parse position in
// function statics. So one process can run one update, and ctest runs each scenario below in its
// own process (see CMakeLists.txt).
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

#include <unistd.h>

#include "host_shim.h"
#include "kick_mcu/app/firmware.h"
#include "kick_mcu/system/boot_control.h"
#include "kick_mcu/system/image.h"
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
namespace c   = kick_mcu::contract;
namespace sys = kick_mcu::system;
using host_updater::HostShim;

namespace
{
constexpr int kHostSuccess = 1;  // MCUUpdate's return values
constexpr int kHostFailure = -1;
constexpr int kStatusSuccess = 3;  // mt::RECOVERY_STATUS::SUCCESS
constexpr int kStatusFailure = 4;  // mt::RECOVERY_STATUS::FAILURE

/// The firmware on a simulated board, with a host updater wired to its UART.
class HostRig : public ::testing::Test
{
protected:
    sys::FlashLayout               layout{ testLayout() };
    std::unique_ptr<sim::SimFlash> flash{ makeFlash( layout ) };
    sim::SimPower                  power;
    sim::SimClock                  clock;
    sim::SimUart                   uart{ 4096 };
    sim::SimBoard                  board;
    sim::SimLedDriver              leds;
    std::unique_ptr<app::Firmware> fw;
    sys::BootDecision              boot;
    Bytes                          v1{ makeImage( { 1, 0, 0 }, 100, 1, 3 ) };
    HostShim                       shim;

    std::mutex          queueMutex;
    std::vector<Bytes>  toMcu;       ///< frames the host worker thread produced
    size_t              wireRead{ 0 };
    std::function<bool( const Decoded& )> dropReply; ///< true: the MCU's reply never reaches the host
    std::vector<Decoded> mcuReplies; ///< every SW_UPDATE reply the MCU sent, dropped or not
    std::vector<std::filesystem::path> scratch;

    void SetUp() override
    {
        provision( *flash, layout, v1 );
        shim.setTransmit( [this]( const Bytes& frame ) {
            std::lock_guard<std::mutex> lock( queueMutex );
            toMcu.push_back( frame );
        } );
        bootFirmware();
        handshake();
    }

    void TearDown() override
    {
        for( auto& p : scratch )
            std::filesystem::remove_all( p );
    }

    void bootFirmware()
    {
        fw.reset();
        uart.clearWire();
        wireRead = 0;
        app::FirmwareConfig cfg;
        boot              = sys::selectBoot( *flash, layout );
        cfg.system.flash  = flash.get();
        cfg.system.layout = layout;
        cfg.system.boot   = boot;
        cfg.system.power  = &power;
        fw = std::make_unique<app::Firmware>( uart, clock, app::InputHardware{ board.matrix, board.pads, board.encoders, board.strip }, app::OutputHardware{ leds }, cfg );
    }

    void stepMcu( int n, bool advance = true )
    {
        for( int i = 0; i < n && !flash->powerLost(); ++i )
        {
            fw->step();
            if( advance )
                clock.advanceUs( 250 );
            uart.drain();
        }
    }

    void injectFromHost( uint8_t type, uint8_t cmd, const Bytes& payload ) { uart.injectRx( fromHost( type, cmd, payload ) ); }

    /// The handshake the host performs before an update; its own details are not under test here.
    void handshake()
    {
        injectFromHost( c::PKT_SYSTEM, c::SYS_COMM_INIT, { 1 } );
        stepMcu( 4 );
        injectFromHost( c::PKT_SYSTEM, c::SYS_COMM_INIT, { 2 } );
        injectFromHost( c::PKT_SYSTEM, c::SYS_CHECK_COMM_STATUS, { 2 } );
        stepMcu( 4 );
    }

    /// Hands every whole frame the MCU has put on the wire since the last call to the host.
    void deliverMcuFrames()
    {
        if( flash->powerLost() )
            return; // a board that lost power says nothing more
        const Bytes& wire = uart.wire();
        while( wireRead + 6 < wire.size() )
        {
            const size_t total = (size_t)wire[wireRead + 5] + 7;
            if( wireRead + total > wire.size() )
                return;
            const Bytes frame( wire.begin() + (std::ptrdiff_t)wireRead, wire.begin() + (std::ptrdiff_t)( wireRead + total ) );
            wireRead += total;

            link::RxParser parser;
            Collector      got;
            got.attach( parser );
            feedAll( parser, frame );
            if( got.frames.size() == 1 && got.frames[0].type == c::PKT_SYSTEM && got.frames[0].cmd == c::SYS_SW_UPDATE )
            {
                mcuReplies.push_back( got.frames[0] );
                if( dropReply && dropReply( got.frames[0] ) )
                    continue;
            }
            shim.onMcuBytes( frame );
        }
    }

    /// Runs the MCU for as long as the host needs it, until `done()` holds or the host stalls for
    /// `stallMs` of real time. Simulated time moves only while the MCU has work in hand, so a slow
    /// test machine cannot trip the MCU's idle timeout by itself.
    bool pumpUntil( const std::function<bool()>& done, int stallMs = 5000 )
    {
        int  active   = 0;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( stallMs );
        while( !done() )
        {
            std::vector<Bytes> frames;
            {
                std::lock_guard<std::mutex> lock( queueMutex );
                frames.swap( toMcu );
            }
            if( !frames.empty() )
            {
                for( auto& f : frames )
                    uart.injectRx( f );
                active   = 256;
                deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( stallMs );
            }
            else if( active == 0 )
            {
                if( std::chrono::steady_clock::now() > deadline )
                    return false;
                std::this_thread::sleep_for( std::chrono::microseconds( 50 ) );
            }
            stepMcu( 1, active > 0 );
            if( active > 0 )
                --active;
            deliverMcuFrames();
        }

        // The host may have queued its last frame (END) and returned between two checks of
        // `done`: play whatever is left to the MCU before giving control back.
        std::vector<Bytes> rest;
        {
            std::lock_guard<std::mutex> lock( queueMutex );
            rest.swap( toMcu );
        }
        for( auto& f : rest )
            uart.injectRx( f );
        for( int i = 0; i < 256 && !rest.empty(); ++i )
        {
            stepMcu( 1 );
            deliverMcuFrames();
        }
        return true;
    }

    /// Simulated time with the host idle: the MCU's timers run.
    void runSimulatedMs( unsigned ms )
    {
        for( unsigned i = 0; i < ms * 4; ++i )
        {
            stepMcu( 1 );
            deliverMcuFrames();
        }
    }

    sys::BootRecord record()
    {
        sys::BootRecord r;
        EXPECT_TRUE( sys::BootControl( *flash, layout ).load( r ) );
        return r;
    }

    sys::ImageCheck slotCheck( sys::Slot s ) { return sys::verifyImage( *flash, layout.baseOf( s ), layout.slotBytes ); }

    std::string stampedHex() const { return KICK_STAMPED_HEX; }

    /// A copy of the stamped hex with one data digit changed in the last data record and the
    /// record checksum rewritten to match. The host checks record checksums, so the file stays
    /// well formed to it, and the change breaks the payload CRC the MCU checks.
    std::string corruptedHex()
    {
        std::ifstream       in( stampedHex() );
        std::vector<std::string> lines;
        for( std::string line; std::getline( in, line ); )
            lines.push_back( line );
        for( size_t i = lines.size(); i-- > 0; )
            if( lines[i].size() > 14 && lines[i].compare( 7, 2, "00" ) == 0 )
            {
                char& digit = lines[i][10];
                digit       = digit == '0' ? '1' : '0';
                // Intel HEX checksum: two's complement of the sum of every byte before it.
                int sum = 0;
                for( size_t pos = 1; pos + 2 < lines[i].size(); pos += 2 )
                    sum += std::stoi( lines[i].substr( pos, 2 ), nullptr, 16 );
                char checksum[3];
                std::snprintf( checksum, sizeof( checksum ), "%02X", ( -sum ) & 0xFF );
                lines[i].replace( lines[i].size() - 2, 2, checksum );
                break;
            }
        const auto dir = std::filesystem::temp_directory_path() / ( "kick-host-updater-" + std::to_string( ::getpid() ) );
        std::filesystem::create_directories( dir );
        scratch.push_back( dir );
        const auto path = dir / "corrupt.hex";
        std::ofstream out( path );
        for( auto& l : lines )
            out << l << "\n";
        return path.string();
    }

    /// The version the MCU announces in its handshake after the next reset.
    Bytes versionAfterReset()
    {
        bootFirmware();
        injectFromHost( c::PKT_SYSTEM, c::SYS_COMM_INIT, { 1 } );
        stepMcu( 4 );
        link::RxParser parser;
        Collector      got;
        got.attach( parser );
        feedAll( parser, uart.wire() );
        for( auto& f : got.frames )
            if( f.type == c::PKT_SYSTEM && f.cmd == c::SYS_GET_SW_VER )
                return f.payload;
        return {};
    }
};
} // namespace

TEST_F( HostRig, HappyPath )
{
    const uint64_t opsAtStart = flash->operations();
    shim.start( stampedHex() );
    ASSERT_TRUE( pumpUntil( [&] { return shim.finished(); } ) ) << "the host never finished";
    shim.join();
    EXPECT_EQ( kHostSuccess, shim.result() );

    const auto events = shim.statusEvents();
    ASSERT_FALSE( events.empty() );
    EXPECT_EQ( kStatusSuccess, events.back().status );
    EXPECT_DOUBLE_EQ( 100.0, events.back().totalPercent );

    // The host's first frame is INITIATE and its last is END, both SYSTEM / SW_UPDATE.
    const auto sent = shim.sentFrames();
    ASSERT_GE( sent.size(), 4u );
    EXPECT_EQ( std::vector<int>{ c::SW_UPDATE_INITIATE }, sent.front().data );
    EXPECT_EQ( std::vector<int>{ c::SW_UPDATE_END }, sent.back().data );
    EXPECT_EQ( c::SYS_SW_UPDATE, sent.front().cmd );

    // The END reply and the reset come after the host has already reported success.
    EXPECT_EQ( 0u, power.resetRequests() );
    runSimulatedMs( 700 );
    EXPECT_EQ( 1u, power.resetRequests() );
    const uint64_t updateOps = flash->operations() - opsAtStart;
    EXPECT_EQ( 1u, fw->system().updater().stats().completed );
    EXPECT_EQ( 0u, fw->system().updater().stats().failed );

    EXPECT_EQ( sys::ImageStatus::Ok, slotCheck( sys::Slot::B ).status );
    EXPECT_EQ( sys::ImageStatus::Ok, slotCheck( sys::Slot::A ).status ) << "the running slot is untouched";
    EXPECT_EQ( ( sys::VersionNo{ 3, 1, 4 } ), slotCheck( sys::Slot::B ).header.version );
    EXPECT_EQ( 15, slotCheck( sys::Slot::B ).header.swRevision );
    EXPECT_EQ( sys::BootState::TrialPending, record().state );

    // The reset: a trial boot of the new image, which tells the host its version in the handshake
    // and confirms itself once the handshake is done.
    EXPECT_EQ( ( Bytes{ 3, 1 } ), versionAfterReset() );
    EXPECT_TRUE( boot.trial );
    EXPECT_EQ( sys::Slot::B, boot.slot );
    injectFromHost( c::PKT_SYSTEM, c::SYS_COMM_INIT, { 2 } );
    injectFromHost( c::PKT_SYSTEM, c::SYS_CHECK_COMM_STATUS, { 2 } );
    stepMcu( 8 );
    EXPECT_TRUE( fw->system().confirmed() );
    EXPECT_EQ( sys::BootState::Confirmed, record().state );
    EXPECT_EQ( sys::Slot::B, record().good );

    std::fprintf( stderr, "flash byte operations in a full update: %llu\n", (unsigned long long)updateOps );
}

TEST_F( HostRig, ImageWithABadCrcIsRefusedAndTheHostReportsFailure )
{
    shim.start( corruptedHex() );
    ASSERT_TRUE( pumpUntil( [&] { return shim.finished(); } ) );
    shim.join();

    // The host sends END and waits for the MCU's answer. The MCU refuses the image with FAILURE, and
    // the host reports the update as failed instead of claiming success.
    EXPECT_EQ( kHostFailure, shim.result() );
    runSimulatedMs( 700 );

    ASSERT_FALSE( mcuReplies.empty() );
    EXPECT_EQ( ( Bytes{ c::SW_UPDATE_FAILURE } ), mcuReplies.back().payload ) << "the MCU answers END with FAILURE";
    EXPECT_EQ( sys::UpdateFailure::BadImage, fw->system().updater().stats().lastFailure );
    EXPECT_EQ( 0u, power.resetRequests() );
    EXPECT_EQ( sys::BootState::Confirmed, record().state );
    EXPECT_EQ( sys::Slot::A, record().good );
    EXPECT_EQ( ( Bytes{ 1, 0 } ), versionAfterReset() ) << "the old image still runs";
    EXPECT_FALSE( boot.trial );
}

TEST_F( HostRig, ADroppedAckIsEndedByTheMcuTimeoutAndTheHostFails )
{
    // Lose the ACK of the third DATA record. The host waits for it (up to its own timeout); the MCU waits for
    // the next record and gives up after idleTimeoutMs, and its FAILURE frees the host.
    int dataAcks = 0;
    dropReply    = [&]( const Decoded& d ) {
        if( d.payload == Bytes{ c::SW_UPDATE_ACK } && ++dataAcks == 4 ) // DATABASE's ACK is the first
            return true;
        return false;
    };
    shim.start( stampedHex() );

    // Run until the MCU has seen the transfer stall, then let simulated time pass.
    ASSERT_TRUE( pumpUntil( [&] { return dataAcks >= 4; } ) );
    for( int i = 0; i < 20; ++i ) // 20 x 400 ms of simulated time, host not answering
    {
        runSimulatedMs( 400 );
        if( shim.finished() )
            break;
        std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    }
    ASSERT_TRUE( pumpUntil( [&] { return shim.finished(); } ) ) << "the MCU timeout did not free the host";
    shim.join();

    EXPECT_EQ( kHostFailure, shim.result() );
    EXPECT_EQ( kStatusFailure, shim.statusEvents().back().status );
    EXPECT_EQ( 1u, fw->system().updater().stats().timeouts );
    EXPECT_EQ( sys::UpdateFailure::Timeout, fw->system().updater().stats().lastFailure );
    EXPECT_EQ( 0u, power.resetRequests() );
    EXPECT_EQ( sys::BootState::Confirmed, record().state );
    EXPECT_NE( sys::ImageStatus::Ok, slotCheck( sys::Slot::B ).status ) << "a half-written slot is not an image";
    EXPECT_EQ( ( Bytes{ 1, 0 } ), versionAfterReset() );
}

TEST_F( HostRig, PowerLostInTheMiddleOfTheTransferLeavesTheOldImageRunning )
{
    // Cut power 100 flash operations in: after the slot erase, inside the image data.
    flash->cutPowerAfter( 100 );
    shim.start( stampedHex() );

    // The MCU is dead from the cut on: stop answering, as a powered-off board would.
    ASSERT_TRUE( pumpUntil( [&] { return flash->powerLost(); } ) ) << "the power cut never fired";
    EXPECT_FALSE( shim.finished() ) << "the host cannot tell; it waits for an ACK that will never come";

    // The operator gives up (the host has no timeout), and power comes back.
    shim.giveUp();
    shim.join();
    EXPECT_EQ( kHostFailure, shim.result() );
    flash->restorePower();

    EXPECT_EQ( sys::BootState::Confirmed, record().state );
    EXPECT_EQ( sys::Slot::A, record().good );
    EXPECT_EQ( ( Bytes{ 1, 0 } ), versionAfterReset() );
    EXPECT_FALSE( boot.trial );
    EXPECT_EQ( sys::Slot::A, boot.slot );
    EXPECT_EQ( sys::ImageStatus::Ok, slotCheck( sys::Slot::A ).status );
    EXPECT_NE( sys::ImageStatus::Ok, slotCheck( sys::Slot::B ).status );
}

// A full update is 550 flash byte operations here (HappyPath prints the count): erase of the 256-byte
// slot, 214 bytes of image, then the boot-control write, which is a 64-byte block erase (470 to 534)
// and the 16-byte record (534 to 550). These cuts land in each half of that last write.
TEST_F( HostRig, PowerLostWhileErasingTheBootControlCopyKeepsTheOldImage )
{
    flash->cutPowerAfter( 500 );
    shim.start( stampedHex() );
    ASSERT_TRUE( pumpUntil( [&] { return flash->powerLost() || shim.finished(); } ) );
    ASSERT_TRUE( flash->powerLost() );
    EXPECT_EQ( 0u, power.resetRequests() );
    shim.giveUp();
    shim.join();
    flash->restorePower();

    EXPECT_EQ( ( Bytes{ 1, 0 } ), versionAfterReset() ) << "the newest record was never touched";
    EXPECT_FALSE( boot.trial );
    EXPECT_EQ( sys::Slot::A, boot.slot );
    EXPECT_EQ( sys::ImageStatus::Ok, slotCheck( sys::Slot::B ).status ) << "the new image is complete, just not armed";
}

TEST_F( HostRig, PowerLostInTheMiddleOfTheBootControlRecordKeepsTheOldImage )
{
    flash->cutPowerAfter( 541 );
    shim.start( stampedHex() );
    ASSERT_TRUE( pumpUntil( [&] { return flash->powerLost() || shim.finished(); } ) );
    ASSERT_TRUE( flash->powerLost() );
    shim.giveUp();
    shim.join();
    flash->restorePower();

    EXPECT_EQ( ( Bytes{ 1, 0 } ), versionAfterReset() ) << "a record cut short fails its CRC and is ignored";
    EXPECT_FALSE( boot.trial );
    EXPECT_EQ( sys::Slot::A, boot.slot );
}
