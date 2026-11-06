// SPDX-License-Identifier: AGPL-3.0-or-later
#include "script_player.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include "button_names.h"
#include "kick_mcu/contract.h"
#include "kick_mcu/input/strip.h"

namespace kick_mcu
{
namespace sim
{
    namespace
    {
        // Splits on whitespace; text inside double quotes is one token with the quotes removed.
        std::vector<std::string> tokenize( const std::string& line )
        {
            std::vector<std::string> out;
            std::string              cur;
            bool                     inQuote = false, have = false;
            for( char c : line )
            {
                if( c == '"' )
                {
                    inQuote = !inQuote;
                    have    = true;
                }
                else if( !inQuote && std::isspace( (unsigned char)c ) )
                {
                    if( have )
                        out.push_back( cur );
                    cur.clear();
                    have = false;
                }
                else
                {
                    cur += c;
                    have = true;
                }
            }
            if( have )
                out.push_back( cur );
            return out;
        }

        bool toInt( const std::string& s, long& out )
        {
            if( s.empty() )
                return false;
            char* end = nullptr;
            out       = std::strtol( s.c_str(), &end, 10 );
            return end != nullptr && *end == '\0';
        }

        std::string lower( std::string s )
        {
            std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) { return (char)std::tolower( c ); } );
            return s;
        }

        uint64_t ms( long v ) { return (uint64_t)( v < 0 ? 0 : v ) * 1000u; }

