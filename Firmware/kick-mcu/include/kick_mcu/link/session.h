// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Connection state and the SYSTEM handshake.
#pragma once

#include <cstdint>

#include "kick_mcu/link/frame_codec.h"
#include "kick_mcu/link/tx_queue.h"

namespace kick_mcu
{
namespace link
{
    enum class SessionState : uint8_t
    {
        NotConnected, ///< waiting for the host to open the line
        Connected,    ///< host has said COMM_INIT [1]; version and status [1] sent, waiting for it to confirm
        Streaming,    ///< handshake done; events flow and the heartbeat runs
    };

    struct SessionConfig
    {
        uint8_t  versionMajor{ 1 };
        uint8_t  versionMinor{ 0 };
        uint32_t heartbeatMs{ 3000 };
    };

    struct SessionStats
    {
        uint32_t handshakes{ 0 };       ///< COMM_INIT [1] received
        uint32_t heartbeats{ 0 };       ///< heartbeat frames queued
        uint32_t reconnects{ 0 };       ///< RECONNECT or COMM_INIT [0] received
        uint32_t ignoredResponses{ 0 }; ///< frames with dir = response; the MCU never asks for one
        uint32_t unexpected{ 0 };       ///< handshake frames that make no sense in the current state
        uint32_t passedThrough{ 0 };    ///< frames given to the pass-through handler
        uint32_t unhandled{ 0 };        ///< frames nobody consumed
    };

    /// Handshake as the host drives it (host side read from input_events.cpp and checked against
    /// the emulator Surface, which the unmodified host talks to):
    ///
    ///   host -> COMM_INIT [1]
    ///   MCU  -> GET_SW_VER [major, minor]
    ///   MCU  -> CHECK_COMM_STATUS [1]
    ///   host -> COMM_INIT [2]
    ///   MCU  -> CHECK_COMM_STATUS [2]
    ///   host -> CHECK_COMM_STATUS [2]          state is now Streaming
    ///   MCU  -> CHECK_COMM_STATUS [2]          every heartbeatMs from here on
    ///
    /// COMM_INIT [1] restarts the sequence from any state, and drops event frames still queued
    /// from the previous session. RECONNECT, which the host defines but never sends, goes back to
    /// NotConnected; the MCU does not start a handshake of its own, because the host is the one
    /// that opens the line.
    ///
    /// There is no link-loss timeout. The host defines none, and the heartbeat is not known to be
    /// answered every time, so the MCU keeps streaming until told otherwise.
    ///
    /// Anything this class does not consume goes to the pass-through handler, which later layers
    /// use for LED, pad, slider, BPM, shutdown and update commands.
    class Session
    {
    public:
        using PassThroughHandler = void ( * )( void* context, const Frame& frame );

        Session( TxQueue& tx, const SessionConfig& config )
            : tx_( tx )
            , config_( config )
        {
        }

        void setPassThrough( PassThroughHandler handler, void* context )
        {
            passThrough_        = handler;
            passThroughContext_ = context;
        }

        /// Handles one frame from the host. `nowMs` is the monotonic clock.
        void onFrame( const Frame& frame, uint32_t nowMs );

        /// Runs the heartbeat. Call from the main loop.
        void poll( uint32_t nowMs );

        SessionState         state() const { return state_; }
        bool                 streaming() const { return state_ == SessionState::Streaming; }
        const SessionStats&  stats() const { return stats_; }
        const SessionConfig& config() const { return config_; }

    private:
        void onSystem( const Frame& frame, uint32_t nowMs );
        void sendStatus( uint8_t stage );
        void dropEventLanes();
        void disconnect();
        void pass( const Frame& frame );

        TxQueue&           tx_;
        SessionConfig      config_;
        SessionState       state_{ SessionState::NotConnected };
        uint32_t           lastHeartbeatMs_{ 0 };
        PassThroughHandler passThrough_{ nullptr };
        void*              passThroughContext_{ nullptr };
        SessionStats       stats_{};
    };

} // namespace link
} // namespace kick_mcu
