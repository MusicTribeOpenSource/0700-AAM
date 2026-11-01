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

// module include files...

#include "drum_voice.h"

namespace mt
{
    /// @brief
    DrumProgram::DrumProgram( AudIO::Design& D, const juce::String& Name, MTIProgram* PDrum )
        : ISynthObject( D, POLYPHONY::MAX )
    {
        setName( Name.toStdString() );
        setProgram( PDrum );

        // Pre-reseve Memory...
        for( auto i = 0; i < getPolyphony(); ++i )
        {
            voices.emplace_back( std::make_unique<DrumVoice>( master ) );
        }
        ISynthObject::addVoices( voices );
    }

    DrumProgram::~DrumProgram()
    {
        setBypass( true );
    }

    void DrumProgram::setProgram( MTIProgram* PDrum )
    {
        // call base class method...
        ISynthObject::setProgram( PDrum );

        noteOffAll();
        pDrum = dynamic_cast<MTDrumProgram*>( PDrum );
        setBypass( ( pDrum == nullptr ) );
    }

    void DrumProgram::noteOn( int Note, int Velocity )
    {
        // Return if no audio synth available or invalid midi length
        if( !pDrum || !CommonUtils::isValidMidiNote( Note ) ) return;

        if( auto pNote = pDrum->getDrumClip( Note ) )
        {
            // Stop clips in same mute group.
            {
                auto notes = pDrum->getNotesInSameMuteGroup( Note );
                for( auto it : notes )
                    if( CommonUtils::isValidMidiNote( it ) )
                        noteOff( it );
            }

            // Stop playback of clips simultaneously.
            {
                auto notes = pNote->sMute.getActions();
                for( auto it : notes )
                    noteOff( it );
            }

            if( !pDrum->isNoteEffectivelyMuted( Note ) && pNote->getSource( 0 ).length() || pDrum->isNoteSolo( Note ) && pNote->getSource( 0 ).length() )
            {
                auto notePoly = pNote->polyPhony.get();

                // Get layers that have valid source.
                std::vector<int> validSourceIndex;
                auto             sources = pNote->getSources();
                for( auto i = 0; i < sources.size(); ++i )
                {
                    if( sources[i].isNotEmpty() )
                        validSourceIndex.push_back( i );
                }

                // Choose a layer to play sound
                auto layerToPlay = 0;
                switch( pNote->layerPlay.get() )
                {
                    break;
                    case LayerPlayType::RANDOM:
                        layerToPlay = AudIO::utils::random<int>( 0, ( (int)validSourceIndex.size() - 1 ) );
                        break;
                    case LayerPlayType::CYCLE:
                        if( noteLayerIdx.find( Note ) == noteLayerIdx.end() )
                            noteLayerIdx[Note] = 0;
                        else
                            noteLayerIdx[Note] = juce::jlimit( 0, (int)( validSourceIndex.size() - 1 ), ++noteLayerIdx[Note] );

                        layerToPlay = noteLayerIdx[Note];
                        break;
                    case LayerPlayType::VELOCITY:
                    {
                        for( const auto& it : validSourceIndex )
                        {
                            if( pNote->layers[it]->velStart.get() <= Velocity && Velocity < pNote->layers[it]->velEnd.get() )
                            {
                                layerToPlay = it;
                                break;
                            }
                        }
                    }
                }; // switch

                if( auto soundSlice = pNote->layers[layerToPlay]->src )
                {
                    // Start playback.
                    if( auto voice = dynamic_cast<DrumVoice*>( getFreeVoice( Note, notePoly ) ) )
                    {
                        voice->setPadGains( pNote->gL.load(), pNote->gR.load() );
                        voice->setSlice( soundSlice );
                        voice->noteOn( Note, Velocity );
                        addVoiceForPlayback( voice );
                    }
                }
            }

            // Trigger playback of clips simultaneously.
            {
                auto notes = pNote->sPlay.getActions();
                for( auto it : notes )
                    noteOn( it, Velocity );
            }
        }
    }

    void DrumProgram::noteOff( int Note )
    {
        if( !pDrum || !CommonUtils::isValidMidiNote( Note ) ) return;
        if( auto pNote = pDrum->getDrumClip( Note ) )
        {
            if( pNote->samplePlay.get() == SamplePlayType::ONE_SHOT )
                return;
        }
        ISynthObject::noteOff( Note );
    }
};
