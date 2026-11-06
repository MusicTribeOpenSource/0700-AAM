// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/system/updater.h"

#include <cstring>

#include "kick_mcu/contract.h"

namespace kick_mcu
{
namespace system
{
    using namespace contract;

    const char* updateFailureName( UpdateFailure failure )
    {
        switch( failure )
        {
            case UpdateFailure::None: return "none";
            case UpdateFailure::NoFlash: return "no-flash";
            case UpdateFailure::NotConfirmed: return "not-confirmed";
            case UpdateFailure::FlashError: return "flash-error";
            case UpdateFailure::OutOfSlot: return "out-of-slot";
            case UpdateFailure::WriteConflict: return "write-conflict";
            case UpdateFailure::BadFrame: return "bad-frame";
            case UpdateFailure::BadImage: return "bad-image";
            case UpdateFailure::NotStarted: return "not-started";
            case UpdateFailure::Busy: return "busy";
            case UpdateFailure::Timeout: return "timeout";
        }
        return "unknown";
    }

    Updater::Updater( bsp::IFlash& flash, const FlashLayout& layout, Slot running, bsp::IPower* power, const bsp::IClock& clock, ReplyFn reply, void* replyContext, const UpdaterConfig& config )
        : flash_( flash )
        , layout_( layout )
        , control_( flash, layout )
        , running_( running )
        , target_( otherSlot( running ) )
        , power_( power )
        , clock_( clock )
        , reply_( reply )
        , replyContext_( replyContext )
        , config_( config )
    {
    }

    void Updater::send( const uint8_t* payload, size_t length )
    {
        if( reply_ != nullptr )
            reply_( replyContext_, payload, length );
    }

    void Updater::ack()
    {
        const uint8_t p[1] = { SW_UPDATE_ACK };
        send( p, 1 );
    }

    void Updater::reject( UpdateFailure why )
    {
        const uint8_t p[1] = { SW_UPDATE_FAILURE };
        send( p, 1 );
        ++stats_.failed;
        stats_.lastFailure = why;
        if( why == UpdateFailure::Timeout )
            ++stats_.timeouts;
    }

    void Updater::fail( UpdateFailure why )
    {
        reject( why );
        // A committed update stands: the trial is already armed, and a late or repeated frame
        // must not cancel the reset that starts it.
        if( state_ != UpdateState::Committed )
            state_ = UpdateState::Idle;
    }

    void Updater::onFrame( const uint8_t* payload, size_t length )
    {
        if( length == 0 )
        {
            ++stats_.ignored;
            return;
        }
        touch();

        switch( payload[0] )
        {
            case SW_UPDATE_INITIATE:
                begin();
                break;
            case SW_UPDATE_DATA:
                data( payload, length );
                break;
            case SW_UPDATE_DATABASE:
                dataBase( payload, length );
                break;
            case SW_UPDATE_END:
                end();
                break;
            default:
                // BEGIN_TRANSFER, ACK, FAILURE and IDLE only ever travel MCU to host.
                ++stats_.ignored;
                break;
        }
    }

    void Updater::begin()
    {
        if( state_ == UpdateState::Committed )
        {
            reject( UpdateFailure::Busy ); // the committed update stands; only the new request is refused
            return;
        }
        if( !layout_.fits( flash_ ) )
        {
            fail( UpdateFailure::NoFlash );
            return;
        }
        // Only update from an image the bootloader has confirmed: that is the one we can fall back to.
        BootRecord record;
        if( !control_.load( record ) || record.state != BootState::Confirmed || record.good != running_ )
        {
            fail( UpdateFailure::NotConfirmed );
            return;
        }

        ++stats_.started;
        state_          = UpdateState::Erasing;
        nextEraseBlock_ = 0;
        ulba_           = 0;
    }

    void Updater::dataBase( const uint8_t* payload, size_t length )
    {
        if( state_ != UpdateState::Receiving )
        {
            fail( UpdateFailure::NotStarted );
            return;
        }
        if( length != 3 )
        {
            fail( UpdateFailure::BadFrame );
            return;
        }
        ulba_ = ( (uint32_t)payload[1] << 8 ) | payload[2];
        ack();
    }

