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
#include "kick_gesture/json.h"

#include <gtest/gtest.h>

using kickgest::JsonValue;
using kickgest::parseJson;

TEST(Json, ParsesNestedDocument)
{
    JsonValue v;
    std::string err;
    ASSERT_TRUE(parseJson(R"({"a": [1, 2.5, -3e2], "b": {"c": "x\ny", "d": true, "e": null}})", v, err)) << err;
    ASSERT_TRUE(v.isObject());
    const JsonValue* a = v.find("a");
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->array.size(), 3u);
    EXPECT_DOUBLE_EQ(a->array[1].number, 2.5);
    EXPECT_DOUBLE_EQ(a->array[2].number, -300.0);
    const JsonValue* b = v.find("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->find("c")->string, "x\ny");
    EXPECT_TRUE(b->find("d")->boolean);
    EXPECT_EQ(b->find("e")->type, JsonValue::Type::Null);
    EXPECT_EQ(v.find("missing"), nullptr);
}

TEST(Json, DecodesUnicodeEscapes)
{
    JsonValue v;
    std::string err;
    ASSERT_TRUE(parseJson(R"("A\u00e9\u20ac")", v, err)) << err;
    EXPECT_EQ(v.string, "A\xC3\xA9\xE2\x82\xAC");
}

TEST(Json, RejectsMalformedInput)
{
    const char* bad[] = {"",       "{",        "[1,]",   "{\"a\":}", "{'a':1}", "01",     "1.",        "1e",
                         "tru",    "\"abc",    "[1 2]",  "{\"a\" 1}", "\"\\x\"", "nul",    "{} {}",     "\"\x01\"",
                         "[1,2",   "{\"a\":1,", "-",     "+1",       "\"\\u12\"", "[}",    "{\"a\":1 \"b\":2}"};
    for (const char* text : bad)
    {
        JsonValue v;
        std::string err;
        EXPECT_FALSE(parseJson(text, v, err)) << "accepted: " << text;
        EXPECT_FALSE(err.empty()) << text;
    }
}

TEST(Json, RejectsExcessiveNesting)
{
    std::string deep(200, '[');
    deep += std::string(200, ']');
    JsonValue v;
    std::string err;
    EXPECT_FALSE(parseJson(deep, v, err));
    EXPECT_NE(err.find("deep"), std::string::npos);

    std::string ok(JsonValue::kMaxDepth, '[');
    ok += std::string(JsonValue::kMaxDepth, ']');
    EXPECT_TRUE(parseJson(ok, v, err)) << err;
}

TEST(Json, RejectsNumbersOutOfRange)
{
    JsonValue v;
    std::string err;
    EXPECT_FALSE(parseJson("1e999", v, err));
}

TEST(Json, EscapeRoundTrips)
{
    const std::string raw = std::string("quote\" back\\ nl\n tab\t ctl\x01 end");
    JsonValue v;
    std::string err;
    ASSERT_TRUE(parseJson("\"" + kickgest::jsonEscape(raw) + "\"", v, err)) << err;
    EXPECT_EQ(v.string, raw);
}

TEST(Json, GarbageNeverCrashes)
{
    // Deterministic byte soup, plus truncations of a valid document.
    uint32_t s = 12345;
    for (int n = 0; n < 2000; ++n)
    {
        std::string text;
        const int len = 1 + static_cast<int>(s % 40);
        for (int i = 0; i < len; ++i)
        {
            s = s * 1664525u + 1013904223u;
            static const char alphabet[] = "{}[]\",:0123456789.eE+-truefalsn \\u";
            text += alphabet[(s >> 16) % (sizeof(alphabet) - 1)];
        }
        JsonValue v;
        std::string err;
        (void)parseJson(text, v, err);
    }
    const std::string doc = R"({"a": [1, 2, {"b": "c"}], "d": 1.5e3})";
    for (size_t i = 0; i < doc.size(); ++i)
    {
        JsonValue v;
        std::string err;
        (void)parseJson(doc.substr(0, i), v, err);
    }
}
