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

#include "mt_audio_asset_manager.h"

namespace mt
{
    MTAudioAssetManager::MTAudioAssetManager()
    {
        empty.push_back( "" );
        tempDir     = getAppDirs( MTTemp );
        soundsDir   = getAppDirs( MTSamples );
        demosDir    = getAppDirs( MTDemos );
        internalDir = getAppDirs( MTProjects );
    }

    void MTAudioAssetManager::close()
    {
        um = nullptr;

        state = EMPTY::VTState;
        audio.clear();
        exports.clear();
        samples.clear();
        sliceMap.clear();

        // Default load from sounds directory
        curDir = soundsDir;
        updateDirectories( -1 );
    }

    void MTAudioAssetManager::setup( juce::ValueTree VT, juce::UndoManager* UM, const juce::String& ProjDir )
    {
        close();

        setRootFolder( ProjDir );
        um = UM;
        restoreFromValueTree( VT );
    }

    void MTAudioAssetManager::restoreFromValueTree( juce::ValueTree vt )
    {
        juce::ScopedLock l( lock );

        state = std::move( vt );

        if( !state.isValid() )
        {
            state = createVTree( IDs::FILEMANAGER,
                                 IDs::id, CommonUtils::createID() );
        }

        // Restore ID.
        if( state.getProperty( IDs::id ).toString().isEmpty() )
            state.setProperty( IDs::id, CommonUtils::createID(), nullptr );

        auto assetRestorer = [&]( juce::ValueTree Tree, AssetTypes Type ) {
            for( auto it : Tree )
            {
                if( it.hasType( IDs::FILE ) )
                {
                    restoreAsset( it, Type );
                }
            }
        };

        assetRestorer( state.getOrCreateChildWithName( IDs::ASSETS, um ), AssetTypes::Samples );
        assetRestorer( state.getOrCreateChildWithName( IDs::AUDIO, um ), AssetTypes::Audio );
        assetRestorer( state.getOrCreateChildWithName( IDs::EXPORTS, um ), AssetTypes::Exports );
    }

    juce::Identifier toId( AssetTypes Type )
    {
        if( AssetTypes::Exports == Type ) return IDs::EXPORTS;
        if( AssetTypes::Audio == Type ) return IDs::AUDIO;
        return IDs::ASSETS;
    }

    int MTAudioAssetManager::add( const juce::String& File, AssetTypes Type )
    {
        return addRemoveInternal( ADD_ASSET, File, Type );
    }

    void MTAudioAssetManager::remove( const juce::String& File, AssetTypes Type )
    {
        addRemoveInternal( REMOVE_ASSET, File, Type );
    }

    void MTAudioAssetManager::createDefaultSlice( juce::ValueTree FileVT, const juce::String& File )
    {
        // Create default slice.
        auto child = createVTree( IDs::SLICE,
                                  IDs::id, CommonUtils::createID() );
        // Add to asset file node.
        FileVT.addChild( child, -1, nullptr );

        // Create slice object
        restoreSlice( child, File );
    }

    void MTAudioAssetManager::restoreAsset( juce::ValueTree assetVT, AssetTypes Type )
    {
        auto file = assetVT.getProperty( IDs::path, "" );

        // Add to asset manager.
        addToAssetMgr( file, Type );

        // Add to sample cache.
        auto LengthInSeconds = ( AssetTypes::Samples == Type ) ? 5.0 : -1;
        asc.add( file, LengthInSeconds );

        // Restore slices.
        auto numChildren = assetVT.getNumChildren();
        if( 0 == numChildren )
        { // If no children, create default slice.
            createDefaultSlice( assetVT, file );
        }
        else
        { // Restore.
            for( auto i = 0; i < numChildren; ++i )
            {
                auto sliceVT = assetVT.getChild( i );
                restoreSlice( sliceVT, file );
            }
        }
    }

    void MTAudioAssetManager::restoreSlice( juce::ValueTree sliceVT, const juce::String& File )
    {
        if( sliceVT.isValid() && sliceVT.hasType( IDs::SLICE ) )
        {
            // Create slice object
            auto sObj = std::make_unique<ASoundSlice>( sliceVT, um );
            sObj->setSource( asc.exists( File ) );

            // Add to slice map.
            sliceMap[File].push_back( std::move( sObj ) );
        }
    }

