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

#pragma once

namespace mt
{
    namespace IDs
    {
#define DECLARE_ID( name ) const juce::Identifier name( #name );

        DECLARE_ID( ROOT )
        DECLARE_ID( RUNTIME )
        DECLARE_ID( TRANSPORT )
        DECLARE_ID( TEMPOS )
        DECLARE_ID( TEMPO )
        DECLARE_ID( PLAYHEAD )
        DECLARE_ID( state )

        DECLARE_ID( manufacturer )
        DECLARE_ID( version )

        DECLARE_ID( PROGRAMS )
        DECLARE_ID( PROGRAM )
        DECLARE_ID( SEQUENCES )
        DECLARE_ID( SEQUENCE )
        DECLARE_ID( TRACKS )
        DECLARE_ID( TRACK )
        DECLARE_ID( PERFORMANCE )
        DECLARE_ID( LOOPER )
        DECLARE_ID( SAMPLER )
        DECLARE_ID( program )

        DECLARE_ID( name )
        DECLARE_ID( id )
        DECLARE_ID( type )
        DECLARE_ID( index )
        DECLARE_ID( midiPort )
        DECLARE_ID( midiCh )
        DECLARE_ID( midiChOut )

        DECLARE_ID( layerType )
        DECLARE_ID( sampleType )

        DECLARE_ID( CLIPS )
        DECLARE_ID( CLIP )
        DECLARE_ID( AUDIOS )
        DECLARE_ID( LAYERS )
        DECLARE_ID( LAYER )
        DECLARE_ID( NOTE )
        DECLARE_ID( NOTELIST )
        DECLARE_ID( RANDOM )
        DECLARE_ID( LFO )
        DECLARE_ID( FILTER )
        DECLARE_ID( TYPE )
        DECLARE_ID( ENVELOPE )
        DECLARE_ID( AMPENV )
        DECLARE_ID( FILTERENV )
        DECLARE_ID( PITCHENV )

        DECLARE_ID( ADSR )
        DECLARE_ID( attack )
        DECLARE_ID( hold )
        DECLARE_ID( decay )
        DECLARE_ID( sustain )
        DECLARE_ID( release )

        DECLARE_ID( COLOURS )
        DECLARE_ID( mode )
        DECLARE_ID( emptyColourmode )
        DECLARE_ID( colour )
        DECLARE_ID( no )

        DECLARE_ID( filter )
        DECLARE_ID( pitch )
        DECLARE_ID( semi )
        DECLARE_ID( fine )
        DECLARE_ID( level )
        DECLARE_ID( keyNote )
        DECLARE_ID( sampleCount )
        DECLARE_ID( minNote )
        DECLARE_ID( maxNote )

        DECLARE_ID( srcId )
        DECLARE_ID( source )
        DECLARE_ID( offset )
        DECLARE_ID( start )
        DECLARE_ID( startTime )
        DECLARE_ID( end )
        DECLARE_ID( endTime )
        DECLARE_ID( length )
        DECLARE_ID( openEnded )
        DECLARE_ID( layerPlay )
        DECLARE_ID( padPlay )
        DECLARE_ID( samplePlay )
        DECLARE_ID( poly )

        DECLARE_ID( SPLAY )
        DECLARE_ID( SMUTE )
        DECLARE_ID( ac1 )
        DECLARE_ID( ac2 )
        DECLARE_ID( ac3 )
        DECLARE_ID( ac4 )

        DECLARE_ID( gain )
        DECLARE_ID( gainDb )
        DECLARE_ID( pan )
        DECLARE_ID( mute )
        DECLARE_ID( bypass )
        DECLARE_ID( solo )
        DECLARE_ID( muteGroup )
        DECLARE_ID( reverse )
        DECLARE_ID( slice )
        DECLARE_ID( loop )
        DECLARE_ID( fade )
        DECLARE_ID( warp )

        DECLARE_ID( isMasterSettings )
        DECLARE_ID( isEnabled )
        DECLARE_ID( isMasterBPM )
        DECLARE_ID( isMasterLength )

        DECLARE_ID( loopStart )
        DECLARE_ID( loopEnd )
        DECLARE_ID( velocity )
        DECLARE_ID( velocityStart )
        DECLARE_ID( velocityEnd )

        DECLARE_ID( GROUPING )
        DECLARE_ID( GROUP )
        DECLARE_ID( member )

        DECLARE_ID( INSERTS )
        DECLARE_ID( Insert )

        DECLARE_ID( FILEMANAGER )
        DECLARE_ID( FILE )
        DECLARE_ID( SLICE )
        DECLARE_ID( ASSETS )
        DECLARE_ID( AUDIO )
        DECLARE_ID( EXPORTS )
        DECLARE_ID( path )

        DECLARE_ID( usingMasterLength )
        DECLARE_ID( usingSequenceLength )
        DECLARE_ID( usingMasterBPM )
        DECLARE_ID( bar )
        DECLARE_ID( beat )
        DECLARE_ID( tick )
        DECLARE_ID( bpm )
        DECLARE_ID( num )
        DECLARE_ID( den )
        DECLARE_ID( quantize )

        DECLARE_ID( freq )
        DECLARE_ID( sync )
        DECLARE_ID( cutoff )
        DECLARE_ID( resonance )
        DECLARE_ID( depth )
        DECLARE_ID( cutoffMod )
        DECLARE_ID( attackMod )
        DECLARE_ID( depthMod )
        DECLARE_ID( pitchMod )
        DECLARE_ID( transpose ) // Semitones to transpose...

