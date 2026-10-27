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

#ifdef PROJECT_HELPERS_H_INCLUDE
/* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
    #error "Incorrect use of JUCE cpp file"
#endif

// module include files...
#include "project.h"

#include "kick_xml_manager.cpp"
#include "kick_settings.cpp"

namespace mt
{
    projectManager::projectManager()
        : KickXML( IDs::ROOT, __projectFileExtension, __projectFileVersion )
    {}

    void projectManager::scanDirectory( const juce::File Directory ) {}

    const juce::String& projectManager::getProjectDir() { return KickXML::getDirectory(); }

    void projectManager::rename( const juce::String& NewName )
    {
        if (!save()) return;

        juce::File       dir, project{ "" };
        juce::ScopedLock l( KickXML::getCriticalSection() );

        auto newProjectFile = project.getSiblingFile( NewName + __projectFileExtension );
        project.moveFileTo( newProjectFile );
        project = "";

        auto newProjectPath = dir.getSiblingFile( NewName );
        if( newProjectPath.exists() )
            newProjectPath.deleteRecursively();

        dir.moveFileTo( newProjectPath );
        dir     = newProjectPath;
        project = newProjectFile;
    }

    juce::ValueTree projectManager::loadOrCreate( const juce::File& XMLFIle )
    {
        auto tree = KickXML::loadOrCreate( XMLFIle );
        if (!tree.isValid()) return {};

        bool HasProjectSubTree = tree.getChildWithName( IDs::Project ).isValid();

        if( HasProjectSubTree ) return tree;

        auto project = juce::ValueTree( IDs::Project );
        project.setProperty( IDs::version, __projectVersion, nullptr );
        tree.appendChild( project, nullptr );
        return tree;
    }

}
