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
// Tests that run the real MediaPipe models. They skip themselves when the models or the test photos were not fetched.
#include "model_test_util.h"
#include "stream.h"

#include "kick_gesture/features.h"
#include "kick_gesture/json.h"
#include "kick_gesture/onnx_hand_model.h"

#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <sstream>

using namespace kickgest;

namespace
{
    const char* const kPhotos[] = {"victory", "fist", "pointing_up", "thumb_up"};

    std::string modelPath(const char* file) { return std::string(KICK_GESTURE_TEST_MODEL_DIR) + "/" + file; }

    std::unique_ptr<OnnxHandModel> makeModel(bool int8, std::string& skip)
    {
        OnnxModelOptions o;
        o.palmModelPath = modelPath(int8 ? "palm_detection_mediapipe_2023feb_int8.onnx" : "palm_detection_mediapipe_2023feb.onnx");
        o.landmarkModelPath = modelPath(int8 ? "handpose_estimation_mediapipe_2023feb_int8.onnx" : "handpose_estimation_mediapipe_2023feb.onnx");
        std::ifstream a(o.palmModelPath), b(o.landmarkModelPath);
        if (!a || !b)
        {
            skip = "model files not found in " KICK_GESTURE_TEST_MODEL_DIR;
            return nullptr;
        }
        std::string err;
        auto m = OnnxHandModel::create(o, err);
        EXPECT_NE(m, nullptr) << err;
        return m;
    }

    bool loadPhoto(const char* name, RgbImage& img, std::string& skip)
    {
        if (!kicktest::loadPpm(std::string(KICK_GESTURE_TEST_IMG_DIR) + "/" + name + ".ppm", img))
        {
            skip = std::string("test photo ") + name + ".ppm not found (run tools/prepare_test_images.py)";
            return false;
        }
        return true;
    }

    struct Reference
    {
        bool found = false;
        float score = 0.f;
        std::vector<std::pair<float, float>> lm;
    };

    Reference loadReference(const char* name)
    {
        Reference r;
        std::ifstream in(KICK_GESTURE_REFERENCE_JSON);
        std::stringstream ss;
        ss << in.rdbuf();
        JsonValue root;
        std::string err;
        EXPECT_TRUE(parseJson(ss.str(), root, err)) << err;
        const JsonValue* e = root.find("images") ? root.find("images")->find(name) : nullptr;
        EXPECT_NE(e, nullptr) << name;
        if (e == nullptr) return r;
        r.found = e->find("found")->boolean;
        if (!r.found) return r;
        r.score = static_cast<float>(e->find("score")->number);
        for (const JsonValue& p : e->find("landmarks")->array) r.lm.emplace_back(static_cast<float>(p.array[0].number), static_cast<float>(p.array[1].number));
        return r;
    }

    // Mean landmark distance as a fraction of the reference palm size, both in pixels of the frame.
    double meanError(const Hand& h, const Reference& ref, int w, int h_px, float offX = 0.f, float offY = 0.f, int canvasW = 0, int canvasH = 0)
    {
        const double cw = canvasW > 0 ? canvasW : w;
        const double ch = canvasH > 0 ? canvasH : h_px;
        const double palm = std::hypot((ref.lm[0].first - ref.lm[9].first) * w, (ref.lm[0].second - ref.lm[9].second) * h_px);
        double sum = 0;
        for (int i = 0; i < kNumLandmarks; ++i)
        {
            const double ex = ref.lm[static_cast<size_t>(i)].first * w + offX;
            const double ey = ref.lm[static_cast<size_t>(i)].second * h_px + offY;
            const double gx = h.lm[static_cast<size_t>(i)].x * cw;
            const double gy = h.lm[static_cast<size_t>(i)].y * ch;
            sum += std::hypot(gx - ex, gy - ey);
        }
        return sum / kNumLandmarks / palm;
    }
}

TEST(OnnxModel, MatchesTheOpenCvZooPipelineOnRealPhotos)
{
    std::string skip;
    auto model = makeModel(false, skip);
    if (!model) GTEST_SKIP() << skip;
    for (const char* name : kPhotos)
    {
        RgbImage img;
        if (!loadPhoto(name, img, skip)) GTEST_SKIP() << skip;
        const Reference ref = loadReference(name);
        ASSERT_TRUE(ref.found) << name;
        std::vector<Hand> hands;
        std::string err;
        ASSERT_TRUE(model->detect(img, hands, err)) << err;
        ASSERT_EQ(hands.size(), 1u) << name;
        EXPECT_GT(hands[0].score, 0.9f) << name;
        EXPECT_LT(meanError(hands[0], ref, img.w, img.h), 0.05) << name << ": mean landmark error over the reference palm size";
        EXPECT_GT(model->lastTimes().palmMs, 0.0);
        EXPECT_GT(model->lastTimes().landmarkMs, 0.0);
    }
}