    int MTAudioAssetManager::addRemoveInternal( operation AddOrRemove, const juce::String& File, AssetTypes Type )
    {
        if( state.isValid() )
        {
            auto v           = state.getOrCreateChildWithName( toId( Type ), nullptr );
            auto f           = v.getChildWithProperty( IDs::path, File );
            auto file        = juce::File( File );
            auto bValidAudio = file.hasFileExtension( ".wav;bwf;aif;aiff;flac;ogg;mp3;wmv;asf;wm;wma" ) && file.getSize();

            juce::ScopedLock l( lock );
            if( ADD_ASSET == AddOrRemove && bValidAudio )
            {
                if( !f.isValid() )
                {
                    auto pathForTree = ( projDir != juce::File() && projDir.exists() && file.isAChildOf( projDir ) )
                                           ? file.getRelativePathFrom( projDir )
                                           : File;

                    // Create Node..
                    auto fileVT = createVTree( IDs::FILE,
                                               IDs::id, CommonUtils::createID(),
                                               IDs::path, pathForTree );

                    // Add this node to actual asset tree.
                    v.appendChild( fileVT, um );

                    // Add to asset manager.
                    addToAssetMgr( File, Type );
                    if( pathForTree != File )
                        addToAssetMgr( pathForTree, Type );

                    // Add to sample cache.
                    auto LengthInSeconds = ( AssetTypes::Samples == Type ) ? 5.0 : -1;
                    asc.add( File, LengthInSeconds );

                    // Create slice object
                    createDefaultSlice( fileVT, File );

                    return AssetAdded;
                }
                else
                {
                    return AssetAlreadyExists;
                }
            }
            else if( REMOVE_ASSET == AddOrRemove && f.isValid() )
            {
                // Remove all slices.
                sliceMap.erase( File );
                // Remove from sample cache.
                asc.remove( File );
                // Remove from asset tree.
                v.removeChild( f, um );
                // Remove from asset manager.
                removeFromAssetMgr( File, Type );
                return AssetRemoved;
            }
        }
        return AssetInvalid;
    }

    void MTAudioAssetManager::addToAssetMgr( const juce::String& File, AssetTypes Type )
    {
        if( File.isNotEmpty() )
        {
            juce::ScopedLock l( lock );
            switch( Type )
            {
                case AssetTypes::Exports:
                    if( std::find( exports.begin(), exports.end(), File ) == exports.end() )
                        exports.push_back( File );
                    break;
                case AssetTypes::Audio:
                    if( std::find( audio.begin(), audio.end(), File ) == audio.end() )
                        audio.push_back( File );
                    break;
                default:
                case AssetTypes::Samples:
                    if( std::find( samples.begin(), samples.end(), File ) == samples.end() )
                        samples.push_back( File );
                    break;
            }
        }
    }

    void MTAudioAssetManager::removeFromAssetMgr( const juce::String& File, AssetTypes Type )
    {
        juce::ScopedLock l( lock );
        switch( Type )
        {
            case AssetTypes::Exports:
                exports.erase( std::find( exports.begin(), exports.end(), File ) );
                break;
            case AssetTypes::Audio:
                audio.erase( std::find( audio.begin(), audio.end(), File ) );
                break;
            default:
            case AssetTypes::Samples:
                samples.erase( std::find( samples.begin(), samples.end(), File ) );
                break;
        }
    }

    void MTAudioAssetManager::setRootFolder( const juce::String& ProjDir )
    {
        projDir = juce::File( ProjDir );
    }

    const std::vector<juce::String>& MTAudioAssetManager::getAssets( AssetTypes Type )
    {
        switch( Type )
        {
            case AssetTypes::Exports: return exports;
            case AssetTypes::Audio: return audio;
            case AssetTypes::Samples: return samples;
        }
        return empty;
    }

    std::vector<juce::String> MTAudioAssetManager::getAssetNames( AssetTypes Type )
    {
        std::vector<juce::String> names;
        auto&                     list = getAssets( Type );
        for( auto& it : list )
        {
            juce::File a( it );
            names.push_back( a.getFileName() );
        }
        return names;
    }

    const juce::String& MTAudioAssetManager::getAsset( AssetTypes Type, int Index )
    {
        juce::ScopedLock l( lock );
        switch( Type )
        {
            case AssetTypes::Exports: return ( Index < exports.size() ) ? exports.at( (size_t)Index ) : empty.at( (size_t)0 );
            case AssetTypes::Audio: return ( Index < audio.size() ) ? audio.at( (size_t)Index ) : empty.at( (size_t)0 );
            case AssetTypes::Samples: return ( Index < samples.size() ) ? samples.at( (size_t)Index ) : empty.at( (size_t)0 );
        }
        return empty.at( (size_t)0 );
    }

