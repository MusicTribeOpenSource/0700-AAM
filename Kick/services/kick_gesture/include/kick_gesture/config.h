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

#include <cstdint>
#include <string>

// Thresholds for the temporal layer. Every field is documented here with its unit, and the same names appear
// in gesture.json as "section": { "key": value }. Distances are in palm sizes: one palm size is the distance
// from the wrist to the base of the middle finger, measured in the same frame, so the thresholds do not
// depend on how far the hand is from the camera.
namespace kickgest
{
    struct GestureConfig
    {
        // tracking
        float minHandScore = 0.8f;       // frames whose best hand scores below this count as "no hand"
        float minEventConfidence = 0.6f; // an event whose confidence is below this is never emitted
        uint32_t lostMs = 300;           // with no hand for this long, tracking ends (open pinch ends too)
        bool flipX = true;               // image x runs opposite to the user's x (camera faces the user)
        bool swapHandedness = true;      // the handedness label of an unmirrored image is the other hand

        // pose
        float fingerExtendedRatio = 1.15f; // finger extended when dist(wrist, tip) / dist(wrist, pip) is above this
        float thumbExtendedRatio = 0.6f;   // thumb extended when dist(thumb tip, index base) / palm size is above this
        float pinchMinReach = 1.2f;        // a pinch needs the index tip at least this far from the wrist (palm sizes), which a fist does not have

        // palm_hold
        uint32_t palmHoldMs = 600;      // open palm held this long
        float palmHoldMaxSpeed = 1.0f;  // palm sizes per second, the hand has to be this still
        uint32_t palmHoldExitMs = 200;  // pose lost this long re-arms the gesture

        // fist
        uint32_t fistMs = 350;
        float fistMaxSpeed = 1.5f;       // palm sizes per second, so a hand closing while it reaches is not a fist
        uint32_t fistExitMs = 200;

        // swipe
        uint32_t swipeWindowMs = 500;  // longest stroke
        float swipeMinTravel = 2.5f;   // palm sizes moved sideways
        float swipeMinSpeed = 4.0f;    // palm sizes per second across the stroke
        float swipeMinDominance = 2.0f; // sideways travel over vertical travel
        float swipeMinMonotonic = 0.8f; // share of sample steps that go the same way as the stroke
        uint32_t swipeMinExtended = 3;  // fingers (of four) that must be extended
        uint32_t swipeRearmMs = 150;    // after a swipe the hand has to be slow this long before the next one counts
        float swipeRestSpeed = 1.5f;    // palm sizes per second that counts as slow for the re-arm

        // pinch: ratio = dist(thumb tip, index tip) / palm size
        float pinchOn = 0.30f;
        float pinchOff = 0.60f;
        float pinchMin = 0.12f;       // ratio that reads as value 0
        uint32_t pinchStartMs = 120;  // ratio below pinchOn for this long starts a pinch
        uint32_t pinchEndMs = 100;    // ratio above pinchOff for this long ends it
        float pinchDeadband = 0.02f;  // smallest value change that produces a pinch_change

        // rate limits: smallest gap in milliseconds between two events of a type
        uint32_t rateSwipeMs = 800; // applies to swipe_left and swipe_right together
        uint32_t ratePalmHoldMs = 1500;
        uint32_t rateFistMs = 1500;
        uint32_t ratePinchStartMs = 300;
        uint32_t ratePinchChangeMs = 50; // 20 Hz at most, the contract ceiling

        // Smallest gap between events of this type. pinch_end is never limited, so every pinch_start gets its end.
        uint32_t minIntervalMs(GestureType t) const;
    };

    // Applies the keys found in the JSON text on top of the defaults already in `cfg`. Unknown sections or keys,
    // wrong types and values outside the allowed range are errors. On failure `cfg` is left unchanged.
    bool parseGestureConfig(const std::string& jsonText, GestureConfig& cfg, std::string& error);

    // Checks the relations between fields (pinchMin < pinchOn < pinchOff, and so on).
    bool validateGestureConfig(const GestureConfig& cfg, std::string& error);

    // Every field as pretty-printed JSON, the format of the shipped gesture.json.
    std::string gestureConfigToJson(const GestureConfig& cfg);
}
