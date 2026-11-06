// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The update state machine: the happy path and every way a transfer can go wrong, each ending
// on a bootable old image, plus a sweep that cuts power at every flash byte operation.
#include <gtest/gtest.h>

#include "system_support.h"

using namespace system_support;
namespace c = kick_mcu::contract;

namespace
{
struct UpdateRig : ::testing::Test, UpdaterRig
{
    Bytes v1{ makeImage( { 1, 0, 0 }, 100, 1 ) };
    Bytes v2{ makeImage( { 1, 1, 0 }, 130, 2 ) };

    void SetUp() override
    {
        provision( *flash, layout, v1 );
        start( Slot::A );
    }

    Bytes slotBytes( Slot s, size_t n ) const
    {
        const size_t base = layout.baseOf( s );
        return Bytes( flash->contents().begin() + (std::ptrdiff_t)base, flash->contents().begin() + (std::ptrdiff_t)( base + n ) );
    }

    BootDecision reboot()
    {
        flash->restorePower();
        return selectBoot( *flash, layout );
    }

    void expectOldImageBoots()
    {
        const BootDecision d = reboot();
        ASSERT_TRUE( d.bootable );
        EXPECT_EQ( Slot::A, d.slot );
        EXPECT_EQ( ( VersionNo{ 1, 0, 0 } ), d.image.header.version );
        EXPECT_EQ( v1, slotBytes( Slot::A, v1.size() ) ) << "the running image was never touched";
    }
};
} // namespace

TEST_F( UpdateRig, HappyPathInstallsTheImageAndArmsATrialBoot )
{
    ASSERT_TRUE( initiate() );
    EXPECT_EQ( Bytes{ c::SW_UPDATE_BEGIN_TRANSFER }, replies.got.back() );
    transfer( v2 );

    EXPECT_EQ( UpdateState::Committed, updater->state() );
    // BEGIN, one ACK for DATABASE, one per DATA record, one for END.
    const size_t records = ( v2.size() + 15 ) / 16;
    EXPECT_EQ( 1u, replies.count( c::SW_UPDATE_BEGIN_TRANSFER ) );
    EXPECT_EQ( 1 + records + 1, replies.count( c::SW_UPDATE_ACK ) );
    EXPECT_EQ( 0u, replies.count( c::SW_UPDATE_FAILURE ) );
    EXPECT_EQ( v2, slotBytes( Slot::B, v2.size() ) );
    EXPECT_EQ( v1, slotBytes( Slot::A, v1.size() ) );

    EXPECT_EQ( 0u, power.resetRequests() ) << "not yet: the ACK has to leave first";
    runResetDelay();
    EXPECT_EQ( 1u, power.resetRequests() );
    updater->poll();
    EXPECT_EQ( 1u, power.resetRequests() ) << "asked once";

    BootDecision d = reboot();
    ASSERT_TRUE( d.bootable );
    EXPECT_EQ( Slot::B, d.slot );
    EXPECT_TRUE( d.trial );
    EXPECT_EQ( ( VersionNo{ 1, 1, 0 } ), d.image.header.version );
}

TEST_F( UpdateRig, DataRecordsCarryTheHostsStaleTrailingByteWhichIsDropped )
{
    ASSERT_TRUE( initiate() );
    dataBase( 0 );
    const uint8_t bytes[4] = { 0x11, 0x22, 0x33, 0x44 };
    data( 0x20, bytes, 4, 0xAB );
    const auto at = flash->contents().begin() + (std::ptrdiff_t)layout.baseOf( Slot::B ) + 0x20;
    EXPECT_EQ( ( Bytes{ 0x11, 0x22, 0x33, 0x44, 0xFF } ), Bytes( at, at + 5 ) ) << "four data bytes written, the stale byte not";
}

TEST_F( UpdateRig, ALargeRecordUpToTheHostsLimitIsAccepted )
{
    ASSERT_TRUE( initiate() );
    dataBase( 0 );
    Bytes record( 29, 0x3C ); // the largest record the host can send without overrunning its own buffer
    data( 0, record.data(), record.size() );
    EXPECT_EQ( c::SW_UPDATE_ACK, replies.last() );
}

