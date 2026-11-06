// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Button names for script files, indexed by button code (DeviceControlContracts::BtnLedSequence).
// Same spellings as the emulator Surface, so a script written for one runs on the other.
// tests/script_player_test.cpp checks the anchors against the host enum.
#pragma once

#include <cctype>
#include <cstdlib>
#include <string>

namespace kick_mcu
{
namespace sim
{
    constexpr int kButtonNameCount = 55;

    inline const char* const* buttonNames()
    {
        static const char* const names[kButtonNameCount] = {
            "Bank A", "Bank B", "Bank C", "Bank D", "Bank E", "Bank F", "Bank G", "Bank H",
            "Main", "Grid", "Browse", "Save", "Track Mix", "Pad Mix", "Track Mute", "Pad Mute",
            "Next Seq", "FX", "Tap Tempo", "Master", "Copy", "+", "Menu", "Pref",
            "Note Repeat", "Latch", "Enter", "-", "Knob CCW", "Back", "Knob CW", "Shift",
            "Step Seq", "Automation", "TC", "On/Off", "Sampler", "Looper", "Sample Edit", "Link Edit",
            "Prog Edit", "MIDI Ctrl", "Record", "Overdub", "Stop", "Play", "Play Start", "Link",
            "Full Level", "Half Level", "16 Levels", "Notes", "Erase", "(code 53)", "Color Edit",
        };
        return names;
    }

    /// Lower-cases and keeps letters, digits, '+' and '-'.
    inline std::string normaliseName( const std::string& s )
    {
        std::string out;
        for( char c : s )
            if( std::isalnum( (unsigned char)c ) || c == '+' || c == '-' )
                out += (char)std::tolower( (unsigned char)c );
        return out;
    }

    /// A button code from a number or a name, or -1.
    inline int findButton( const std::string& text )
    {
        if( text.empty() )
            return -1;
        if( text.find_first_not_of( "0123456789" ) == std::string::npos )
        {
            const long v = std::strtol( text.c_str(), nullptr, 10 );
            return v >= 0 && v < kButtonNameCount && v != 53 ? (int)v : -1;
        }
        const std::string n = normaliseName( text );
        if( n == "plus" )
            return 21;
        if( n == "minus" )
            return 27;
        for( int i = 0; i < kButtonNameCount; ++i )
            if( i != 53 && normaliseName( buttonNames()[i] ) == n )
                return i;
        return -1;
    }

} // namespace sim
} // namespace kick_mcu