TEST(OnnxModel, Int8ModelsAgreeWithinALooserBound)
{
    // Measured against the float32 reference. The OpenCV Zoo's own Python pipeline running the same int8 files lands
    // 0.07 to 0.15 palm sizes from its float32 result on these photos, so 0.12 here is quantization noise, not the port.
    std::string skip;
    auto model = makeModel(true, skip);
    if (!model) GTEST_SKIP() << skip;
    for (const char* name : kPhotos)
    {
        RgbImage img;
        if (!loadPhoto(name, img, skip)) GTEST_SKIP() << skip;
        const Reference ref = loadReference(name);
        std::vector<Hand> hands;
        std::string err;
        ASSERT_TRUE(model->detect(img, hands, err)) << err;
        ASSERT_EQ(hands.size(), 1u) << name;
        EXPECT_LT(meanError(hands[0], ref, img.w, img.h), 0.12) << name;
    }
}

TEST(OnnxModel, LandmarksFollowThePhotoWhenItSitsInALargerFrame)
{
    // The photo pasted 1:1 into a 4:3 canvas: the letterbox and the aspect handling must map back to canvas coordinates.
    std::string skip;
    auto model = makeModel(true, skip);
    if (!model) GTEST_SKIP() << skip;
    RgbImage photo;
    if (!loadPhoto("victory", photo, skip)) GTEST_SKIP() << skip;
    const Reference ref = loadReference("victory");
    RgbImage canvas;
    canvas.resize(640, 480);
    for (size_t i = 0; i < canvas.px.size(); i += 3)
    {
        canvas.px[i] = 128;
        canvas.px[i + 1] = 128;
        canvas.px[i + 2] = 118;
    }
    const int ox = 230, oy = 60;
    for (int y = 0; y < photo.h; ++y)
        for (int x = 0; x < photo.w; ++x)
            for (int c = 0; c < 3; ++c)
                canvas.px[(static_cast<size_t>(y + oy) * 640 + static_cast<size_t>(x + ox)) * 3 + static_cast<size_t>(c)] = photo.px[(static_cast<size_t>(y) * static_cast<size_t>(photo.w) + static_cast<size_t>(x)) * 3 + static_cast<size_t>(c)];
    std::vector<Hand> hands;
    std::string err;
    ASSERT_TRUE(model->detect(canvas, hands, err)) << err;
    ASSERT_EQ(hands.size(), 1u);
    EXPECT_LT(meanError(hands[0], ref, photo.w, photo.h, static_cast<float>(ox), static_cast<float>(oy), 640, 480), 0.10);
}

TEST(OnnxModel, FeaturesReadTheRealPoses)
{
    std::string skip;
    auto model = makeModel(true, skip);
    if (!model) GTEST_SKIP() << skip;
    struct Expect
    {
        const char* name;
        bool ext[4];
        bool thumb;
    };
    // victory: index and middle up. fist: all curled. pointing_up: index only. thumb_up: thumb out, fingers curled.
    const Expect expect[] = {
        {"victory", {true, true, false, false}, false},
        {"fist", {false, false, false, false}, false},
        {"pointing_up", {true, false, false, false}, false},
        {"thumb_up", {false, false, false, false}, true},
    };
    GestureConfig cfg;
    cfg.flipX = false;
    for (const Expect& ex : expect)
    {
        RgbImage img;
        if (!loadPhoto(ex.name, img, skip)) GTEST_SKIP() << skip;
        std::vector<Hand> hands;
        std::string err;
        ASSERT_TRUE(model->detect(img, hands, err)) << err;
        ASSERT_EQ(hands.size(), 1u) << ex.name;
        HandFeatures f;
        ASSERT_TRUE(computeFeatures(hands[0], static_cast<float>(img.w) / static_cast<float>(img.h), cfg, f));
        std::printf("[features] %-12s ext=%d%d%d%d thumb=%d pinchRatio=%.2f indexReach=%.2f\n", ex.name, f.ext[0], f.ext[1], f.ext[2], f.ext[3], f.thumbExt, f.pinchRatio, f.indexReach);
        for (int i = 0; i < 4; ++i) EXPECT_EQ(f.ext[i], ex.ext[i]) << ex.name << " finger " << i;
        EXPECT_EQ(f.thumbExt, ex.thumb) << ex.name;
    }
}

TEST(OnnxModel, FistPhotoIsNotAPinch)
{
    std::string skip;
    auto model = makeModel(true, skip);
    if (!model) GTEST_SKIP() << skip;
    RgbImage img;
    if (!loadPhoto("fist", img, skip)) GTEST_SKIP() << skip;
    std::vector<Hand> hands;
    std::string err;
    ASSERT_TRUE(model->detect(img, hands, err)) << err;
    ASSERT_EQ(hands.size(), 1u);
    GestureConfig cfg;
    HandFeatures f;
    ASSERT_TRUE(computeFeatures(hands[0], static_cast<float>(img.w) / static_cast<float>(img.h), cfg, f));
    const bool pinchPose = f.pinchRatio <= cfg.pinchOn && f.indexReach >= cfg.pinchMinReach;
    EXPECT_FALSE(pinchPose) << "ratio " << f.pinchRatio << " reach " << f.indexReach;
}

