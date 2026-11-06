// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/app/scan_loop.h"

#include "kick_mcu/contract.h"

namespace kick_mcu
{
namespace app
{
    ScanLoop::ScanLoop( InputHardware hardware, input::IEventSink& sink, const bsp::IClock& clock, const ScanConfig& config )
        : hw_( hardware )
        , sink_( sink )
        , clock_( clock )
        , debouncer_( config.debounce )
        , pads_( config.pads )
        , encoders_( config.encoders )
        , strip_( config.strip )
    {
        debouncer_.reset( clock_.nowUs() );
    }

    void ScanLoop::reportButtons( uint64_t stable )
    {
        // Only the codes the host knows about; the matrix may have more bits than keys.
        constexpr uint64_t codeMask = ( uint64_t( 1 ) << contract::kBtnCodeCount ) - 1;
        stable &= codeMask;

        if( !sink_.accepting() )
        {
            // Track the keys, tell no one. Whatever is held now is not announced later, and its
            // eventual release is swallowed too (below), so the host never sees half a press.
            reported_       = stable;
            heldBeforeLink_ = stable;
            return;
        }

        uint64_t diff = ( stable ^ reported_ ) & codeMask;
        if( diff == 0 )
            return;

        // SHIFT first, so a shifted press that arrives in the same tick is read in the right layer.
        for( int pass = 0; pass < 2 && diff != 0; ++pass )
        {
            for( unsigned code = 0; code < contract::kBtnCodeCount; ++code )
            {
                const bool isShift = code == contract::kBtnShift;
                if( ( pass == 0 ) != isShift )
                    continue;
                const uint64_t bit = uint64_t( 1 ) << code;
                if( !( diff & bit ) )
                    continue;

                const bool down = ( stable & bit ) != 0;
                if( !down && ( heldBeforeLink_ & bit ) )
                {
                    heldBeforeLink_ &= ~bit;
                    reported_ &= ~bit;
                    continue;
                }
                const uint8_t payload[2] = { (uint8_t)code, down ? contract::BTN_PRESSED : contract::BTN_RELEASED };
                if( sink_.send( link::Lane::Button, contract::PKT_BUTTON, contract::BTN_STATUS, payload, 2 ) )
                {
                    reported_ = down ? ( reported_ | bit ) : ( reported_ & ~bit );
                    ++stats_.buttonFrames;
                }
                else
                    ++stats_.buttonRetries;
            }
        }
    }

    void ScanLoop::tick()
    {
        const uint32_t now = clock_.nowUs();
        ++stats_.ticks;

        reportButtons( debouncer_.update( hw_.matrix.scan(), now ) );

        uint16_t pressure[contract::kPadCount];
        hw_.pads.sample( pressure );
        pads_.update( pressure, now, sink_ );

        encoders_.update( hw_.encoders.readPhases(), sink_ );
        strip_.update( hw_.strip, now, sink_ );
    }

} // namespace app
} // namespace kick_mcu
