// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The simulated serial line to the Kick host: a pty, or a TCP listener. Same two options and the
// same files the emulator Surface uses, so emu/run.sh and Kick connect to either one the same way.
// POSIX only.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace kick_mcu
{
namespace sim
{
    class SerialTransport
    {
    public:
        virtual ~SerialTransport() = default;

        /// Opens a pty, links it at `<root>/dev/ttymxc0` and prints KICK_SERIAL_PORT=<path> to
        /// stdout. Returns null with `error` set on failure.
        static std::unique_ptr<SerialTransport> openPty( const std::string& root, std::string& error );

        /// Listens on 127.0.0.1:`port`, writes `tcp://127.0.0.1:<port>` to `<root>/surface.port`
        /// and prints KICK_SERIAL_TCP=<port>. One client at a time.
        static std::unique_ptr<SerialTransport> listenTcp( const std::string& root, int port, std::string& error );

        /// Accepts a waiting client, if any. Call every loop. Returns true on the call that
        /// accepted a new one, so the owner can drop output queued for the old one.
        virtual bool service() = 0;

        /// Reads whatever has arrived, without blocking. Returns the byte count (0 for none).
        virtual size_t read( uint8_t* buffer, size_t capacity ) = 0;

        /// Writes bytes without blocking. What the peer cannot take yet is kept and retried by
        /// service(), in order. Returns false if the output backlog is over its cap and the
        /// bytes were dropped.
        virtual bool write( const uint8_t* data, size_t length ) = 0;

        virtual bool connected() const = 0;
    };

} // namespace sim
} // namespace kick_mcu
