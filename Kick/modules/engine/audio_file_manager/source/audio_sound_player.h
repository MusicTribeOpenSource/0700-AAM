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
    class ASoundPlayer
        : public ASliceRenderer
    {
    public:
        ASoundPlayer();
        ~ASoundPlayer();

        bool isValidSource();

        void setSoundToPlay( const juce::String& File, double LengthInSeconds ); // LengthInSeconds <= 0, play complete file.
        void removeSound();
        void purge() { removeSound(); }

        double getSampleRate() const noexcept { return slice.getReSampleRate(); }
        void   setSampleRate( double SampleRate ) { slice.setReSampleRate( SampleRate ); };

        bool isLooping() const noexcept { return slice.isLooping(); }
        void setLooping( bool Loop ) noexcept { slice.setLooping( Loop ); }

        int  getTranspose() const noexcept { return slice.fine(); }
        void setTranspose( int Loop ) noexcept { slice.setFine( Loop ); }

    private:
        juce::AudioFormatManager audioFmtMgr;
        ASound::Ptr              sound{ new ASound("", audioFmtMgr, 1.f) };
        ASoundSlice              slice;
    };

} // namespace mt
