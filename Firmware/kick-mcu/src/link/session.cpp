// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_mcu/link/session.h"

namespace kick_mcu
{
namespace link
{
    using namespace contract;

    void Session::sendStatus( uint8_t stage )
    {
        const uint8_t payload[1] = { stage };
        tx_.enqueue( Lane::System, PKT_SYSTEM, DIR_REQUEST, SYS_CHECK_COMM_STATUS, payload, 1 );
    }

    void Session::dropEventLanes()
    {
        tx_.clear( Lane::Button );
        tx_.clear( Lane::PadKnob );
        tx_.clear( Lane::Aftertouch );
    }

    void Session::disconnect()
    {
        state_ = SessionState::NotConnected;
        dropEventLanes();
    }

    void Session::pass( const Frame& frame )
    {
        if( passThrough_ != nullptr )
        {
            ++stats_.passedThrough;
            passThrough_( passThroughContext_, frame );
        }
        else
            ++stats_.unhandled;
    }

    void Session::onFrame( const Frame& frame, uint32_t nowMs )
    {
        if( frame.dir == DIR_RESPONSE )
        {
            ++stats_.ignoredResponses;
            return;
        }

        if( frame.type == PKT_SYSTEM )
            onSystem( frame, nowMs );
        else
            pass( frame );
    }

    void Session::onSystem( const Frame& frame, uint32_t nowMs )
    {
        const uint8_t arg = frame.length > 0 ? frame.payload[0] : 0xFF;

        switch( frame.cmd )
        {
            case SYS_COMM_INIT:
                if( arg == COMM_CONNECTED )
                {
                    ++stats_.handshakes;
                    dropEventLanes();
                    state_ = SessionState::Connected;
                    const uint8_t version[2] = { config_.versionMajor, config_.versionMinor };
                    tx_.enqueue( Lane::System, PKT_SYSTEM, DIR_REQUEST, SYS_GET_SW_VER, version, 2 );
                    sendStatus( COMM_CONNECTED );
                }
                else if( arg == kCommStageLive )
                {
                    if( state_ == SessionState::NotConnected )
                        state_ = SessionState::Connected;
                    sendStatus( kCommStageLive );
                }
                else if( arg == COMM_NOT_CONNECTED )
                {
                    ++stats_.reconnects;
                    disconnect();
                }
                else
                    ++stats_.unexpected;
                break;

            case SYS_CHECK_COMM_STATUS:
                if( arg == kCommStageLive && state_ != SessionState::NotConnected )
                {
                    if( state_ == SessionState::Connected )
                        lastHeartbeatMs_ = nowMs;
                    state_ = SessionState::Streaming;
                }
                else
                    ++stats_.unexpected;
                break;

            case SYS_RECONNECT:
                ++stats_.reconnects;
                disconnect();
                break;

            default:
                pass( frame );
                break;
        }
    }

    void Session::poll( uint32_t nowMs )
    {
        if( state_ != SessionState::Streaming )
            return;
        if( (uint32_t)( nowMs - lastHeartbeatMs_ ) < config_.heartbeatMs )
            return;
        lastHeartbeatMs_ = nowMs;
        ++stats_.heartbeats;
        sendStatus( kCommStageLive );
    }

} // namespace link
} // namespace kick_mcu
