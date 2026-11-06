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

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace kicktest;

namespace
{
    bool sameEvent(const GestureEvent& a, const GestureEvent& b)
    {
        return a.type == b.type && a.hasValue == b.hasValue && a.value == b.value && a.hand == b.hand && a.confidence == b.confidence && a.tMs == b.tMs;
    }
}

TEST(Engine, PalmHoldFiresOnceAfterTheHoldTime)
{
    Runner r;
    const uint64_t start = r.t;
    r.hold(synthOpenPalm(0.5f, 0.5f), 4000);
    ASSERT_EQ(r.events.size(), 1u);
    const GestureEvent& e = r.events[0];
    EXPECT_EQ(e.type, GestureType::PalmHold);
    EXPECT_FALSE(e.hasValue);
    EXPECT_GE(e.tMs - start, r.cfg.palmHoldMs);
    EXPECT_LT(e.tMs - start, r.cfg.palmHoldMs + 2 * r.step);
    EXPECT_NEAR(e.confidence, 0.95f, 1e-4);
}

TEST(Engine, PalmHoldNeedsTheHandToLeaveBeforeItFiresAgain)
{
    Runner r;
    r.hold(synthOpenPalm(0.5f, 0.5f), 1000);
    ASSERT_EQ(r.count(GestureType::PalmHold), 1);
    // Closing the hand for longer than exit_ms re-arms it; the rate limit (1500 ms) still applies.
    r.hold(synthFist(0.5f, 0.5f), 300);
    r.hold(synthOpenPalm(0.5f, 0.5f), 400);
    EXPECT_EQ(r.count(GestureType::PalmHold), 1) << "rate limit";
    r.hold(synthOpenPalm(0.5f, 0.5f), 1500);
    EXPECT_EQ(r.count(GestureType::PalmHold), 2);
}

TEST(Engine, PalmHoldSurvivesAShortDropout)
{
    Runner r;
    r.hold(synthOpenPalm(0.5f, 0.5f), 300);
    r.hold(synthFist(0.5f, 0.5f), 70); // one frame, below exit_ms
    r.hold(synthOpenPalm(0.5f, 0.5f), 500);
    EXPECT_EQ(r.count(GestureType::PalmHold), 1);
}

TEST(Engine, MovingOpenPalmIsNotAHold)
{
    Runner r;
    // About 3 palm sizes per second for four seconds, back and forth slowly enough to avoid a swipe.
    for (int i = 0; i < 4; ++i)
    {
        r.sweep(synthOpenPalm(0.3f, 0.5f), 0.3f, 0.5f, 0.4f, 0.5f, 1000);
        r.sweep(synthOpenPalm(0.4f, 0.5f), 0.4f, 0.5f, 0.3f, 0.5f, 1000);
    }
    EXPECT_EQ(r.count(GestureType::PalmHold), 0);
}

TEST(Engine, FistFiresOnceAndCarriesTheHand)
{
    Runner r;
    r.hold(synthFist(0.5f, 0.5f), 2000);
    ASSERT_EQ(r.events.size(), 1u);
    EXPECT_EQ(r.events[0].type, GestureType::Fist);
    // rightProb 0.8 on an unmirrored image is the user's left hand with the default swap.
    EXPECT_EQ(r.events[0].hand, Handedness::Left);
}

TEST(Engine, HandednessFollowsTheSwapSetting)
{
    GestureConfig c;
    c.swapHandedness = false;
    Runner r(c);
    r.hold(synthFist(0.5f, 0.5f), 1000);
    ASSERT_EQ(r.events.size(), 1u);
    EXPECT_EQ(r.events[0].hand, Handedness::Right);
}

TEST(Engine, SwipeRightAndLeft)
{
    Runner r;
    r.hold(synthOpenPalm(0.2f, 0.5f), 300);
    r.sweep(synthOpenPalm(0.2f, 0.5f), 0.2f, 0.5f, 0.8f, 0.5f, 350);
    ASSERT_EQ(r.count(GestureType::SwipeRight), 1);
    EXPECT_EQ(r.count(GestureType::SwipeLeft), 0);
    r.hold(synthOpenPalm(0.8f, 0.5f), 1200); // rest, and let the rate limit pass
    r.events.clear();
    r.sweep(synthOpenPalm(0.8f, 0.5f), 0.8f, 0.5f, 0.2f, 0.5f, 350);
    ASSERT_EQ(r.events.size(), 1u);
    EXPECT_EQ(r.events[0].type, GestureType::SwipeLeft);
}