TEST_F( UpdateRig, UsesTheUpperAddressFromDatabase )
{
    ASSERT_TRUE( initiate() );
    dataBase( 1 ); // 0x10000: past a 256-byte slot
    const uint8_t b = 0x42;
    data( 0, &b, 1 );
    EXPECT_EQ( c::SW_UPDATE_FAILURE, replies.last() );
    EXPECT_EQ( UpdateFailure::OutOfSlot, updater->stats().lastFailure );
    EXPECT_EQ( UpdateState::Idle, updater->state() );
    expectOldImageBoots();
}

TEST_F( UpdateRig, BadImageCrcFailsAtEndAndTheOldImageStillBoots )
{
    Bytes bad = v2;
    bad[bad.size() - 3] ^= 0x80;
    ASSERT_TRUE( initiate() );
    transfer( bad );
    EXPECT_EQ( c::SW_UPDATE_FAILURE, replies.last() );
    EXPECT_EQ( UpdateFailure::BadImage, updater->stats().lastFailure );
    EXPECT_EQ( UpdateState::Idle, updater->state() );
    expectOldImageBoots();
    BootRecord r;
    ASSERT_TRUE( BootControl( *flash, layout ).load( r ) );
    EXPECT_EQ( BootState::Confirmed, r.state ) << "nothing was armed";
}

TEST_F( UpdateRig, ATruncatedTransferFailsAtEnd )
{
    ASSERT_TRUE( initiate() );
    transfer( v2, 16, 2 ); // the last two records never arrive
    EXPECT_EQ( c::SW_UPDATE_FAILURE, replies.last() );
    expectOldImageBoots();
}

TEST_F( UpdateRig, AHeaderThatLiesAboutItsLengthFailsAtEnd )
{
    Bytes lie = v2;
    ImageHeader h;
    ASSERT_EQ( ImageStatus::Ok, decodeImageHeader( lie.data(), h ) );
    h.imageBytes += 40; // longer than what is sent, still inside the slot
    encodeImageHeader( h, lie.data() );
    ASSERT_TRUE( initiate() );
    transfer( lie );
    EXPECT_EQ( c::SW_UPDATE_FAILURE, replies.last() );
    expectOldImageBoots();
}

TEST_F( UpdateRig, ADroppedAckEndsInATimeoutFailureAndTheOldImage )
{
    ASSERT_TRUE( initiate() );
    dataBase( 0 );
    for( uint32_t r = 0; r < 3; ++r )
        data( r * 16, v2.data() + r * 16, 16 );
    // The host never got an ACK for the next record, so it sends nothing more and waits forever.
    replies.got.clear();
    clock.advanceMs( config.idleTimeoutMs - 1 );
    updater->poll();
    EXPECT_EQ( UpdateState::Receiving, updater->state() );
    clock.advanceMs( 2 );
    updater->poll();
    EXPECT_EQ( UpdateState::Idle, updater->state() );
    EXPECT_EQ( Bytes{ c::SW_UPDATE_FAILURE }, replies.got.back() ) << "FAILURE unblocks a host that is waiting for an ACK";
    EXPECT_EQ( 1u, updater->stats().timeouts );
    expectOldImageBoots();

    // And a later transfer from scratch works.
    start( Slot::A );
    ASSERT_TRUE( initiate() );
    transfer( v2 );
    EXPECT_EQ( UpdateState::Committed, updater->state() );
}

TEST_F( UpdateRig, ARecordSentTwiceIsAcknowledgedAndNotRewritten )
{
    ASSERT_TRUE( initiate() );
    dataBase( 0 );
    const size_t records = ( v2.size() + 15 ) / 16;
    for( size_t r = 0; r < records; ++r )
    {
        const size_t n = std::min<size_t>( 16, v2.size() - r * 16 );
        data( (uint32_t)( r * 16 ), v2.data() + r * 16, n );
        data( (uint32_t)( r * 16 ), v2.data() + r * 16, n ); // the ACK was lost, so the host asks again
    }
    send( { c::SW_UPDATE_END } );
    EXPECT_EQ( records, updater->stats().duplicates );
    EXPECT_EQ( records, updater->stats().records );
    EXPECT_EQ( 0u, flash->illegalWrites() );
    EXPECT_EQ( UpdateState::Committed, updater->state() );
}

