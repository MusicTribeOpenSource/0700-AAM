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
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include "../../common/source/kick_ValueTreeUtils.h"
#ifndef KICK_XML_MANAGER_H_INCLUDE
    #define KICK_XML_MANAGER_H_INCLUDE

namespace mt
{
    const char* const __manufacturer = "Musictribe";
    class KickXML
        : private ValueTreeAllEventListener
        , private juce::AsyncUpdater
    {
    public:
        KickXML( juce::Identifier ID, const juce::String& FileExtension, const juce::String& Version );
        ~KickXML();

        std::function<void()> onLoad = []() {};
        // Every persistence path uses this barrier before serializing live state.
        // Called on the message thread, including saves requested by a loader.
        std::function<juce::Result()> prepareSave;
        std::function<void(const juce::String&)> onSaveFailure;
        std::function<void(bool)> onDirtyChanged;

        // Main API's...
        juce::ValueTree loadOrCreate( const juce::File& XMLFIle );
        enum class SaveReason { Explicit, Autosave, Close };
        bool            save(SaveReason reason = SaveReason::Explicit);
        bool            close();
        bool            isDirty() const noexcept { return dirty.load(); }
        const juce::String& getLastSaveError() const noexcept { return lastSaveError; }

        void                   setAutoSave( bool AutoSave ) { autoSave = AutoSave; }
        void                   setAutoSaveTime( int AutoSaveTimeInMS ) { autoSaveTimeOutInMS = AutoSaveTimeInMS; }
        juce::ValueTree&       getState() { return vt; }
        const juce::String&    getDirectory() { return directoryText; }
        juce::CriticalSection& getCriticalSection() { return lock; };

    protected:
        // Shutdown only: do not write stale state from base-class destruction
        // after the derived owner's live-program callbacks have gone away.
        void detachWithoutSaving();

    private:
        void valueTreeChanged() override;
        void handleAsyncUpdate() override;
        bool saveOnMessageThread(SaveReason);
        bool failSave(const juce::String&, SaveReason);

    private:
        const juce::Identifier id;
        const juce::String     extn;
        const juce::String     version;

        juce::CriticalSection lock;
        juce::File            xmlFileDir;
        juce::File            xmlFile;
        juce::String          directoryText, lastSaveError;
        juce::ValueTree       vt;
        juce::UndoManager     undo;
        std::atomic<bool> dirty{false}, autosaveDue{false}, saving{false};

        static bool autoSave;
        static int  autoSaveTimeOutInMS;
        int         prefAutoSaveTimeOutInMS = 1500;

        struct pvtTimer : public juce::Timer
        {
            KickXML& Parent;
            explicit pvtTimer( KickXML& parent )
                : Parent( parent ) {}
            ~pvtTimer() { stopTimer(); }

            void timerCallback() override { stopTimer(); Parent.autosaveDue.store(true); Parent.triggerAsyncUpdate(); }
        };
        pvtTimer timer{ *this };
    };

}

#endif
