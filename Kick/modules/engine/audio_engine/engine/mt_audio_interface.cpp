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

#include "config.h"

void createSampleProject()
{
#if 0
    auto& ai = mt::getAudioInterface();

    auto s1 = ai.createSequence( "MT1stSequence" );
    //auto s2 = ai.createSequence( "MT2ndSequence" );
    //auto s3 = ai.createSequence( "MT3rdSequence" );

    auto cwd = juce::File::getCurrentWorkingDirectory();
    auto dp = ai.createProgram( mt::MTProgramType::Drum, "MTDrumProgram" );
    auto kp = ai.createProgram( mt::MTProgramType::Keygroup, "MTKeyBoardProgram" );
    auto ap = ai.createProgram( mt::MTProgramType::Audio, "MTAudioProgram" );
    juce::ignoreUnused( ap );

    // Drum program samples...
    auto path = ai.getSoundsDir().getFullPathName() + "/Cymatic/";
    ai.addSampleToProgram( dp, 0, (path + "Basses/Cymatics x S1 - Juno Bass 1 - F.wav") );
    ai.addSampleToProgram( dp, 1, (path + "Basses/Cymatics x S1 - Juno Bass 3 - A.wav") );
    ai.addSampleToProgram( dp, 2, (path + "Basses/Cymatics x S1 - Juno Bass 4 - B.wav") );
    ai.addSampleToProgram( dp, 3, (path + "Kicks/Cymatics x S1 - Kick 1.wav") );
    ai.addSampleToProgram( dp, 4, (path + "Kicks/Cymatics x S1 - Kick 4.wav") );
    ai.addSampleToProgram( dp, 5, (path + "Kicks/Cymatics x S1 - Kick 8.wav") );
    ai.addSampleToProgram( dp, 6, (path + "Kicks/Cymatics x S1 - Kick 12.wav") );
    ai.addSampleToProgram( dp, 7, (path + "Kicks/Cymatics x S1 - Kick 13.wav") );
    ai.addSampleToProgram( dp, 8, (path + "Percussion/Cymatics x S1 - Percussion 1.wav") );
    ai.addSampleToProgram( dp, 9, (path + "Percussion/Cymatics x S1 - Percussion 3.wav") );
    ai.addSampleToProgram( dp, 10, (path + "Percussion/Cymatics x S1 - Percussion 6.wav") );
    ai.addSampleToProgram( dp, 11, (path + "Percussion/Cymatics x S1 - Percussion 9.wav") );
    ai.addSampleToProgram( dp, 12, (path + "Snares/Cymatics x S1 - Snare 3.wav") );
    ai.addSampleToProgram( dp, 13, (path + "Snares/Cymatics x S1 - Snare 5.wav") );
    ai.addSampleToProgram( dp, 14, (path + "Snares/Cymatics x S1 - Snare 6.wav") );
    ai.addSampleToProgram( dp, 15, (path + "Snares/Cymatics x S1 - Snare 9.wav") );
    ai.addSampleToProgram( dp, 16, (path + "Snares/Cymatics x S1 - Snare 14.wav") );

    mt::MTSoundSampleData sample{ (path + "Snares/Cymatics x S1 - Snare 14.wav"), 2, 2, 2, 3 };
    sample.startTime = 0.0;
    ai.addSampleToProgram( dp, sample );

    ai.removeSampleFromProgram( dp, (path + "Basses/Cymatics x S1 - Juno Bass 4 - B.wav"), 2 );
    ai.addSampleToProgram( dp, mt::MTSoundSampleData( (path + "Basses/Cymatics x S1 - Percussion 3.wav"), 2, 2, 2, 1 ) );

    // Keyboard program samples...
    ai.addSampleToProgram( kp, mt::MTSoundSampleData( (path + "Basses/Cymatics x S1 - Juno Bass 3 - A.wav"), 5, 5 ) );

    ai.addSampleToProgram( ap, mt::MTSoundSampleData( (path + "Melody Loops/Cymatics x S1 - Melody Loop 5 - 125 BPM G# Min.wav") ) );

    auto k11 = ai.createTrack( s1, "1stTrack" );
    auto k12 = ai.createTrack( s1, "2ndTrack" );

    //auto t21 = ai.createTrack( s2, "1stTrack" );
    //auto k22 = ai.createTrack( s2, "2ndTrack" );
    //auto k23 = ai.createTrack( s2, "3rdTrack" );
    //
    //auto k31 = ai.createTrack( s3, "1stTrack" );
    //auto k32 = ai.createTrack( s3, "2ndTrack" );
    //auto k33 = ai.createTrack( s3, "3rdTrack" );
    //auto k34 = ai.createTrack( s3, "4thTrack" );
    //auto k35 = ai.createTrack( s3, "5thTrack" );

    ai.setProgramToTrack( dp, s1, k11 );
    ai.setProgramToTrack( kp, s1, k12 );
#endif
}

namespace mt
{
    static const std::set<std::shared_ptr<MTNote>> emptyNotes;
    static const std::vector<juce::String>         emptyList;

    sigslot::signal<>                                     signals::midiInputsChanged = {};
    sigslot::signal<int /*index*/, juce::String /*Bank*/> signals::bankChanged       = {};
    sigslot::signal<TransportState /*State*/>             signals::playStateChanged  = {};
    sigslot::signal<int /*State*/, int /*State*/>         signals::padColourChanged  = {};

    sigslot::signal<const MTSelected& /*Selected*/> signals::onProgramSelectionChange  = {};
    sigslot::signal<const MTSelected& /*Selected*/> signals::onSequenceSelectionChange = {};
    sigslot::signal<const MTSelected& /*Selected*/> signals::onTrackSelectionChange    = {};

    sigslot::signal<>              signals::onSongStopped              = {};
    sigslot::signal<int>           signals::onNextSeqAboutToPlayInSong = {};
    sigslot::signal<int, int, int> signals::onSongSequencePlay         = {};
    sigslot::signal<int>           signals::onNextSequencePlay         = {};

    sigslot::signal<int> signals::onBar       = {};
    sigslot::signal<int> signals::onBeat      = {};
    sigslot::signal<int> signals::onTick      = {};
    sigslot::signal<int> signals::onLoopBegin = {};
    sigslot::signal<int> signals::onLoopEnd   = {};
    sigslot::signal<>    signals::onNote      = {};
    sigslot::signal<>    signals::on1_2       = {};
    sigslot::signal<>    signals::on1_2T      = {};
    sigslot::signal<>    signals::on1_4       = {};
    sigslot::signal<>    signals::on1_4T      = {};
    sigslot::signal<>    signals::on1_8       = {};
    sigslot::signal<>    signals::on1_8T      = {};
    sigslot::signal<>    signals::on1_16      = {};
    sigslot::signal<>    signals::on1_16T     = {};
    sigslot::signal<>    signals::on1_32      = {};
    sigslot::signal<>    signals::on1_32T     = {};

    // API definitions
    MTAudioInterface& getAudioInterface()
    {
        // Destroy before JUCE shuts down its device, MIDI and message services.
        static auto* audioInterface = new MTAudioInterface( ProjectInfo::projectName, ProjectInfo::companyName, ProjectInfo::versionString );
        return *audioInterface;
    }

    MTAudioInterface::MTAudioInterface( const char* Name, const char* Manufacturer, const char* Version )
        : manager( std::make_unique<MTAudioManager>( Name, Manufacturer, Version ) )
        , MTLooperInterface( manager )

    {}

    MTAudioInterface::~MTAudioInterface()
    {
        manager.reset();
    }
    ///
    /// Listners
    ///
    void MTAudioInterface::addListener( MTAudioInterfaceListener* l ) { manager->addListener( l ); }
    void MTAudioInterface::removeListener( MTAudioInterfaceListener* l ) { manager->removeListener( l ); }

    ///
    /// Project API's
    ///
    bool MTAudioInterface::isProjectLoaded() { return manager->isProjectLoaded(); }
    bool MTAudioInterface::loadProject( juce::File& ProjectFile ) { return manager->loadProject( ProjectFile ); }
    bool MTAudioInterface::closeProject() { return manager->closeProject(); }
    bool MTAudioInterface::saveProject() { return manager->saveProject(); }
    juce::String MTAudioInterface::getProjectSaveError() const { return manager->getLastSaveError(); }
    bool MTAudioInterface::isProjectDirty() const { return manager->isDirty(); }

    ///
    /// Selection API's
    ///
    const MTSelected&   MTAudioInterface::getSelected() { return manager->getSelected(); }
    const juce::String& MTAudioInterface::getSelectedStr() { return manager->getSelectedStr(); }
    int                 MTAudioInterface::getSelectedSequence() const { return manager->getSelectedSequence(); }
    int                 MTAudioInterface::getSelectedTrack() const { return manager->getSelectedTrack(); }
    int                 MTAudioInterface::getSelectedProgram() const { return manager->getSelectedProgram(); }

    void MTAudioInterface::setSelected( MTSelected Selected )
    {
        manager->setSelected( Selected );
        manager->updatePlayheads();
    }

    void MTAudioInterface::setSelectedSequence( int Selected )
    {
        manager->setSelectedSequence( Selected );
        manager->updatePlayheads();
    }

    void MTAudioInterface::setSelectedSequence( const juce::String& Selected )
    {
        manager->setSelectedSequence( Selected );
        manager->updatePlayheads();
    }

