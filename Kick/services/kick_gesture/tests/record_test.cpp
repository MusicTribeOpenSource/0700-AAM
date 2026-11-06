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
#include "stream.h"

#include "kick_gesture/event_json.h"
#include "kick_gesture/json.h"
#include "kick_gesture/record.h"
#include "kick_gesture/replay.h"

#include <gtest/gtest.h>

#include <cmath>
#include <sstream>

using namespace kicktest;

namespace
{
    // A session with every gesture in it, as the frames a live run would produce (quantized).
    std::vector<LandmarkFrame> buildSession()
    {
        std::vector<LandmarkFrame> frames;
        Runner r;
        auto add = [&](const LandmarkFrame& f) { frames.push_back(f); };
        auto hold = [&](const SynthPose& p, uint32_t ms) {
            for (uint32_t e = 0; e < ms; e += r.step)
            {
                add(synthFrame(r.t, p, r.aspect, r.cfg.flipX));
                r.t += r.step;
            }
        };
        hold(synthOpenPalm(0.2f, 0.5f), 300);
        for (int i = 0; i <= 5; ++i)
        {
            add(synthFrame(r.t, synthOpenPalm(0.2f + 0.12f * static_cast<float>(i), 0.5f), r.aspect, r.cfg.flipX));
            r.t += r.step;
        }
        hold(synthOpenPalm(0.8f, 0.5f), 1000);
        hold(synthFist(0.5f, 0.5f), 700);
        hold(synthPinch(0.5f, 0.5f, 0.2f), 500);
        for (int i = 0; i < 10; ++i)
        {
            add(synthFrame(r.t, synthPinch(0.5f, 0.5f, 0.2f + 0.03f * static_cast<float>(i)), r.aspect, r.cfg.flipX));
            r.t += r.step;
        }
        hold(synthOpenPalm(0.8f, 0.5f), 800);
        for (int i = 0; i <= 5; ++i)
        {
            add(synthFrame(r.t, synthOpenPalm(0.8f - 0.12f * static_cast<float>(i), 0.5f), r.aspect, r.cfg.flipX));
            r.t += r.step;
        }
        hold(synthOpenPalm(0.2f, 0.5f), 300);
        add(synthEmptyFrame(r.t, r.aspect));
        for (LandmarkFrame& f : frames) f.quantize();
        return frames;
    }

    std::vector<std::string> eventLines(std::istream& in, const GestureConfig& cfg = GestureConfig())
    {
        GestureEngine engine(cfg);
        std::vector<std::string> lines;
        const ReplayResult r = replayStream(in, engine, [&](const GestureEvent& e) { lines.push_back(eventToJson(e)); });
        EXPECT_TRUE(r.ok) << r.error;
        return lines;
    }
}

TEST(Record, RoundTripKeepsEveryNumber)
{
    const std::vector<LandmarkFrame> frames = buildSession();
    std::stringstream ss;
    LandmarkWriter w(ss);
    w.writeHeader();
    for (const LandmarkFrame& f : frames) w.write(f);

    LandmarkReader rd(ss);
    LandmarkFrame got;
    size_t n = 0;
    while (rd.next(got))
    {
        ASSERT_LT(n, frames.size());
        const LandmarkFrame& want = frames[n];
        EXPECT_EQ(got.tMs, want.tMs);
        EXPECT_EQ(got.aspect, want.aspect);
        ASSERT_EQ(got.hands.size(), want.hands.size());
        for (size_t h = 0; h < got.hands.size(); ++h)
        {
            EXPECT_EQ(got.hands[h].score, want.hands[h].score);
            EXPECT_EQ(got.hands[h].rightProb, want.hands[h].rightProb);
            for (int i = 0; i < kNumLandmarks; ++i)
            {
                EXPECT_EQ(got.hands[h].lm[static_cast<size_t>(i)].x, want.hands[h].lm[static_cast<size_t>(i)].x) << n << ":" << i;
                EXPECT_EQ(got.hands[h].lm[static_cast<size_t>(i)].y, want.hands[h].lm[static_cast<size_t>(i)].y);
                EXPECT_EQ(got.hands[h].lm[static_cast<size_t>(i)].z, want.hands[h].lm[static_cast<size_t>(i)].z);
            }
        }
        ++n;
    }
    EXPECT_TRUE(rd.error().empty()) << rd.error();
    EXPECT_EQ(n, frames.size());
}

