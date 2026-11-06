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
#pragma once

// Plain C++ on purpose (no JUCE): the gesture mapping core and its unit tests include this header directly.

#include <cstdint>
#include <cstring>
#include <string>

namespace contracts
{
    // The seven gesture types of docs/gesture-events.md, in the order of that table.
    enum class GestureKind : int
    {
        SwipeLeft = 0,
        SwipeRight,
        PalmHold,
        Fist,
        PinchStart,
        PinchChange,
        PinchEnd,
        Count
    };

    constexpr int kGestureKindCount = static_cast<int>( GestureKind::Count );

    inline const char* gestureKindName( GestureKind k )
    {
        switch( k )
        {
            case GestureKind::SwipeLeft: return "swipe_left";
            case GestureKind::SwipeRight: return "swipe_right";
            case GestureKind::PalmHold: return "palm_hold";
            case GestureKind::Fist: return "fist";
            case GestureKind::PinchStart: return "pinch_start";
            case GestureKind::PinchChange: return "pinch_change";
            case GestureKind::PinchEnd: return "pinch_end";
            case GestureKind::Count: break;
        }
        return "unknown";
    }

    // False for a name that is not one of the seven.
    inline bool parseGestureKind( const char* name, GestureKind& out )
    {
        if( name == nullptr )
            return false;
        for( int i = 0; i < kGestureKindCount; ++i )
        {
            if( std::strcmp( name, gestureKindName( static_cast<GestureKind>( i ) ) ) == 0 )
            {
                out = static_cast<GestureKind>( i );
                return true;
            }
        }
        return false;
    }

    enum class GestureHand : int
    {
        Left = 0,
        Right,
        Unknown
    };

    inline const char* gestureHandName( GestureHand h )
    {
        switch( h )
        {
            case GestureHand::Left: return "left";
            case GestureHand::Right: return "right";
            case GestureHand::Unknown: break;
        }
        return "unknown";
    }

    inline GestureHand parseGestureHand( const char* name )
    {
        if( name != nullptr )
        {
            if( std::strcmp( name, "left" ) == 0 )
                return GestureHand::Left;
            if( std::strcmp( name, "right" ) == 0 )
                return GestureHand::Right;
        }
        return GestureHand::Unknown;
    }

    // One gesture event as kick-gesture writes it (docs/gesture-events.md).
    struct GestureEvent
    {
        GestureKind kind{ GestureKind::SwipeLeft };
        // Only pinch_start, pinch_change and pinch_end carry a value.
        bool         hasValue{ false };
        float        value{ 0.f };
        GestureHand  hand{ GestureHand::Unknown };
        float        confidence{ 0.f };
        // Milliseconds on the camera frame clock. Not wall-clock time and not comparable between runs of the service.
        uint64_t tMs{ 0 };
    };

    // The status lines kick-gesture writes next to its events, plus whether the service is connected to the app at all.
    struct GestureServiceStatus
    {
        // True while a kick-gesture process holds a registered connection to the app's gesture server.
        bool connected{ false };
        // running, degraded, disabled, idle, no_camera, error, or empty before the first status line arrives.
        std::string state;
        std::string reason;
        double      rateHz{ 0.0 };
    };

    enum class CameraState : int
    {
        // kick-camera does not answer on its control socket.
        ServiceMissing = 0,
        // The service is up and the camera device is closed.
        Off,
        // The camera device is open and frames are being published.
        Streaming,
        // The service reported a device error, or the source ended.
        Error
    };

    struct CameraStatus
    {
        CameraState state{ CameraState::ServiceMissing };
        // The camera service's own `capturing` flag, which the on-screen indicator follows. It is true while the
        // device is open: Streaming, and for a moment around an open that fails.
        bool capturing{ false };
        // Reason text from the camera service for Error, empty otherwise.
        std::string detail;

        bool operator==( const CameraStatus& o ) const { return state == o.state && capturing == o.capturing && detail == o.detail; }
        bool operator!=( const CameraStatus& o ) const { return !( *this == o ); }
    };
}
