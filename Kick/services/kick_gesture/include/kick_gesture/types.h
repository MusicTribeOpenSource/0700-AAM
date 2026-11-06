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

#include <array>
#include <cstdint>
#include <vector>

namespace kickgest
{
    constexpr int kNumLandmarks = 21;

    // MediaPipe hand landmark order, which every ILandmarkModel backend must produce.
    enum Landmark : int
    {
        WRIST = 0,
        THUMB_CMC = 1,
        THUMB_MCP = 2,
        THUMB_IP = 3,
        THUMB_TIP = 4,
        INDEX_MCP = 5,
        INDEX_PIP = 6,
        INDEX_DIP = 7,
        INDEX_TIP = 8,
        MIDDLE_MCP = 9,
        MIDDLE_PIP = 10,
        MIDDLE_DIP = 11,
        MIDDLE_TIP = 12,
        RING_MCP = 13,
        RING_PIP = 14,
        RING_DIP = 15,
        RING_TIP = 16,
        PINKY_MCP = 17,
        PINKY_PIP = 18,
        PINKY_DIP = 19,
        PINKY_TIP = 20
    };

    // x is a fraction of the frame width and y a fraction of the frame height, both in image orientation
    // (origin top left, y down). z is in the same units as x, relative to the wrist, and is not used by the
    // gesture logic.
    struct Point3
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    struct Hand
    {
        std::array<Point3, kNumLandmarks> lm{};
        // The model's confidence that a hand is present, 0 to 1.
        float score = 0.f;
        // The model's handedness output as it came out: near 1 is "right", near 0 is "left", for the image as
        // the camera delivered it. GestureConfig::swapHandedness decides how that maps to the user's hand.
        float rightProb = 0.5f;
    };

    // What one camera frame produced: zero or more hands, best first.
    struct LandmarkFrame
    {
        // CLOCK_MONOTONIC milliseconds of the frame (camera timestamp). All gesture timing uses this clock only.
        uint64_t tMs = 0;
        // Frame width divided by frame height. It turns the normalized coordinates into isotropic ones.
        float aspect = 4.f / 3.f;
        std::vector<Hand> hands;

        // Rounds every coordinate and score to six decimals, the precision of the record file. The live path
        // calls this before the engine so a recorded stream replays to the same events as the live run.
        void quantize();
    };

    enum class GestureType : int
    {
        SwipeLeft = 0,
        SwipeRight,
        PalmHold,
        Fist,
        PinchStart,
        PinchChange,
        PinchEnd,
        Count
    };

    constexpr int kGestureTypeCount = static_cast<int>(GestureType::Count);

    // Contract names, as in docs/gesture-events.md.
    const char* gestureName(GestureType t);
    bool parseGestureName(const char* name, GestureType& out);

    enum class Handedness
    {
        Left,
        Right,
        Unknown
    };
    const char* handednessName(Handedness h);

    struct GestureEvent
    {
        GestureType type = GestureType::SwipeLeft;
        // Only pinch_start, pinch_change and pinch_end carry a value (0 to 1).
        bool hasValue = false;
        float value = 0.f;
        Handedness hand = Handedness::Unknown;
        float confidence = 0.f;
        uint64_t tMs = 0;
    };
}