TEST(Engine, SwipeDirectionFollowsFlipX)
{
    GestureConfig c;
    c.flipX = false;
    Runner r(c);
    r.hold(synthOpenPalm(0.2f, 0.5f), 300);
    r.sweep(synthOpenPalm(0.2f, 0.5f), 0.2f, 0.5f, 0.8f, 0.5f, 350);
    // The synth writes image coordinates the same way for both settings, so a rightward move in the user's
    // frame is still swipe_right: the setting only changes how the engine reads the image.
    EXPECT_EQ(r.count(GestureType::SwipeRight), 1);
}

TEST(Engine, SwipeWorksWithThreeFingersOut)
{
    Runner r;
    SynthPose p = synthOpenPalm(0.2f, 0.5f);
    p.curl[3] = 1.f; // pinky folded
    r.hold(p, 300);
    r.sweep(p, 0.2f, 0.5f, 0.8f, 0.5f, 350);
    EXPECT_EQ(r.count(GestureType::SwipeRight), 1);
}

TEST(Engine, ReturnStrokeOfASwipeDoesNotFireTheOtherWay)
{
    Runner r;
    r.hold(synthOpenPalm(0.2f, 0.5f), 300);
    r.sweep(synthOpenPalm(0.2f, 0.5f), 0.2f, 0.5f, 0.8f, 0.5f, 300);
    // Straight back with no pause.
    r.sweep(synthOpenPalm(0.8f, 0.5f), 0.8f, 0.5f, 0.2f, 0.5f, 400);
    r.hold(synthOpenPalm(0.2f, 0.5f), 200);
    EXPECT_EQ(r.count(GestureType::SwipeRight), 1);
    EXPECT_EQ(r.count(GestureType::SwipeLeft), 0);
}

TEST(Engine, SlowDriftIsNotASwipe)
{
    Runner r;
    r.sweep(synthOpenPalm(0.2f, 0.5f), 0.2f, 0.5f, 0.8f, 0.5f, 3000);
    EXPECT_EQ(r.count(GestureType::SwipeRight), 0);
}

TEST(Engine, ShortFlickIsNotASwipe)
{
    Runner r;
    r.sweep(synthOpenPalm(0.45f, 0.5f), 0.45f, 0.5f, 0.55f, 0.5f, 200); // 1 palm size
    EXPECT_EQ(r.count(GestureType::SwipeRight), 0);
}

TEST(Engine, TalkingHandsProduceNothing)
{
    Runner r;
    uint32_t seed = 7;
    auto rnd = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24) - 0.5f;
    };
    // Ten seconds of small, fast wiggles with a half-open hand: amplitude 0.6 palm sizes at 2 Hz plus noise.
    SynthPose p = synthOpenPalm(0.5f, 0.5f);
    for (uint32_t ms = 0; ms < 10000; ms += r.step)
    {
        const float ph = 6.2831853f * 2.f * static_cast<float>(ms) / 1000.f;
        p.x = 0.5f + 0.6f * 0.09f * std::sin(ph) + 0.004f * rnd();
        p.y = 0.5f + 0.05f * std::sin(ph * 0.5f) + 0.004f * rnd();
        p.curl[2] = 0.5f + 0.4f * std::sin(ph * 0.7f); // fingers moving about
        p.curl[3] = 0.4f + 0.4f * std::sin(ph * 1.3f);
        r.frame(p);
    }
    EXPECT_TRUE(r.events.empty()) << "first event: " << gestureName(r.events.front().type);
}

TEST(Engine, ReachingForAKnobIsNotAGesture)
{
    Runner r;
    // The hand comes up from the lower right toward a knob: mostly vertical, fingers closing as it goes.
    SynthPose p = synthOpenPalm(0.7f, 0.9f);
    const uint32_t n = 12;
    for (uint32_t i = 0; i <= n; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(n);
        p.x = 0.7f - 0.05f * u;
        p.y = 0.9f - 0.5f * u;
        for (float& c : p.curl) c = 0.6f * u;
        r.frame(p);
    }
    // And a diagonal reach that is mostly sideways but not clean enough.
    r.gap(300);
    for (uint32_t i = 0; i <= n; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(n);
        p.x = 0.2f + 0.5f * u;
        p.y = 0.3f + 0.4f * u;
        r.frame(synthOpenPalm(p.x, p.y));
    }
    EXPECT_TRUE(r.events.empty());
}

