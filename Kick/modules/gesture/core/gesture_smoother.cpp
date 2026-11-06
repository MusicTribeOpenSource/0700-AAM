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

#include "gesture_smoother.h"

#include <algorithm>
#include <cmath>

namespace kick_gesture
{
    namespace
    {
        float clamp01( float v )
        {
            if( !( v > 0.f ) ) // also catches NaN
                return 0.f;
            return v > 1.f ? 1.f : v;
        }
    }

    void PinchSmoother::configure( float tauMs, float maxSlewPerSec )
    {
        tauMs_         = std::max( 0.f, tauMs );
        maxSlewPerSec_ = std::max( 0.f, maxSlewPerSec );
    }

    void PinchSmoother::reset( float v, uint64_t tMs )
    {
        value_  = clamp01( v );
        lastMs_ = tMs;
    }

    float PinchSmoother::update( float target, uint64_t tMs )
    {
        if( tMs < lastMs_ )
        {
            lastMs_ = tMs;
            return value_;
        }
        if( tMs == lastMs_ )
            return value_;

        const float dtMs  = static_cast<float>( tMs - lastMs_ );
        lastMs_           = tMs;
        const float goal  = clamp01( target );
        const float alpha = tauMs_ <= 0.f ? 1.f : 1.f - std::exp( -dtMs / tauMs_ );
        float       step  = ( goal - value_ ) * alpha;
        const float limit = maxSlewPerSec_ * dtMs / 1000.f;
        step              = std::max( -limit, std::min( limit, step ) );
        value_            = clamp01( value_ + step );
        return value_;
    }
}
