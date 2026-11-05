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
#include "audio_voice.h"

namespace mt
{
    /// @brief
    Metronome::Metronome( AudIO::Design& D, const juce::ValueTree& Tree, const juce::String& Name )
        : ISynthObject( D, 1, AudIO::BusType::Other )
        , slice( Tree, nullptr )
    {
        setName( Name.toStdString() );
        DawInserts::setBypassAllEffects( true );
        setBypass( false );

        // Pre-reseve Memory...
        for( auto i = 0; i < getPolyphony(); ++i )
            voices.emplace_back( std::make_unique<AVoice>( master ) );

        ISynthObject::addVoices( voices );
        slice.setReSampleRate( getSampleRate() );
    }

    Metronome::~Metronome()
    {
        noteOffAll();
        setBypass( true );
        voices.clear();
    }

    void Metronome::setSource( juce::String& File )
    {
        auto path  = juce::File::isAbsolutePath( File ) ? juce::File( File ) : getAppDirs( MTAssets ).getChildFile( File );
        auto sound = getAudioSoundCache().getOrLoadMetronomeSound( path.getFullPathName() );
        slice.setSource( sound );
    }

    void Metronome::noteOn( int Note, int Velocity )
    {
        noteOff( Note );
        if( auto voice = getFreeVoice( Note, 1 ) )
        {
            voice->setSlice( &slice );
            if( auto avoice = dynamic_cast<AVoice*>( voice ) )
            {
                // Note 0 = Bar downbeat accent (pitched higher)
                // Note 1 = Regular beat
                avoice->setPitchRatio( Note == 0 ? 1.5 : 1.0 );
            }
            voice->noteOn( Note, Velocity );
            addVoiceForPlayback( voice );
        }
    }

};
