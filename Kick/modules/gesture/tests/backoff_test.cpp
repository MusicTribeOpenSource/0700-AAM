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

#include "backoff.h"

#include <gtest/gtest.h>

using kick_gesture::Backoff;

TEST( Backoff, DueBeforeTheFirstFailure )
{
    Backoff b( 1000, 30000 );
    EXPECT_TRUE( b.due( 0 ) );
    EXPECT_EQ( 0u, b.remainingMs( 0 ) );
}

TEST( Backoff, DoublesAndStopsAtTheCap )
{
    Backoff  b( 1000, 30000 );
    uint64_t now = 0;
    const uint32_t expected[] = { 1000, 2000, 4000, 8000, 16000, 30000, 30000, 30000 };
    for( uint32_t want : expected )
    {
        EXPECT_EQ( want, b.nextDelayMs() );
        b.failure( now );
        EXPECT_FALSE( b.due( now + want - 1 ) );
        EXPECT_TRUE( b.due( now + want ) );
        now += want;
    }
}

TEST( Backoff, RemainingCountsDown )
{
    Backoff b( 1000, 30000 );
    b.failure( 5000 );
    EXPECT_EQ( 1000u, b.remainingMs( 5000 ) );
    EXPECT_EQ( 250u, b.remainingMs( 5750 ) );
    EXPECT_EQ( 0u, b.remainingMs( 6000 ) );
}

TEST( Backoff, SuccessStartsOver )
{
    Backoff b( 1000, 30000 );
    for( int i = 0; i < 10; ++i )
        b.failure( 0 );
    EXPECT_EQ( 30000u, b.nextDelayMs() );
    b.success();
    EXPECT_TRUE( b.due( 0 ) );
    EXPECT_EQ( 1000u, b.nextDelayMs() );
}

TEST( Backoff, AMaximumBelowTheStartIsRaisedToTheStart )
{
    Backoff b( 500, 100 );
    b.failure( 0 );
    b.failure( 0 );
    EXPECT_EQ( 500u, b.nextDelayMs() );
}

TEST( Backoff, NeverOverflowsOnAHugeCap )
{
    Backoff b( 1u << 30, 0xFFFFFFFFu );
    for( int i = 0; i < 8; ++i )
        b.failure( 0 );
    EXPECT_EQ( 0xFFFFFFFFu, b.nextDelayMs() );
}

TEST( Backoff, NoTightLoopInOneMinute )
{
    // An app that polls whenever due, against a service that stays down, makes few attempts.
    Backoff  b( 1000, 30000 );
    int      attempts = 0;
    for( uint64_t now = 0; now < 60000; now += 10 )
    {
        if( !b.due( now ) )
            continue;
        ++attempts;
        b.failure( now );
    }
    EXPECT_LE( attempts, 8 );
    EXPECT_GE( attempts, 5 );
}