namespace
{
    // The frames the issue names for the fuzz test, plus noise and patterns.
    std::vector<RgbImage> fuzzImages(int w, int h)
    {
        std::vector<RgbImage> v;
        auto solid = [&](uint8_t r, uint8_t g, uint8_t b) {
            RgbImage im;
            im.resize(w, h);
            for (size_t i = 0; i < im.px.size(); i += 3)
            {
                im.px[i] = r;
                im.px[i + 1] = g;
                im.px[i + 2] = b;
            }
            v.push_back(std::move(im));
        };
        solid(0, 0, 0);
        solid(255, 255, 255);
        solid(128, 128, 128);
        solid(255, 0, 0);
        uint32_t s = 99;
        for (int k = 0; k < 6; ++k)
        {
            RgbImage im;
            im.resize(w, h);
            for (uint8_t& b : im.px)
            {
                s = s * 1664525u + 1013904223u;
                b = static_cast<uint8_t>(s >> 24);
            }
            v.push_back(std::move(im));
        }
        RgbImage cb;
        cb.resize(w, h);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const uint8_t c = ((x / 8 + y / 8) & 1) ? 255 : 0;
                for (int k = 0; k < 3; ++k) cb.px[(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3 + static_cast<size_t>(k)] = c;
            }
        v.push_back(std::move(cb));
        return v;
    }
}

TEST(OnnxModel, FuzzFramesProduceNoHandsAndNoEvents)
{
    std::string skip;
    for (const bool int8 : {true, false})
    {
        auto model = makeModel(int8, skip);
        if (!model) GTEST_SKIP() << skip;
        for (const auto& size : {std::make_pair(320, 240), std::make_pair(640, 480), std::make_pair(160, 120)})
        {
            kicktest::Runner r;
            for (const RgbImage& img : fuzzImages(size.first, size.second))
            {
                std::vector<Hand> hands;
                std::string err;
                ASSERT_TRUE(model->detect(img, hands, err)) << err;
                LandmarkFrame f;
                f.tMs = r.t;
                f.aspect = static_cast<float>(size.first) / static_cast<float>(size.second);
                f.hands = hands;
                f.quantize();
                r.engine.process(f, r.events);
                r.t += r.step;
                // Held for a second each so a state machine would have had time to fire.
                for (int i = 0; i < 15; ++i)
                {
                    ASSERT_TRUE(model->detect(img, hands, err)) << err;
                    f.tMs = r.t;
                    f.hands = hands;
                    f.quantize();
                    r.engine.process(f, r.events);
                    r.t += r.step;
                }
                EXPECT_TRUE(hands.empty() || hands[0].score < r.cfg.minHandScore) << "hand found in a fuzz frame, score " << (hands.empty() ? 0.f : hands[0].score);
            }
            EXPECT_TRUE(r.events.empty()) << (int8 ? "int8 " : "fp32 ") << size.first << "x" << size.second << ": " << gestureName(r.events.front().type);
        }
    }
}

TEST(OnnxModel, RejectsATooSmallFrameWithoutCrashing)
{
    std::string skip;
    auto model = makeModel(true, skip);
    if (!model) GTEST_SKIP() << skip;
    RgbImage tiny;
    tiny.resize(4, 4);
    std::vector<Hand> hands;
    std::string err;
    EXPECT_FALSE(model->detect(tiny, hands, err));
    EXPECT_FALSE(err.empty());
    RgbImage empty;
    EXPECT_FALSE(model->detect(empty, hands, err));
}

TEST(OnnxModel, CreateReportsAMissingModelFile)
{
    OnnxModelOptions o;
    o.palmModelPath = "/nonexistent/palm.onnx";
    o.landmarkModelPath = "/nonexistent/lm.onnx";
    std::string err;
    EXPECT_EQ(OnnxHandModel::create(o, err), nullptr);
    EXPECT_FALSE(err.empty());
}

TEST(OnnxModel, CreateRejectsTheWrongNetwork)
{
    std::string skip;
    OnnxModelOptions o;
    // The landmark model in the palm slot: its outputs are not [1, 2016, 18].
    o.palmModelPath = modelPath("handpose_estimation_mediapipe_2023feb_int8.onnx");
    o.landmarkModelPath = modelPath("handpose_estimation_mediapipe_2023feb_int8.onnx");
    std::ifstream probe(o.palmModelPath);
    if (!probe) GTEST_SKIP() << "model files not found";
    std::string err;
    EXPECT_EQ(OnnxHandModel::create(o, err), nullptr);
    EXPECT_FALSE(err.empty());
}
