// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Plays a script of fake hardware events into a SimBoard over time.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "kick_mcu/app/scan_loop.h"
#include "kick_mcu/output/led_state.h"
#include "sim_board.h"

namespace kick_mcu
{
namespace sim
{
    /// What the script can ask of the world around the board.
    struct ScriptHooks
    {
        /// True once the host handshake is done.
        std::function<bool()> streaming;
        /// Turns pad aftertouch on or off, as the host's PAD_SET_AFTER_TOUCH does.
        std::function<void( bool )> setAftertouch;
        /// The host's log file as text so far; empty if there is none. Used by `expect host`.
        std::function<std::string()> hostLog;
        /// The firmware's current LED table. Used by `expect led` and `expect pad`.
        std::function<const output::LedFrame&()> ledTable;
    };

    /// A script is one command per line, `#` starts a comment. Commands:
    ///
    ///   wait <ms>
    ///   press <button> [holdMs]            button is a name ("Track Mix", "+", "Shift") or a code 0-54; hold defaults to 60
    ///   pad <1-16> [velocity] [holdMs] [aftertouch]
    ///                                      a strike that reads as `velocity` (1-127, default 100) through the linear
    ///                                      curve, held for holdMs (default 80), settling to `aftertouch` (0-127,
    ///                                      default 0) after 2 ms
    ///   turn <0|1-16> <steps> [intervalUs] steps > 0 clockwise; 0 is the jog wheel; one quadrature transition every
    ///                                      intervalUs (default 1000), transitionsPerStep transitions per step
    ///   strip <1-100> [holdMs]             touch at that position for holdMs (default 60), then lift
    ///   aftertouch on|off
    ///   expect streaming [timeoutMs]       wait for the host handshake (default 60000); `expect connected` is the same
    ///   mark-host                          remember how much of the host log exists now
    ///   expect host "<text>" [count] [timeoutMs]
    ///                                      wait until the host log, after the last mark, holds <text> on at least
    ///                                      `count` (default 1) lines; default timeout 3000
    ///   expect led <button> on|off [timeoutMs]
    ///                                      wait until that button's LED state is not OFF (on) or is OFF (off); the
    ///                                      button is a name or code as for `press`, unquoted names may hold spaces;
    ///                                      default timeout 3000. Same meaning as the Surface's `expect led`.
    ///   expect pad <1-16> <colourId|off> [timeoutMs]
    ///                                      wait until the pad has a state other than OFF and that PadColourIds value,
    ///                                      or (off) is OFF or has colour 15 (no colour); default timeout 3000
    ///   quit [code]
    ///
    /// Aliases for scripts written for the emulator Surface: button = press, knob = turn, slider = strip.
    ///
    /// A key release is debounced for longer than a press (4 ms against 1 ms by default), so a `press`
    /// that follows another one immediately can reach the host before the first one's release. Put a
    /// `wait` between them when the order matters.
    ///
    /// Time is whatever the caller passes to advance(), in microseconds, so tests run it on a virtual
    /// clock and the simulator on the real one. A command's events are scheduled from the moment it
    /// starts; the next command starts when the last event of the previous one is done.
    class ScriptPlayer
    {
    public:
        enum class State : uint8_t
        {
            Running,
            Done,   ///< every command ran
            Failed, ///< an expect timed out; see failure()
        };

        ScriptPlayer( SimBoard& board, const app::ScanConfig& config, ScriptHooks hooks );

        /// Parses the script. Returns false and sets error() with the line number on the first bad line,
        /// before anything has run, so a typo does not surface minutes into a run.
        bool load( const std::string& text );

        State advance( uint64_t nowUs );

        State              state() const { return state_; }
        int                exitCode() const { return exitCode_; }
        const std::string& error() const { return error_; }     ///< from load()
        const std::string& failure() const { return failure_; } ///< from advance()

    private:
        enum class Op : uint8_t
        {
            KeyDown,
            KeyUp,
            PadRaw,
            EncoderStep,
            StripTouch,
            StripLift,
        };

        struct Step
        {
            uint64_t atUs; ///< relative to the start of the command
            Op       op;
            int      a;
            int      b;
        };

        enum class Kind : uint8_t
        {
            Wait,
            Scheduled, ///< press, pad, turn, strip: a list of timed steps
            ExpectStreaming,
            MarkHost,
            ExpectHost,
            ExpectLed,
            ExpectPad,
            Aftertouch,
            Quit,
        };

        struct Command
        {
            Kind              kind{ Kind::Wait };
            int               line{ 0 };
            std::string       text;
            uint64_t          durationUs{ 0 }; ///< wait length, or how long a scheduled command lasts
            int               count{ 1 };
            int               value{ 0 };      ///< quit code, or aftertouch on/off, or the button code or pad index of an expect
            int               want{ 0 };       ///< expect led: 1 for on, 0 for off. expect pad: a colour id, or -1 for off
            std::vector<Step> steps;
            std::string       source;
        };

        bool parseLine( int lineNo, const std::string& line );
        bool fail( int lineNo, const std::string& why );
        void apply( const Step& step );
        void begin( uint64_t nowUs );
        void fault( const std::string& why );

        SimBoard&         board_;
        app::ScanConfig   config_;
        ScriptHooks       hooks_;
        std::vector<Command> commands_;
        size_t            index_{ 0 };
        size_t            nextStep_{ 0 };
        uint64_t          startedUs_{ 0 };
        bool              started_{ false };
        size_t            hostMark_{ 0 };
        State             state_{ State::Running };
        int               exitCode_{ 0 };
        std::string       error_;
        std::string       failure_;
    };

} // namespace sim
} // namespace kick_mcu
