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

juce::File getAppDirs( DirType T )
{
    const auto resource = juce::SystemStats::getEnvironmentVariable("KICK_RESOURCE_ROOT", {});
    const auto data = juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {});
    if (resource.isNotEmpty()) {
        const auto root = juce::File::getCurrentWorkingDirectory().getChildFile(resource);
        if (T == MTAssets) return root.getChildFile("assets");
        if (T == MTTemp) return root.getChildFile("templates");
        if (T == MTDemos) return root.getChildFile("demos");
    }
    if (data.isNotEmpty()) {
        const auto root = juce::File::getCurrentWorkingDirectory().getChildFile(data);
        if (T == MTPrefs) return root.getChildFile("prefs");
        if (T == MTProjects) return root.getChildFile("projects");
        if (T == MTLog) return root.getChildFile("logs");
        if (T == MTSamples) return root.getChildFile("sounds");
    }
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
        return juce::File( "./data/samplesKick/" );
    else if( T == MTDemos )
        return juce::File( "./demos" );
    else if( T == MTTemp )
        return juce::File( "./templates" );
    else if( T == MTProjects )
        return juce::File( "./projects" );
    else if( T == MTLog )
        return juce::File( "./logs/" );
    else if( T == MTPrefs )
        return juce::File( "./prefs" );
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