    void MTAudioInterface::setSelectedTrack( int Selected )
    {
        manager->setSelectedTrack( Selected );
        manager->updatePlayheads();
    }

    void MTAudioInterface::setSelectedTrack( const juce::String& Selected )
    {
        manager->setSelectedTrack( Selected );
        manager->updatePlayheads();
    }

    void MTAudioInterface::setSelectedProgram( int Selected ) { manager->setSelectedProgram( Selected ); }
    void MTAudioInterface::setSelectedProgram( const juce::String& Selected ) { manager->setSelectedProgram( Selected ); }
    void MTAudioInterface::setSelectedSequenceAndTrack( int Sequence, int Track ) { manager->setSelectedSequenceAndTrack( Sequence, Track ); }
    void MTAudioInterface::setSelectedSequenceAndTrack( const juce::String& Sequence, const juce::String& Track ) { manager->setSelectedSequenceAndTrack( Sequence, Track ); }

    ///
    /// Bank API's
    ///
    void               MTAudioInterface::setBank( const juce::String& Bank ) { manager->setBank( Bank ); }
    const juce::String MTAudioInterface::getBank() { return manager->getBank(); }
    const int          MTAudioInterface::getBankOffset()
    {
        auto bank = manager->getBank();
        return (int)( 16 * (int)( *bank.toRawUTF8() - 'A' ) );
    }

    ///
    /// Pad Performance API's
    ///
    MTIPadPerformanceData* MTAudioInterface::getPadPerformanceData( const int Program )
    {
        if( 0 > Program )
            return manager.get();
        else
            return manager->getProgram( Program );
    }

    ///
    /// Master API's
    ///
    const int         MTAudioInterface::getMasterBPM() { return manager->getBPM(); }
    void              MTAudioInterface::setMasterBPM( const int BPM ) { return manager->setBPM( BPM ); }
    const MTBarNBeats MTAudioInterface::getMasterLength() { return manager->getLength(); }
    void              MTAudioInterface::setMasterLength( const int Bars, const double Beats ) { return manager->setLength( Bars, Beats ); }

    MTBarNBeats MTAudioInterface::toBnB( const MTSelected& Selection, unsigned int TimeInTicks )
    {
        if( auto s = manager->getSequence( Selection.sequence ) ) return s->toBnB( TimeInTicks );
        return {};
    }

    int MTAudioInterface::toTicks( const MTSelected& Selection, MTBarNBeats& Time )
    {
        if( auto s = manager->getSequence( Selection.sequence ) ) return s->toTicks( Time );
        return -1;
    }

    MTBarNBeats MTAudioInterface::toBnB( const MTSelected& Selection, double TimeInBeats )
    {
        if( auto s = manager->getSequence( Selection.sequence ) ) return s->toBnB( TimeInBeats );
        return {};
    }

    const int MTAudioInterface::getBars( int Sequence )
    {
        auto seq = 0 > Sequence ? MTAudioInterface::getSelectedSequence() : Sequence;
        if( auto s = manager->getSequence( seq ) ) return s->getLoopEnd();
        return -1;
    }

    float MTAudioInterface::toSeconds( int Sequence, MTBarNBeats& Time )
    {
        auto seq = 0 > Sequence ? MTAudioInterface::getSelectedSequence() : Sequence;
        if( auto s = manager->getSequence( seq ) ) return s->getLengthInSeconds( Time );
        return 0.f;
    }

    MTTempoMgr* MTAudioInterface::getTempoMgr( int Sequence )
    {
        return manager->getSequence( Sequence );
    }

    std::vector<juce::String>& split( const std::string& s, char delim, std::vector<juce::String>& elems )
    {
        std::stringstream ss( s );
        std::string       item;
        while( std::getline( ss, item, delim ) )
        {
            elems.push_back( item );
        }
        return elems;
    }

    // Program
    int MTAudioInterface::createProgram( MTProgramType Type, const juce::String& Name )
    {
        if( auto program = manager->createProgram( Type, Name ) )
            return manager->getProgramIndex( program );
        return -1;
    }

    int MTAudioInterface::deleteProgram( int Index )
    {
        return manager->deleteProgram( Index );
    }

    int MTAudioInterface::deleteProgram( const juce::String& Name )
    {
        return manager->deleteProgram( Name );
    }

    int MTAudioInterface::renameProgram( int Program, const juce::String& NewName )
    {
        if( auto p = getProgram( Program ) ) return p->setName( NewName );
        return -1;
    }

    void MTAudioInterface::setKeyGroupPadColour( MTPadColors padData )
    {
        clearAllPadColour();
        keyGroupPadColour = padData;
    }

    void MTAudioInterface::clearAllPadColour( void )
    {
        for( int index = 0; index < 128; index++ )
            keyGroupPadColour.Colours[index] = 0;
    }

    int MTAudioInterface::getProgramIndexByName( const juce::String& ProgramName )
    {
        return manager->getProgramIndexByName( ProgramName );
    }

    int MTAudioInterface::getProgramIndexByID( const juce::String& ProgramID )
    {
        return manager->getProgramIndexById( ProgramID );
    }

    juce::String MTAudioInterface::getProgramNameByIndex( int ProgramIndex )
    {
        return manager->getProgramName( ProgramIndex );
    }

    int MTAudioInterface::duplicateProgram( int program )
    {
        return manager->duplicateProgram( program );
    }

    int MTAudioInterface::setLevelForSelectedProgram( int Index, int Level )
    {
        return manager->setLevelForSelectedProgram( Index, Level );
    }

    int MTAudioInterface::setMuteForSelectedProgram( int Index, bool Mute )
    {
        return manager->setMuteForSelectedProgram( Index, Mute );
    }

    int MTAudioInterface::setPanForSelectedProgram( int Index, double Pan )
    {
        return manager->setPanForSelectedProgram( Index, Pan );
    }

    int MTAudioInterface::setPitchForSelectedProgram( int Index, int Semi, int Fine )
    {
        return manager->setPitchForSelectedProgram( Index, Semi, Fine );
    }

    void MTAudioInterface::setSimultaneousNotePlay( int Program, int Note, const std::vector<int>& PlayNotes )
    {
        if( auto p = getProgram( Program ) )
        {
            switch( p->getType() )
            {
                case MTProgramType::Drum: return dynamic_cast<MTDrumProgram*>( p )->setSimultaneousNotePlay( Note, PlayNotes );
                case MTProgramType::Keygroup: return dynamic_cast<MTKeygroupProgram*>( p )->setSimultaneousNotePlay( Note, PlayNotes );
                case MTProgramType::Clip: return; // dynamic_cast<MTClipProgram*>( p )->setSimultaneousNotePlay( Note, PlayNotes );
            }
        }
    }

    void MTAudioInterface::setSimultaneousNoteMute( int Program, int Note, const std::vector<int>& MuteTargets )
    {
        if( auto p = getProgram( Program ) )
        {
            switch( p->getType() )
            {
                case MTProgramType::Drum: return dynamic_cast<MTDrumProgram*>( p )->setSimultaneousNoteMute( Note, MuteTargets );
                case MTProgramType::Keygroup: return dynamic_cast<MTKeygroupProgram*>( p )->setSimultaneousNoteMute( Note, MuteTargets );
                case MTProgramType::Clip: return; // dynamic_cast<MTClipProgram*>( p )->setSimultaneousNoteMute( Note, MuteTargets );
            }
        }
    }

    void MTAudioInterface::addSampleToProgram( int Program, int KeyNote, const juce::String& Sample, int Slice, int Layer )
    {
        if( auto p = getProgram( Program ) )
        {
            switch( p->getType() )
            {
                case MTProgramType::Drum: return dynamic_cast<MTDrumProgram*>( p )->addSample( KeyNote, Sample, Slice, Layer );
                case MTProgramType::Keygroup: return dynamic_cast<MTKeygroupProgram*>( p )->addSample( KeyNote, Sample, Slice, Layer );
                case MTProgramType::Clip: return dynamic_cast<MTClipProgram*>( p )->addSample( KeyNote, Sample, Slice, Layer );
            }
        }
    }

    void MTAudioInterface::removeSampleFromProgram( int Program, int KeyNote )
    {
        if( auto p = getProgram( Program ) )
        {
            p->removeSample( KeyNote );
        }
    }

    void MTAudioInterface::removeSampleFromProgram( int Program, int KeyNote, const juce::String& Sample )
    {
        if( auto p = getProgram( Program ) )
        {
            switch( p->getType() )
            {
                case MTProgramType::Drum: return dynamic_cast<MTDrumProgram*>( p )->removeSample( KeyNote, Sample );
                case MTProgramType::Keygroup: return dynamic_cast<MTKeygroupProgram*>( p )->removeSample( KeyNote, Sample );
                case MTProgramType::Clip: return dynamic_cast<MTClipProgram*>( p )->removeSample( KeyNote, Sample );
            }
        }
    }

    int MTAudioInterface::getNumProgramSamples( int Program )
    {
        if( auto p = getProgram( Program ) ) return p->getNumSamples();
        return 0;
    }

    void MTAudioInterface::getProgramSamples( int Program, std::vector<MTISample*>& Samples )
    {
        if( auto p = getProgram( Program ) )
            p->getSamples( Samples );
    }

