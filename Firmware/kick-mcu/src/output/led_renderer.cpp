// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/output/led_renderer.h"

namespace kick_mcu
{
namespace output
{
    using namespace contract;

    LedRefresh::LedRefresh( bsp::ILedDriver& driver, const LedState& state, BlinkClock& blink, const bsp::IClock& clock, const RenderConfig& config )
        : driver_( driver )
        , state_( state )
        , blink_( blink )
        , clock_( clock )
        , config_( config )
        , gamma_( config.gamma )
    {
    }

    void LedRefresh::renderPad( const PadLed& pad, bool blinkOn, uint8_t out[3] ) const
    {
        uint8_t  id    = pad.colour;
        uint32_t scale = 256;
        switch( pad.state )
        {
            case STATE_ON: break;
            case STATE_SAMPLE_ASSIGNED: scale = config_.sampleAssignedQ8; break;
            case STATE_SAMPLE_NOT_ASSIGNED: scale = config_.sampleNotAssignedQ8; break;
            case STATE_ON_BLINK: id = blinkOn ? pad.colour : pad.offColour; break;
            case STATE_OFF_BLINK: id = blinkOn ? pad.offColour : pad.colour; break;
            default: out[0] = out[1] = out[2] = 0; return; // OFF, and any state the host should not send
        }
        if( colours_.isTransparent( id ) )
        {
            out[0] = out[1] = out[2] = 0;
            return;
        }

        const uint32_t velocity = pad.velocity > 127 ? 127 : pad.velocity;
        scale                   = ( scale * ( velocity * 256 / 127 ) ) >> 8;
        scale                   = ( scale * config_.masterQ8 ) >> 8;

        const Rgb rgb = colours_.lookup( id );
        out[0]        = gamma_.apply( (uint8_t)( ( rgb.r * scale ) >> 8 ) );
        out[1]        = gamma_.apply( (uint8_t)( ( rgb.g * scale ) >> 8 ) );
        out[2]        = gamma_.apply( (uint8_t)( ( rgb.b * scale ) >> 8 ) );
    }

    namespace
    {
        LedLoad loadOf( const RenderedFrame& f )
        {
            LedLoad load;
            for( uint8_t d : f.buttons )
                load.buttonDuty += d;
            for( const auto& pad : f.pads )
                load.padDuty += (uint32_t)pad[0] + pad[1] + pad[2];
            for( uint8_t d : f.strip )
                load.stripDuty += d;
            return load;
        }
    } // namespace

    void LedRefresh::render( const LedFrame& frame, bool blinkOn, RenderedFrame& out, RenderInfo* info ) const
    {
        for( unsigned i = 0; i < kButtonLeds; ++i )
        {
            const uint8_t s   = frame.buttons[i].state;
            const bool    lit = s == STATE_ON || s == STATE_SAMPLE_ASSIGNED || ( s == STATE_ON_BLINK && blinkOn ) || ( s == STATE_OFF_BLINK && !blinkOn );
            out.buttons[i]    = lit ? config_.buttonDuty : 0;
        }

        for( unsigned i = 0; i < kPadLeds; ++i )
            renderPad( frame.pads[i], blinkOn, out.pads[i] );

        const StripLeds& strip = frame.strip;
        if( strip.mode == StripMode::Individual )
        {
            for( unsigned i = 0; i < kStripLedCount; ++i )
                out.strip[i] = (uint8_t)( strip.individual[i] * config_.stripDuty / 100 );
        }
        else
        {
            const unsigned lit = ( strip.percent * kStripLedCount + 50 ) / 100; // round to nearest, like the Surface
            for( unsigned i = 0; i < kStripLedCount; ++i )
                out.strip[i] = i < lit ? config_.stripDuty : 0;
        }

        const LedLoad  demand = loadOf( out );
        const uint16_t scale  = scaleQ8( config_.current, demand );
        if( scale < 256 )
        {
            for( uint8_t& d : out.buttons )
                d = (uint8_t)( ( d * scale ) >> 8 );
            for( auto& pad : out.pads )
                for( uint8_t& d : pad )
                    d = (uint8_t)( ( d * scale ) >> 8 );
            for( uint8_t& d : out.strip )
                d = (uint8_t)( ( d * scale ) >> 8 );
        }

        if( info != nullptr )
        {
            info->demandMa = estimateMa( config_.current, demand );
            info->outputMa = estimateMa( config_.current, loadOf( out ) );
            info->scaleQ8  = scale;
        }
    }

    void LedRefresh::drawNow()
    {
        blink_.update();

        RenderedFrame next;
        RenderInfo    info;
        render( state_.front(), blink_.on(), next, &info );

        ++stats_.draws;
        lastDrawMs_     = clock_.nowMs();
        everDrawn_      = true;
        stats_.demandMa = info.demandMa;
        stats_.outputMa = info.outputMa;
        stats_.scaleQ8  = info.scaleQ8;
        if( info.scaleQ8 < 256 )
            ++stats_.limited;

        if( stats_.pushes != 0 && next == last_ )
            return;
        last_ = next;
        ++stats_.pushes;
        for( unsigned i = 0; i < kButtonLeds; ++i )
            driver_.setButton( i, next.buttons[i] );
        for( unsigned i = 0; i < kPadLeds; ++i )
            driver_.setPad( i, next.pads[i][0], next.pads[i][1], next.pads[i][2] );
        for( unsigned i = 0; i < kStripLedCount; ++i )
            driver_.setStrip( i, next.strip[i] );
        driver_.commit();
    }

    void LedRefresh::service()
    {
        if( everDrawn_ && (uint32_t)( clock_.nowMs() - lastDrawMs_ ) < config_.refreshIntervalMs )
            return;
        drawNow();
    }

} // namespace output
} // namespace kick_mcu