TEST_F( UpdateRig, ADifferentRecordAtAnAddressAlreadyWrittenIsRefusedNotMerged )
{
    ASSERT_TRUE( initiate() );
    dataBase( 0 );
    const uint8_t first[2]  = { 0x0F, 0x0F };
    const uint8_t second[2] = { 0xF0, 0xF0 }; // would need cleared bits to come back
    data( 0x40, first, 2 );
    ASSERT_EQ( c::SW_UPDATE_ACK, replies.last() );
    data( 0x40, second, 2 );
    EXPECT_EQ( c::SW_UPDATE_FAILURE, replies.last() );
    EXPECT_EQ( UpdateFailure::WriteConflict, updater->stats().lastFailure );
    EXPECT_EQ( 0x0F, flash->contents()[layout.baseOf( Slot::B ) + 0x40] ) << "the first record is intact";
}

TEST_F( UpdateRig, AMidTransferResetLeavesTheOldImageAndAFreshUpdateWorks )
{
    ASSERT_TRUE( initiate() );
    transfer( v2, 16, 4, false );
    // Power cycle: the Updater is gone, the flash keeps what it has.
    expectOldImageBoots();
    start( Slot::A );
    EXPECT_EQ( UpdateState::Idle, updater->state() );
    ASSERT_TRUE( initiate() );
    transfer( v2 );
    EXPECT_EQ( UpdateState::Committed, updater->state() );
}

TEST_F( UpdateRig, DataBeforeInitiateIsRefused )
{
    const uint8_t b = 1;
    data( 0, &b, 1 );
    EXPECT_EQ( c::SW_UPDATE_FAILURE, replies.last() );
    EXPECT_EQ( UpdateFailure::NotStarted, updater->stats().lastFailure );
    send( { c::SW_UPDATE_END } );
    EXPECT_EQ( UpdateFailure::NotStarted, updater->stats().lastFailure );
}

TEST_F( UpdateRig, ShortOrOversizeRecordsAreRefused )
{
    ASSERT_TRUE( initiate() );
    send( { c::SW_UPDATE_DATA, 0, 0, 0xAA } ); // cmd, address, one byte: no room for data plus the stale byte
    EXPECT_EQ( UpdateFailure::BadFrame, updater->stats().lastFailure );
    ASSERT_TRUE( initiate() );
    send( { c::SW_UPDATE_DATABASE, 0 } );
    EXPECT_EQ( UpdateFailure::BadFrame, updater->stats().lastFailure );
}

TEST_F( UpdateRig, WillNotUpdateFromAnImageThatIsNotConfirmed )
{
    BootControl( *flash, layout ).commitTrial( Slot::A, Slot::B );
    EXPECT_FALSE( initiate() );
    EXPECT_EQ( UpdateFailure::NotConfirmed, updater->stats().lastFailure );
    EXPECT_EQ( c::SW_UPDATE_FAILURE, replies.last() );
}

TEST_F( UpdateRig, NeverTouchesTheRunningSlotEvenWhenTheOtherOneIsRunning )
{
    // Run from B this time: the update must go to A.
    ASSERT_TRUE( writeSlot( *flash, layout, Slot::B, v1 ) );
    ASSERT_TRUE( BootControl( *flash, layout ).confirm( Slot::B ) );
    start( Slot::B );
    ASSERT_TRUE( initiate() );
    EXPECT_EQ( Slot::A, updater->target() );
    transfer( v2 );
    EXPECT_EQ( v1, slotBytes( Slot::B, v1.size() ) );
    EXPECT_EQ( v2, slotBytes( Slot::A, v2.size() ) );
}

TEST_F( UpdateRig, AnInitiateAfterACommitIsRefusedAndTheCommitStands )
{
    ASSERT_TRUE( initiate() );
    transfer( v2 );
    ASSERT_EQ( UpdateState::Committed, updater->state() );
    replies.got.clear();
    send( { c::SW_UPDATE_INITIATE } );
    EXPECT_EQ( c::SW_UPDATE_FAILURE, replies.last() );
    EXPECT_EQ( UpdateState::Committed, updater->state() );
    EXPECT_EQ( Slot::B, reboot().slot );
}

