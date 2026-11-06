// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/app/firmware.h"

#include "kick_mcu/contract.h"

namespace kick_mcu
{
namespace app
{
    Firmware::Firmware( bsp::IUart& uart, const bsp::IClock& clock, InputHardware hardware, OutputHardware outputs, const FirmwareConfig& config )
        : Firmware( uart, clock, hardware, outputs.leds, config )
    {
    }

    Firmware::Firmware( bsp::IUart& uart, const bsp::IClock& clock, InputHardware hardware, const FirmwareConfig& config )
        : Firmware( uart, clock, hardware, static_cast<bsp::ILedDriver&>( nullLeds_ ), config )
    {
    }

    namespace
    {
        /// The handshake's GET_SW_VER carries the version of the image that is running.
        link::LinkConfig withVersion( link::LinkConfig link, const system::VersionNo& version )
        {
            link.session.versionMajor = version.major;
            link.session.versionMinor = version.minor;
            return link;
        }
    } // namespace

    Firmware::Firmware( bsp::IUart& uart, const bsp::IClock& clock, InputHardware hardware, bsp::ILedDriver& leds, const FirmwareConfig& config )
        : selfCheck_( system::runSelfCheck( config.system.flash, config.system.layout, config.system.boot, config.system.buildVersion ) )
        , link_( uart, clock, withVersion( config.link, selfCheck_.version ) )
        , sink_( link_ )
        , scan_( hardware, sink_, clock, config.scan )
        , commands_( ledState_ )
        , blink_( clock, config.blinkPeriodMs )
        , refresh_( leds, ledState_, blink_, clock, config.render )
        , system_( config.system, selfCheck_, link_.tx(), clock, &Firmware::quiesce, this )
        , blinkFollowsBpm_( config.blinkFollowsBpm )
    {
        sink_.setMute( &system_.quiescedFlag() );
        link_.setPassThrough( &Firmware::onHostFrame, this );
    }

    void Firmware::quiesce( void* context )
    {
        Firmware* self = static_cast<Firmware*>( context );
        self->ledState_.reset();
        self->refresh_.drawNow();
    }

    void Firmware::onHostFrame( void* context, const link::Frame& frame )
    {
        Firmware* self = static_cast<Firmware*>( context );
        if( self->system_.handle( frame ) )
            return;
        if( self->system_.quiesced() && ( frame.type == contract::PKT_LED || frame.type == contract::PKT_PAD || frame.type == contract::PKT_SLIDER ) )
            return; // the unit was told to shut down; it stays dark
        if( frame.type == contract::PKT_PAD && frame.cmd == contract::PAD_SET_AFTER_TOUCH && frame.length >= 1 )
        {
            self->scan_.pads().setAftertouchEnabled( frame.payload[0] != 0 );
            return;
        }
        if( self->commands_.handle( frame ) != output::CommandResult::NotMine )
            return; // applied, or rejected and counted in commands().stats()
        ++self->hostFramesIgnored_;
    }

    void Firmware::step()
    {
        scan_.tick();
        link_.poll();
        system_.poll( link_.session().streaming() );
        if( blinkFollowsBpm_ && system_.bpm().valid() && system_.bpm().raw() != lastBpmApplied_ )
        {
            lastBpmApplied_ = system_.bpm().raw();
            blink_.setPeriodMs( system_.bpm().beatPeriodMs() );
        }
        refresh_.service();
    }

} // namespace app
} // namespace kick_mcu