TEST(Record, ReplayEqualsTheLiveRun)
{
    // The live path quantizes and feeds the engine; the recorded copy goes through the file. Same events, byte for byte.
    const std::vector<LandmarkFrame> frames = buildSession();
    GestureEngine live{GestureConfig()};
    std::vector<GestureEvent> events;
    for (const LandmarkFrame& f : frames) live.process(f, events);
    live.endSession(frames.back().tMs, events);
    std::vector<std::string> liveLines;
    for (const GestureEvent& e : events) liveLines.push_back(eventToJson(e));

    std::stringstream ss;
    LandmarkWriter w(ss);
    w.writeHeader();
    for (const LandmarkFrame& f : frames) w.write(f);
    const std::vector<std::string> replayed = eventLines(ss);

    ASSERT_FALSE(liveLines.empty());
    EXPECT_EQ(replayed, liveLines);
    // Every gesture of the vocabulary appears in this session, so the equality above covers all seven.
    for (int t = 0; t < kGestureTypeCount; ++t)
    {
        bool seen = false;
        for (const std::string& l : liveLines) seen = seen || l.find(std::string("\"") + gestureName(static_cast<GestureType>(t)) + "\"") != std::string::npos;
        EXPECT_TRUE(seen) << gestureName(static_cast<GestureType>(t));
    }
}

TEST(Record, ReplayIsDeterministic)
{
    const std::vector<LandmarkFrame> frames = buildSession();
    std::stringstream a, b;
    for (std::stringstream* ss : {&a, &b})
    {
        LandmarkWriter w(*ss);
        w.writeHeader();
        for (const LandmarkFrame& f : frames) w.write(f);
    }
    EXPECT_EQ(eventLines(a), eventLines(b));
}

TEST(Record, ReplayClosesAnOpenPinchAtTheEnd)
{
    Runner r;
    std::stringstream ss;
    LandmarkWriter w(ss);
    w.writeHeader();
    for (int i = 0; i < 12; ++i)
    {
        LandmarkFrame f = synthFrame(r.t, synthPinch(0.5f, 0.5f, 0.2f), r.aspect, true);
        f.quantize();
        w.write(f);
        r.t += r.step;
    }
    const std::vector<std::string> lines = eventLines(ss);
    ASSERT_GE(lines.size(), 2u);
    EXPECT_NE(lines.front().find("pinch_start"), std::string::npos);
    EXPECT_NE(lines.back().find("pinch_end"), std::string::npos);
}

TEST(Record, ReaderRejectsMalformedLines)
{
    struct Case
    {
        const char* text;
        const char* needle;
    };
    const std::string badHeader = R"({"format":"something-else","version":1})";
    const Case cases[] = {
        {"not json", "line 1"},
        {"[1,2]", "not an object"},
        {R"({"aspect":1.3,"hands":[]})", "t_ms"},
        {R"({"t_ms":-5,"aspect":1.3,"hands":[]})", "t_ms"},
        {R"({"t_ms":1.5,"aspect":1.3,"hands":[]})", "t_ms"},
        {R"({"t_ms":1,"aspect":0,"hands":[]})", "aspect"},
        {R"({"t_ms":1,"aspect":1.3})", "hands"},
        {R"({"t_ms":1,"aspect":1.3,"hands":[{"score":0.9}]})", "score, right and lm"},
        {R"({"t_ms":1,"aspect":1.3,"hands":[{"score":0.9,"right":0.5,"lm":[1,2,3]}]})", "63"},
        {badHeader.c_str(), "format"},
        {R"({"format":"kick-gesture-landmarks","version":9})", "version"},
    };
    for (const Case& c : cases)
    {
        std::stringstream ss(c.text);
        LandmarkReader rd(ss);
        LandmarkFrame f;
        EXPECT_FALSE(rd.next(f)) << c.text;
        EXPECT_NE(rd.error().find(c.needle), std::string::npos) << c.text << " -> " << rd.error();
    }
    // A hand whose lm holds a string among 63 entries.
    std::string withString = "\"x\"";
    for (int i = 1; i < 63; ++i) withString += ",0.5";
    std::stringstream ss(std::string(R"({"t_ms":1,"aspect":1.3,"hands":[{"score":0.9,"right":0.5,"lm":[)") + withString + "]}]}");
    LandmarkReader rd(ss);
    LandmarkFrame f;
    EXPECT_FALSE(rd.next(f));
    EXPECT_NE(rd.error().find("numbers only"), std::string::npos) << rd.error();
}

