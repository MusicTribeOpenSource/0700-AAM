// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/system/system.h"

#include "kick_mcu/contract.h"

namespace kick_mcu
{
namespace system
{
    using namespace contract;

    namespace
    {
        /// Stands in for the flash when the build has none, so the Updater can exist and answer
        /// FAILURE. layout.fits() is false for it (size 0), so nothing ever reaches these methods.
        class NoFlash : public bsp::IFlash
        {
        public:
            size_t sizeBytes() const override { return 0; }
            size_t eraseBlockBytes() const override { return 0; }
            bool   read( size_t, uint8_t*, size_t ) override { return false; }
            bool   write( size_t, const uint8_t*, size_t ) override { return false; }
            bool   eraseBlock( size_t ) override { return false; }
        };
        NoFlash gNoFlash;
    } // namespace

    System::System( const SystemConfig& config, const SelfCheckResult& selfCheck, link::TxQueue& tx, const bsp::IClock& clock, QuiesceFn onQuiesce, void* quiesceContext )
        : config_( config )
        , selfCheck_( selfCheck )
        , tx_( tx )
        , clock_( clock )
        , onQuiesce_( onQuiesce )
        , quiesceContext_( quiesceContext )
        , updater_( config.flash != nullptr ? *config.flash : static_cast<bsp::IFlash&>( gNoFlash ), config.layout, selfCheck.slot, config.power, clock, &System::replyUpdate, this, config.updater )
        , confirmed_( !( config.flash != nullptr && config.boot.trial ) )
    {
    }

    bool System::replyUpdate( void* context, const uint8_t* payload, size_t length )
    {
        System* self = static_cast<System*>( context );
        return self->tx_.enqueue( link::Lane::System, PKT_SYSTEM, DIR_REQUEST, SYS_SW_UPDATE, payload, length );
    }

    bool System::handle( const link::Frame& frame )
    {
        if( frame.type != PKT_SYSTEM )
            return false;

        switch( frame.cmd )
        {
            case SYS_GET_SW_VER:
            {
                uint8_t report[kVersionReportBytes];
                const size_t n = encodeVersionReport( selfCheck_, report );
                tx_.enqueue( link::Lane::System, PKT_SYSTEM, DIR_REQUEST, SYS_GET_SW_VER, report, n );
                return true;
            }

            case SYS_BPM:
                bpm_.onPayload( frame.payload, frame.length );
                return true;

            case SYS_SHUTDOWN:
            {
                if( !shutdown_.quiesced() )
                {
                    shutdown_.request( clock_.nowMs() );
                    if( onQuiesce_ != nullptr )
                        onQuiesce_( quiesceContext_ );
                }
                const uint8_t ack[1] = { 1 };
                tx_.enqueue( link::Lane::System, PKT_SYSTEM, DIR_REQUEST, SYS_SHUTDOWN, ack, 1 );
                return true;
            }

            case SYS_SW_UPDATE:
                if( !shutdown_.quiesced() )
                    updater_.onFrame( frame.payload, frame.length );
                return true;

            default:
                return false;
        }
    }

    void System::poll( bool streaming )
    {
        const uint32_t now = clock_.nowMs();

        updater_.poll();
        shutdown_.poll( now, tx_.empty(), config_.power );

        if( !confirmed_ && streaming && selfCheck_.ok() && ( !triedConfirm_ || (uint32_t)( now - lastConfirmTryMs_ ) >= config_.confirmRetryMs ) )
        {
            triedConfirm_     = true;
            lastConfirmTryMs_ = now;
            BootControl control( *config_.flash, config_.layout );
            if( control.confirm( selfCheck_.slot ) )
                confirmed_ = true;
            else
                ++confirmFailures_;
        }
    }

} // namespace system
} // namespace kick_mcu
