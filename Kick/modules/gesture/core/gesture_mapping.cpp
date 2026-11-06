/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "gesture_mapping.h"

#include <cmath>
#include <locale>
#include <sstream>

namespace kick_gesture
{
    namespace
    {
        using contracts::GestureKind;

        Action none() { return {}; }
        Action knob( int id, int direction, float scale ) { return { ActionKind::Parameter, id, direction, scale }; }
        Action bank( int id, int direction ) { return { ActionKind::Bank, id, direction, 1.f }; }
        Action transport( TransportCommand c ) { return { ActionKind::Transport, static_cast<int>( c ), 0, 1.f }; }

        // The scale a pinch_change mapping gets when picked from the settings page: encoder steps over a full pinch.
        constexpr float kPinchTravelSteps = 32.f;

        std::string formatFloat( float v )
        {
            std::ostringstream os;
            os.imbue( std::locale::classic() );
            os << v;
            return os.str();
        }

        bool parseFloat( const std::string& s, float& out )
        {
            std::istringstream is( s );
            is.imbue( std::locale::classic() );
            float v = 0.f;
            is >> v;
            if( is.fail() || !is.eof() || !std::isfinite( v ) )
                return false;
            out = v;
            return true;
        }

        bool parseInt( const std::string& s, int& out )
        {
            std::istringstream is( s );
            is.imbue( std::locale::classic() );
            int v = 0;
            is >> v;
            if( is.fail() || !is.eof() )
                return false;
            out = v;
            return true;
        }

        bool parseKind( const std::string& s, ActionKind& out )
        {
            for( int i = 0; i <= static_cast<int>( ActionKind::Bank ); ++i )
            {
                if( s == actionKindName( static_cast<ActionKind>( i ) ) )
                {
                    out = static_cast<ActionKind>( i );
                    return true;
                }
            }
            return false;
        }

        std::vector<std::string> split( const std::string& s, char sep )
        {
            std::vector<std::string> parts;
            size_t                   start = 0;
            for( ;; )
            {
                const size_t at = s.find( sep, start );
                if( at == std::string::npos )
                {
                    parts.push_back( s.substr( start ) );
                    return parts;
                }
                parts.push_back( s.substr( start, at - start ) );
                start = at + 1;
            }
        }

        std::string trim( const std::string& s )
        {
            const auto b = s.find_first_not_of( " \t\r\n" );
            if( b == std::string::npos )
                return {};
            const auto e = s.find_last_not_of( " \t\r\n" );
            return s.substr( b, e - b + 1 );
        }
    }

    const char* actionKindName( ActionKind k )
    {
        switch( k )
        {
            case ActionKind::None: return "none";
            case ActionKind::Parameter: return "parameter";
            case ActionKind::Button: return "button";
            case ActionKind::Transport: return "transport";
            case ActionKind::Bank: return "bank";
        }
        return "none";
    }

    int buttonIdFor( TransportCommand c )
    {
        switch( c )
        {
            case TransportCommand::Play: return panel::kPlay;
            case TransportCommand::Stop: return panel::kStop;
            case TransportCommand::Record: return panel::kRecord;
            case TransportCommand::Overdub: return panel::kOverdub;
            case TransportCommand::Count: break;
        }
        return panel::kStop;
    }

    bool validateAction( GestureKind g, const Action& a, std::string& why )
    {
        if( static_cast<int>( g ) < 0 || g >= GestureKind::Count )
        {
            why = "unknown gesture";
            return false;
        }
        switch( a.kind )
        {
            case ActionKind::None: return true;
            case ActionKind::Parameter:
                if( a.id < panel::kFirstQLink || a.id > panel::kLastQLink )
                {
                    why = "knob must be 1 to 16";
                    return false;
                }
                if( a.direction != 1 && a.direction != -1 )
                {
                    why = "direction must be 1 or -1";
                    return false;
                }
                if( !( a.scale > 0.f ) || a.scale > 128.f )
                {
                    why = "scale must be above 0 and at most 128";
                    return false;
                }
                return true;
            case ActionKind::Button:
                if( g == GestureKind::PinchChange )
                {
                    why = "pinch_change is continuous and only takes a knob";
                    return false;
                }
                if( a.id < 0 || a.id >= panel::kNumButtons )
                {
                    why = "button must be 0 to 52";
                    return false;
                }
                return true;
            case ActionKind::Transport:
                if( g == GestureKind::PinchChange )
                {
                    why = "pinch_change is continuous and only takes a knob";
                    return false;
                }
                if( a.id < 0 || a.id >= static_cast<int>( TransportCommand::Count ) )
                {
                    why = "transport must be 0 (play), 1 (stop), 2 (record) or 3 (overdub)";
                    return false;
                }
                return true;
            case ActionKind::Bank:
                if( g == GestureKind::PinchChange )
                {
                    why = "pinch_change is continuous and only takes a knob";
                    return false;
                }
                if( a.direction < -1 || a.direction > 1 )
                {
                    why = "bank direction must be -1, 0 or 1";
                    return false;
                }
                if( a.direction == 0 && ( a.id < panel::kBankA || a.id > panel::kBankH ) )
                {
                    why = "bank must be 0 (A) to 7 (H)";
                    return false;
                }
                return true;
        }
        why = "unknown action kind";
        return false;
    }