TEST(Engine, PinchLifecycle)
{
    Runner r;
    r.hold(synthOpenPalm(0.5f, 0.5f), 200);
    // Close: ratio 0.9 down to 0.2 over about 270 ms, then squeeze and relax, then open.
    for (int i = 0; i < 5; ++i) r.frame(synthPinch(0.5f, 0.5f, 0.9f - 0.175f * static_cast<float>(i)));
    r.hold(synthPinch(0.5f, 0.5f, 0.2f), 400);
    for (int i = 0; i <= 12; ++i) r.frame(synthPinch(0.5f, 0.5f, 0.2f + 0.3f * static_cast<float>(i) / 12.f));
    for (int i = 0; i <= 12; ++i) r.frame(synthPinch(0.5f, 0.5f, 0.5f - 0.3f * static_cast<float>(i) / 12.f));
    r.hold(synthPinch(0.5f, 0.5f, 0.9f), 400);

    ASSERT_GE(r.events.size(), 4u);
    EXPECT_EQ(r.events.front().type, GestureType::PinchStart);
    EXPECT_NEAR(r.events.front().value, (0.2f - 0.12f) / 0.48f, 0.05f);
    EXPECT_EQ(r.events.back().type, GestureType::PinchEnd);
    EXPECT_EQ(r.count(GestureType::PinchStart), 1);
    EXPECT_EQ(r.count(GestureType::PinchEnd), 1);
    EXPECT_GE(r.count(GestureType::PinchChange), 2);

    float last = r.events.front().value;
    for (const GestureEvent& e : r.events)
    {
        if (e.type == GestureType::PinchChange)
        {
            EXPECT_GE(e.value, 0.f);
            EXPECT_LE(e.value, 1.f);
            EXPECT_GE(std::fabs(e.value - last), r.cfg.pinchDeadband - 1e-6f);
            last = e.value;
        }
    }
    EXPECT_FLOAT_EQ(r.events.back().value, last) << "pinch_end repeats the last value";
    EXPECT_EQ(r.count(GestureType::PalmHold), 0);
    EXPECT_EQ(r.count(GestureType::SwipeLeft) + r.count(GestureType::SwipeRight), 0);
}

TEST(Engine, PinchHysteresis)
{
    // Between pinch.on (0.30) and pinch.off (0.60) nothing starts and nothing ends.
    Runner idle;
    for (int i = 0; i < 60; ++i) idle.frame(synthPinch(0.5f, 0.5f, (i % 2) ? 0.35f : 0.55f));
    EXPECT_EQ(idle.count(GestureType::PinchStart), 0);

    Runner active;
    active.hold(synthPinch(0.5f, 0.5f, 0.2f), 400);
    ASSERT_EQ(active.count(GestureType::PinchStart), 1);
    for (int i = 0; i < 60; ++i) active.frame(synthPinch(0.5f, 0.5f, (i % 2) ? 0.35f : 0.55f));
    EXPECT_EQ(active.count(GestureType::PinchEnd), 0);
}

TEST(Engine, PinchChangeStaysUnderTwentyHertz)
{
    GestureConfig c;
    Runner r(c, 20); // 50 fps input
    r.hold(synthPinch(0.5f, 0.5f, 0.2f), 400);
    for (int i = 0; i < 200; ++i) r.frame(synthPinch(0.5f, 0.5f, 0.2f + 0.3f * (0.5f + 0.5f * std::sin(static_cast<float>(i) * 0.4f))));
    uint64_t prev = 0;
    int changes = 0;
    for (const GestureEvent& e : r.events)
    {
        if (e.type != GestureType::PinchChange) continue;
        if (prev != 0) EXPECT_GE(e.tMs - prev, 50u);
        prev = e.tMs;
        ++changes;
    }
    EXPECT_GT(changes, 10);
}

