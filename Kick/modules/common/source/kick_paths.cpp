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

//common
#define MT_SOURCE_SRCDIR "."

#if KICK_EMU
    #include "kick_emu_hal.h"
#endif

juce::File getAppDirs( DirType T )
{
    auto seprator = juce::File::getSeparatorString();
#if defined( _WIN32 ) || defined( _WIN64 )
    auto rootP = juce::File::getSpecialLocation( juce::File::currentApplicationFile ).getParentDirectory().getFullPathName() + seprator;
    if( T == MTAssets )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "assets" );
    else if( T == MTSamples )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "samples" );
    else if( T == MTDemos )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "demos" );
    else if( T == MTTemp )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "templates" );
    else if( T == MTProjects )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "projects" );
    else if( T == MTLog )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "logs" + seprator );
    else if( T == MTPrefs )
        return juce::File( rootP + juce::String( MT_SOURCE_SRCDIR ) + seprator + "prefs" + seprator );
#elif defined( __APPLE_CPP__ ) || defined( __APPLE_CC__ ) || defined( JUCE_MAC )
    if( T == MTAssets )
        return juce::File( "./assets" );
    else if( T == MTSamples )
#if KICK_EMU
        return juce::File( kick_emu::devicePath( "/data/sounds" ) ); // same place as on the unit
#else
        return juce::File( "./data/samplesKick/" );
#endif
    else if( T == MTDemos )
        return juce::File( "./demos" );
    else if( T == MTTemp )
        return juce::File( "./templates" );
#if KICK_EMU
    // Writable state lives in the emulated device root, like /home/Kick on the unit.
    else if( T == MTProjects )
        return juce::File( kick_emu::devicePath( "/home/Kick/projects/" ) );
    else if( T == MTLog )
        return juce::File( kick_emu::devicePath( "/home/Kick/logs/" ) );
    else if( T == MTPrefs )
        return juce::File( kick_emu::devicePath( "/home/Kick/prefs" ) );
#else
    else if( T == MTProjects )
        return juce::File( "./projects" );
    else if( T == MTLog )
        return juce::File( "./logs/" );
    else if( T == MTPrefs )
        return juce::File( "./prefs" );
#endif
#elif defined( LINUX ) || defined( __linux__ )
    if( T == MTAssets )
        return juce::File( "/app/assets" );
    else if( T == MTSamples )
        return juce::File( "/data/sounds" );
    else if( T == MTDemos )
        return juce::File( "/app/demos/" );
    else if( T == MTTemp )
        return juce::File( "/app/templates/" );
    else if( T == MTProjects )
        return juce::File( "/home/Kick/projects/" );
    else if( T == MTLog )
        return juce::File( "/home/Kick/logs/" );
    else if( T == MTPrefs )
        return juce::File( "/home/Kick/prefs" );
#endif

    return juce::File::getSpecialLocation( juce::File::tempDirectory );
}

juce::File getDefaultPath()
{
    juce::File rootP;
    auto       seprator = juce::File::getSeparatorString();

#if defined( _WIN32 ) || defined( _WIN64 )
    rootP = juce::File::getSpecialLocation( juce::File::currentApplicationFile ).getParentDirectory().getFullPathName() + seprator;

#elif defined( __APPLE_CPP__ ) || defined( __APPLE_CC__ ) || defined( JUCE_MAC )
    rootP = "./app/";

#elif defined( LINUX ) || defined( __linux__ )
    rootP = "./app/";
#endif
    return rootP;
}