TEST_F( UpdateRig, AStrayEndOrDataAfterACommitDoesNotCancelTheReset )
{
    ASSERT_TRUE( initiate() );
    transfer( v2 );
    ASSERT_EQ( UpdateState::Committed, updater->state() );
    send( { c::SW_UPDATE_END } );
    const uint8_t b = 0x42;
    data( 0, &b, 1 );
    EXPECT_EQ( UpdateState::Committed, updater->state() ) << "the armed trial is not undone by late frames";
    runResetDelay();
    EXPECT_EQ( 1u, power.resetRequests() );
}

TEST_F( UpdateRig, SubcommandsOnlyTheMcuSendsAreIgnored )
{
    replies.got.clear();
    for( int sub : { (int)c::SW_UPDATE_BEGIN_TRANSFER, (int)c::SW_UPDATE_ACK, (int)c::SW_UPDATE_FAILURE, (int)c::SW_UPDATE_IDLE, 99 } )
        send( { (uint8_t)sub } );
    send( {} );
    EXPECT_TRUE( replies.got.empty() );
    EXPECT_EQ( 6u, updater->stats().ignored );
}

TEST_F( UpdateRig, ANewInitiateMidTransferRestartsCleanly )
{
    ASSERT_TRUE( initiate() );
    transfer( v2, 16, 3, false );
    ASSERT_TRUE( initiate() ); // the host restarted
    transfer( v2 );
    EXPECT_EQ( UpdateState::Committed, updater->state() );
    EXPECT_EQ( v2, slotBytes( Slot::B, v2.size() ) );
}

// ---- power cuts ---------------------------------------------------------------------------------

namespace
{
/// The whole life of an update on one flash: update, reset into the trial, confirm. Every flash
/// operation counts toward the power-cut budget, so cutting at n covers the erase, each record,
/// the boot-control commit, the bootloader's trial mark, and the confirm.
struct Lifecycle
{
    FlashLayout layout{ testLayout() };
    std::unique_ptr<sim::SimFlash> flash{ makeFlash( layout ) };
    Bytes v1{ makeImage( { 1, 0, 0 }, 100, 1 ) };
    Bytes v2{ makeImage( { 1, 1, 0 }, 130, 2 ) };
    bool confirmWanted{ true };

    void run( int64_t cutAfter )
    {
        provision( *flash, layout, v1 );
        const uint64_t before = flash->operations();
        if( cutAfter >= 0 )
            flash->cutPowerAfter( (uint64_t)cutAfter );

        Replies replies;
        sim::SimClock clock;
        sim::SimPower power;
        Updater u( *flash, layout, Slot::A, &power, clock, &Replies::sink, &replies );

        auto sendp = [&]( const Bytes& p ) { u.onFrame( p.data(), p.size() ); };
        sendp( { c::SW_UPDATE_INITIATE } );
        for( int i = 0; i < 64 && u.state() == UpdateState::Erasing; ++i )
            u.poll();
        if( u.state() == UpdateState::Receiving )
        {
            sendp( { c::SW_UPDATE_DATABASE, 0, 0 } );
            for( size_t off = 0; off < v2.size() && u.state() == UpdateState::Receiving; off += 16 )
            {
                const size_t n = std::min<size_t>( 16, v2.size() - off );
                Bytes p{ c::SW_UPDATE_DATA, (uint8_t)( off >> 8 ), (uint8_t)off };
                p.insert( p.end(), v2.begin() + (std::ptrdiff_t)off, v2.begin() + (std::ptrdiff_t)( off + n ) );
                p.push_back( 0x77 );
                sendp( p );
            }
            sendp( { c::SW_UPDATE_END } );
        }
        clock.advanceMs( 600 );
        u.poll();

        const BootDecision d = selectBoot( *flash, layout );
        if( d.bootable && d.trial && confirmWanted )
            BootControl( *flash, layout ).confirm( d.slot );
        operationsUsed = flash->operations() - before;
    }

    uint64_t operationsUsed{ 0 };