    std::vector<juce::String> MTAudioInterface::getProgramSampleNames( int program )
    {
        if( auto p = manager->getProgram( program ) )
            return p->getSampleNames();
        return {};
    }
    std::vector<juce::String> MTAudioInterface::getProgramSampleNames( const juce::String& programName )
    {
        if( auto p = manager->getProgramByName( programName ) )
            return p->getSampleNames();
        return {};
    }

    SamplePlayType MTAudioInterface::getSamplePlayType( int Program, int Note )
    {
        if( auto p = getProgram( manager->getSelectedProgram() ) )
            return p->getNoteSamplePlay( Note );
        return SamplePlayType::ONE_SHOT;
    }

    SamplePlayType MTAudioInterface::getSamplePlayType( int Note )
    {
        return getSamplePlayType( manager->getSelectedProgram(), Note );
    }

    void MTAudioInterface::copyPadDataToPad( int SourceProgram, int DestinationProgram, int SourcePad, int DestinationPad, int Layer )
    {
        if( auto pS = getProgram( SourceProgram ) )
        {
            if( auto pD = getProgram( DestinationProgram ) )
            {
                juce::String source{ "" };
                int          slice{ 0 };
                switch( pS->getType() )
                {
                    case MTProgramType::Drum:
                        source = dynamic_cast<MTDrumProgram*>( pS )->getLayerSource( SourcePad, Layer );
                        slice  = dynamic_cast<MTDrumProgram*>( pS )->getLayerSlice( SourcePad, Layer );

                        break;
                    case MTProgramType::Keygroup:
                        source = dynamic_cast<MTKeygroupProgram*>( pS )->getLayerSource( SourcePad, Layer );
                        slice  = dynamic_cast<MTKeygroupProgram*>( pS )->getLayerSlice( SourcePad, Layer );
                        break;
                    case MTProgramType::Clip:
                        source = dynamic_cast<MTClipProgram*>( pS )->getLayerSource( SourcePad, Layer );
                        slice  = dynamic_cast<MTClipProgram*>( pS )->getLayerSlice( SourcePad, Layer );
                        break;
                }
                if( source.isNotEmpty() )
                {
                    switch( pD->getType() )
                    {
                        case MTProgramType::Drum: return dynamic_cast<MTDrumProgram*>( pD )->addSample( DestinationPad, source, slice, Layer );
                        case MTProgramType::Keygroup: return dynamic_cast<MTKeygroupProgram*>( pD )->addSample( DestinationPad, source, slice, Layer );
                        case MTProgramType::Clip: return dynamic_cast<MTClipProgram*>( pD )->addSample( DestinationPad, source, slice, Layer );
                    }
                }
            }
        }
    }

    void MTAudioInterface::copyPadsDataToBank( int SourceProgram, int DestinationProgram, int SourceBank, int DestinationBank, int Layer )
    {
        if( auto pS = getProgram( SourceProgram ) )
        {
            if( auto pD = getProgram( DestinationProgram ) )
            {
                auto sourceStartPad      = SourceBank * 16;
                auto destinationStartPad = DestinationBank * 16;
                for( auto it = sourceStartPad; it < sourceStartPad + 16; ++it, ++destinationStartPad )
                    copyPadDataToPad( SourceProgram, DestinationProgram, it, destinationStartPad, Layer );
            }
        }
    }

    void MTAudioInterface::copyPadsDataToRange( int SourceProgram, int DestinationProgram, int SourceStartPad, int SourceEndPad, int DestinationStartPad, int Layer )
    {
        if( auto pS = getProgram( SourceProgram ) )
        {
            if( auto pD = getProgram( DestinationProgram ) )
            {
                for( auto it = SourceStartPad; it <= SourceStartPad + SourceEndPad; ++it, ++DestinationStartPad )
                    copyPadDataToPad( SourceProgram, DestinationProgram, it, DestinationStartPad );
            }
        }
    }

    int MTAudioInterface::getNumPrograms()
    {
        return (int)manager->getPrograms().size();
    }

    MTProgramType MTAudioInterface::getProgramType( const int program )
    {
        if( auto p = manager->getProgram( program ) )
            return p->getType();
        return MTProgramType::Unknown;
    }

    MTProgramType MTAudioInterface::getProgramType( const juce::String& program )
    {
        if( auto p = manager->getProgramByName( program ) )
            return p->getType();
        return MTProgramType::Unknown;
    }

    MTIProgramData* MTAudioInterface::getProgramDataForTrack( int Seq, int Track )
    {
        auto t = manager->getTrack( Seq, Track );
        if( t )
            return dynamic_cast<MTIProgramData*>( t->getProgram() );
        return nullptr;
    }

    MTIProgram* MTAudioInterface::getProgramForTrack( int Seq, int Track )
    {
        auto t = manager->getTrack( Seq, Track );
        if( t )
            return t->getProgram();
        return nullptr;
    }

    juce::ValueTree MTAudioInterface::getProgramVT( int Program )
    {
        if( auto p = getProgram( Program ) )
            return p->getState();
        return {};
    }

    MTIProgram* MTAudioInterface::getProgram( int Program )
    {
        return manager->getProgram( Program );
    }

    MTIProgram* MTAudioInterface::getProgram( const juce::String& Program )
    {
        return manager->getProgram( getProgramIndexByName( Program ) );
    }

    MTIProgramData* MTAudioInterface::getProgramData( int Program )
    {
        return dynamic_cast<MTIProgramData*>( getProgram( Program ) );
    }

    MTIProgramData* MTAudioInterface::getProgramData( const juce::String& Program )
    {
        return dynamic_cast<MTIProgramData*>( getProgram( Program ) );
    }

    MTIProgramData* MTAudioInterface::getProgramDataForProgramID( const juce::String& ProgramID )
    {
        auto id = getProgramIndexByID( ProgramID );
        return getProgramData( id );
    }

    MTIProgramData* MTAudioInterface::getSelectedProgramData()
    {
        return dynamic_cast<MTIProgramData*>( getProgram( getSelectedProgram() ) );
    }

    int MTAudioInterface::getProgramList( std::vector<MTIProgramData*>& List )
    {
        List.clear();
        for( const auto& p : manager->getPrograms() )
        {
            List.push_back( p.get() );
        }
        return 0;
    }

    const std::vector<juce::String>& MTAudioInterface::getProgramList() const noexcept
    {
        return manager->getProgramList();
    }

    int MTAudioInterface::setProgramColourMode( int Program, PadColorMode Mode )
    {
        if( auto p = getProgram( Program ) )
            return p->setColorMode( Mode );
        return -1;
    }

    int MTAudioInterface::setProgramEmptyPadColorMode( int Program, EmptyPadColorMode Mode )
    {
        if( auto p = getProgram( Program ) )
            return p->setEmptyColorMode( Mode );
        return -1;
    }

    int MTAudioInterface::setProgramPadColour( int Program, int Colour, int Pad )
    {
        if( auto p = getProgram( Program ) )
            return p->setPadColor( Colour, Pad );
        return -1;
    }

    std::vector<juce::String> MTAudioInterface::getProgramNames( MTProgramCategory cateogryType )
    {
        std::vector<std::unique_ptr<MTIProgram>> const& programData = manager->getPrograms();
        std::vector<juce::String>                       programs;

        for( const auto& it : programData )
        {
            if( cateogryType == MTProgramCategory::Audio && it->getType() == MTProgramType::Audio )
                programs.push_back( it->getName() );
            else if( cateogryType == MTProgramCategory::Midi && it->getType() != MTProgramType::Audio )
                programs.push_back( it->getName() );
            else if( cateogryType == MTProgramCategory::Unused )
                programs.push_back( it->getName() );
        }
        return programs;
    }

    int MTAudioInterface::setProgramToTrack( int Program, int Sequence, int Track )
    {
        auto p = getProgram( Program );
        auto t = manager->getTrack( Sequence, Track );
        if( p && t )
        {
            t->setProgram( p );
            return 0;
        }
        return -1;
    }

    int MTAudioInterface::setProgramToTrack( const juce::String& Program, int Sequence, int Track )
    {
        auto p = manager->getProgramByName( Program );
        auto t = manager->getTrack( Sequence, Track );
        if( p && t )
        {
            t->setProgram( p );
            return 0;
        }
        return -1;
    }

    // Pad Mutes
    bool MTAudioInterface::togglePadMute( const int Program, const int Pad )
    {
        if( auto prog = getProgram( Program ) )
            return prog->toggleNoteMute( Pad );
        return false;
    }

    int MTAudioInterface::padMute( const int Program, const int Pad, bool Mute )
    {
        if( auto prog = getProgram( Program ) )
        {
            prog->setNoteMute( Pad, Mute );
            return 0;
        }
        return -1;
    }

    juce::Array<int>* MTAudioInterface::getPadMutes( const int Program )
    {
        auto p = getProgram( Program );
        //if( p ) return &p->getPadMutes();
        return nullptr;
    }

    void MTAudioInterface::triggerPadMuteStatusRefresh( const int Program )
    {
        auto p = getProgram( Program );
        //if( p ) p->triggerPadMutes();
    }

    int MTAudioInterface::padAddToGroup( const int Program, const int Pad, const int Group )
    {
        if( auto prog = getProgram( Program ) )
        {
            prog->setNoteMuteGroup( Pad, Group );
            return 0;
        }
        return -1;
    }

    int MTAudioInterface::padRemoveFromGroup( const int Program, const int Pad )
    {
        if( auto prog = getProgram( Program ) )
        {
            prog->setNoteMuteGroup( Pad, -1 );
            return 0;
        }
        return -1;
    }

