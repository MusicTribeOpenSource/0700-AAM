/*
 * Copyright (c) 2022 MUSIC Tribe
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

namespace mt
{
    class MTMidiDeviceManager
        : private juce::MidiInputCallback
        , private juce::AsyncUpdater
    {
    public:
        std::function<void( const MTMidiMessage& )>        onMidiMessage  = []( const MTMidiMessage& ) {};
        std::function<void( juce::Array<MTMidiMessage>& )> onMidiMessages = []( juce::Array<MTMidiMessage>& ) {};

        MTMidiDeviceManager();
        ~MTMidiDeviceManager();

        void dumpMidiInfo( bool isInputDevice );

        ///
        /// STATIC functions
        ///
        // MIDI Device related API's...
        static int                              getNumMidiInputDevices() noexcept;
        static int                              getNumMidiOutputDevices() noexcept;
        static const std::vector<juce::String>& getMidiInputPortNames() noexcept;
        static const std::vector<juce::String>& getMidiOutputPortNames() noexcept;

        static juce::ReferenceCountedObjectPtr<MidiDeviceListEntry>    getMidiDevice( int index, bool isInput ) noexcept;
        static const juce::ReferenceCountedArray<MidiDeviceListEntry>& getMidiDeviceList( bool isInput ) noexcept;
        static const std::vector<juce::String>&                        getMidiDeviceNames( bool isInput ) noexcept;

        ///
        /// Public functions
        ///
        void openMidiDevice( MidiDeviceListEntry::Ptr entry );
        void closeMidiDevice( MidiDeviceListEntry::Ptr entry );
        void openMidiDevice( bool isInput, int index );
        void closeMidiDevice( bool isInput, int index );

        // Send MIDI message
        void sendToOutputs( const juce::MidiMessage& msg );
        void sendToOutput( juce::MidiDeviceInfo device, const juce::MidiMessage& msg );
        void sendToOutput( const juce::String& Device, const juce::MidiMessage& Msg );

        /// MIDI device list updater...
        void refreshMIDIDeviceLists();

    private:
        // MIDI devices...
        static std::vector<juce::String>                        midiInPorts;
        static std::vector<juce::String>                        midiOutPorts;
        static juce::ReferenceCountedArray<MidiDeviceListEntry> midiInputs;
        static juce::ReferenceCountedArray<MidiDeviceListEntry> midiOutputs;
        // JUCE removes virtual ports when their MidiOutput owners are destroyed.
        std::vector<std::unique_ptr<juce::MidiOutput>> virtualOutputs;

        juce::CriticalSection      midiMonitorLock;
        juce::Array<MTMidiMessage> incomingMessages;

        KickPrivateTimer internalTimer;

        juce::ReferenceCountedObjectPtr<MidiDeviceListEntry> findMidiDevice( juce::MidiDeviceInfo device, bool isInputDevice ) const;
        juce::ReferenceCountedObjectPtr<MidiDeviceListEntry> findMidiDevice( const juce::String& device, bool isInputDevice ) const;

        bool hasMidiDeviceListChanged( const juce::Array<juce::MidiDeviceInfo>& availableDevices, bool isInputDevice );
        void updateMidiDeviceList( bool isInputDevice );
        void closeUnpluggedMidiDevices( const juce::Array<juce::MidiDeviceInfo>& currentlyPluggedInDevices, bool isInputDevice );
        void handleIncomingMidiMessage( juce::MidiInput* source, const juce::MidiMessage& message ) override;
        void handleAsyncUpdate() override;

    private:
        juce::Array<MTMidiMessage> midiMessages;
    };

}