        DECLARE_ID( BUSES )
        DECLARE_ID( BUS )
        DECLARE_ID( IN )
        DECLARE_ID( OUT )
        DECLARE_ID( ROUTE )
        DECLARE_ID( SEND )
        DECLARE_ID( SUBMIX )
        DECLARE_ID( sg0 )
        DECLARE_ID( sg1 )
        DECLARE_ID( sg2 )
        DECLARE_ID( sg3 )
        DECLARE_ID( _1 )
        DECLARE_ID( _2 )
        DECLARE_ID( _3 )
        DECLARE_ID( _4 )
        DECLARE_ID( route )

        DECLARE_ID( CVNote )
        DECLARE_ID( CVPort )
        DECLARE_ID( CVGate )
        DECLARE_ID( CVModWheel )
        DECLARE_ID( CVVelocity )
        DECLARE_ID( CVStartNote )

        DECLARE_ID( MIDIPort )
        DECLARE_ID( BankLSB )
        DECLARE_ID( BankMSB )
        DECLARE_ID( ProgCH )

        DECLARE_ID( CC0 )
        DECLARE_ID( MIDIControl )
        DECLARE_ID( MIDILEARN )

        DECLARE_ID( SONGS )
        DECLARE_ID( song )
        DECLARE_ID( step )
        DECLARE_ID( repeat )

        // Preferences ID;s
        DECLARE_ID( PREFERENCES )
        DECLARE_ID( BROWSERFAVOURITE )
        DECLARE_ID( status )
        DECLARE_ID( ETHERNET )
        DECLARE_ID( ipAdd )
        DECLARE_ID( subnetMask )
        DECLARE_ID( gateway )
        DECLARE_ID( dns )
        DECLARE_ID( CHILD )

        DECLARE_ID( PREFERENCEINFO )
        DECLARE_ID( brand )
        DECLARE_ID( Kicklogo )
        DECLARE_ID( firmWareVal )
        DECLARE_ID( serialNoVal )
        DECLARE_ID( ControlSurfaceVal )

        DECLARE_ID( PROJECTDEFAULTS )
        DECLARE_ID( Onlaunch )
        DECLARE_ID( MaxNumOfRecentProject )
        DECLARE_ID( recordBitDeapth )
        DECLARE_ID( Autosave )
        DECLARE_ID( Autoload )
        DECLARE_ID( SaveAfter )
        DECLARE_ID( TimeTempo )
        DECLARE_ID( TimeBars )
        DECLARE_ID( TimeNumerator )
        DECLARE_ID( TimeDenominator )
        DECLARE_ID( rootNote )
        DECLARE_ID( scale )
        DECLARE_ID( octave )
        DECLARE_ID( chord )
        DECLARE_ID( bank )
        DECLARE_ID( ProjectSequance )
        DECLARE_ID( ProjectTrackSeq )
        DECLARE_ID( DefaultProjectName );
        DECLARE_ID( DefaultProjectState );
        DECLARE_ID( DefaultProjectStateName );

        DECLARE_ID( FULLPATH )
        DECLARE_ID( CurrentProject )
        DECLARE_ID( PREFHARDWARE )
        DECLARE_ID( padThreshold )
        DECLARE_ID( padSensitivity )
        DECLARE_ID( padCurve )
        DECLARE_ID( padVelocity )
        DECLARE_ID( afterTouch )
        DECLARE_ID( screenBrightness )
        DECLARE_ID( tapTempoLed )
        DECLARE_ID( noOftaps )
        DECLARE_ID( timeZone )

        DECLARE_ID( PREFMIDICONTROL )
        DECLARE_ID( MidiInputName )
        DECLARE_ID( MidiOutputName )
        DECLARE_ID( midiInputTrackState )
        DECLARE_ID( midiInputControlState )
        DECLARE_ID( midiOutputTrackState )
        DECLARE_ID( midiOutputControlState )

        DECLARE_ID( PREFCV )
        DECLARE_ID( CV0 )
        DECLARE_ID( CV1 )
        DECLARE_ID( CV2 )
        DECLARE_ID( CV3 )
        DECLARE_ID( CV4 )
        DECLARE_ID( CV5 )
        DECLARE_ID( CV6 )
        DECLARE_ID( CV7 )
        DECLARE_ID( PREFCVCLIB )
        DECLARE_ID( CLIB0 )
        DECLARE_ID( CLIB1 )
        DECLARE_ID( CLIB2 )
        DECLARE_ID( CLIB3 )

        DECLARE_ID( METRONOME )
        DECLARE_ID( enable )
        DECLARE_ID( countIn )
        DECLARE_ID( rate )

        DECLARE_ID( MIDICONTROL )
        DECLARE_ID( KNOBCONTROL )

        DECLARE_ID( probability )
        DECLARE_ID( ratchet )
        DECLARE_ID( WIFI )
        DECLARE_ID( wifienabled )
        DECLARE_ID( wifissid )
        DECLARE_ID( wifipasswd )
        DECLARE_ID( GESTURE )
        DECLARE_ID( gestureenabled )
        DECLARE_ID( gesturesensitivity )
        DECLARE_ID( gesturemapping )
        DECLARE_ID( Project )

        DECLARE_ID( IsPreviewOn )
        DECLARE_ID( IsPreviewAuto )

#undef DECLARE_ID
    }

    struct MTIVTBase
    {
        virtual ~MTIVTBase() = default;

        void setupBase( juce::ValueTree VT, juce::UndoManager* Undo )
        {
            vTree = VT;
            undo  = Undo;
        }

        virtual bool isValid() = 0;
        bool         isInValid() { return !isValid(); };
        virtual void makeDefaults() { vTree.removeAllProperties( undo ); }

        // private.
        juce::ValueTree    vTree;
        juce::UndoManager* undo{ nullptr };
    };
}
