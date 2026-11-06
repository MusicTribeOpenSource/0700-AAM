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

#include "kick_gesture/image.h"
#include "kick_gesture/types.h"

#include <string>
#include <vector>

namespace kickgest
{
    // The seam between the camera and the gesture logic. A backend takes one RGB frame and returns the hands in it
    // as 21 landmarks each. Replacing the model (another runtime, another network) means writing one of these;
    // nothing else in the service depends on how the landmarks were found.
    //
    // Contract for implementations:
    //  - landmarks are in MediaPipe order (types.h) with x and y as fractions of the frame width and height;
    //  - `out` is cleared first and holds the hands best first, at most as many as the backend was configured for;
    //  - Hand::score is the confidence that a real hand is there, 0 to 1, and low scores are returned as they are
    //    (the engine applies the gate), except that a backend may drop what its own threshold rejects;
    //  - a blank, saturated or noisy frame is not an error: it returns true with no hands;
    //  - false is for failures of the backend itself, with the reason in `error`;
    //  - detect() is not thread safe. The service calls it from one thread.
    class ILandmarkModel
    {
    public:
        virtual ~ILandmarkModel() = default;
        virtual const char* name() const = 0;
        virtual bool detect(const RgbImage& frame, std::vector<Hand>& out, std::string& error) = 0;
    };
}
