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

#include "kick_gesture/types.h"

#include <istream>
#include <ostream>
#include <string>

// Landmark record files: one JSON object per line, the first line a header.
//
//   {"format":"kick-gesture-landmarks","version":1}
//   {"t_ms":123456,"aspect":1.333333,"hands":[{"score":0.987,"right":0.91,"lm":[x0,y0,z0,x1,y1,z1,...]}]}
//
// "lm" has 63 numbers, 21 landmarks in MediaPipe order. A record holds hand geometry only, never pixels. Numbers have six
// decimals, the precision LandmarkFrame::quantize() rounds to, so a recorded stream replays to the events the live run produced.
namespace kickgest
{
    constexpr int kRecordVersion = 1;

    class LandmarkWriter
    {
    public:
        explicit LandmarkWriter(std::ostream& out) : out_(out) {}
        void writeHeader();
        // Writes the frame as it is. Call quantize() on it first when it came from a live run.
        void write(const LandmarkFrame& frame);

    private:
        std::ostream& out_;
    };

    class LandmarkReader
    {
    public:
        explicit LandmarkReader(std::istream& in) : in_(in) {}

        // True and a frame, or false. After false, error() is empty at the end of the stream and holds the reason
        // (with the line number) for a malformed line. Blank lines and the header line are skipped.
        bool next(LandmarkFrame& out);
        const std::string& error() const { return error_; }
        size_t line() const { return line_; }

    private:
        std::istream& in_;
        std::string error_;
        size_t line_ = 0;
    };
}