    /// After the (possibly cut) run: power back, reboot, and check the invariant.
    void checkInvariant( int64_t cut )
    {
        flash->restorePower();
        const BootDecision d = selectBoot( *flash, layout );
        ASSERT_TRUE( d.bootable ) << "cut after " << cut << ": nothing bootable";
        ASSERT_EQ( ImageStatus::Ok, d.image.status ) << "cut after " << cut;
        if( d.slot == Slot::A )
            EXPECT_EQ( ( VersionNo{ 1, 0, 0 } ), d.image.header.version ) << "cut after " << cut;
        else
            EXPECT_EQ( ( VersionNo{ 1, 1, 0 } ), d.image.header.version ) << "cut after " << cut;
        // Whatever booted, the unit is not stuck: a second reboot also boots something valid.
        const BootDecision d2 = selectBoot( *flash, layout );
        ASSERT_TRUE( d2.bootable ) << "cut after " << cut << ": second boot";
        ASSERT_EQ( ImageStatus::Ok, d2.image.status ) << "cut after " << cut << ": second boot";
    }
};
} // namespace

TEST( UpdatePowerCut, WithoutACutTheLifecycleEndsOnTheNewImageConfirmed )
{
    Lifecycle l;
    l.run( -1 );
    l.flash->restorePower();
    const BootDecision d = selectBoot( *l.flash, l.layout );
    EXPECT_EQ( Slot::B, d.slot );
    EXPECT_FALSE( d.trial );
    EXPECT_EQ( ( VersionNo{ 1, 1, 0 } ), d.image.header.version );
}

TEST( UpdatePowerCut, CuttingPowerAtEveryFlashByteOperationAlwaysLeavesABootableImage )
{
    Lifecycle probe;
    probe.run( -1 );
    const uint64_t total = probe.operationsUsed;
    ASSERT_GT( total, 600u ) << "the sweep should cover erases, records and control writes";

    for( uint64_t cut = 0; cut <= total; ++cut )
    {
        Lifecycle l;
        l.run( (int64_t)cut );
        l.checkInvariant( (int64_t)cut );
        if( ::testing::Test::HasFatalFailure() )
            return;
    }
}

TEST( UpdatePowerCut, TheSameSweepWithoutEverConfirmingNeverEndsOnTheNewImage )
{
    Lifecycle probe;
    probe.confirmWanted = false;
    probe.run( -1 );
    for( uint64_t cut = 0; cut <= probe.operationsUsed; ++cut )
    {
        Lifecycle l;
        l.confirmWanted = false;
        l.run( (int64_t)cut );
        l.flash->restorePower();
        // run() left the new image in a trial (or not); one more reboot without a confirm reverts it.
        selectBoot( *l.flash, l.layout );
        const BootDecision settled = selectBoot( *l.flash, l.layout );
        ASSERT_TRUE( settled.bootable ) << cut;
        EXPECT_EQ( Slot::A, settled.slot ) << "cut after " << cut << ": an unconfirmed image must not stick";
    }
}

TEST( UpdatePowerCut, AfterAnyCutAFreshUpdateStillSucceeds )
{
    Lifecycle probe;
    probe.run( -1 );
    // Every 7th cut point keeps this affordable; the sweep above covers all of them.
    for( uint64_t cut = 0; cut <= probe.operationsUsed; cut += 7 )
    {
        Lifecycle l;
        l.run( (int64_t)cut );
        l.flash->restorePower();
        const BootDecision d = selectBoot( *l.flash, l.layout );
        ASSERT_TRUE( d.bootable ) << cut;
        if( d.trial )
            BootControl( *l.flash, l.layout ).confirm( d.slot );

        // Confirm may have been what got cut, so boot once more to settle where we are.
        const BootDecision running = selectBoot( *l.flash, l.layout );
        ASSERT_TRUE( running.bootable ) << cut;
        UpdaterRig rig;
        rig.layout = l.layout;
        rig.flash  = std::move( l.flash );
        rig.start( running.slot );
        const Bytes next = makeImage( { 2, 0, 0 }, 90, 9 );
        ASSERT_TRUE( rig.initiate() ) << "cut after " << cut;
        rig.transfer( next );
        EXPECT_EQ( UpdateState::Committed, rig.updater->state() ) << "cut after " << cut;
        const BootDecision after = selectBoot( *rig.flash, rig.layout );
        ASSERT_TRUE( after.bootable ) << cut;
        EXPECT_TRUE( after.trial ) << cut;
        EXPECT_EQ( ( VersionNo{ 2, 0, 0 } ), after.image.header.version ) << cut;
    }
}