    // Sequence

    int MTAudioInterface::createSequence( const juce::String& Name )
    {
        return manager->createSequence( Name );
    }

    int MTAudioInterface::deleteSequence( int Index )
    {
        return manager->deleteSequence( Index );
    }

    int MTAudioInterface::deleteSequence( const juce::String& Name )
    {
        return manager->deleteSequence( Name );
    }

    int MTAudioInterface::getNumSequences()
    {
        return manager->getNumSequences();
    }

    int MTAudioInterface::copySequence( int Index )
    {
        return manager->copySequence( Index );
    }
    int MTAudioInterface::clearSequence( int Index )
    {
        return manager->clearSequence( Index );
    }

    int MTAudioInterface::renameSequence( int Sequence, const juce::String& Name )
    {
        if( auto s = manager->getSequence( Sequence ) )
            return s->setName( Name.toStdString() );
        return -1;
    }

    int MTAudioInterface::getSequenceDataList( std::vector<MTISequenceData*>& List )
    {
        return manager->getSequenceDataList( List );
    }

    const std::vector<juce::String>& MTAudioInterface::getSequenceList() const noexcept
    {
        return manager->getSequenceList();
    }

    MTISequenceData* MTAudioInterface::getSequenceData( int Sequence )
    {
        return dynamic_cast<MTISequenceData*>( manager->getSequence( Sequence ) );
    }

    juce::ValueTree& MTAudioInterface::getSequenceState( int Sequence )
    {
        return manager->getSequenceState( Sequence );
    }

    juce::ValueTree& MTAudioInterface::getSequenceState( const juce::String& Name )
    {
        return manager->getSequenceState( Name );
    }

    juce::ValueTree MTAudioInterface::getSequenceTempoState( int Sequence, int Tempo )
    {
        return manager->getSequenceTempoState( Sequence, Tempo );
    }

    int MTAudioInterface::setSequenceBPM( int Sequence, int Bpm )
    {
        if( auto s = manager->getSequence( Sequence ) ) return s->setBPM( Bpm );
        return -1;
    }

    int MTAudioInterface::setSequenceLength( int Sequence, int Bars )
    {
        return manager->setSequenceLength( Sequence, Bars );
    }

    int MTAudioInterface::setSequenceLength( int Sequence, float Multiplier )
    {
        return manager->setSequenceLength( Sequence, Multiplier );
    }

    int MTAudioInterface::copyTrackBars( const SequenceCopyBars& copyBars )
    {
        std::vector<MTNote> noteList;
        MTSelected          selection;
        selection.sequence = copyBars.sourceSequence;
        for( auto trackIndex = 0; trackIndex < getNumTracks( selection.sequence ); trackIndex++ )
        {
            selection.track = trackIndex;
            getNotesInRange( selection, noteList, copyBars.noteRange );
            auto s = manager->getSequence( copyBars.destSequence );
            if( s ) s->copyTrackBars( trackIndex, copyBars, noteList );
            noteList.clear();
        }
        return 1;
    }
    int MTAudioInterface::modifySequenceLength( int Sequence, int ModifyBars, bool Insert, int AtBarPosition )
    {
        auto s = manager->getSequence( ( Sequence ) );
        if( s ) return s->modifySequenceLength( ModifyBars, Insert, AtBarPosition );
        return -1;
    }

    int MTAudioInterface::setTimeSignature( int /*Sequence*/, int /*Numerator*/, int /*Denominator*/ )
    {
        return -1;
    }

    int MTAudioInterface::setSequenceLoop( int Sequence, bool Loop )
    {
        if( auto s = manager->getSequence( Sequence ) ) return s->setLooping( Loop );
        return -1;
    }

    int MTAudioInterface::setSequenceLoopStart( int Sequence, int StartBar )
    {
        if( auto s = manager->getSequence( Sequence ) ) return s->setLoopStart( StartBar );
        return -1;
    }

    int MTAudioInterface::setSequenceLoopEnd( int Sequence, int EndBar )
    {
        if( auto s = manager->getSequence( Sequence ) ) return s->setLoopEnd( EndBar );
        return -1;
    }

    int MTAudioInterface::setSequenceLoopPoints( int Sequence, int StartBar, int EndBar )
    {
        if( auto s = manager->getSequence( Sequence ) ) return s->setLoopPoints( StartBar, EndBar );
        return -1;
    }

    int MTAudioInterface::setSequenceLengthToMaster( int Sequence, bool UseMaster )
    {
        auto s = manager->getSequence( Sequence );
        //if( s ){ return s->useMasterLength( UseMaster ); }
        return -1;
    }
    int MTAudioInterface::setSequenceBPMToMaster( int Sequence, bool UseMaster )
    {
        if( auto s = manager->getSequence( Sequence ) ) { return s->useMasterBPM( UseMaster ); }
        return -1;
    }

    int MTAudioInterface::sequenceBounceToSample( int Sequence, juce::File& File )
    {
        //if( auto s = manager->getSequence( Sequence ) )
        //    return s->bounceToSample( File );
        return -1;
    }

    double MTAudioInterface::getSequenceLengthinMs()
    {
        if( auto s = manager->getSequence() ) return s->getLengthInMS();
        return 0.0;
    }

    int MTAudioInterface::getSequenceLength()
    {
        if( auto s = manager->getSequence() ) return s->getLength();
        return 1;
    }

    int MTAudioInterface::getSequenceLength( int Sequence )
    {
        if( auto s = manager->getSequence( Sequence ) ) return s->getLength();
        return 1;
    }

    int MTAudioInterface::getSequenceLoopEnd()
    {
        if( auto s = manager->getSequence() ) return s->getLoopEnd();
        return 1;
    }

    int MTAudioInterface::getSequenceLoopEnd( int Sequence )
    {
        if( auto s = manager->getSequence( Sequence ) ) return s->getLoopEnd();
        return 1;
    }

    bool MTAudioInterface::getLoopingState()
    {
        if( auto s = manager->getSequence() ) return s->getLoopingState();
        return false;
    }

    // Playhead...
    MTPlayhead& MTAudioInterface::getPlayhead()
    {
        return manager->getPlayhead();
    }

    MTPlayhead& MTAudioInterface::getTrackPlayhead()
    {
        return manager->getTrackPlayhead();
    }

    void MTAudioInterface::seek( int posInTicks )
    {
        manager->seek( posInTicks );
    }

    void MTAudioInterface::setBBT( int knob, int value )
    {
        manager->getSequence()->setBBT( knob, value );
    }

    // Transport API's
    int MTAudioInterface::setSequencePlay( int Sequence, bool FromBeginning, bool Now )
    {
        return manager->play( Sequence, FromBeginning, Now );
    }

    int MTAudioInterface::setSequencePause()
    {
        return manager->pause();
    }

    int MTAudioInterface::setSequenceStop()
    {
        return manager->stop();
    }

    bool MTAudioInterface::isSequencePlaying()
    {
        return manager->isPlaying();
    }

    void MTAudioInterface::seqAddTrackToGroup( const int Sequence, const int Track, const int Group )
    {
        if( auto s = manager->getTrack( Sequence, Track ) )
            s->setMuteGroup( Group );
    }

    void MTAudioInterface::seqRemoveTrackFromGroup( const int Sequence, const int Track )
    {
        if( auto s = manager->getTrack( Sequence, Track ) )
            s->setMuteGroup( -1 );
    }

    // Track

    int MTAudioInterface::createTrack( int Sequence, const juce::String& Name )
    {
        return manager->createTrack( Sequence, Name );
    }

    int MTAudioInterface::deleteTrack( int Sequence, int Index )
    {
        return manager->deleteTrack( Sequence, Index );
    }

    int MTAudioInterface::deleteTrack( int Sequence, const juce::String& Name )
    {
        return manager->deleteTrack( Sequence, Name );
    }

    int MTAudioInterface::getNumTracks( int Sequence )
    {
        if( auto s = manager->getSequence( Sequence ) )
            return s->getNumTracks();
        return 0;
    }

    int MTAudioInterface::getTrackIndexByName( int Sequence, const juce::String& Track )
    {
        if( auto s = manager->getSequence( Sequence ) )
        {
            if( auto t = s->getTrack( Track ) )
                return t->getIndex();
        }
        return -1;
    }

    const juce::Array<juce::String> MTAudioInterface::getValidRoutableTracks( int Sequence, int Track )
    {
        if( auto s = manager->getSequence( Sequence ) )
            return s->getValidRoutableTrackList( Track );
        return {};
    }

    const juce::Array<juce::String> MTAudioInterface::getValidRoutableTracks( int Sequence, const juce::String& Track )
    {
        return getValidRoutableTracks( Sequence, getTrackIndexByName( Sequence, Track ) );
    }

    const int MTAudioInterface::getNumMidiTracks( int Sequence )
    {
        if( auto s = manager->getSequence( Sequence ) )
            return s->getNumMidiTracks();
        return 0;
    }

    const juce::Array<int>& MTAudioInterface::getMidiTrackIndexes( int Sequence )
    {
        static const juce::Array<int> _dummyIndexes;
        if( auto s = manager->getSequence( Sequence ) )
            return s->getMidiTrackIndexes();
        return _dummyIndexes;
    }

    int MTAudioInterface::resetTrackSolos()
    {
        if( auto s = manager->getSequence() )
            return s->resetTrackSolos();
        return -1;
    }