    void Updater::data( const uint8_t* payload, size_t length )
    {
        if( state_ != UpdateState::Receiving )
        {
            fail( UpdateFailure::NotStarted );
            return;
        }
        // cmd, addrHi, addrLo, at least one data byte, and the host's stale trailing byte.
        if( length < 5 || length > kMaxPayloadFromHost )
        {
            fail( UpdateFailure::BadFrame );
            return;
        }
        const size_t   count  = length - 4;
        const uint8_t* bytes  = payload + 3;
        const uint64_t offset = ( (uint64_t)ulba_ << 16 ) | ( (uint32_t)payload[1] << 8 ) | payload[2];
        if( offset + count > layout_.slotBytes )
        {
            fail( UpdateFailure::OutOfSlot );
            return;
        }

        const size_t address = layout_.baseOf( target_ ) + (size_t)offset;
        uint8_t      current[kMaxPayloadFromHost];
        if( !flash_.read( address, current, count ) )
        {
            fail( UpdateFailure::FlashError );
            return;
        }
        if( std::memcmp( current, bytes, count ) == 0 )
        {
            // The host sent this record again (it never got the ACK, or replays). Nothing to write.
            ++stats_.duplicates;
            ack();
            return;
        }
        if( !flash_.write( address, bytes, count ) )
        {
            // A write that only needs to clear bits cannot fail on an erased slot, so a refusal
            // here is either a different record at an address already written, or a dead device.
            bool needsSetBit = false;
            for( size_t i = 0; i < count; ++i )
                needsSetBit = needsSetBit || ( ( bytes[i] & ~current[i] ) != 0 );
            fail( needsSetBit ? UpdateFailure::WriteConflict : UpdateFailure::FlashError );
            return;
        }
        ++stats_.records;
        ack();
    }

    void Updater::end()
    {
        if( state_ != UpdateState::Receiving )
        {
            fail( UpdateFailure::NotStarted );
            return;
        }
        const ImageCheck check = verifyImage( flash_, layout_.baseOf( target_ ), layout_.slotBytes );
        if( check.status != ImageStatus::Ok )
        {
            fail( UpdateFailure::BadImage );
            return;
        }
        if( !control_.commitTrial( running_, target_ ) )
        {
            fail( UpdateFailure::FlashError );
            return;
        }
        ++stats_.completed;
        state_          = UpdateState::Committed;
        resetAtMs_      = clock_.nowMs() + config_.resetDelayMs;
        resetRequested_ = false;
        ack();
    }

    void Updater::poll()
    {
        const uint32_t now = clock_.nowMs();

        switch( state_ )
        {
            case UpdateState::Idle:
                break;

            case UpdateState::Erasing:
            {
                const size_t blocks = layout_.slotBytes / layout_.blockBytes;
                if( !flash_.eraseBlock( layout_.baseOf( target_ ) + nextEraseBlock_ * layout_.blockBytes ) )
                {
                    fail( UpdateFailure::FlashError );
                    break;
                }
                touch(); // the host is waiting for BEGIN, so progress here is not silence
                if( ++nextEraseBlock_ >= blocks )
                {
                    state_ = UpdateState::Receiving;
                    const uint8_t p[1] = { SW_UPDATE_BEGIN_TRANSFER };
                    send( p, 1 );
                }
                break;
            }

            case UpdateState::Receiving:
                if( (uint32_t)( now - lastActivityMs_ ) > config_.idleTimeoutMs )
                    fail( UpdateFailure::Timeout );
                break;

            case UpdateState::Committed:
                if( !resetRequested_ && (int32_t)( now - resetAtMs_ ) >= 0 )
                {
                    resetRequested_ = true;
                    if( power_ != nullptr )
                        power_->requestReset();
                }
                break;
        }
    }

} // namespace system
} // namespace kick_mcu
