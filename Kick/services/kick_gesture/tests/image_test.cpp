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
#include "kick_gesture/image.h"
#include "kick_gesture/palm_anchors.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace kickgest;

TEST(PalmAnchors, CountAndLayout)
{
    const auto& a = palmAnchors();
    ASSERT_EQ(a.size(), 2016u);
    // Values checked against the OpenCV Zoo's anchor table (max difference 2e-8).
    EXPECT_NEAR(a[0].x, 0.0208333f, 1e-6);
    EXPECT_NEAR(a[0].y, 0.0208333f, 1e-6);
    EXPECT_FLOAT_EQ(a[0].x, a[1].x); // two anchors per cell in the first layer
    EXPECT_NEAR(a[2].x, 0.0625f, 1e-6);
    EXPECT_NEAR(a[48].y, 0.0625f, 1e-6); // next row of the 24 grid
    EXPECT_NEAR(a[1152].x, 0.0416667f, 1e-6); // first anchor of the 12 x 12 layer
    EXPECT_NEAR(a[2015].x, 0.958333f, 1e-6);
    EXPECT_NEAR(a[2015].y, 0.958333f, 1e-6);
}

TEST(Image, Yuyv)
{
    // Two pixels per 4 bytes: Y0 U Y1 V. Limited-range white, black and mid grey, then pure-ish colours.
    kickcam::FrameFormat f;
    f.width = 4;
    f.height = 1;
    f.stride = 8;
    f.fourcc = kickcam::kFourccYuyv;
    const uint8_t data[8] = {235, 128, 16, 128, 126, 128, 126, 128};
    RgbImage img;
    ASSERT_TRUE(convertToRgb(data, sizeof(data), f, img));
    ASSERT_EQ(img.w, 4);
    EXPECT_NEAR(img.px[0], 255, 1);
    EXPECT_NEAR(img.px[1], 255, 1);
    EXPECT_NEAR(img.px[2], 255, 1);
    EXPECT_NEAR(img.px[3], 0, 1);
    EXPECT_NEAR(img.px[6], 128, 2);

    // BT.601 red is Y 82, U 90, V 240.
    const uint8_t red[4] = {82, 90, 82, 240};
    kickcam::FrameFormat r = f;
    r.width = 2;
    r.stride = 4;
    ASSERT_TRUE(convertToRgb(red, sizeof(red), r, img));
    EXPECT_NEAR(img.px[0], 255, 3);
    EXPECT_NEAR(img.px[1], 0, 3);
    EXPECT_NEAR(img.px[2], 0, 3);
}

TEST(Image, Nv12MatchesYuyvForTheSameColour)
{
    kickcam::FrameFormat f;
    f.width = 4;
    f.height = 2;
    f.stride = 4;
    f.fourcc = kickcam::kFourccNv12;
    // 4x2 luma of Y=82, then one row of interleaved U V pairs for the two column pairs.
    std::vector<uint8_t> d(8, 82);
    d.insert(d.end(), {90, 240, 90, 240});
    RgbImage img;
    ASSERT_TRUE(convertToRgb(d.data(), d.size(), f, img));
    for (size_t i = 0; i < img.px.size(); i += 3)
    {
        EXPECT_NEAR(img.px[i], 255, 3);
        EXPECT_NEAR(img.px[i + 1], 0, 3);
        EXPECT_NEAR(img.px[i + 2], 0, 3);
    }
}

TEST(Image, RejectsShortOrUnsupportedFrames)
{
    kickcam::FrameFormat f;
    f.width = 4;
    f.height = 2;
    f.stride = 8;
    f.fourcc = kickcam::kFourccYuyv;
    const uint8_t data[8] = {};
    RgbImage img;
    EXPECT_FALSE(convertToRgb(data, sizeof(data), f, img)) << "needs 16 bytes";
    EXPECT_FALSE(convertToRgb(nullptr, 100, f, img));
    f.fourcc = kickcam::makeFourcc('M', 'J', 'P', 'G');
    std::vector<uint8_t> big(100);
    EXPECT_FALSE(convertToRgb(big.data(), big.size(), f, img));
}

TEST(Image, WarpIdentityAndOutsideIsBlack)
{
    RgbImage src;
    src.resize(4, 4);
    for (size_t i = 0; i < src.px.size(); ++i) src.px[i] = static_cast<uint8_t>(i * 5);
    std::vector<float> out(4 * 4 * 3);
    warpToFloat(src, Affine(), 4, 4, out.data());
    for (size_t i = 0; i < out.size(); ++i) EXPECT_NEAR(out[i], static_cast<float>(src.px[i]) / 255.f, 1e-5) << i;

    Affine shifted;
    shifted.c = 100.f; // far outside
    warpToFloat(src, shifted, 4, 4, out.data());
    for (float v : out) EXPECT_EQ(v, 0.f);

    Affine nan;
    nan.c = std::nanf("");
    warpToFloat(src, nan, 4, 4, out.data());
    for (float v : out) EXPECT_EQ(v, 0.f);
}

TEST(Image, WarpInterpolatesBetweenPixels)
{
    RgbImage src;
    src.resize(2, 1);
    src.px = {0, 0, 0, 200, 100, 50};
    Affine half;
    half.c = 0.5f; // halfway between the two pixels
    float out[3];
    warpToFloat(src, half, 1, 1, out);
    EXPECT_NEAR(out[0], 100.f / 255.f, 1e-5);
    EXPECT_NEAR(out[1], 50.f / 255.f, 1e-5);
    EXPECT_NEAR(out[2], 25.f / 255.f, 1e-5);
}

TEST(Image, FitIntoLetterboxes)
{
    RgbImage src;
    src.resize(10, 10);
    std::fill(src.px.begin(), src.px.end(), 200);
    const RgbImage out = fitInto(src, 40, 20); // a 20 x 20 picture centred, bars left and right
    ASSERT_EQ(out.w, 40);
    EXPECT_EQ(out.px[(10 * 40 + 20) * 3], 200);
    EXPECT_EQ(out.px[(10 * 40 + 2) * 3], 0);
    EXPECT_EQ(out.px[(10 * 40 + 38) * 3], 0);
}