TEST(Engine, PinchEndsWhenTheHandIsLost)
{
    Runner r;
    r.hold(synthPinch(0.5f, 0.5f, 0.2f), 400);
    ASSERT_TRUE(r.engine.pinchActive());
    r.gap(150); // shorter than lost_ms
    EXPECT_EQ(r.count(GestureType::PinchEnd), 0);
    r.hold(synthPinch(0.5f, 0.5f, 0.2f), 200);
    r.gap(500);
    EXPECT_EQ(r.count(GestureType::PinchEnd), 1);
    EXPECT_FALSE(r.engine.pinchActive());
}

TEST(Engine, EndSessionClosesAnOpenPinch)
{
    Runner r;
    r.hold(synthPinch(0.5f, 0.5f, 0.2f), 400);
    ASSERT_EQ(r.count(GestureType::PinchStart), 1);
    r.engine.endSession(r.t, r.events);
    ASSERT_EQ(r.count(GestureType::PinchEnd), 1);
    EXPECT_FALSE(r.engine.pinchActive());
    const size_t n = r.events.size();
    r.engine.endSession(r.t + 10, r.events);
    EXPECT_EQ(r.events.size(), n) << "a second endSession has nothing to close";
}

TEST(Engine, ClockGoingBackwardsClosesAnOpenPinch)
{
    Runner r;
    r.hold(synthPinch(0.5f, 0.5f, 0.2f), 400);
    ASSERT_TRUE(r.engine.pinchActive());
    r.t = 10;
    r.frame(synthOpenPalm(0.5f, 0.5f));
    EXPECT_EQ(r.count(GestureType::PinchEnd), 1);
    EXPECT_EQ(r.engine.stats().outOfOrderFrames, 1u);
}

TEST(Engine, PinchSuppressesSwipeAndHold)
{
    Runner r;
    r.hold(synthPinch(0.2f, 0.5f, 0.2f), 400);
    r.sweep(synthPinch(0.2f, 0.5f, 0.2f), 0.2f, 0.5f, 0.8f, 0.5f, 350);
    r.hold(synthPinch(0.8f, 0.5f, 0.2f), 2000);
    EXPECT_EQ(r.count(GestureType::SwipeRight), 0);
    EXPECT_EQ(r.count(GestureType::PalmHold), 0);
    EXPECT_EQ(r.count(GestureType::Fist), 0);
    EXPECT_EQ(r.count(GestureType::PinchStart), 1);
}

TEST(Engine, FistIsNotAPinch)
{
    // The thumb tip sits next to the curled index tip in a fist. The index tip is close to the wrist, so it is no pinch.
    Runner r;
    r.hold(synthFist(0.5f, 0.5f), 1000);
    EXPECT_EQ(r.count(GestureType::PinchStart), 0);
    EXPECT_EQ(r.count(GestureType::Fist), 1);
}

TEST(Engine, LowScoreHandsAreIgnored)
{
    Runner r;
    SynthPose p = synthFist(0.5f, 0.5f);
    p.score = 0.7f; // below min_hand_score
    r.hold(p, 3000);
    EXPECT_TRUE(r.events.empty());
    EXPECT_GT(r.engine.stats().framesWithoutHand, 10u);
}

TEST(Engine, LowConfidenceEventsAreHeldBack)
{
    GestureConfig c;
    c.minEventConfidence = 0.99f;
    Runner r(c);
    r.hold(synthFist(0.5f, 0.5f), 2000); // score 0.95
    EXPECT_TRUE(r.events.empty());
    EXPECT_GT(r.engine.stats().confidenceSuppressed, 0u);
}

TEST(Engine, NonFiniteLandmarksAreSurvivable)
{
    Runner r;
    LandmarkFrame f = synthFrame(r.t, synthFist(0.5f, 0.5f), r.aspect, true);
    f.hands[0].lm[8].x = std::numeric_limits<float>::quiet_NaN();
    for (int i = 0; i < 40; ++i)
    {
        f.tMs = r.t;
        r.engine.process(f, r.events);
        r.t += r.step;
    }
    f.hands[0].lm[8].x = 0.5f;
    f.hands[0].lm[0].y = std::numeric_limits<float>::infinity();
    for (int i = 0; i < 40; ++i)
    {
        f.tMs = r.t;
        r.engine.process(f, r.events);
        r.t += r.step;
    }
    f.aspect = 0.f;
    f.tMs = r.t;
    r.engine.process(f, r.events);
    EXPECT_TRUE(r.events.empty());
    EXPECT_GT(r.engine.stats().nonFiniteFrames, 40u);
}

