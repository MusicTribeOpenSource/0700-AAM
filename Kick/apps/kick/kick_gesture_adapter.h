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
// Turns kick-gesture events into front panel input for the monolith kick application.

#include "contracts/dcl/dcl_contracts.h"
#include "contracts/dcl/dcl_signals.h"
#include "contracts/gesture/gesture_signals.h"
#include "engine/project/kick_settings.h"
#include "gesture/gesture.h"
#include "PktMgr/PktMgr.hpp"

#include <cstdlib>
#include <string>

// The core keeps its own copy of the panel numbers so it builds without JUCE. These checks keep the copies honest.
namespace kick_gesture::panel
{
    using Dev = contracts::DeviceControlContracts;
    static_assert( kBankA == Dev::EN_BANK_A, "panel::kBankA" );
    static_assert( kBankH == Dev::EN_BANK_H, "panel::kBankH" );
    static_assert( kNumBanks == Dev::EN_BANK_H - Dev::EN_BANK_A + 1, "panel::kNumBanks" );
    static_assert( kRecord == Dev::EN_RECORD, "panel::kRecord" );
    static_assert( kOverdub == Dev::EN_OVERDUB, "panel::kOverdub" );
    static_assert( kStop == Dev::EN_STOP, "panel::kStop" );
    static_assert( kPlay == Dev::EN_PLAY, "panel::kPlay" );
    static_assert( kPlayStart == Dev::EN_PLAYSTART, "panel::kPlayStart" );
    static_assert( kErase == Dev::EN_ERASE, "panel::kErase" );
    static_assert( kFirstQLink == Dev::EN_KNOB_1, "panel::kFirstQLink" );
    static_assert( kLastQLink == Dev::EN_KNOB_16, "panel::kLastQLink" );
}

class KickGestureAdapter
{
public:
    KickGestureAdapter()
    {
        // The emulator test script turns gestures on this way, so it never has to click through the settings page.
        if( const char* env = std::getenv( "KICK_EMU_GESTURES" ) )
            forceEnabled_ = ( *env != '\0' && *env != '0' );

        server_.start();
        kick_gesture::sharedCameraService().start();
        gestureConnection_ = contracts::GestureSignals::gestureReceived.connect( [this]( const contracts::GestureEvent& e ) { onGesture( e ); } );
    }

    ~KickGestureAdapter()
    {
        gestureConnection_.disconnect();
        kick_gesture::sharedCameraService().stop();
        server_.stop();
    }

private:
    void onGesture( const contracts::GestureEvent& e )
    {
        auto& settings = mt::getSettings();

        // Read the settings on every event, so a change on the settings page applies to the very next gesture.
        const std::string text = settings.gesture.mapping().toStdString();
        if( text != lastMappingText_ )
        {
            lastMappingText_ = text;
            kick_gesture::MappingTable table;
            std::string                problems;
            kick_gesture::MappingTable::fromString( text, table, problems );
            if( !problems.empty() )
                juce::Logger::writeToLog( "gesture: ignored mapping entries: " + juce::String( problems ) );
            router_.setMapping( table );
        }
        router_.setSensitivity( settings.gesture.sensitivity() );

        auto&                    ae = mt::getAudioInterface();
        kick_gesture::AppState state;
        state.gesturesEnabled = forceEnabled_ || settings.gesture.enabled();
        state.recording       = ae.isLooperRecording() || ae.isLooperOverdubbing() || ae.isRecorderRunning() || ae.isRecorderOverdubbing();
        const juce::String bank = ae.getBank();
        state.currentBank       = bank.isEmpty() ? 0 : juce::jlimit( 0, kick_gesture::panel::kNumBanks - 1, static_cast<int>( bank[0] ) - 'A' );

        const auto outcome = router_.handle( e, state );
        if( outcome.mapped && outcome.decision != kick_gesture::GateDecision::Allow )
            juce::Logger::writeToLog( juce::String( "gesture: " ) + contracts::gestureKindName( e.kind ) + " blocked: " + kick_gesture::gateDecisionName( outcome.decision ) );

        for( const auto& c : outcome.commands )
        {
            if( c.type == kick_gesture::Command::Type::ButtonTap )
                tapButton( c.id );
            else
                turnKnob( c.id, c.steps );
        }
    }

    // Same payload the keyboard path sends for a pad button: id, state, velocity, aftertouch.
    static void sendButton( int id, int state )
    {
        CPktMgr::HWMsg_t msg;
        msg.data = { id, state, 80, 100 };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

    static void tapButton( int id )
    {
        sendButton( id, contracts::DeviceControlContracts::BTN_PRESSED );
        sendButton( id, contracts::DeviceControlContracts::BTN_RELEASED );
    }

    // The Q-Link packet the front panel sends: knob, direction, steps.
    static void turnKnob( int knob, int steps )
    {
        if( steps == 0 )
            return;
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::EN_PKT_TYPE_KNOBS;
        msg.pktCmd  = contracts::DeviceControlContracts::EN_KNOB_QLINK;
        msg.data    = { knob, steps > 0 ? static_cast<int>( contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE ) : static_cast<int>( contracts::DeviceControlContracts::EN_KNOB_ANTICLOCKWISE ), std::abs( steps ) };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

    sigslot::scoped_connection     gestureConnection_;
    kick_gesture::GestureIpcServer server_;
    kick_gesture::GestureRouter    router_;
    std::string                    lastMappingText_{ "\x01" }; // never equal to a real text, so the first event loads the mapping
    bool                           forceEnabled_{ false };
};
