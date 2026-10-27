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

    MTAudioProgramManager::MTAudioProgramManager( MTSelected& Selected, AudIO::DawEngine& Engine )
        : selected( Selected ), engine( Engine )
    {
        // Reserve default storage size.
        programs.reserve( 64 );

        // Connect callbacks...
        programNameChanged.connect( [&]( MTIProgram* ) { updateProgramList(); } );
    }

    MTAudioProgramManager::~MTAudioProgramManager()
    {
        programs.clear();
    }

    void MTAudioProgramManager::close()
    {
        vTree       = juce::ValueTree{ IDs::PROGRAMS };
        undoManager = nullptr;

        MTIProgram::resetSoloingPrograms();
        programs.clear();
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        updateProgramList();
    }

    void MTAudioProgramManager::setup( juce::ValueTree VT, juce::UndoManager* UM )
    {
        vTree       = VT;
        undoManager = UM;

        // Initialize programs....
        for( auto it : vTree )
        {
            if( it.hasType( IDs::PROGRAM ) )
            {
                createProgram( it, false );
            }
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        updateProgramList();
    }

    ////////////////////////////
    //// Selection
    ////////////////////////////
    void MTAudioProgramManager::setSelectedProgram( int num )
    {
        auto selection = AudIO::utils::limit<int>( -1, (int)programs.size() - 1, num );
        auto prev      = selected.program;
        sprog          = "--";

        if( juce::isPositiveAndBelow( selection, (int)programs.size() ) )
        {
            selected.program = selection;
            sprog            = programs[selection]->getName();
        }

        if( ( prev != selected.program ) && progSelectionChanged )
        {
            progSelectionChanged();
        }
    }

    void MTAudioProgramManager::setSelectedProgram( const juce::String& Program )
    {
        setSelectedProgram( getProgramIndexByName( Program ) );
    }

    ////////////////////////////
    //// Program
    ////////////////////////////

    MTIProgram* MTAudioProgramManager::createProgram( juce::ValueTree& VT, bool addToTree )
    {
        MTIProgram* prog = nullptr;
        auto        type = toEnum<MTProgramType>( (int)VT.getProperty( IDs::type ) );

        if( type == MTProgramType::Drum ) prog = new MTDrumProgram( engine, VT, undoManager );
        if( type == MTProgramType::Keygroup ) prog = new MTKeygroupProgram( engine, VT, undoManager );
        if( type == MTProgramType::Clip ) prog = new MTClipProgram( engine, VT, undoManager );
        if( type == MTProgramType::Midi ) prog = new MTMidiProgram( engine, VT, undoManager );
        if( type == MTProgramType::Cv ) prog = new MTCVProgram( engine, VT, undoManager );
        if( type == MTProgramType::Audio ) prog = new MTAudioProgram( engine, VT, undoManager );
        if( type == MTProgramType::Plugin ) prog = new MTClapProgram( engine, VT, undoManager );

        if( prog )
        {
            programs.emplace_back( prog );

            // Add this program to project file.
            if( addToTree ) vTree.appendChild( VT, undoManager );
            // Update list and call listener...
            updateProgramList();
        }
        return prog;
    }

    MTIProgram* MTAudioProgramManager::createProgram( MTProgramType Type, const juce::String& Name )
    {
        // Return program if it already exists...
        if( auto prg = getProgramByName( Name ) )
            return prg;

        // Create new program...
        juce::ValueTree vt = createVTree( IDs::PROGRAM,
                                          IDs::name, Name,
                                          IDs::type, toInt( Type ) );

        auto prog = createProgram( vt, true );
        if( 1 == programs.size() )
            setSelectedProgram( 0 );

        return prog;
    }

    int MTAudioProgramManager::deleteProgram( int Index )
    {
        if( juce::isPositiveAndBelow( Index, programs.size() ) )
        {
            auto selectedProgram = getSelectedProgram();

            auto& ptr = programs[(size_t)Index];
            programBeingDeleted( ptr.get() );
            vTree.removeChild( ptr->getState(), undoManager );
            programs.erase( programs.begin() + Index );

            // If we are deleting currently selected program.
            if( selectedProgram == Index )
                setSelectedProgram( --selectedProgram );

            // Update list and call listener...
            updateProgramList();
            return 0;
        }
        return -1;
    }

    int MTAudioProgramManager::deleteProgram( const juce::String& Name )
    {
        return deleteProgram( getProgramIndexByName( Name ) );
    }

    int MTAudioProgramManager::duplicateProgram( int program )
    {
        if( auto prog = getProgram( program ) )
        {
            if (!prog->prepareToSave()) return -1;
            auto name = prog->getName() + "_copy";
            auto vt   = prog->getState().createCopy();

            vt.setProperty( IDs::name, name, nullptr );
            vt.setProperty( IDs::id, "", nullptr ); // this will be replaced with unique id.

            createProgram( vt, true );
            setSelectedProgram( static_cast<int>(programs.size()) - 1 );
            return 0;
        }
        return -1;
    }

    int MTAudioProgramManager::getProgramIndex( MTIProgram* program )
    {
        for( uint8_t index = 0; index < programs.size(); index++ )
        {
            if( program == programs[(size_t)index].get() )
            {
                return ( index );
            }
        }
        return ( -1 );
    }

    MTIProgram* MTAudioProgramManager::getProgram( int index )
    {
        if( juce::isPositiveAndBelow( index, programs.size() ) )
            return programs[(size_t)index].get();
        return nullptr;
    }

    MTIProgram* MTAudioProgramManager::getProgramByName( const juce::String& Name )
    {
        if( Name.isNotEmpty() )
            for( auto& it : programs )
                if( Name == it->getName() )
                    return it.get();
        return nullptr;
    }

    MTIProgram* MTAudioProgramManager::getProgramById( const juce::String& ID )
    {
        if( ID.isNotEmpty() )
            for( auto& it : programs )
                if( ID == it->getID() )
                    return it.get();
        return nullptr;
    }

    int MTAudioProgramManager::getProgramIndexByName( const juce::String& Name )
    {
        if( Name.isNotEmpty() )
            for( int index = 0; index < (int)programs.size(); ++index )
                if( Name == programs[(size_t)index]->getName() )
                    return ( index );
        return -1;
    }

    int MTAudioProgramManager::getProgramIndexById( const juce::String& ID )
    {
        if( ID.isNotEmpty() )
            for( int index = 0; index < (int)programs.size(); ++index )
                if( ID == programs[(size_t)index]->getID() )
                    return ( index );
        return -1;
    }

    juce::String MTAudioProgramManager::getProgramName( const int Index )
    {
        if( auto program = getProgram( Index ) )
            return program->getName();
        return "";
    }

    void MTAudioProgramManager::forAllPrograms( std::function<void( MTIProgram* P )> func )
    {
        if( func )
        {
            for( const auto& it : programs )
            {
                func( it.get() );
            }
        }
    }

    void MTAudioProgramManager::updateProgramList() noexcept
    {
        programList.clear();

        // Update program name list..
        forAllPrograms( [&]( MTIProgram* P ) {
            P->setIndex( (int)programList.size() );
            programList.push_back( P->getName() );
        } );

        // Call listeners...
        if( progListChanged ) progListChanged();
    }

    int MTAudioProgramManager::setLevelForSelectedProgram( int Index, int Level )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setLevel( Level );
            else
            {
                auto gain = CommonUtils::midiToGain( Level );
                prog->setNoteGain( Index, gain );
            }
        }
        return 0;
    }

    int MTAudioProgramManager::setMuteForSelectedProgram( int Index, bool Mute )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setMute( Mute );
            else
                prog->setNoteMute( Index, Mute );
        }
        return 0;
    }

    int MTAudioProgramManager::setPanForSelectedProgram( int Index, double Pan )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setPan( (float)Pan );
            else
                prog->setNotePan( Index, Pan );
        }
        return 0;
    }

    int MTAudioProgramManager::setSemiForSelectedProgram( int Index, int Semi )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setSemi( Semi );
            else
                prog->setNoteSemi( Index, Semi );
        }
        return 0;
    }

    int MTAudioProgramManager::setFineForSelectedProgram( int Index, int Fine )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setSemi( Fine );
            else
                prog->setNoteFine( Index, Fine );
        }
        return 0;
    }

    int MTAudioProgramManager::setPitchForSelectedProgram( int Index, int Semi, int Fine )
    {
        if( auto prog = getProgram( selected.program ) )
        {
            if( 0 > Index )
                prog->setPitch( Semi, Fine );
            else
                prog->setNotePitch( Index, Semi, Fine );
        }
        return 0;
    }

    void MTAudioProgramManager::processMidiMeters()
    {
        forAllPrograms( []( MTIProgram* P ) {
            P->processMidiMeter();
        } );
    }

}