TEST(Record, ReaderSkipsBlankLinesAndHeader)
{
    std::stringstream ss("\n{\"format\":\"kick-gesture-landmarks\",\"version\":1}\n\r\n{\"t_ms\":5,\"aspect\":1.5,\"hands\":[]}\n   \n");
    LandmarkReader rd(ss);
    LandmarkFrame f;
    ASSERT_TRUE(rd.next(f));
    EXPECT_EQ(f.tMs, 5u);
    EXPECT_TRUE(f.hands.empty());
    EXPECT_FALSE(rd.next(f));
    EXPECT_TRUE(rd.error().empty());
}

TEST(Record, ReplayStopsAndReportsOnAMalformedLine)
{
    std::stringstream ss("{\"t_ms\":5,\"aspect\":1.5,\"hands\":[]}\nbroken\n");
    GestureEngine engine{GestureConfig()};
    const ReplayResult r = replayStream(ss, engine, [](const GestureEvent&) {});
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("line 2"), std::string::npos);
    EXPECT_EQ(r.frames, 1u);
}

TEST(EventJson, FormatMatchesTheContract)
{
    GestureEvent e;
    e.type = GestureType::PinchChange;
    e.hasValue = true;
    e.value = 0.5125f;
    e.hand = Handedness::Left;
    e.confidence = 0.9344f;
    e.tMs = 123456;
    EXPECT_EQ(eventToJson(e), R"({"type":"pinch_change","value":0.512,"hand":"left","confidence":0.934,"t_ms":123456})");

    e.type = GestureType::SwipeRight;
    e.hasValue = false;
    e.hand = Handedness::Unknown;
    EXPECT_EQ(eventToJson(e), R"({"type":"swipe_right","hand":"unknown","confidence":0.934,"t_ms":123456})");
}

TEST(EventJson, ClampsOutOfRangeNumbers)
{
    GestureEvent e;
    e.type = GestureType::PinchStart;
    e.hasValue = true;
    e.value = 3.f;
    e.confidence = -1.f;
    EXPECT_EQ(eventToJson(e), R"({"type":"pinch_start","value":1.000,"hand":"unknown","confidence":0.000,"t_ms":0})");
    e.value = std::nanf("");
    EXPECT_NE(eventToJson(e).find("\"value\":0.000"), std::string::npos);
}

TEST(EventJson, StatusMessage)
{
    StatusMessage s;
    s.state = "degraded";
    s.reason = "cpu_soft";
    s.rateHz = 6.0;
    s.cpu = 0.644;
    s.tempC = 71.04;
    s.tMs = 99;
    EXPECT_EQ(statusToJson(s), R"({"type":"status","state":"degraded","reason":"cpu_soft","rate_hz":6.0,"cpu":0.64,"temp_c":71.0,"t_ms":99})");
    s.reason.clear();
    s.tempC.reset();
    EXPECT_EQ(statusToJson(s), R"({"type":"status","state":"degraded","rate_hz":6.0,"cpu":0.64,"t_ms":99})");
}

TEST(EventJson, EveryEventParsesAsJsonWithTheDocumentedFields)
{
    const std::vector<LandmarkFrame> frames = buildSession();
    GestureEngine engine{GestureConfig()};
    std::vector<GestureEvent> events;
    for (const LandmarkFrame& f : frames) engine.process(f, events);
    ASSERT_FALSE(events.empty());
    for (const GestureEvent& e : events)
    {
        JsonValue v;
        std::string err;
        ASSERT_TRUE(parseJson(eventToJson(e), v, err)) << err;
        GestureType t;
        ASSERT_TRUE(parseGestureName(v.find("type")->string.c_str(), t));
        EXPECT_EQ(v.find("value") != nullptr, e.hasValue);
        EXPECT_TRUE(v.find("hand")->isString());
        EXPECT_GE(v.find("confidence")->number, 0.0);
        EXPECT_LE(v.find("confidence")->number, 1.0);
        EXPECT_EQ(static_cast<uint64_t>(v.find("t_ms")->number), e.tMs);
    }
}

TEST(GestureNames, RoundTrip)
{
    for (int i = 0; i < kGestureTypeCount; ++i)
    {
        GestureType t;
        ASSERT_TRUE(parseGestureName(gestureName(static_cast<GestureType>(i)), t));
        EXPECT_EQ(static_cast<int>(t), i);
    }
    GestureType t;
    EXPECT_FALSE(parseGestureName("wave", t));
    EXPECT_FALSE(parseGestureName(nullptr, t));
}
