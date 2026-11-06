// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The scan loop: samples every input once per tick and hands events to the transport.
#pragma once

#include <cstdint>

#include "kick_mcu/bsp/bsp.h"
#include "kick_mcu/input/debounce.h"
#include "kick_mcu/input/encoders.h"
#include "kick_mcu/input/event_sink.h"
#include "kick_mcu/input/pads.h"
#include "kick_mcu/input/strip.h"

namespace kick_mcu
{
namespace app
{
    struct ScanConfig
    {
        input::DebounceConfig debounce{};
        input::PadConfig      pads{};
        input::EncoderConfig  encoders{};
        input::StripConfig    strip{};
    };

    /// The board's input drivers, borrowed. Everything pin- and part-specific is behind these.
    struct InputHardware
    {
        bsp::IMatrixScanner& matrix;
        bsp::IPadSensor&     pads;
        bsp::IEncoderBank&   encoders;
        bsp::IStrip&         strip;
    };

    struct ScanStats
    {
        uint32_t ticks{ 0 };
        uint32_t buttonFrames{ 0 };  ///< button press and release frames queued
        uint32_t buttonRetries{ 0 }; ///< ticks a button frame was refused and kept for the next tick
    };

    /// One call to tick() reads every input once and enqueues what changed. It never waits on
    /// the UART: the only thing it does with the transport is put frames in the transmit queue,
    /// and the main loop's Link::poll() sends them.
    ///
    /// Buttons: the debounced state is compared with what the host was last told. A key that
    /// differs gets a frame, SHIFT first and then in code order. If the queue refuses a frame
    /// the difference stays, so the frame is retried every tick until it is accepted: a lost
    /// SHIFT release would leave the host in shifted mode. A press and release that both
    /// happen while the queue is refusing cancel out, which leaves the host's view correct.
    ///
    /// While the link is not up (IEventSink::accepting() is false) every input is still read
    /// and tracked, but its events are discarded. The host is not listening, and when the link
    /// comes up it should see only what happens from then on. A key or pad held at that moment
    /// is not announced, and its release is not sent either, so the host never sees half a press.
    class ScanLoop
    {
    public:
        ScanLoop( InputHardware hardware, input::IEventSink& sink, const bsp::IClock& clock, const ScanConfig& config = {} );

        void tick();

        input::Debouncer&      debouncer() { return debouncer_; }
        input::PadScanner&     pads() { return pads_; }
        input::EncoderBank&    encoders() { return encoders_; }
        input::StripScanner&   strip() { return strip_; }
        const ScanStats&       stats() const { return stats_; }

    private:
        void reportButtons( uint64_t stable );

        InputHardware       hw_;
        input::IEventSink&  sink_;
        const bsp::IClock&  clock_;
        input::Debouncer    debouncer_;
        input::PadScanner   pads_;
        input::EncoderBank  encoders_;
        input::StripScanner strip_;
        uint64_t            reported_{ 0 };        ///< what the host believes is held
        uint64_t            heldBeforeLink_{ 0 };  ///< keys already down when the link came up; their release is not sent
        ScanStats           stats_{};
    };

} // namespace app
} // namespace kick_mcu