    juce::String MTAudioAssetManager::getAssetName( AssetTypes Type, int Index )
    {
        auto tmp = getAsset( Type, Index );
        return juce::File( tmp ).getFileName();
    }

    ASound::Ptr MTAudioAssetManager::getAssetSound( const juce::String& File )
    {
        return asc.getOrLoad( File );
    }

    ASound::Ptr MTAudioAssetManager::getAssetSound( int Index, AssetTypes Type )
    {
        auto LengthInSeconds = ( AssetTypes::Samples == Type ) ? 5.0 : -1;
        auto asset           = getAssetName( Type, Index );
        return asset.isEmpty() ? nullptr : asc.getOrLoad( asset, LengthInSeconds );
    }

    std::vector<std::unique_ptr<ASoundSlice>>& MTAudioAssetManager::getAssetSlices( const juce::String& File )
    {
        return sliceMap[File];
    }

    std::vector<std::unique_ptr<ASoundSlice>>& MTAudioAssetManager::getAssetSlices( int Index, AssetTypes Type )
    {
        return getAssetSlices( getAssetName( Type, Index ) );
    }

    ASoundSlice* MTAudioAssetManager::getAssetSlice( const juce::String& File, int Index )
    {
        return ( Index < sliceMap[File].size() ) ? sliceMap[File].at( Index ).get() : nullptr;
    }

    void MTAudioAssetManager::addAssetSlice( const juce::String& File, ASoundSlice* Slice, int Index )
    {
        if( sliceMap.find( File ) != sliceMap.end() )
        {
            auto& slices     = sliceMap[File];
            int   maxEntries = slices.size();
            // Find or create parent and slice objects.
            auto parent = slices.at( 0 )->getState().getParent();
            auto child  = Slice->getState().createCopy();
            // Create slice object.
            auto sObj = std::make_unique<ASoundSlice>( child, um );
            sObj->setSource( asc.exists( File ) );
            // Add to map, and tree.
            int pos = ( 0 >= Index ) ? slices.size() : Index;
            parent.addChild( child, pos, um );
            slices.insert( ( slices.begin() + pos ), std::move( sObj ) );
        }
    }

    void MTAudioAssetManager::removeAssetSlice( const juce::String& File, int Index )
    {
        if( sliceMap.find( File ) != sliceMap.end() )
        {
            auto& slices     = sliceMap[File];
            int   maxEntries = slices.size();

            auto parent = slices.at( 0 )->getState().getParent();
            if( 0 > Index && Index < maxEntries )
            {
                parent.removeChild( Index, um );
                slices.erase( slices.begin() + Index, slices.begin() + Index + 1 );
            }
        }
    }

    void MTAudioAssetManager::removeAllSlices( const juce::String& File )
    {
        if( sliceMap.find( File ) != sliceMap.end() )
        {
            auto& slices     = sliceMap[File];
            int   maxEntries = slices.size();

            auto parent      = slices.at( 0 )->getState().getParent();
            auto numChildren = parent.getNumChildren();
            for( auto i = 1; i < numChildren; ++i )
                parent.removeChild( i, um );

            slices.erase( slices.begin() + 1, slices.end() );
        }
    }

    void MTAudioAssetManager::updateDirectories( int Index )
    {
        dirList = curDir.findChildFiles( juce::File::TypesOfFileToFind::findFilesAndDirectories, false );

        if( juce::isPositiveAndBelow( Index, dirList.size() ) )
        {
            selectedDir = Index;
            selDir      = dirList[Index];
        }
        else
        {
            selectedDir = 0;
            selDir      = dirList[0];
        }
        updateFiles();
    }

    void MTAudioAssetManager::updateFiles()
    {
        fileList = selDir.findChildFiles( juce::File::TypesOfFileToFind::findFiles, false, "*.wav;*.bwav;*.bwf;*.aiff;*.aif;*.ogg;*.mp3;*.flac" );
    }

    void MTAudioAssetManager::dirSelect( int Index )
    {
        updateDirectories( Index );
    }

    void MTAudioAssetManager::dirEnter( int Index )
    {
        if( juce::isPositiveAndBelow( Index, dirList.size() ) )
            curDir = dirList[Index];

        updateDirectories( -1 );
    }

    void MTAudioAssetManager::dirGoUp()
    {
        curDir = curDir.getParentDirectory();
        updateDirectories( -1 );
    }

    void MTAudioAssetManager::setCurrentDir( juce::File& currDir )
    {
        curDir = currDir;
        updateDirectories( -1 );
    }

}
