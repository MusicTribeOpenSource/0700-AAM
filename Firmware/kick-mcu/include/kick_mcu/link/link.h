// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The whole transport: UART bytes in, frames out, handshake in between.
#pragma once

#include <atomic>
#include <cstdint>

#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/link/rx_parser.h"
#include "kick_mcu/link/session.h"
#include "kick_mcu/link/spsc_ring.h"
#include "kick_mcu/link/tx_queue.h"

namespace kick_mcu
{
namespace link
{
    struct LinkConfig
    {
        SessionConfig session{};

        /// A partial frame is abandoned when no byte has arrived for this long. A host frame is a
        /// single burst of at most a few milliseconds at 230400 baud, so a gap this long means the
        /// rest is not coming.
        uint32_t interByteTimeoutMs{ 10 };
    };

    struct LinkStats
    {
        uint32_t rxOverruns{ 0 };         ///< bytes lost because the receive ring was full
        uint32_t droppedNotStreaming{ 0 }; ///< events refused before the handshake finished
    };

    /// Owns the receive ring, parser, transmit queue and session, and wires them to a UART.
    ///
    /// Two contexts touch it. The UART receive interrupt calls only the byte handler this class
    /// installs, which pushes into a lock-free ring. Everything else, poll() and send(), runs on
    /// the main loop. poll() never waits: it drains the ring, feeds the parser, runs the
    /// heartbeat and pumps the transmit queue, then returns, so a scan loop can call it every pass.
    ///
    /// The ring holds 255 bytes, about 11 ms of line at 230400 baud (23 040 bytes/s), so poll()
    /// has to run at least that often or bytes are lost and counted in rxOverruns.
    class Link
    {
    public:
        Link( bsp::IUart& uart, const bsp::IClock& clock, const LinkConfig& config = {} );
        ~Link();

        Link( const Link& )            = delete;
        Link& operator=( const Link& ) = delete;

        void poll();

        /// Queues an event for the host. Events (everything but SYSTEM) are refused until the
        /// session is Streaming, since the host is not listening for them before then. Returns
        /// false when refused, rejected or dropped; the stats say which.
        bool send( Lane lane, uint8_t type, uint8_t cmd, const uint8_t* payload, size_t length );

        /// Frames from the host that the session did not consume (LED, pad, slider, BPM, update).
        void setPassThrough( Session::PassThroughHandler handler, void* context ) { session_.setPassThrough( handler, context ); }

        Session&       session() { return session_; }
        TxQueue&       tx() { return tx_; }
        const RxParser& parser() const { return parser_; }
        LinkStats      stats() const;

    private:
        static void onUartByte( void* context, uint8_t byte );
        static void onParsedFrame( void* context, const Frame& frame );

        bsp::IUart&         uart_;
        const bsp::IClock&  clock_;
        LinkConfig          config_;
        SpscByteRing<256>   rxRing_;
        std::atomic<uint32_t> rxOverruns_{ 0 };
        RxParser            parser_;
        TxQueue             tx_;
        Session             session_;
        uint32_t            lastByteMs_{ 0 };
        uint32_t            droppedNotStreaming_{ 0 };
    };

} // namespace link
} // namespace kick_mcu