    void MTAudioInterface::renameTrack( int Sequence, int Track, const juce::String& Name )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            t->setName( Name );
    }

    int MTAudioInterface::getTrackListData( int Sequence, std::vector<MTITrackData*>& List )
    {
        if( auto s = manager->getSequence( Sequence ) )
        {
            List.clear();
            return s->forAllTracks( [&]( MTTrack* T ) {
                List.push_back( dynamic_cast<MTITrackData*>( T ) );
            } );
        }
        return -1;
    }

    int MTAudioInterface::getTrackListData( std::vector<MTITrackData*>& List )
    {
        return getTrackListData( getSelectedSequence(), List );
    }

    const std::vector<juce::String>& MTAudioInterface::getTrackList( int Sequence ) const noexcept
    {
        if( auto s = manager->getSequence( Sequence ) )
            return s->getTrackList();
        return emptyList;
    }

    MTITrackData* MTAudioInterface::getTrackData( int Sequence, int Track )
    {
        return dynamic_cast<MTITrackData*>( manager->getTrack( Sequence, Track ) );
    }

    const MTMeterData& MTAudioInterface::getTrackMeter( int Sequence, int Track )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            return t->getMeter();
        return AudIO::_dummyMeter;
    }

    juce::ValueTree& MTAudioInterface::getTrackState( int Sequence, int Track )
    {
        return manager->getTrackState( Sequence, Track );
    }

    juce::ValueTree& MTAudioInterface::getTrackState( int Sequence, const juce::String& Name )
    {
        return manager->getTrackState( Sequence, Name );
    }

