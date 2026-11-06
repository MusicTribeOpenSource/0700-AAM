// SPDX-License-Identifier: AGPL-3.0-or-later
//
// A window onto the host's real firmware updater (Kick/modules/hardware/source/kick_FirmwareUpdate.cpp),
// compiled unmodified. This header has no host, JUCE or gtest includes, so the test file that uses
// it stays clean of the host's macros (FAIL, DATA and friends collide with gtest).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace host_updater
{
using Bytes = std::vector<uint8_t>;

/// What the host reported through its swupdateStatus callback, in order.
struct StatusEvent
{
    int    status;     ///< mt::RECOVERY_STATUS as an int
    double totalPercent;
};

class HostShim
{
public:
    /// Hooks the host's DclSignals so that what the updater emits becomes wire bytes.
    HostShim();
    ~HostShim();

    HostShim( const HostShim& )            = delete;
    HostShim& operator=( const HostShim& ) = delete;

    /// Called from the host's worker thread with one whole frame (no trailer), the way the
    /// transport would write it. Must be thread safe.
    void setTransmit( std::function<void( const Bytes& )> transmit );

    /// Feeds bytes the MCU sent. They are split into frames, decoded by the host's own PktMgr and
    /// SYSTEM / SW_UPDATE frames go to the same logic as SWUpdateScreen::handleCommand. Call from
    /// one thread at a time.
    void onMcuBytes( const Bytes& bytes );

    /// Runs FirmwareUpdate::MCUUpdate( hexPath ) on its own thread, as updateFirmware() does.
    void start( const std::string& hexPath );

    bool finished() const;

    /// MCUUpdate's return value: 1 for success, -1 for failure. Only valid once finished().
    int result() const;

    /// The host has no timeout: if the MCU goes quiet, MCUUpdate spins forever. This stands in for
    /// an operator who gives up, by putting the host's own state machine in its FAILURE state.
    void giveUp();

    /// Waits for the worker thread. Call after finished().
    void join();

    std::vector<StatusEvent> statusEvents() const;

    /// Frames the host sent to the MCU, decoded: type, command, data (as the host's ints).
    struct SentFrame
    {
        int              type;
        int              cmd;
        std::vector<int> data;
    };
    std::vector<SentFrame> sentFrames() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace host_updater
