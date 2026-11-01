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

#include "iSynthObject.h"

namespace mt
{
    class MTDrumProgram;
    class DrumProgram : public ISynthObject
    {
    public:
        explicit DrumProgram( AudIO::Design& D, const juce::String& Name, MTIProgram* PDrum );
        ~DrumProgram();

        void setProgram( MTIProgram* PDrum ) override;
        void noteOn( int Note, int Velocity ) override;
        using ISynthObject::noteOff;
        void noteOff( int Note ) override;

    private:
        MTDrumProgram*                            pDrum{ nullptr };
        std::vector<std::unique_ptr<ISynthVoice>> voices;
        std::map<int, int>                        noteLayerIdx;
    };
};