        uint32_t ceilDiv( uint32_t a, uint32_t b ) { return ( a + b - 1 ) / b; }
    } // namespace

    ScriptPlayer::ScriptPlayer( SimBoard& board, const app::ScanConfig& config, ScriptHooks hooks )
        : board_( board )
        , config_( config )
        , hooks_( std::move( hooks ) )
    {
    }

    bool ScriptPlayer::fail( int lineNo, const std::string& why )
    {
        error_ = "script line " + std::to_string( lineNo ) + ": " + why;
        return false;
    }

    bool ScriptPlayer::load( const std::string& text )
    {
        commands_.clear();
        error_.clear();
        std::istringstream in( text );
        std::string        line;
        int                lineNo = 0;
        while( std::getline( in, line ) )
        {
            ++lineNo;
            const auto hash = line.find( '#' );
            if( hash != std::string::npos )
                line.erase( hash );
            if( tokenize( line ).empty() )
                continue;
            if( !parseLine( lineNo, line ) )
                return false;
        }
        return true;
    }

    bool ScriptPlayer::parseLine( int lineNo, const std::string& line )
    {
        const auto  t   = tokenize( line );
        std::string cmd = lower( t[0] );
        Command     c;
        c.line   = lineNo;
        c.source = line;
        long v = 0;

        if( cmd == "button" )
            cmd = "press";
        else if( cmd == "knob" )
            cmd = "turn";
        else if( cmd == "slider" )
            cmd = "strip";

        if( cmd == "wait" )
        {
            if( t.size() != 2 || !toInt( t[1], v ) || v < 0 )
                return fail( lineNo, "wait takes a number of milliseconds" );
            c.kind       = Kind::Wait;
            c.durationUs = ms( v );
        }
        else if( cmd == "press" )
        {
            // The name can hold spaces ("Track Mix"), so try everything after the command as a name
            // first, then everything but the last word with that word as the hold time.
            const auto firstSpace = line.find_first_of( " \t" );
            std::string rest      = firstSpace == std::string::npos ? "" : line.substr( firstSpace );
            const auto  b         = rest.find_first_not_of( " \t\r" );
            rest                  = b == std::string::npos ? "" : rest.substr( b );
            const auto  e         = rest.find_last_not_of( " \t\r" );
            rest                  = e == std::string::npos ? "" : rest.substr( 0, e + 1 );

            int  code = findButton( rest );
            long hold = 60;
            if( code < 0 )
            {
                const auto sp = rest.find_last_of( " \t" );
                if( sp != std::string::npos && toInt( rest.substr( sp + 1 ), hold ) && hold >= 0 )
                    code = findButton( rest.substr( 0, sp ) );
            }
            if( code < 0 )
                return fail( lineNo, "unknown button: " + rest );
            c.kind       = Kind::Scheduled;
            c.steps      = { { 0, Op::KeyDown, code, 0 }, { ms( hold ), Op::KeyUp, code, 0 } };
            c.durationUs = ms( hold );
        }
        else if( cmd == "pad" )
        {
            long pad = 0, vel = 100, hold = 80, at = 0;
            if( t.size() < 2 || t.size() > 5 || !toInt( t[1], pad ) || pad < 1 || pad > 16 )
                return fail( lineNo, "pad takes 1-16, then optional velocity, holdMs, aftertouch" );
            if( t.size() > 2 && ( !toInt( t[2], vel ) || vel < 1 || vel > 127 ) )
                return fail( lineNo, "velocity is 1-127" );
            if( t.size() > 3 && ( !toInt( t[3], hold ) || hold < 0 ) )
                return fail( lineNo, "holdMs is a number of milliseconds" );
            if( t.size() > 4 && ( !toInt( t[4], at ) || at < 0 || at > 127 ) )
                return fail( lineNo, "aftertouch is 0-127" );

            // Raw readings that the pad scanner turns back into these numbers through the linear curve.
            const auto& pc   = config_.pads;
            const uint32_t peak = std::min<uint32_t>( 4095, pc.onThreshold + ceilDiv( (uint32_t)vel * ( pc.fullScale - pc.onThreshold ), 127 ) );
            const uint32_t sustain = std::min<uint32_t>( 4095, pc.offThreshold + ceilDiv( (uint32_t)at * ( pc.fullScale - pc.offThreshold ), 127 ) );
            const int idx = (int)pad - 1;

            c.kind  = Kind::Scheduled;
            c.steps = { { 0, Op::PadRaw, idx, (int)peak } };
            if( ms( hold ) > 2000 )
                c.steps.push_back( { 2000, Op::PadRaw, idx, (int)sustain } );
            c.steps.push_back( { ms( hold ), Op::PadRaw, idx, 0 } );
            c.durationUs = ms( hold );
        }
        else if( cmd == "turn" )
        {
            long knob = 0, steps = 0, interval = 1000;
            if( t.size() < 3 || t.size() > 4 || !toInt( t[1], knob ) || knob < 0 || knob > 16 || !toInt( t[2], steps ) || steps == 0 || steps > 100000 || steps < -100000 )
                return fail( lineNo, "turn takes 0-16, then a non-zero number of steps up to 100000 (negative is anticlockwise)" );
            if( t.size() > 3 && ( !toInt( t[3], interval ) || interval < 1 ) )
                return fail( lineNo, "intervalUs is a number of microseconds" );

            const long transitions = ( steps < 0 ? -steps : steps ) * config_.encoders.transitionsPerStep;
            c.kind                 = Kind::Scheduled;
            for( long i = 0; i < transitions; ++i )
                c.steps.push_back( { (uint64_t)i * (uint64_t)interval, Op::EncoderStep, (int)knob, steps > 0 ? 1 : 0 } );
            c.durationUs = (uint64_t)transitions * (uint64_t)interval;
        }
        else if( cmd == "strip" )
        {
            long pct = 0, hold = 60;
            if( t.size() < 2 || t.size() > 3 || !toInt( t[1], pct ) || pct < 1 || pct > 100 )
                return fail( lineNo, "strip takes a position 1-100 and an optional holdMs" );
            if( t.size() > 2 && ( !toInt( t[2], hold ) || hold < 0 ) )
                return fail( lineNo, "holdMs is a number of milliseconds" );
            // The smallest sensor position that maps to this percentage.
            const int position = (int)ceilDiv( (uint32_t)( pct - 1 ) * 1023u, (uint32_t)( contract::kStripMax - 1 ) );
            c.kind             = Kind::Scheduled;
            c.steps            = { { 0, Op::StripTouch, position, 0 }, { ms( hold ), Op::StripLift, 0, 0 } };
            c.durationUs       = ms( hold );
        }
        else if( cmd == "aftertouch" )
        {
            if( t.size() != 2 || ( lower( t[1] ) != "on" && lower( t[1] ) != "off" ) )
                return fail( lineNo, "aftertouch takes on or off" );
            c.kind  = Kind::Aftertouch;
            c.value = lower( t[1] ) == "on" ? 1 : 0;
        }
        else if( cmd == "expect" )
        {
            const std::string what = t.size() > 1 ? lower( t[1] ) : "";
            if( what == "streaming" || what == "connected" )
            {
                long timeout = 60000;
                if( t.size() > 3 || ( t.size() == 3 && !toInt( t[2], timeout ) ) )
                    return fail( lineNo, "expect streaming takes an optional timeout in milliseconds" );
                c.kind       = Kind::ExpectStreaming;
                c.durationUs = ms( timeout );
            }
            else if( what == "host" )
            {
                long count = 1, timeout = 3000;
                if( t.size() < 3 || t.size() > 5 || ( t.size() > 3 && ( !toInt( t[3], count ) || count < 1 ) ) || ( t.size() > 4 && !toInt( t[4], timeout ) ) )
                    return fail( lineNo, "expect host \"<text>\" [count] [timeoutMs]" );
                c.kind       = Kind::ExpectHost;
                c.text       = t[2];
                c.count      = (int)count;
                c.durationUs = ms( timeout );
            }
            else if( what == "led" )
            {
                // expect led <button name or code, spaces allowed> on|off [timeoutMs]
                std::vector<std::string> rest( t.begin() + 2, t.end() );
                long                     timeout = 3000;
                if( rest.size() >= 2 && toInt( rest.back(), timeout ) )
                    rest.pop_back();
                const std::string state = rest.empty() ? "" : lower( rest.back() );
                if( rest.size() < 2 || ( state != "on" && state != "off" ) || timeout < 0 )
                    return fail( lineNo, "expect led <button> on|off [timeoutMs]" );
                rest.pop_back();
                std::string name;
                for( const auto& w : rest )
                    name += ( name.empty() ? "" : " " ) + w;
                const int code = findButton( name );
                if( code < 0 || code >= (int)output::kButtonLeds )
                    return fail( lineNo, "no LED for button: " + name );
                c.kind       = Kind::ExpectLed;
                c.value      = code;
                c.want       = state == "on" ? 1 : 0;
                c.durationUs = ms( timeout );
            }
            else if( what == "pad" )
            {
                long pad = 0, colour = 0, timeout = 3000;
                if( t.size() < 4 || t.size() > 5 || !toInt( t[2], pad ) || pad < 1 || pad > 16 )
                    return fail( lineNo, "expect pad <1-16> <colourId|off> [timeoutMs]" );
                if( lower( t[3] ) == "off" )
                    colour = -1;
                else if( !toInt( t[3], colour ) || colour < 0 || colour >= contract::kPadColourCount )
                    return fail( lineNo, "pad colour is 0-17 or off" );
                if( t.size() > 4 && ( !toInt( t[4], timeout ) || timeout < 0 ) )
                    return fail( lineNo, "timeoutMs is a number of milliseconds" );
                c.kind       = Kind::ExpectPad;
                c.value      = (int)pad - 1;
                c.want       = (int)colour;
                c.durationUs = ms( timeout );
            }
            else
                return fail( lineNo, "unknown expect: " + what );
        }
        else if( cmd == "mark-host" )
            c.kind = Kind::MarkHost;
        else if( cmd == "quit" )
        {
            if( t.size() > 2 || ( t.size() == 2 && !toInt( t[1], v ) ) )
                return fail( lineNo, "quit takes an optional exit code" );
            c.kind  = Kind::Quit;
            c.value = (int)v;
        }
        else
            return fail( lineNo, "unknown command: " + t[0] );

        std::stable_sort( c.steps.begin(), c.steps.end(), []( const Step& a, const Step& b ) { return a.atUs < b.atUs; } );
        commands_.push_back( std::move( c ) );
        return true;
    }

    void ScriptPlayer::apply( const Step& s )
    {
        switch( s.op )
        {
            case Op::KeyDown: board_.matrix.press( (unsigned)s.a ); break;
            case Op::KeyUp: board_.matrix.release( (unsigned)s.a ); break;
            case Op::PadRaw: board_.pads.set( (unsigned)s.a, (uint16_t)s.b ); break;
            case Op::EncoderStep: board_.encoders.step( (unsigned)s.a, s.b != 0 ); break;
            case Op::StripTouch: board_.strip.touch( (uint16_t)s.a ); break;
            case Op::StripLift: board_.strip.lift(); break;
        }
    }

    void ScriptPlayer::fault( const std::string& why )
    {
        failure_  = "script line " + std::to_string( commands_[index_].line ) + ": FAIL " + why + ": " + commands_[index_].source;
        state_    = State::Failed;
        exitCode_ = 1;
    }

    ScriptPlayer::State ScriptPlayer::advance( uint64_t nowUs )
    {
        while( state_ == State::Running )
        {
            if( index_ >= commands_.size() )
            {
                state_ = State::Done;
                break;
            }
            Command& c = commands_[index_];
            if( !started_ )
            {
                started_   = true;
                startedUs_ = nowUs;
                nextStep_  = 0;
            }
            const uint64_t elapsed = nowUs - startedUs_;
            bool           finished = false;

            switch( c.kind )
            {
                case Kind::Wait:
                    finished = elapsed >= c.durationUs;
                    break;

                case Kind::Scheduled:
                    while( nextStep_ < c.steps.size() && c.steps[nextStep_].atUs <= elapsed )
                        apply( c.steps[nextStep_++] );
                    finished = nextStep_ >= c.steps.size();
                    break;

                case Kind::ExpectStreaming:
                    if( hooks_.streaming && hooks_.streaming() )
                        finished = true;
                    else if( elapsed >= c.durationUs )
                        fault( "the host never completed the handshake" );
                    break;

                case Kind::MarkHost:
                    hostMark_ = hooks_.hostLog ? hooks_.hostLog().size() : 0;
                    finished  = true;
                    break;

                case Kind::ExpectHost:
                {
                    int found = 0;
                    if( hooks_.hostLog )
                    {
                        const std::string log = hooks_.hostLog();
                        std::istringstream in( hostMark_ <= log.size() ? log.substr( hostMark_ ) : log );
                        std::string        l;
                        while( std::getline( in, l ) )
                            if( l.find( c.text ) != std::string::npos )
                                ++found;
                    }
                    if( found >= c.count )
                        finished = true;
                    else if( elapsed >= c.durationUs )
                        fault( "host log has " + std::to_string( found ) + " of the " + std::to_string( c.count ) + " expected lines containing '" + c.text + "'" );
                    break;
                }

                case Kind::ExpectLed:
                case Kind::ExpectPad:
                {
                    bool ok = false;
                    if( hooks_.ledTable )
                    {
                        const output::LedFrame& table = hooks_.ledTable();
                        if( c.kind == Kind::ExpectLed )
                            ok = ( table.buttons[c.value].state != contract::STATE_OFF ) == ( c.want != 0 );
                        else
                        {
                            const output::PadLed& p = table.pads[c.value];
                            ok = c.want < 0 ? ( p.state == contract::STATE_OFF || p.colour == contract::kPadColourTrans ) : ( p.state != contract::STATE_OFF && p.colour == c.want );
                        }
                    }
                    if( ok )
                        finished = true;
                    else if( elapsed >= c.durationUs )
                        fault( c.kind == Kind::ExpectLed ? "button LED did not reach the expected state" : "pad LED did not reach the expected state" );
                    break;
                }

                case Kind::Aftertouch:
                    if( hooks_.setAftertouch )
                        hooks_.setAftertouch( c.value != 0 );
                    finished = true;
                    break;

                case Kind::Quit:
                    exitCode_ = c.value;
                    state_    = State::Done;
                    return state_;
            }

            if( state_ != State::Running )
                break;
            if( !finished )
                break;
            ++index_;
            started_ = false;
        }
        return state_;
    }

} // namespace sim
} // namespace kick_mcu