#define copyTrackHelper( ft, tt )    \
    if( ft && tt && ft != tt )       \
    {                                \
        auto notes = ft->getNotes(); \
        tt->setNote( notes, true );  \
        return 0;                    \
    }                                \
    return -1;

    int MTAudioInterface::copyTrack( int Sequence, int From, int To )
    {
        auto ft = manager->getTrack( Sequence, From );
        auto tt = manager->getTrack( Sequence, To );
        //copyTrackHelper( ft, tt );
        return -1;
    }

    int MTAudioInterface::copyTrack( int Sequence, const juce::String& From, const juce::String& To )
    {
        return manager->copyTrack( Sequence, From, To );
        return -1;
    }

    void MTAudioInterface::setTrackToUseSeqLength( int Sequence, int Track, bool UseSeqLength )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            t->setUsingSeqLength( UseSeqLength );
    }

    int MTAudioInterface::getTrackLength( int Sequence, int Track )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            return t->getLength();
        return 0;
    }

    void MTAudioInterface::setTrackLength( int Sequence, int Track, int Bars )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            t->setLength( Bars );
    }

    int MTAudioInterface::getTrackLevel( int Sequence, int Track )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            return t->getLevel();
        return 0;
    }

    void MTAudioInterface::setTrackLevel( int Sequence, int Track, int Level )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            t->setLevel( Level );
    }
    void MTAudioInterface::setTrackTranspose( int Sequence, int Track, int Transpose )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            t->setTranspose( Transpose );
    }

    void MTAudioInterface::setTrackMute( int Sequence, int Track, bool Mute )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            t->setMute( Mute );
    }

    bool MTAudioInterface::getTrackMute( int Sequence, int Track )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
        {
            return t->isMute();
        }
        return false;
    }

    int MTAudioInterface::setTrackSolo( int Sequence, int Track, bool Solo )
    {
        if( auto s = manager->getSequence( Sequence ) )
            return s->setTrackSolo( Track, Solo );
        return -1;
    }

    bool MTAudioInterface::getTrackSolo( int Sequence, int Track )
    {
        if( auto s = manager->getSequence( Sequence ) )
            return s->isTrackSolo( Track );
        return false;
    }

    int MTAudioInterface::getNumSoloTracks( int Sequence )
    {
        if( auto s = manager->getSequence( Sequence ) )
            return s->getNumSoloTracks();
        return 0;
    }

    void MTAudioInterface::setTrackMidiPort( int Sequence, int Track, const juce::String& MidiPort )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            t->setMidiPort( MidiPort );
    }

    void MTAudioInterface::setTrackMidiChannel( int Sequence, int Track, int MidiChannel )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            t->setMidiChannel( MidiChannel );
    }

    void MTAudioInterface::setTrackMidiOutChannel( int Sequence, int Track, int MidiChannel )
    {
        if( auto t = manager->getTrack( Sequence, Track ) )
            t->setMidiOutChannel( MidiChannel );
    }

    int MTAudioInterface::deleteTimSig( const MTSelected& Selection, int bar )
    {
        auto s = manager->getSequence( Selection.sequence );
        //if( s ){
        //    s->deleteTimeSig( bar );
        //}
        return -1;
    }

    int MTAudioInterface::insertTimeSig( const MTSelected& Selection, int bar, int num, int den )
    {
        auto s = manager->getSequence( Selection.sequence );
        //if( s ){
        //    s->insertTimeSig( bar, num, den );
        //}
        return -1;
    }

    juce::ValueTree MTAudioInterface::getNoteList()
    {
        if( auto t = manager->getTrack() ) return t->getNoteListVT();
        return {};
    }

    juce::ValueTree MTAudioInterface::getNoteList( int SequenceIndex, int TrackIndex )
    {
        if( auto t = manager->getTrack( SequenceIndex, TrackIndex ) ) return t->getNoteListVT();
        return {};
    }

    juce::Array<juce::ValueTree>& MTAudioInterface::getSelectedNoteList()
    {
        static juce::Array<juce::ValueTree> _emptyNL;
        if( auto t = manager->getTrack() ) return t->getSelectedNoteList();
        return _emptyNL;
    }

    int MTAudioInterface::getNumNotes( const MTSelected& Selection )
    {
        if( auto t = manager->getTrack( Selection ) ) return t->getNumNotes();
        return 0;
    }

    int MTAudioInterface::getNumSelectedNotes( const MTSelected& Selection )
    {
        if( auto t = manager->getTrack( Selection ) ) return t->getNumSelectedNotes();
        return 0;
    }

    MTNote MTAudioInterface::getNoteData( const MTSelected& Selection, int NoteId )
    {
        if( auto t = manager->getTrack( Selection ) ) return t->getNoteData( NoteId );
        return MTNote{ juce::ValueTree{ IDs::NOTE } };
    }

    int MTAudioInterface::addOrModifyNote( int Note, int Start, int Length, int Velocity, int Probability, int Ratchet )
    {
        if( auto t = manager->getTrack() ) return t->addOrModifyNote( Note, Start, Length, Velocity, Probability, Ratchet );
        return -1;
    }

    int MTAudioInterface::addOrModifyNotesInTrack( int Sequence, int Track, int Note, int Start, int Length, int Velocity, int Probability, int Ratchet )
    {
        if( auto t = manager->getTrack( Sequence, Track ) ) return t->addOrModifyNote( Note, Start, Length, Velocity, Probability, Ratchet );
        return -1;
    }

    int MTAudioInterface::eraseNote( int Note, int Start, int Length )
    {
        if( auto t = manager->getTrack() ) return t->eraseNote( Note, Start, Length );
        return -1;
    }

    bool MTAudioInterface::isInvalidNoteExists()
    {
        if( auto t = manager->getTrack() ) return t->isInvalidNoteExists();
        return false;
    }

    int MTAudioInterface::eraseInvalidNotes()
    {
        if( auto t = manager->getTrack() ) return t->eraseInvalidNotes();
        return -1;
    }

    MTNote MTAudioInterface::getNoteData( int Note, int Start, int Length )
    {
        if( auto t = manager->getTrack() ) return t->getNoteData( Note, Start, Length );
        return MTNote{ juce::ValueTree{ IDs::NOTE } };
    }

    std::pair<juce::ValueTree, bool> MTAudioInterface::getNoteVT( int Note, int Start, int Length )
    {
        std::pair<juce::ValueTree, bool> retVal;
        retVal.first  = {};
        retVal.second = false;
        if( auto t = manager->getTrack() ) return t->getNoteVT( Note, Start, Length );
        return retVal;
    }

    int MTAudioInterface::getNoteID( int Note, int StartTime, int Length )
    {
        if( auto t = manager->getTrack() ) return t->getNoteID( Note, StartTime, Length );
        return -1;
    }

    int MTAudioInterface::getSelectedNoteID( int Note, int StartTime, int Length )
    {
        if( auto t = manager->getTrack() ) return t->getSelectedNoteID( Note, StartTime, Length );
        return -1;
    }

    int MTAudioInterface::getNotesInRange( const MTSelected& Selection, std::vector<MTNote>& Notes, const NotesInRange& Range )
    {
        if( auto t = manager->getTrack( Selection ) ) return t->getNotesInRange( Notes, Range );
        return -1;
    }

    int MTAudioInterface::deleteNotesInRange( const MTSelected& Selection, const NotesInRange& Range )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->deleteNotesInRange( Range );
        return -1;
    }

    int MTAudioInterface::deleteNoteInTimeRange( const MTSelected& Selection, int Start, int End )
    {
        if( auto t = manager->getTrack( Selection ) ) return t->deleteNoteInTimeRange( Start, End );
        return -1;
    }

    int MTAudioInterface::selectAllNotes( bool Select )
    {
        if( auto t = manager->getTrack() ) return Select ? t->selectAllNotes() : t->deSelectAllNotes();
        return -1;
    }

    int MTAudioInterface::selectAllInNote( const MTSelected& Selection, const int NoteNumber, bool Select )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->selectAllInNote( NoteNumber, Select );
        return -1;
    }

    int MTAudioInterface::selectNotesInRange( const NotesInRange& Range, bool Select )
    {
        if( auto t = manager->getTrack() ) return Select ? t->selectNotesInRange( Range ) : t->deSelectNotesInRange( Range );
        return -1;
    }

    bool MTAudioInterface::isSelectedNoteInRange( const NotesInRange& Range )
    {
        if( auto t = manager->getTrack() ) return t->isSelectedNoteInRange( Range );
        return false;
    }

    bool MTAudioInterface::isValidSelectedNote( const int Note, const int Start, const int Length )
    {
        if( auto t = manager->getTrack() ) return t->isValidSelectedNote( Note, Start, Length );
        return false;
    }

    bool MTAudioInterface::isValidNote( const int Note, const int Start, const int Length )
    {
        if( auto t = manager->getTrack() ) return t->isValidNote( Note, Start, Length );
        return false;
    }

    bool MTAudioInterface::isValidNote( const int Note, const int Start )
    {
        if( auto t = manager->getTrack() ) return t->isValidNote( Note, Start );
        return false;
    }

    bool MTAudioInterface::isValidNoteInRange( const NotesInRange& Range )
    {
        if( auto t = manager->getTrack() ) return t->isValidNoteInRange( Range );
        return false;
    }

    int MTAudioInterface::setNoteVelocities( const NotesInRange& Range, int Velocity )
    {
        if( auto t = manager->getTrack() ) return t->setNoteVelocityInRange( Range, Velocity );
        return -1;
    }

    int MTAudioInterface::setNoteProbabilities( const NotesInRange& Range, int Probability )
    {
        if( auto t = manager->getTrack() ) return t->setNoteProbabilityInRange( Range, Probability );
        return -1;
    }

    int MTAudioInterface::setNoteRatchets( const NotesInRange& Range, int Ratchet )
    {
        if( auto t = manager->getTrack() ) return t->setNoteRatchetInRange( Range, Ratchet );
        return -1;
    }

    int MTAudioInterface::editNote( const int NoteID, double DeltaTimeInBeats, int DeltaNoteNumber, double DeltaLength )
    {
        if( auto t = manager->getTrack() )
            return t->editNote( NoteID, DeltaTimeInBeats, DeltaNoteNumber, DeltaLength, true );
        return -1;
    }

    int MTAudioInterface::editNoteLength( const int NoteID, double DeltaLength, bool LeftEdge )
    {
        if( auto t = manager->getTrack() )
            return t->editNoteLength( NoteID, DeltaLength, true, LeftEdge );
        return -1;
    }

    int MTAudioInterface::editNote( const MTSelected& Selection, const int NoteID, int NoteStart )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNote( NoteID, NoteStart );
        return -1;
    }

    int MTAudioInterface::editNote( const MTSelected& Selection, const std::vector<int>& NoteIds, double DeltaTimeInBeats, int DeltaNoteNumber, double DeltaLength )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNote( NoteIds, DeltaTimeInBeats, DeltaNoteNumber, DeltaLength );
        return -1;
    }

    int MTAudioInterface::editNote( const MTSelected& Selection, const NotesInRange& Range, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNote( Range, DeltaTimeInTicks, DeltaNoteNumber, DeltaLength );
        return -1;
    }

    int MTAudioInterface::nudgeNotes( const MTSelected& Selection, int noteNumber, double DeltaTimeInTicks )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->nudgeNotes( noteNumber, DeltaTimeInTicks );
        return -1;
    }

    int MTAudioInterface::nudgeSelectedNotes( const MTSelected& Selection, double DeltaTimeInTicks, bool Snap, double TotalTicks )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->nudgeSelectedNotes( DeltaTimeInTicks, Snap, TotalTicks );
        return -1;
    }

    int MTAudioInterface::transposeSelectedNotes( const MTSelected& Selection, int DeltaNoteNumber )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->transposeSelectedNotes( DeltaNoteNumber );
        return -1;
    }

    int MTAudioInterface::editSelectedNotesStart( const MTSelected& Selection, int DeltaNoteStart, bool Snap )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editSelectedNotesStart( DeltaNoteStart, Snap );
        return -1;
    }

    int MTAudioInterface::editSelectedNotesEnd( const MTSelected& Selection, int DeltaNoteEnd )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editSelectedNotesEnd( DeltaNoteEnd );
        return -1;
    }

    /*
    int MTAudioInterface::editNote( juce::ValueTree nTree, double DeltaTimeInBeats, int DeltaNoteNumber, double DeltaLength )
    {
        if( auto t = manager->getTrack() )
            return t->editNote( nTree, DeltaTimeInBeats, DeltaNoteNumber, DeltaLength );
        return -1;
    }

    int MTAudioInterface::editNoteLength( juce::ValueTree NoteOnEdit, const int deltaLength, bool isLeftEdge )
    {
        if( auto t = manager->getTrack() )
            return t->editNoteLength( NoteOnEdit, deltaLength, isLeftEdge );
        return -1;
    }
    */

    int MTAudioInterface::editSelectedNotesLength( const MTSelected& Selection, const int deltaLength, bool isLeftEdge )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editSelectedNote( deltaLength, isLeftEdge );
        return -1;
    }

    int MTAudioInterface::editSelectedNotesVelocity( const MTSelected& Selection, const int Velocity )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editSelectedNotesVelocity( Velocity );
        return -1;
    }

    int MTAudioInterface::editNoteVelocity( const MTSelected& Selection, int NoteID, const int Velocity )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNoteVelocity( NoteID, Velocity );
        return -1;
    }

    int MTAudioInterface::editNoteVelocity( const MTSelected& Selection, const std::vector<int>& NoteIDs, const std::vector<int>& Velocities )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNoteVelocity( NoteIDs, Velocities );
        return -1;
    }

    int MTAudioInterface::editNoteVelocity( const MTSelected& Selection, const NotesInRange& Range, int Velocity )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNoteVelocity( Range, Velocity );
        return -1;
    }

    int MTAudioInterface::editSelectedNotesProbability( const MTSelected& Selection, const int Probability )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editSelectedNotesProbability( Probability );
        return -1;
    }

    int MTAudioInterface::editNoteProbability( const MTSelected& Selection, int NoteID, const int Probability )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNoteProbability( NoteID, Probability );
        return -1;
    }

    int MTAudioInterface::editNoteProbability( const MTSelected& Selection, const std::vector<int>& NoteIDs, const std::vector<int>& Probabilities )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNoteProbability( NoteIDs, Probabilities );
        return -1;
    }

    int MTAudioInterface::editNoteProbability( const MTSelected& Selection, const NotesInRange& Range, int Probability )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNoteProbability( Range, Probability );
        return -1;
    }

    int MTAudioInterface::editSelectedNotesRatchet( const MTSelected& Selection, const int Ratchet )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editSelectedNotesRatchet( Ratchet );
        return -1;
    }

    int MTAudioInterface::editNoteRatchet( const MTSelected& Selection, int NoteID, const int Ratchet )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNoteRatchet( NoteID, Ratchet );
        return -1;
    }

    int MTAudioInterface::editNoteRatchet( const MTSelected& Selection, const std::vector<int>& NoteIDs, const std::vector<int>& Ratchets )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNoteRatchet( NoteIDs, Ratchets );
        return -1;
    }

    int MTAudioInterface::editNoteRatchet( const MTSelected& Selection, const NotesInRange& Range, int Ratchet )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editNoteRatchet( Range, Ratchet );
        return -1;
    }

    int MTAudioInterface::deleteSelectedNotes( const MTSelected& Selection )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->deleteSelectedNotes();
        return -1;
    }

    int MTAudioInterface::editMuteSelectedNotes( const MTSelected& Selection, const bool MuteStatus )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editMuteSelectedNotes( MuteStatus );
        return -1;
    }

    int MTAudioInterface::editSelectedNotes( const MTSelected& Selection, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editSelectedNotes( DeltaTimeInTicks, DeltaNoteNumber, DeltaLength );
        return -1;
    }

    int MTAudioInterface::pasteNote( const int NoteID, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength )
    {
        if( auto t = manager->getTrack() )
            return t->pasteNote( NoteID, DeltaTimeInTicks, DeltaNoteNumber, DeltaLength );
        return -1;
    }

    int MTAudioInterface::pasteSelectedNotes( const MTSelected& Selection, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->pasteSelectedNotes( DeltaTimeInTicks, DeltaNoteNumber, DeltaLength );
        return -1;
    }

    int MTAudioInterface::pasteNotes( const MTSelected& Selection, juce::Array<juce::ValueTree>& selectedNotes, double DeltaTimeInTicks )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->pasteNotes( selectedNotes, DeltaTimeInTicks );
        return -1;
    }
    /*
    int MTAudioInterface::pasteNote( juce::ValueTree NTree, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength )
    {
        if( auto t = manager->getTrack() )
            return t->pasteNote( NTree, DeltaTimeInTicks, DeltaNoteNumber, DeltaLength );
        return -1;
    }
    */

    int MTAudioInterface::pasteSelectedNotesDefault()
    {
        if( auto t = manager->getTrack() )
            return t->pasteSelectedNotesDefault();
        return -1;
    }

    int MTAudioInterface::selectNote( const MTSelected& Selection, bool Select )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->selectAllNotes( Select );
        return -1;
    }

    int MTAudioInterface::selectNote( const MTSelected& Selection, const int NoteId, bool Select )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->selectNote( NoteId, Select );
        return -1;
    }

    int MTAudioInterface::selectNote( const MTSelected& Selection, const std::vector<int>& NoteIds, bool Select )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->selectNote( NoteIds, Select );
        return -1;
    }

    int MTAudioInterface::selectNote( const MTSelected& Selection, const int Note, const int Start, const int Length, bool Select )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->selectNote( Note, Start, Length, Select );
        return -1;
    }

    int MTAudioInterface::selectNote( const MTSelected& Selection, const int Note, const int Start, bool Select )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->selectNote( Note, Start, Select );
        return -1;
    }

    int MTAudioInterface::halfSpeedNotes( const MTSelected& Selection )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->scaleAllNotes( 2.0 );
        return -1;
    }

    int MTAudioInterface::doubleSpeedNotes( const MTSelected& Selection )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->scaleAllNotes( 0.5 );
        return -1;
    }

    int MTAudioInterface::moveAllNotes( const MTSelected& Selection, double DeltaInBeats )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->editSelectedNotes( DeltaInBeats, 0, 0 );
        return -1;
    }

    int MTAudioInterface::deleteAllNotes( const MTSelected& Selection )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->deleteAllNotes();
        return -1;
    }

    int MTAudioInterface::doubleLengthWithEvents( const MTSelected& Selection )
    {
        if( auto t = manager->getTrack( Selection ) )
            return 0; // Need To Add Code
        return -1;
    }

    int MTAudioInterface::splitNotes( const MTSelected& Selection, const track::splitInfo& Info )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->splitNotes( Info );
        return -1;
    }

    int MTAudioInterface::splitSelectedNotes( const MTSelected& Selection, const track::splitInfo& Info )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->splitSelectedNotes( Info );
        return -1;
    }

    int MTAudioInterface::humanizeNotes( const MTSelected& Selection, const track::humanizeInfo& Info )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->humanizeNotes( Info );
        return -1;
    }

    int MTAudioInterface::randomNotes( const MTSelected& Selection, const track::randomEventsInfo& Info )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->randomNotes( Info );
        return -1;
    }

    int MTAudioInterface::pitchQuantizeNotes( const MTSelected& Selection, const track::pitchQuantizeInfo& Info )
    {
        if( auto t = manager->getTrack( Selection ) )
            return t->pitchQuantizeNotes( Info );
        return -1;
    }

    int MTAudioInterface::explode( const MTSelected& Selection )
    {
        auto t = manager->getTrack( Selection );
        if( t )
            return t->explode();
        return -1;
    }

    int MTAudioInterface::copyNotesTo( const MTSelected& Selection, const track::copyTrackNotes Info )
    {
        auto t = manager->getTrack( Selection );
        if( t )
            return t->copyNotesTo( Info );
        return -1;
    }

    int MTAudioInterface::addLiveMidiMsg( const MTSelected& Selection, const juce::MidiMessage& Msg )
    {
        DBG( "addLiveMidiNoteFuntion" );
        if( auto t = manager->getTrack( Selection ) )
            t->processIncomingMidiMessage( Msg );
        return 0;
    }

    int MTAudioInterface::addLiveMidiNote( const MTSelected& Selection, int Note, int Velocity )
    {
        auto msg = juce::MidiMessage::noteOn( 1, Note, (juce::uint8)Velocity );
        return addLiveMidiMsg( Selection, msg );
    }

    int MTAudioInterface::stopLiveMidiNote( const MTSelected& Selection, int Note )
    {
        auto msg = juce::MidiMessage::noteOff( 1, Note );
        return addLiveMidiMsg( Selection, msg );
    }

    int MTAudioInterface::addLiveMidiNote( const int Program, const int Note, const int Velocity )
    {
        if( auto prog = manager->getProgram( Program ) )
        {
            prog->noteOn( Note, Velocity );
            return 0;
        }
        return -1;
    }

    int MTAudioInterface::stopLiveMidiNote( const int Program, const int Note )
    {
        if( auto prog = manager->getProgram( Program ) )
        {
            prog->noteOff( Note );
            return 0;
        }
        return -1;
    }

    MTSampleEditorInterface* MTAudioInterface::createSampleEditor() { return manager->createSampleEditor(); }
    juce::UndoManager&       MTAudioInterface::getUndoManager() noexcept { return manager->getUndoManager(); }
    juce::ValueTree&         MTAudioInterface::getValueTree() noexcept { return manager->getValueTree(); }
    juce::ValueTree          MTAudioInterface::getTempoState() noexcept { return manager->getTempoState( 0 ); }
    juce::ValueTree&         MTAudioInterface::getRuntimeState() noexcept { return manager->getRuntimeState(); }

    // Asset Manager related...
    int MTAudioInterface::getNumFolders() { return manager->getDirectories().size(); }
    int MTAudioInterface::getNumFiles() { return manager->getDirectories().size(); }

    const juce::File&              MTAudioInterface::getInternalDir() { return manager->getInternalDir(); }
    const juce::File&              MTAudioInterface::getSoundsDir() { return manager->getSoundsDir(); }
    const juce::File&              MTAudioInterface::getDemosDir() { return manager->getDemosDir(); }
    const juce::File&              MTAudioInterface::getTempDir() { return manager->getTempDir(); }
    const juce::File&              MTAudioInterface::getProjectDir() { return manager->getProjDir(); }
    const juce::File&              MTAudioInterface::getCurrentDir() { return manager->getCurDir(); }
    const juce::Array<juce::File>& MTAudioInterface::getDirectories() { return manager->getDirectories(); }
    const juce::Array<juce::File>& MTAudioInterface::getFiles() { return manager->getFiles(); }

    void MTAudioInterface::dirSelect( int Index ) { manager->dirSelect( Index ); }
    void MTAudioInterface::dirEnter( int Index ) { manager->dirEnter( Index ); }
    void MTAudioInterface::dirGoUp() { manager->dirGoUp(); }
    void MTAudioInterface::setCurrentDir( juce::File& currDir ) { manager->setCurrentDir( currDir ); }

    const std::vector<juce::String>& MTAudioInterface::getAssets( AssetTypes Type ) { return manager->getAssets( Type ); }
    std::vector<juce::String>        MTAudioInterface::getAssetNames( AssetTypes Type ) { return manager->getAssetNames( Type ); }
    const juce::String&              MTAudioInterface::getAssetName( AssetTypes Type, int Index ) { return manager->getAssetName( Type, Index ); }
    const juce::String&              MTAudioInterface::getAsset( AssetTypes Type, int Index ) { return manager->getAsset( Type, Index ); }
    int                              MTAudioInterface::addAsset( const juce::String& File, AssetTypes Type ) { return manager->add( File, Type ); }
    void                             MTAudioInterface::removeAsset( const juce::String& File, AssetTypes Type ) { return manager->remove( File, Type ); }
    juce::ValueTree                  MTAudioInterface::getAssetVT() { return manager->getAssetsVT(); }
    void                             MTAudioInterface::addAssetSlice( const juce::String& FileName, ASoundSlice* Slice, int Index ) { manager->addAssetSlice( FileName, Slice, Index ); }
    ASoundSlice*                     MTAudioInterface::getAssetSlice( const juce::String& FileName, int Slice ) { return manager->getAssetSlice( FileName, Slice ); }

    // Preview Utilities...
    void        MTAudioInterface::createPreview( const juce::File& F, float Gain ) { manager->createPreview( F, Gain ); }
    void        MTAudioInterface::stopPreview() { manager->stopPreview(); }
    void        MTAudioInterface::loopPreview( bool Loop ) { manager->loopPreview( Loop ); }
    void        MTAudioInterface::setPreviewMute( bool Mute ) { manager->setPreviewMute( Mute ); }
    void        MTAudioInterface::setPreviewGain( float Gain ) { manager->setPreviewGain( Gain ); }
    MTMeterData MTAudioInterface::readPreviewMeter() { return manager->readPreviewMeter(); }

    bool MTAudioInterface::isPreviewRunning() { return manager->isPreviewRunning(); }
    bool MTAudioInterface::isPreviewLooping() { return manager->isPreviewLooping(); }
    bool MTAudioInterface::isPreviewMuted() { return manager->isPreviewMuted(); }

    // Audio Recorder...
    void MTAudioInterface::enableRecorder( bool Enable ) { manager->enableRecorder( Enable ); }
    void MTAudioInterface::setupRecorder( double timeInSeconds, float Gain, float Pan, bool Looping, bool Overdub ) { return manager->setupRecorder( timeInSeconds, Gain, Pan, Looping, Overdub ); }
    void MTAudioInterface::setMonitorOn( bool Monitor ) { manager->setMonitorOn( Monitor ); }
    void MTAudioInterface::setRecorderLoop( bool Looping ) { manager->setRecorderLoop( Looping ); }
    void MTAudioInterface::setRecorderOverdub( bool Overdub ) { manager->setRecorderOverdub( Overdub ); }
    void MTAudioInterface::setRecorderPan( float Pan ) { manager->setRecorderPan( Pan ); }
    void MTAudioInterface::setRecorderGain( float Gain ) { manager->setRecorderGain( Gain ); }
    void MTAudioInterface::setRecorderGainDB( float GainDB ) { manager->setRecorderGainDB( GainDB ); }
    void MTAudioInterface::setRecorderThreshold( float Gain ) { manager->setRecorderThreshold( Gain ); }
    void MTAudioInterface::setRecorderThresholdDB( float DB ) { manager->setRecorderThresholdDB( DB ); }
    void MTAudioInterface::setRecorderDirection( bool ForwardDirection ) { manager->setRecorderDirection( ForwardDirection ); }

    void MTAudioInterface::startRecorder( int Mode, bool ForwardDirection, bool FromBeginging ) { manager->startRecorder( Mode, ForwardDirection, FromBeginging ); }
    void MTAudioInterface::stopRecorder() { manager->stopRecorder(); }
    void MTAudioInterface::clearRecording() { manager->clearRecording(); }
    void MTAudioInterface::saveRecording( const std::string& FilePath ) { manager->saveRecording( FilePath ); }

    float MTAudioInterface::getRecorderMode() { return manager->getRecorderMode(); }
    float MTAudioInterface::getRecorderGain() { return manager->getRecorderGain(); }
    float MTAudioInterface::getRecorderProgress() { return manager->getRecorderProgress(); }
    float MTAudioInterface::getRecorderThreshold() { return manager->getRecorderThreshold(); }
    float MTAudioInterface::getRecorderThresholdDB() { return manager->getRecorderThresholdDB(); }

    bool MTAudioInterface::isRecorderEnabled() { return manager->isRecorderEnabled(); }
    bool MTAudioInterface::isRecorderMonitorOn() { return manager->isRecorderMonitorOn(); }
    bool MTAudioInterface::isRecorderLooping() { return manager->isRecorderLooping(); }
    bool MTAudioInterface::isRecorderOverdubbing() { return manager->isRecorderOverdubbing(); }
    bool MTAudioInterface::isRecorderInForwardDirection() { return manager->isRecorderInForwardDirection(); }
    bool MTAudioInterface::isRecorderRunning() { return manager->isRecorderRunning(); }
    bool MTAudioInterface::isRecorderPreviewing() { return manager->isRecorderPreviewing(); }

    MTMeterData MTAudioInterface::getRecorderInputMeter() { return manager->getRecorderInputMeter(); }
    MTMeterData MTAudioInterface::getRecorderOutputMeter() { return manager->getRecorderOutputMeter(); }

    void                      MTAudioInterface::startRecorderPreview( double fromStartTimeInSeconds ) { manager->startRecorderPreview( fromStartTimeInSeconds ); }
    void                      MTAudioInterface::stopRecorderPreview() { manager->stopRecorderPreview(); }
    juce::AudioBuffer<float>& MTAudioInterface::getRecorderAudioBuffer() { return manager->getRecorderAudioBuffer(); }

    void MTAudioInterface::setRecorderListener( MTRecorderListener& Listener ) { manager->setRecorderListener( Listener ); }

    juce::AudioDeviceManager& MTAudioInterface::getAudioDeviceManager()
    {
        return manager->getAudioDeviceManager();
    }

    MTAudioAssetManager& MTAudioInterface::getAssetManager()
    {
        return *manager;
    }

    MTIInsertsInterface* MTAudioInterface::getInsertsInterface( const int Program )
    {
        return dynamic_cast<MTIInsertsInterface*>( manager->getProgram( Program ) );
    }

    MTIInsertsInterface* MTAudioInterface::getInsertsInterface( const juce::String& BusName )
    {
        //return manager->getBusManager().getBus( BusName );
        return nullptr;
    }

    // MIDI device and utilities...
    int MTAudioInterface::getNumMIDIDevices( bool isInputOrOutput ) const noexcept
    {
        return isInputOrOutput ? manager->getNumMidiInputDevices() : manager->getNumMidiOutputDevices();
    }

    const juce::ReferenceCountedArray<MidiDeviceListEntry>& MTAudioInterface::getMidiDevices( bool isInputOrOutput ) const noexcept
    {
        return manager->getMidiDeviceList( isInputOrOutput );
    }

    const std::vector<juce::String>& MTAudioInterface::getMidiDeviceNames( bool isInputOrOutput ) noexcept
    {
        return manager->getMidiDeviceNames( isInputOrOutput );
    }

    void MTAudioInterface::sendMidiMessage( const juce::String& Device, const juce::MidiMessage& Msg ) noexcept
    {
        //DBG( "MIDI Message: " << Device << " " << Msg.getDescription() // Provides a human-readable description of the message
        //                      << " [Raw Data: " << Msg.getRawData()[0] << " "
        //                      << Msg.getRawData()[1] << " "
        //                      << Msg.getRawData()[2] << "]" );
        manager->sendToOutput( Device, Msg );
    }

    MTBusManager& MTAudioInterface::getBusManager()
    {
        return manager->getBusManager();
    }

    // Input BUS...
    const std::vector<std::string>& MTAudioInterface::getInputBusNames()
    {
        return manager->getBusManager().getInputBusNames();
    }

    // Output BUS...
    const std::vector<std::string>& MTAudioInterface::getOutputBusNames()
    {
        return manager->getBusManager().getOutputBusNames();
    }

    const MTInsertsMap& MTAudioInterface::getInsertsMap()
    {
        return AudIO::DawInserts::getEffectsMap();
    }

    std::vector<juce::String> MTAudioInterface::getInsertCategory()
    {
        std::set<juce::String> categories;

        const auto& map = getInsertsMap();
        for( const auto& it : map )
        {
            categories.emplace( it.second.Type );
        }

        std::vector<juce::String> list;
        for( const auto& it : categories )
            list.emplace_back( it );

        return list;
    }

    std::vector<juce::String> MTAudioInterface::getInsertManufacturers()
    {
        std::set<juce::String> categories;

        const auto& map = getInsertsMap();
        for( const auto& it : map )
        {
            categories.emplace( it.second.Manufacturer );
        }

        std::vector<juce::String> list;
        for( const auto& it : categories )
            list.emplace_back( it );

        return list;
    }

    std::vector<juce::String> MTAudioInterface::getAvailableInsertsList()
    {
        std::vector<juce::String> list;
        const auto&               map = getInsertsMap();
        for( const auto& it : map )
        {
            list.emplace_back( juce::String( it.first ) );
        }
        return list;
    }

    std::vector<juce::String> MTAudioInterface::getAvailableInsertsListByCatogary( const juce::String& Catogary )
    {
        std::vector<juce::String> list;
        const auto&               map = getInsertsMap();
        for( const auto& it : map )
        {
            const auto& info = it.second;
            if( Catogary.equalsIgnoreCase( info.Type.c_str() ) )
                list.emplace_back( juce::String( it.first ) );
        }
        return list;
    }

    std::vector<juce::String> MTAudioInterface::getAvailableInsertsListByManufacturer( const juce::String& Manufacturer )
    {
        std::vector<juce::String> list;
        const auto&               map = getInsertsMap();
        for( const auto& it : map )
        {
            const auto& info = it.second;
            if( Manufacturer.equalsIgnoreCase( info.Manufacturer.c_str() ) )
                list.emplace_back( juce::String( it.first ) );
        }
        return list;
    }
    void MTAudioInterface::performUndo()
    {
        manager->performUndo();
    }
    void MTAudioInterface::performRedo()
    {
        manager->performRedo();
    }

    std::vector<juce::String> MTAudioInterface::getSongsList() { return manager->getSongsList(); }

    void       MTAudioInterface::createSong( const juce::String& Name ) { manager->createSong( Name ); }
    void       MTAudioInterface::deleteSong( const juce::String& Name ) { manager->deleteSong( Name ); }
    MTSongInfo MTAudioInterface::getSongInfo( const juce::String& SongName ) { return manager->getSongInfo( SongName ); }
    void       MTAudioInterface::addSongStep( const juce::String& SongName, MTSongStepInfo& Step, int AtIndex ) { manager->addSongStep( SongName, Step, AtIndex ); }
    void       MTAudioInterface::editSongStep( const juce::String& SongName, MTSongStepInfo& Step, int AtIndex ) { manager->editSongStep( SongName, Step, AtIndex ); }
    void       MTAudioInterface::deleteSongStep( const juce::String& SongName, int AtIndex ) { manager->deleteSongStep( SongName, AtIndex ); }
    void       MTAudioInterface::deleteAllSongSteps( const juce::String& SongName ) { manager->deleteAllSongSteps( SongName ); }
    bool       MTAudioInterface::isSongExists( const juce::String& SongName ) { return manager->isSongExists( SongName ); }
    void       MTAudioInterface::playSong( const juce::String& SongName, int SequenceIndexToPlay ) { manager->playSong( SongName, SequenceIndexToPlay ); }
    void       MTAudioInterface::resumeSong() { manager->resumeSong(); }
    void       MTAudioInterface::stopSong() { manager->stopSong(); }

    void MTAudioInterface::playSequence( int Sequence ) { manager->playSequence( Sequence ); }
    void MTAudioInterface::stopSequence() { manager->stopSequence(); }
    void MTAudioInterface::setNextSequence( int NextSequence ) { manager->setNextSequence( NextSequence ); }
    void MTAudioInterface::setNextBarFlag( bool NextBarFlag ) { manager->setNextBarFlag( NextBarFlag ); }

}
