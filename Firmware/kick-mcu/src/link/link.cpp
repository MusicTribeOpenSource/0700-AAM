// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/link/link.h"

namespace kick_mcu
{
namespace link
{
    Link::Link( bsp::IUart& uart, const bsp::IClock& clock, const LinkConfig& config )
        : uart_( uart )
        , clock_( clock )
        , config_( config )
        , session_( tx_, config.session )
    {
        parser_.setHandler( &Link::onParsedFrame, this );
        uart_.onByte( &Link::onUartByte, this );
        lastByteMs_ = clock_.nowMs();
    }

    Link::~Link()
    {
        uart_.onByte( nullptr, nullptr );
    }

    void Link::onUartByte( void* context, uint8_t byte )
    {
        Link* self = static_cast<Link*>( context );
        if( !self->rxRing_.push( byte ) )
            self->rxOverruns_.fetch_add( 1, std::memory_order_relaxed );
    }

    void Link::onParsedFrame( void* context, const Frame& frame )
    {
        Link* self = static_cast<Link*>( context );
        self->session_.onFrame( frame, self->clock_.nowMs() );
    }

    void Link::poll()
    {
        const uint32_t now = clock_.nowMs();

        uint8_t byte;
        bool    gotBytes = false;
        while( rxRing_.pop( byte ) )
        {
            gotBytes = true;
            parser_.feed( byte );
        }

        if( gotBytes )
            lastByteMs_ = now;
        else if( parser_.inFrame() && (uint32_t)( now - lastByteMs_ ) > config_.interByteTimeoutMs )
            parser_.expire();

        session_.poll( now );
        tx_.pump( uart_ );
    }

    bool Link::send( Lane lane, uint8_t type, uint8_t cmd, const uint8_t* payload, size_t length )
    {
        if( type != contract::PKT_SYSTEM && !session_.streaming() )
        {
            ++droppedNotStreaming_;
            return false;
        }
        return tx_.enqueue( lane, type, contract::DIR_REQUEST, cmd, payload, length );
    }

    LinkStats Link::stats() const
    {
        LinkStats s;
        s.rxOverruns          = rxOverruns_.load( std::memory_order_relaxed );
        s.droppedNotStreaming = droppedNotStreaming_;
        return s;
    }

} // namespace link
} // namespace kick_mcu
