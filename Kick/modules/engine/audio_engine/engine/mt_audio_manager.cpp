/*
 * Copyright (c) 2021 MUSIC Tribe
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

namespace mt
{

    ///
    /// MTAudioManager
    ///

    MTAudioManager::MTAudioManager( const char* PrjName, const char* PrjManufacturer, const char* PrjVersion )
        : MTAudioRecorderManager( engine )
        , MTAudioProgramManager( selected, engine )
        , MTAudioTransportManager( selected, *this )
        , MTIPadPerformanceData( juce::ValueTree{ IDs::PERFORMANCE }, nullptr )
    {
        juce::ignoreUnused( PrjName, PrjManufacturer, PrjVersion );
        prepareSave = [this] {
            juce::String failures;
            forAllPrograms([&](MTIProgram* program) {
                if (!program->prepareToSave()) {
                    const auto plugin = program->getState().getChildWithName("CLAP").getProperty("name").toString();
                    failures += "Program '" + program->getName() + "'" +
                                (plugin.isEmpty() ? juce::String{} : " (" + plugin + ")") + ": " + program->saveError() + "\n";
                }
            });
            return failures.isEmpty() ? juce::Result::ok() : juce::Result::fail(failures);
        };
        onSaveFailure = [this](const juce::String& reason) {
            listeners.call([&](MTAudioInterfaceListener& listener) { listener.onProjectSaveFailed(reason); });
        };
        onDirtyChanged = [this](bool dirty) {
            listeners.call([&](MTAudioInterfaceListener& listener) { listener.onProjectDirtyChanged(dirty); });
        };

        //juce::MessageManager::callAsync( [&]() {
        // Audio previwer object...
        if( auto pPreview = std::make_unique<AudioPreview>( engine ) )
        {
            _previewer = pPreview.get();
            engine.addMeter( engine.createObject( std::move( pPreview ) ) );
            _previewer->initAudioRouting();
            assert( _previewer != nullptr );
        }

        // Recorder object...
        MTAudioRecorderManager::initRecorder();
        //} );

        // Metronome object...
        _metronome = std::make_unique<MTMetronome>( engine, getSettings().metronome.vTree );

        // Initialize Audio device...
        juce::MessageManager::callAsync( [&]() {
            dm = std::make_unique<AudioDevice>( AUDIO_NUM_INPUTS, AUDIO_NUM_OUTPUTS, juce::String() );

            if( dm.get() )
            {
                engine.addAudioDevice( "Master", dm.get(), true );
                engine.startAudioDevice( AUDIO_SAMPLE_RATE, AUDIO_BLOCK_SIZE );
            }
        } );

        // MIDI messages handler

        auto midiMesageHandler = [&]( const MTMidiMessage& Msg ) {
            //String messageText;
            //messageText << "[" << Msg.source->getName() << "] : " << Msg.message.getDescription() << "\n";
            //DBG( messageText );
            auto& msg = Msg.message;

            if( msg.isMidiClock() ) {}
            else if( msg.isMidiStart() ) {}
            else if( msg.isMidiStop() ) {}
            else if( msg.isMidiContinue() ) {}
            else if( msg.isTempoMetaEvent() ) {}
            else if( msg.isTimeSignatureMetaEvent() ) {}
            else
            {
                if( auto seq = getSequence( selected.sequence ) )
                {
                    seq->forAllTracks( [&]( MTTrack* T ) {
                        T->processIncomingMidiMessage( Msg );
                    } );
                }
            }
        };

        MTMidiDeviceManager::onMidiMessage = midiMesageHandler;
        //MTMidiDeviceManager::onMidiMessages = [&]( juce::Array<MTMidiMessage>& midiMessages ) {
        //    for( auto& m : midiMessages )
        //        midiMesageHandler( m );
        //};

        transportStateChanged = [&]( TransportState State ) {
            if( TransportState::Playing == State )
                stopPreview();

            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onStateChanged( State ); } );
            signals::playStateChanged( State );
        };

        trackSelectionChanged = [&]() { // Trigger views to update...
            selectedStr = sseq + "|" + strack + "|" + sprog;
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onTrackSelectionChange( selected ); } );
            signals::onTrackSelectionChange( selected );
        };

        seqSelectionChanged = [&]() { // Trigger views to update...
            selectedStr = sseq + "|" + strack + "|" + sprog;
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onSequenceSelectionChange( selected ); } );
            signals::onSequenceSelectionChange( selected );
        };

        progSelectionChanged = [&]() { // Trigger views to update...
            selectedStr = sseq + "|" + strack + "|" + sprog;
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onProgramSelectionChange( selected ); } );
            signals::onProgramSelectionChange( selected );
        };

        trackListChanged = [&]( int Sequence ) { // Trigger views to update...
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onTrackListChanged( Sequence ); } );
        };

        seqListChanged = [&]() { // Trigger views to update...
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onSequenceListChanged(); } );
        };

        progListChanged = [&]() { // Trigger views to update...
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onProgramListChanged(); } );
        };

        seqTimeSignatureChanged = [&]( float BPM, int Bars, int Num, int Den ) { // Trigger views to update...
            listeners.call( [&, BPM, Bars, Num, Den]( MTAudioInterfaceListener& l ) { l.onBPMAndTimeSignatureChanged( BPM, Bars, Num, Den ); } );
        };
    }

    MTAudioManager::~MTAudioManager()
    {
        MTAudioRecorderManager::closeRecorder();
        internalTimer.stopTimer();
        _previewer = nullptr;

        MTAudioTransportManager::stop();
        engine.stopAudioDevice();
        closeProject();
        // Base destruction happens after the live program manager is gone.
        // A failed shutdown save must not be replaced with stale XML later.
        detachWithoutSaving();
        prepareSave = {}; onSaveFailure = {}; onDirtyChanged = {};
    }

    ////////////////////////////
    //// Project
    ////////////////////////////
    bool MTAudioManager::loadProject( juce::File& ProjectFile )
    {
        //DBG( "LP: Enter FM: " << engine.getFeeMemory() );
        if (!closeProject()) return false;
        //DBG( "LP: FM2: " << engine.getFeeMemory() );
        undoManager.clearUndoHistory();
        vTree = projectManager::loadOrCreate( ProjectFile );
        if (!vTree.isValid()) return false;

#if defined( USE_VALUE_TREE_DEBUGGER ) && ( USE_VALUE_TREE_DEBUGGER > 0 )
        debugger.setSource( vTree );
#endif

        // Initialize bus manager...
        busMgr.setup( vTree.getOrCreateChildWithName( IDs::BUSES, nullptr ), &undoManager );

        // Initialize recorder...
        MTAudioRecorderManager::setup( vTree, &undoManager );

        // Initialize Asset manager...
        MTAudioAssetManager::setup( vTree.getOrCreateChildWithName( IDs::FILEMANAGER, nullptr ), &undoManager, projectManager::getProjectDir() );

        // Initialize master tempo settings...
        MTTempoMgr::setup( vTree.getOrCreateChildWithName( IDs::TEMPOS, nullptr ), &undoManager );

        // Initialize transport state...
        MTAudioTransportManager::setup();

        // Initialize Programs....
        MTAudioProgramManager::setup( vTree.getOrCreateChildWithName( IDs::PROGRAMS, nullptr ), &undoManager );
        //DBG( "LP: FM3: " << engine.getFeeMemory() );

        std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );

        // Initialize Song Manager...
        MTSongManager::init( vTree.getOrCreateChildWithName( IDs::SONGS, nullptr ), &undoManager );

        // Initialize Sequences....
        MTAudioSequenceManager::setup( vTree.getOrCreateChildWithName( IDs::SEQUENCES, nullptr ), &undoManager );

        projectLoaded = true;

        //DBG( "LP: Exit FM: " << engine.getFeeMemory() );
        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onProjectLoaded( ProjectFile ); } );

        // Reset Selection.
        setSelectedSequenceAndTrack( 0, 0 );
        setSelectedProgram( 0 );

        // Start timer for metering and playhead...
        internalTimer.setCallbackFunction( [&]() { interTimerCallbackHandler(); } );
        internalTimer.startTimerHz( 25 );
        return true;
    }

    bool MTAudioManager::closeProject()
    {
        // Persist while the programs still exist. On failure the active project,
        // playback and selection remain available for recovery and retry.
        if (!projectManager::close()) return false;
        internalTimer.stopTimer();
        projectLoaded = false;
        MTAudioTransportManager::close();

        // Call listeners
        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onProjectClose(); } );

        //DBG( "CP: Enter FM: " << engine.getFeeMemory() );
        selected = { -1, -1, -1 };

        // Save and close project first...
        vTree = juce::ValueTree{ IDs::ROOT };

        // Now close rest of the things...
        MTAudioAssetManager::close();
        MTSongManager::close();

        //DBG( "CP: FM2: " << engine.getFeeMemory() );
        engine.reset();
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        //DBG( "CP: FM3: " << engine.getFeeMemory() );

        MTAudioSequenceManager::close();
        MTAudioProgramManager::close();

        std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
        //DBG( "CP: End FM: " << engine.getFeeMemory() );

        // Reset Selection.
        setSelectedSequenceAndTrack( -1, -1 );
        setSelectedProgram( -1 );
        return true;
    }

    bool MTAudioManager::saveProject()
    {
        return projectManager::save();
    }
    ////////////////////////////
    //// Undo/Redo..
    ////////////////////////////
    void MTAudioManager::performUndo()
    {
        if( undoManager.canUndo() )
            undoManager.undo();
    }

    void MTAudioManager::performRedo()
    {
        if( undoManager.canRedo() )
            undoManager.redo();
    }

    ////////////////////////////
    //// Listeners..
    ////////////////////////////
    void MTAudioManager::addListener( MTAudioInterfaceListener* l )
    {
        listeners.add( l );
        if( l )
        {
            l->onSequenceListChanged();
            l->onTrackListChanged( selected.sequence );
            l->onProgramListChanged();
            l->onProgramSelectionChange( selected );
            l->onSequenceSelectionChange( selected );
            l->onTrackSelectionChange( selected );
        }
    }

    void MTAudioManager::removeListener( MTAudioInterfaceListener* l ) { listeners.remove( l ); }

    ////////////////////////////
    //// Selection
    ////////////////////////////
    void MTAudioManager::setSelected( MTSelected Selected )
    {
        setSelectedProgram( Selected.program );
        setSelectedSequenceAndTrack( Selected.sequence, Selected.track );
    }

    ////////////////////////////
    //// Creators
    ////////////////////////////
    MTSampleEditorInterface* MTAudioManager::createSampleEditor()
    {
        MTSampleEditorInterface* sei = new MTSampleEditorInterface;
        return sei;
    }

    juce::AudioDeviceManager& MTAudioManager::getAudioDeviceManager()
    {
        return dm->getJuceAudioDeviceMgr();
    }

    void MTAudioManager::triggerTrackNotesChanged( int Sequence, int Track )
    {
        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onTrackNoteChanged( Sequence, Track ); } );
    }

    void MTAudioManager::triggerTrackProgramChanged( int Track, MTIProgram* Program )
    {
        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onTrackProgramChanged( Track, getProgramIndex( Program ) ); } );
    }

    ////////////////////////////
    //// Preview Utilities
    ////////////////////////////
    void MTAudioManager::createPreview( const juce::File& F, float Gain )
    {
        _previewer->setGain( (double)Gain );
        _previewer->previewOn( F.getFullPathName() );
    }

    void MTAudioManager::stopPreview() { _previewer->previewOff(); }
    void MTAudioManager::loopPreview( bool Loop ) { _previewer->setLooping( Loop ); }
    void MTAudioManager::setPreviewMute( bool Mute ) { _previewer->setMute( Mute ); }
    bool MTAudioManager::isPreviewRunning() { return _previewer->isRunning(); }
    bool MTAudioManager::isPreviewLooping() { return _previewer->isLooping(); }
    bool MTAudioManager::isPreviewMuted() { return _previewer->isMuted(); }

    void MTAudioManager::setPreviewGain( float Gain ) { _previewer->setGain( (double)Gain ); }

    MTMeterData MTAudioManager::readPreviewMeter()
    {
        return engine.readMeter( _previewer->getId() );
    }

    ////////////////////////////
    //// Bank
    ////////////////////////////
    void MTAudioManager::setBank( const juce::String& Bank )
    {
        auto b     = Bank.toUpperCase();
        auto index = (int)( *b.getCharPointer() - 'A' );
        if( 0 <= index && index < 8 )
        {
            bank = Bank;
            signals::bankChanged( index, bank );
        }
    }

    ////////////////////////////
    //// Bank
    ////////////////////////////
    void MTAudioManager::interTimerCallbackHandler()
    {
        MTAudioTransportManager::updatePlayheads();
        MTAudioSequenceManager::processMidiMeters();
        MTAudioProgramManager::processMidiMeters();

        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onPlayHeadChanged( MTAudioSequenceManager::getPlayhead().bnb ); } );
        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onTrackMetersUpdated(); } );
    }

    //////////////////////////
    //// Song Utilities
    /////////////////////////

    void MTAudioManager::playSong( const juce::String& SongName, int SequenceIndexToPlay )
    {
        auto songInfo = getSongInfo( SongName );
        if( songInfo.steps.size() && SequenceIndexToPlay < songInfo.steps.size() )
            setupSong( songInfo, SequenceIndexToPlay ); // Setting up the queue
    }

    void MTAudioManager::resumeSong()
    {
        resume( getSelectedSequence() );
    }

    void MTAudioManager::stopSong()
    {
        stop();
    }
}