namespace
{
    std::vector<GestureEvent> randomSession(uint32_t seed)
    {
        Runner r;
        uint32_t s = seed;
        auto rnd = [&s]() {
            s = s * 1664525u + 1013904223u;
            return static_cast<float>(s >> 8) / static_cast<float>(1u << 24);
        };
        SynthPose p = synthOpenPalm(0.5f, 0.5f);
        for (int i = 0; i < 6000; ++i) // about 400 s at 15 fps
        {
            if (rnd() < 0.04f)
            {
                const float k = rnd();
                p = k < 0.25f ? synthOpenPalm(rnd(), 0.2f + 0.6f * rnd()) : k < 0.5f ? synthFist(rnd(), 0.2f + 0.6f * rnd()) : synthPinch(rnd(), 0.2f + 0.6f * rnd(), 0.1f + 0.7f * rnd());
                p.score = 0.75f + 0.25f * rnd();
            }
            else
            {
                p.x += 0.08f * (rnd() - 0.5f);
                p.y += 0.04f * (rnd() - 0.5f);
                if (p.pinch >= 0.f) p.pinch = std::fmax(0.05f, std::fmin(1.2f, p.pinch + 0.2f * (rnd() - 0.5f)));
            }
            if (rnd() < 0.03f)
                r.gap(static_cast<uint32_t>(rnd() * 600));
            else
                r.frame(p);
        }
        r.engine.endSession(r.t, r.events);
        return r.events;
    }
}

TEST(Engine, ContractInvariantsHoldOnRandomStreams)
{
    const GestureConfig cfg;
    for (uint32_t seed = 1; seed <= 5; ++seed)
    {
        const std::vector<GestureEvent> ev = randomSession(seed);
        bool pinching = false;
        uint64_t lastT = 0;
        uint64_t lastOfType[kGestureTypeCount] = {};
        bool seen[kGestureTypeCount] = {};
        uint64_t lastSwipe = 0;
        bool seenSwipe = false;
        for (const GestureEvent& e : ev)
        {
            EXPECT_GE(e.tMs, lastT);
            lastT = e.tMs;
            EXPECT_GE(e.confidence, cfg.minEventConfidence - 1e-6f) << gestureName(e.type);
            EXPECT_LE(e.confidence, 1.f);
            const bool pinchType = e.type == GestureType::PinchStart || e.type == GestureType::PinchChange || e.type == GestureType::PinchEnd;
            EXPECT_EQ(e.hasValue, pinchType);
            if (pinchType)
            {
                EXPECT_GE(e.value, 0.f);
                EXPECT_LE(e.value, 1.f);
            }
            if (e.type == GestureType::PinchStart)
            {
                EXPECT_FALSE(pinching) << "pinch_start while already pinching, seed " << seed;
                pinching = true;
            }
            else if (e.type == GestureType::PinchChange)
                EXPECT_TRUE(pinching) << "pinch_change outside a pinch, seed " << seed;
            else if (e.type == GestureType::PinchEnd)
            {
                EXPECT_TRUE(pinching) << "pinch_end without a start, seed " << seed;
                pinching = false;
            }
            const int i = static_cast<int>(e.type);
            if (e.type == GestureType::SwipeLeft || e.type == GestureType::SwipeRight)
            {
                if (seenSwipe) EXPECT_GE(e.tMs - lastSwipe, cfg.rateSwipeMs);
                lastSwipe = e.tMs;
                seenSwipe = true;
            }
            else if (e.type != GestureType::PinchEnd)
            {
                if (seen[i]) EXPECT_GE(e.tMs - lastOfType[i], cfg.minIntervalMs(e.type)) << gestureName(e.type);
                seen[i] = true;
                lastOfType[i] = e.tMs;
            }
        }
        EXPECT_FALSE(pinching) << "a pinch was left open, seed " << seed;
        EXPECT_FALSE(ev.empty()) << "the random stream should exercise something, seed " << seed;
    }
}

TEST(Engine, SameStreamSameEvents)
{
    const std::vector<GestureEvent> a = randomSession(42);
    const std::vector<GestureEvent> b = randomSession(42);
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) EXPECT_TRUE(sameEvent(a[i], b[i])) << i;
}
