/*
 * Copyright (c) 2024 MUSIC Tribe
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
    ///
    /// ASoundPlayer
    ///
    ASoundPlayer::ASoundPlayer()
    {
        audioFmtMgr.registerBasicFormats();
        ASliceRenderer::setSlice( &slice );
        slice.setSource( sound.get() );
    }

    ASoundPlayer::~ASoundPlayer()
    {
        ASliceRenderer::stop();
    }

    void ASoundPlayer::setSoundToPlay( const juce::String& File, double LengthInSeconds )
    {
        ASliceRenderer::stop();
        sound->setSound( File, LengthInSeconds );
        slice.setSource(sound.get());

        slice.refresh();
        slice.setLoopPoints( 0, sound->getNumSamplesPerChannel() );

        ASliceRenderer::reset();
    }

    void ASoundPlayer::removeSound()
    {
        sound->purge();
        slice.setSource( nullptr );
    }

    bool ASoundPlayer::isValidSource()
    {
        return sound->isValid();
    }

} // namespace mt
