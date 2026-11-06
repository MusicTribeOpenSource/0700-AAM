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

#include <cstdint>

namespace kick_gesture
{
    // Retry timing for a link that can be down for a long time: the delay doubles after every failure, up to a cap,
    // and goes back to the start after a success. The caller passes in the time (a monotonic clock in milliseconds),
    // so the logic has no clock of its own and a test can drive it.
    class Backoff
    {
    public:
        explicit Backoff( uint32_t initialMs = 1000, uint32_t maxMs = 30000 ) : initialMs_( initialMs ), maxMs_( maxMs < initialMs ? initialMs : maxMs ), delayMs_( initialMs ) {}

        // True when the next attempt may run. Always true before the first failure.
        bool due( uint64_t nowMs ) const { return !waiting_ || nowMs >= nextMs_; }

        // Records a failed attempt at `nowMs`. The next attempt is due after the current delay; the delay then doubles.
        void failure( uint64_t nowMs )
        {
            waiting_ = true;
            nextMs_  = nowMs + delayMs_;
            const uint64_t doubled = static_cast<uint64_t>( delayMs_ ) * 2u;
            delayMs_ = doubled > maxMs_ ? maxMs_ : static_cast<uint32_t>( doubled );
        }

        void success()
        {
            waiting_ = false;
            delayMs_ = initialMs_;
        }

        // The delay the next failure will schedule.
        uint32_t nextDelayMs() const { return delayMs_; }
        // Milliseconds until the next attempt, 0 when it is due.
        uint64_t remainingMs( uint64_t nowMs ) const { return due( nowMs ) ? 0 : nextMs_ - nowMs; }

    private:
        uint32_t initialMs_;
        uint32_t maxMs_;
        uint32_t delayMs_;
        uint64_t nextMs_{ 0 };
        bool     waiting_{ false };
    };
}
