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
#include "kick_gesture/config.h"

#include <gtest/gtest.h>

using namespace kickgest;

TEST(GestureConfig, DefaultsRoundTripThroughJson)
{
    const GestureConfig defaults;
    GestureConfig parsed;
    parsed.palmHoldMs = 1; // anything that differs, to prove the parse overwrote it
    std::string err;
    ASSERT_TRUE(parseGestureConfig(gestureConfigToJson(defaults), parsed, err)) << err;
    EXPECT_EQ(gestureConfigToJson(parsed), gestureConfigToJson(defaults));
    EXPECT_EQ(parsed.palmHoldMs, defaults.palmHoldMs);
    EXPECT_FLOAT_EQ(parsed.pinchOn, defaults.pinchOn);
    EXPECT_EQ(parsed.flipX, defaults.flipX);
}

TEST(GestureConfig, PartialFileKeepsOtherDefaults)
{
    GestureConfig c;
    std::string err;
    ASSERT_TRUE(parseGestureConfig(R"({"swipe": {"min_travel": 3.5}, "tracking": {"flip_x": false}})", c, err)) << err;
    EXPECT_FLOAT_EQ(c.swipeMinTravel, 3.5f);
    EXPECT_FALSE(c.flipX);
    EXPECT_EQ(c.palmHoldMs, GestureConfig().palmHoldMs);
}

TEST(GestureConfig, RejectsBadInputAndLeavesConfigUntouched)
{
    struct Case
    {
        const char* json;
        const char* needle;
    };
    const Case cases[] = {
        {R"([])", "object"},
        {R"({"nope": {}})", "unknown section"},
        {R"({"swipe": {"nope": 1}})", "unknown key"},
        {R"({"swipe": 3})", "must be an object"},
        {R"({"swipe": {"min_travel": "far"}})", "must be a number"},
        {R"({"swipe": {"min_travel": 0.01}})", "between"},
        {R"({"swipe": {"window_ms": 500.5}})", "whole number"},
        {R"({"tracking": {"flip_x": 1}})", "true or false"},
        {R"({"version": 2})", "version"},
        {R"({"pinch": {"on": 0.7}})", "pinch.on must be below pinch.off"},
        {R"({"pinch": {"min": 0.5}})", "pinch.min must be below pinch.on"},
        {R"({"rate_limit_ms": {"pinch_change": 10}})", "between"},
        {R"({"swipe": )", "offset"},
    };
    for (const Case& c : cases)
    {
        GestureConfig cfg;
        cfg.swipeMinTravel = 7.f;
        std::string err;
        EXPECT_FALSE(parseGestureConfig(c.json, cfg, err)) << c.json;
        EXPECT_NE(err.find(c.needle), std::string::npos) << c.json << " -> " << err;
        EXPECT_FLOAT_EQ(cfg.swipeMinTravel, 7.f) << "config changed by a failed parse: " << c.json;
    }
}

TEST(GestureConfig, PinchChangeCeilingIsTwentyHertz)
{
    EXPECT_GE(GestureConfig().minIntervalMs(GestureType::PinchChange), 50u);
    EXPECT_EQ(GestureConfig().minIntervalMs(GestureType::PinchEnd), 0u);
}

TEST(GestureConfig, ShippedFileMatchesDefaults)
{
#ifdef KICK_GESTURE_SHIPPED_JSON
    GestureConfig c;
    c.palmHoldMs = 1;
    FILE* f = std::fopen(KICK_GESTURE_SHIPPED_JSON, "rb");
    ASSERT_NE(f, nullptr) << KICK_GESTURE_SHIPPED_JSON;
    std::string text;
    char buf[1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    std::fclose(f);
    EXPECT_EQ(text, gestureConfigToJson(GestureConfig())) << "regenerate with: kick-gesture --print-config > systemd/gesture.json";
#else
    GTEST_SKIP() << "shipped gesture.json path not defined";
#endif
}
