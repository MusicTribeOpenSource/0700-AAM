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
#include <gtest/gtest.h>

using kick_gesture::PinchSmoother;

TEST( Smoother, ResetSetsTheOutputWithoutFiltering )
{
    PinchSmoother s;
    s.reset( 0.7f, 1000 );
    EXPECT_FLOAT_EQ( 0.7f, s.value() );
    s.reset( 3.f, 1000 );
    EXPECT_FLOAT_EQ( 1.f, s.value() );
    s.reset( -1.f, 1000 );
    EXPECT_FLOAT_EQ( 0.f, s.value() );
}

TEST( Smoother, ConvergesOnAConstantTarget )
{
    PinchSmoother s;
    s.configure( 100.f, 100.f ); // the slew limit is out of the way
    s.reset( 0.f, 0 );
    float v = 0.f;
    for( uint64_t t = 50; t <= 1000; t += 50 )
        v = s.update( 1.f, t );
    // 1000 ms is 10 time constants.
    EXPECT_NEAR( 1.f, v, 1e-3f );
}

TEST( Smoother, OnePoleStepMatchesTheFormula )
{
    PinchSmoother s;
    s.configure( 100.f, 100.f );
    s.reset( 0.f, 0 );
    const float v = s.update( 1.f, 100 ); // dt equals tau
    EXPECT_NEAR( 1.f - std::exp( -1.f ), v, 1e-5f );
}

TEST( Smoother, SlewLimitCapsTheSpeed )
{
    PinchSmoother s;
    s.configure( 0.f, 2.f ); // no low-pass, 2.0 per second at most
    s.reset( 0.f, 0 );
    // A jump of 1.0 in 100 ms would be 10 per second. The limit allows 0.2 for that dt.
    EXPECT_NEAR( 0.2f, s.update( 1.f, 100 ), 1e-6f );
    EXPECT_NEAR( 0.4f, s.update( 1.f, 200 ), 1e-6f );
    // The slew limit also holds on the way down.
    EXPECT_NEAR( 0.2f, s.update( 0.f, 300 ), 1e-6f );
}

TEST( Smoother, NeverLeavesTheUnitRange )
{
    PinchSmoother s;
    s.configure( 10.f, 100.f );
    s.reset( 0.5f, 0 );
    for( uint64_t t = 50; t <= 2000; t += 50 )
    {
        const float v = s.update( t % 100 == 0 ? 5.f : -5.f, t );
        EXPECT_GE( v, 0.f );
        EXPECT_LE( v, 1.f );
    }
    EXPECT_FALSE( std::isnan( s.update( std::nanf( "" ), 3000 ) ) );
}

TEST( Smoother, SmoothingCutsJitterToAThird )
{
    // A hand held still still moves the aperture a little each frame. Alternating +-0.05 around 0.5 at 20 Hz.
    PinchSmoother s;
    s.configure( 220.f, 3.f ); // what sensitivity 5 uses
    s.reset( 0.5f, 0 );
    float lo = 1.f, hi = 0.f;
    for( int i = 1; i <= 200; ++i )
    {
        const float raw = 0.5f + ( i % 2 ? 0.05f : -0.05f );
        const float v   = s.update( raw, static_cast<uint64_t>( i ) * 50 );
        if( i > 40 )
        {
            lo = std::min( lo, v );
            hi = std::max( hi, v );
        }
    }
    EXPECT_LT( hi - lo, 0.1f / 3.f ) << "raw peak to peak is 0.1";
    EXPECT_NEAR( 0.5f, 0.5f * ( hi + lo ), 0.01f );
}

TEST( Smoother, SameTimestampChangesNothing )
{
    PinchSmoother s;
    s.configure( 50.f, 100.f );
    s.reset( 0.2f, 100 );
    EXPECT_FLOAT_EQ( 0.2f, s.update( 1.f, 100 ) );
}

TEST( Smoother, BackwardsClockKeepsTheOutputAndTakesTheNewReference )
{
    PinchSmoother s;
    s.configure( 100.f, 100.f );
    s.reset( 0.2f, 5000 );
    EXPECT_FLOAT_EQ( 0.2f, s.update( 1.f, 100 ) ); // the service restarted: the clock went back
    // The next update measures dt from 100, not from 5000.
    const float v = s.update( 1.f, 200 );
    EXPECT_NEAR( 0.2f + 0.8f * ( 1.f - std::exp( -1.f ) ), v, 1e-5f );
}
