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

// Plain C++ (no JUCE). The mapping core and its unit tests build without the app.

#include "contracts/gesture/gesture_event.h"

#include <array>
#include <string>
#include <vector>

namespace kick_gesture
{
    enum class ActionKind : int
    {
        None = 0,
        Parameter, // turn a Q-Link knob
        Button,    // tap a front panel button by its id
        Transport, // Play, Stop, Record or Overdub
        Bank       // select a pad bank, absolute or relative
    };

    enum class TransportCommand : int
    {
        Play = 0,
        Stop,
        Record,
        Overdub,
        Count
    };

    // Front panel numbers restated from contracts::DeviceControlContracts so this header needs no JUCE. The adapter
    // in apps/kick static_asserts every one of them against the real enums.
    namespace panel
    {
        constexpr int kBankA     = 0;
        constexpr int kBankH     = 7;
        constexpr int kNumBanks  = 8;
        constexpr int kRecord    = 42;
        constexpr int kOverdub   = 43;
        constexpr int kStop      = 44;
        constexpr int kPlay      = 45;
        constexpr int kPlayStart = 46;
        constexpr int kErase     = 52;
        constexpr int kNumButtons = 53; // DeviceControlContracts::SIZE
        constexpr int kFirstQLink = 1;
        constexpr int kLastQLink  = 16;
    }

    // What a gesture does. The meaning of the fields depends on `kind`:
    //   None       nothing.
    //   Parameter  id is the Q-Link knob (1 to 16). direction is +1 or -1. scale is the number of encoder steps one
    //              gesture turns it by (rounded, at least 1), or for pinch_change the steps a full 0 to 1 pinch
    //              travel turns it by.
    //   Button     id is the button number (0 to 52). direction and scale are unused.
    //   Transport  id is a TransportCommand. direction and scale are unused.
    //   Bank       direction 0: select bank `id` (0 is A, 7 is H). direction +1 or -1: the next or previous bank from
    //              the current one, wrapping around. scale is unused.
    struct Action
    {
        ActionKind kind{ ActionKind::None };
        int        id{ 0 };
        int        direction{ 0 };
        float      scale{ 1.f };

        bool operator==( const Action& o ) const { return kind == o.kind && id == o.id && direction == o.direction && scale == o.scale; }
        bool operator!=( const Action& o ) const { return !( *this == o ); }
    };

    // The front panel button a transport command presses.
    int buttonIdFor( TransportCommand c );

    // True if `a` is allowed for gesture `g`. When it is not, `why` says why. None is always allowed.
    // pinch_change is continuous, so it only takes None or Parameter.
    bool validateAction( contracts::GestureKind g, const Action& a, std::string& why );

    // Short text for the settings page, for example "Bank next", "Stop", "Knob 2 up", "Knob 1 follows pinch".
    std::string describeAction( contracts::GestureKind g, const Action& a );

    // The actions the settings page lets the user pick between for a gesture, None first. The text format below
    // can hold more than this list does, for example Record and Overdub, which the page leaves out on purpose.
    const std::vector<Action>& actionChoices( contracts::GestureKind g );

    // gesture -> action. Starts with the defaults:
    //   swipe_left  previous bank        swipe_right  next bank
    //   palm_hold   Play                 fist         Stop
    //   pinch_change  Q-Link knob 1, 32 steps over a full pinch travel
    //   pinch_start and pinch_end  nothing
    class MappingTable
    {
    public:
        MappingTable();

        const Action& get( contracts::GestureKind g ) const { return actions_[static_cast<size_t>( g )]; }

        // Stores `a` if it is valid for `g`. On failure the table is unchanged and `why` is set.
        bool set( contracts::GestureKind g, const Action& a, std::string& why );

        static MappingTable defaults();

        // "swipe_left=bank:0:-1:1;swipe_right=bank:0:1:1;..." One entry per gesture, in the order of the event
        // contract: name=kind:id:direction:scale. This is what the preferences file stores.
        std::string toString() const;

        // Starts from the defaults and applies every entry of `text`. An entry that is malformed, names an unknown
        // gesture or holds an invalid action is skipped and listed in `problems`, and the default stays for that
        // gesture. Returns false when anything was skipped. An empty text gives the defaults and returns true.
        static bool fromString( const std::string& text, MappingTable& out, std::string& problems );

        bool operator==( const MappingTable& o ) const { return actions_ == o.actions_; }
        bool operator!=( const MappingTable& o ) const { return !( *this == o ); }

    private:
        std::array<Action, contracts::kGestureKindCount> actions_;
    };

    const char* actionKindName( ActionKind k );
}