    std::string describeAction( GestureKind g, const Action& a )
    {
        switch( a.kind )
        {
            case ActionKind::None: return "None";
            case ActionKind::Parameter:
            {
                const std::string knobName = "Knob " + std::to_string( a.id );
                if( g == GestureKind::PinchChange )
                    return knobName + ( a.direction > 0 ? " follows pinch" : " follows pinch, inverted" );
                return knobName + ( a.direction > 0 ? " up" : " down" );
            }
            case ActionKind::Button: return "Button " + std::to_string( a.id );
            case ActionKind::Transport:
                switch( static_cast<TransportCommand>( a.id ) )
                {
                    case TransportCommand::Play: return "Play";
                    case TransportCommand::Stop: return "Stop";
                    case TransportCommand::Record: return "Record";
                    case TransportCommand::Overdub: return "Overdub";
                    case TransportCommand::Count: break;
                }
                return "Transport";
            case ActionKind::Bank:
                if( a.direction > 0 )
                    return "Bank next";
                if( a.direction < 0 )
                    return "Bank previous";
                return std::string( "Bank " ) + static_cast<char>( 'A' + a.id );
        }
        return "None";
    }

    const std::vector<Action>& actionChoices( GestureKind g )
    {
        static const std::vector<Action> discrete = [] {
            std::vector<Action> v{ none(), bank( 0, -1 ), bank( 0, 1 ), transport( TransportCommand::Play ), transport( TransportCommand::Stop ) };
            for( int k = 1; k <= 4; ++k )
            {
                v.push_back( knob( k, 1, 1.f ) );
                v.push_back( knob( k, -1, 1.f ) );
            }
            return v;
        }();
        static const std::vector<Action> continuous = [] {
            std::vector<Action> v{ none() };
            for( int k = 1; k <= 4; ++k )
            {
                v.push_back( knob( k, 1, kPinchTravelSteps ) );
                v.push_back( knob( k, -1, kPinchTravelSteps ) );
            }
            return v;
        }();
        return g == GestureKind::PinchChange ? continuous : discrete;
    }

    MappingTable::MappingTable()
    {
        for( auto& a : actions_ )
            a = none();
        actions_[static_cast<size_t>( GestureKind::SwipeLeft )]   = bank( 0, -1 );
        actions_[static_cast<size_t>( GestureKind::SwipeRight )]  = bank( 0, 1 );
        actions_[static_cast<size_t>( GestureKind::PalmHold )]    = transport( TransportCommand::Play );
        actions_[static_cast<size_t>( GestureKind::Fist )]        = transport( TransportCommand::Stop );
        actions_[static_cast<size_t>( GestureKind::PinchChange )] = knob( 1, 1, kPinchTravelSteps );
    }

    MappingTable MappingTable::defaults() { return MappingTable(); }

    bool MappingTable::set( GestureKind g, const Action& a, std::string& why )
    {
        if( !validateAction( g, a, why ) )
            return false;
        Action stored = a;
        if( a.kind == ActionKind::None )
            stored = none();
        actions_[static_cast<size_t>( g )] = stored;
        return true;
    }

    std::string MappingTable::toString() const
    {
        std::string out;
        for( int i = 0; i < contracts::kGestureKindCount; ++i )
        {
            const auto& a = actions_[static_cast<size_t>( i )];
            if( i > 0 )
                out += ';';
            out += contracts::gestureKindName( static_cast<GestureKind>( i ) );
            out += '=';
            out += actionKindName( a.kind );
            out += ':' + std::to_string( a.id ) + ':' + std::to_string( a.direction ) + ':' + formatFloat( a.scale );
        }
        return out;
    }

    bool MappingTable::fromString( const std::string& text, MappingTable& out, std::string& problems )
    {
        out = MappingTable();
        problems.clear();
        auto problem = [&problems]( const std::string& entry, const std::string& why ) {
            if( !problems.empty() )
                problems += "; ";
            problems += "\"" + entry + "\": " + why;
        };

        for( const auto& rawEntry : split( text, ';' ) )
        {
            const std::string entry = trim( rawEntry );
            if( entry.empty() )
                continue;
            const auto eq = entry.find( '=' );
            if( eq == std::string::npos )
            {
                problem( entry, "expected name=kind:id:direction:scale" );
                continue;
            }
            GestureKind g;
            if( !contracts::parseGestureKind( trim( entry.substr( 0, eq ) ).c_str(), g ) )
            {
                problem( entry, "unknown gesture" );
                continue;
            }
            const auto fields = split( entry.substr( eq + 1 ), ':' );
            Action     a;
            if( fields.size() != 4 || !parseKind( trim( fields[0] ), a.kind ) || !parseInt( trim( fields[1] ), a.id ) ||
                !parseInt( trim( fields[2] ), a.direction ) || !parseFloat( trim( fields[3] ), a.scale ) )
            {
                problem( entry, "expected kind:id:direction:scale with numbers" );
                continue;
            }
            std::string why;
            if( !out.set( g, a, why ) )
                problem( entry, why );
        }
        return problems.empty();
    }
}
