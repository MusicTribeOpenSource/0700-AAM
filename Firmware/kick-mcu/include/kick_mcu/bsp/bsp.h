// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Board support interfaces. Everything above this line of the firmware (link, input, output,
// system, app) talks to hardware only through these, so binding to the real MCU, once the board
// exists, means writing one `bsp/<mcu>/` directory and touching nothing else.
//
// No vendor headers and no heap. Callbacks are a function pointer plus a context pointer rather
// than std::function, because std::function allocates.
//
// Only IUart and IClock are used by the transport layer (this PR). The scanner, LED, flash and
// power interfaces are declared here so the later input, LED and system work shares one BSP
// header; their shapes are a first cut and may change when those layers are written.
#pragma once

#include <cstddef>
#include <cstdint>

namespace kick_mcu
{
namespace bsp
{
    struct ByteSpan
    {
        const uint8_t* data{ nullptr };
        size_t         size{ 0 };
    };

    /// Called once per received byte. On hardware this runs in the UART receive interrupt (or the
    /// DMA half-transfer handler), so it must do nothing but store the byte.
    using ByteHandler = void ( * )( void* context, uint8_t byte );

    class IUart
    {
    public:
        virtual ~IUart() = default;

        /// Queues bytes for transmission and returns how many were accepted. It never blocks.
        /// The transmitter only calls it after checking that writableBytes() covers the whole
        /// frame, so a frame always goes out as one contiguous burst.
        virtual size_t write( ByteSpan bytes ) = 0;

        /// Free space in the transmit FIFO or DMA buffer, in bytes.
        virtual size_t writableBytes() const = 0;

        /// Installs the receive handler. One handler at a time; a second call replaces the first.
        virtual void onByte( ByteHandler handler, void* context ) = 0;
    };

    class IClock
    {
    public:
        virtual ~IClock() = default;

        /// Monotonic milliseconds since an arbitrary start. Wraps at 2^32, so compare with
        /// subtraction (`now - then`), never with `<`.
        virtual uint32_t nowMs() const = 0;

        /// Monotonic microseconds, same wrap rule.
        virtual uint32_t nowUs() const = 0;
    };

    /// Button matrix. A bit per key, bit n set while key n (button codes 0-52) is down.
    class IMatrixScanner
    {
    public:
        virtual ~IMatrixScanner() = default;
        virtual uint64_t scan() = 0;
    };

    /// Pad sensors. 16 raw 12-bit pressure readings, 0 = no touch.
    class IPadSensor
    {
    public:
        virtual ~IPadSensor() = default;
        virtual void sample( uint16_t ( &pressure )[16] ) = 0;
    };

    /// Quadrature encoders. Two bits (A, B) per encoder, encoder n in bits 2n and 2n+1.
    /// Encoder 0 is the jog wheel, 1 to 16 are the Q-Link knobs.
    class IEncoderBank
    {
    public:
        virtual ~IEncoderBank() = default;
        virtual uint64_t readPhases() = 0;
    };

    /// Touch strip. Returns true while touched and sets position to 0-1023 from the bottom.
    class IStrip
    {
    public:
        virtual ~IStrip() = default;
        virtual bool read( uint16_t& position ) = 0;
    };

    /// LED outputs. Values are staged and become visible on commit().
    class ILedDriver
    {
    public:
        virtual ~ILedDriver() = default;
        virtual void setButton( unsigned index, uint8_t brightness ) = 0;
        virtual void setPad( unsigned index, uint8_t red, uint8_t green, uint8_t blue ) = 0;
        virtual void setStrip( unsigned index, uint8_t brightness ) = 0;
        virtual void commit() = 0;
    };

    /// Flash for the update path. Erase works on whole blocks, write only clears bits.
    class IFlash
    {
    public:
        virtual ~IFlash() = default;
        virtual size_t sizeBytes() const = 0;
        virtual size_t eraseBlockBytes() const = 0;
        virtual bool   read( size_t address, uint8_t* out, size_t length ) = 0;
        virtual bool   write( size_t address, const uint8_t* data, size_t length ) = 0;
        virtual bool   eraseBlock( size_t address ) = 0;
    };

    class IPower
    {
    public:
        virtual ~IPower() = default;
        /// Drops the power rail. Does not return on hardware.
        virtual void requestOff() = 0;
        /// Resets the MCU. Does not return on hardware.
        virtual void requestReset() = 0;
    };

} // namespace bsp
} // namespace kick_mcu
