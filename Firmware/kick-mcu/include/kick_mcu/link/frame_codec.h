// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Frame layout, CRC-8 and the encoder.
#pragma once

#include <cstddef>
#include <cstdint>

#include "kick_mcu/contract.h"

namespace kick_mcu
{
namespace link
{
    /// Folds one byte into a running CRC-8 (polynomial 0x31, no reflection, no final xor).
    /// Start from contract::kCrcInit.
    uint8_t crc8Update( uint8_t crc, uint8_t byte );

    uint8_t crc8( const uint8_t* data, size_t length );

    /// A decoded frame. `length` is the payload length, which is the wire `size` byte minus one.
    struct Frame
    {
        uint8_t type{ 0 };
        uint8_t dir{ 0 };
        uint8_t cmd{ 0 };
        uint8_t length{ 0 };
        uint8_t payload[contract::kMaxPayloadFromHost]{};
    };

    /// Which way a frame is going. The two directions differ in two ways, both forced by the host:
    ///
    ///  - MCU to host frames carry one extra byte after the CRC (contract::kMcuToHostTrailer),
    ///    because the host consumes size + 7 bytes per frame and discards the last one unread.
    ///    Without it the host swallows the first byte of the next frame.
    ///  - MCU to host payloads are limited to 19 bytes, one under the host's 20-byte RX buffer.
    enum class Wire
    {
        McuToHost,
        HostToMcu,
    };

    /// Largest encoded frame the MCU itself ever sends: header, 19 payload bytes, CRC, trailer.
    constexpr size_t kMaxEncodedToHost = contract::kHeaderBytes + contract::kMaxPayloadToHost + contract::kCrcBytes + 1;

    /// Total encoded size for a payload length, or 0 when the payload is too long for the wire.
    size_t encodedSize( Wire wire, size_t payloadLength );

    /// Writes one frame into `out` and returns its length in bytes, or 0 when `capacity` is too
    /// small or the payload is over the limit for `wire`. `payload` may be null when length is 0.
    size_t encodeFrame( uint8_t* out, size_t capacity, Wire wire, uint8_t type, uint8_t dir, uint8_t cmd, const uint8_t* payload, size_t length );

} // namespace link
} // namespace kick_mcu
