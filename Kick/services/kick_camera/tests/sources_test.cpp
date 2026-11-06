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
#include "kick_camera/sources.h"

#include "test_util.h"

#include <gtest/gtest.h>

#include <chrono>

using namespace kickcam;
using kicktest::TempDir;
using kicktest::writeFile;

namespace
{
    FrameFormat yuyv(uint32_t w, uint32_t h) { return {w, h, w * 2, kFourccYuyv}; }
    FrameFormat nv12(uint32_t w, uint32_t h) { return {w, h, w, kFourccNv12}; }

    std::vector<uint8_t> grab(FrameSource& s)
    {
        FrameView v;
        std::string error;
        const ReadResult r = s.next(v, 2000, error);
        EXPECT_EQ(r, ReadResult::Frame) << error;
        return r == ReadResult::Frame ? std::vector<uint8_t>(v.data, v.data + v.size) : std::vector<uint8_t>();
    }
}

TEST(PatternSource, FramesFollowTheDocumentedPatternAndTheSquareMoves)
{
    PatternSource s(yuyv(64, 48), 200);
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    const auto f0 = grab(s), f1 = grab(s);
    std::vector<uint8_t> expect0(64 * 48 * 2), expect1(64 * 48 * 2);
    PatternSource::render(yuyv(64, 48), 0, expect0.data());
    PatternSource::render(yuyv(64, 48), 1, expect1.data());
    EXPECT_EQ(f0, expect0);
    EXPECT_EQ(f1, expect1);
    EXPECT_NE(f0, f1);
    // Luma of pixel (0, 0) is the left end of the gradient, and the chroma bytes are neutral.
    EXPECT_EQ(f0[0], 0);
    EXPECT_EQ(f0[1], 128);
    EXPECT_EQ(f0[3], 128);
}

TEST(PatternSource, Nv12HasALumaPlaneThenNeutralChroma)
{
    PatternSource s(nv12(64, 48), 200);
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    const auto f = grab(s);
    ASSERT_EQ(f.size(), 64u * 48 * 3 / 2);
    for (size_t i = 64 * 48; i < f.size(); ++i) ASSERT_EQ(f[i], 128) << i;
}

TEST(PatternSource, KeepsTheRequestedFrameRate)
{
    PatternSource s(yuyv(32, 32), 20);
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 6; ++i) grab(s);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    EXPECT_GE(ms, 230) << "6 frames at 20 fps should span about 250 ms (first frame is immediate)";
    EXPECT_LT(ms, 600);
}

TEST(PatternSource, ReturnsTimeoutWhenTheNextFrameIsNotDueYet)
{
    PatternSource s(yuyv(32, 32), 1);
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    grab(s);
    FrameView v;
    EXPECT_EQ(s.next(v, 10, error), ReadResult::Timeout);
}

TEST(PatternSource, RefusesAnUnusableFormat)
{
    PatternSource s({33, 32, 66, kFourccYuyv}, 15);
    std::string error;
    EXPECT_FALSE(s.open(error));
    EXPECT_FALSE(error.empty());
}

TEST(FileSource, RawFramesLoopWhenAsked)
{
    TempDir dir;
    std::vector<uint8_t> bytes;
    for (uint8_t frame = 0; frame < 3; ++frame) bytes.insert(bytes.end(), 16, static_cast<uint8_t>(0x10 + frame));
    writeFile(dir.file("raw.yuv"), bytes);

    FileSource s(dir.file("raw.yuv"), yuyv(4, 2), 200, true);
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    std::vector<uint8_t> first;
    for (int i = 0; i < 5; ++i)
    {
        const auto f = grab(s);
        ASSERT_EQ(f.size(), 16u);
        EXPECT_EQ(f[0], 0x10 + i % 3) << "frame " << i;
        if (i == 0) first = f;
        if (i == 3)
        {
            EXPECT_EQ(f, first);
        }
    }
}

TEST(FileSource, EndsWhenNotLoopingAndDropsAPartialLastFrame)
{
    TempDir dir;
    std::vector<uint8_t> bytes(16 * 2 + 5, 0x33);
    writeFile(dir.file("raw.yuv"), bytes);
    FileSource s(dir.file("raw.yuv"), yuyv(4, 2), 200, false);
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    grab(s);
    grab(s);
    FrameView v;
    EXPECT_EQ(s.next(v, 2000, error), ReadResult::EndOfStream);
}

TEST(FileSource, Y4m420BecomesNv12WithInterleavedChroma)
{
    TempDir dir;
    std::string y4m = "YUV4MPEG2 W4 H2 F30:1 Ip A1:1 C420jpeg\nFRAME\n";
    const std::string y = "\x01\x02\x03\x04\x05\x06\x07\x08", u = "\xA1\xA2", v = "\xB1\xB2";
    writeFile(dir.file("a.y4m"), y4m + y + u + v);

    FileSource s(dir.file("a.y4m"), yuyv(999, 999), 0, true); // the requested size is ignored for Y4M
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    EXPECT_EQ(s.format(), nv12(4, 2));
    EXPECT_EQ(s.fps(), 30u) << "fps comes from the file when none was requested";
    const auto f = grab(s);
    const std::vector<uint8_t> expect = {1, 2, 3, 4, 5, 6, 7, 8, 0xA1, 0xB1, 0xA2, 0xB2};
    EXPECT_EQ(f, expect);
}

TEST(FileSource, Y4m422BecomesYuyv)
{
    TempDir dir;
    const std::string header = "YUV4MPEG2 W4 H2 F15:1 C422\nFRAME\n";
    const std::string y = "\x01\x02\x03\x04\x05\x06\x07\x08", u = "\xA1\xA2\xA3\xA4", v = "\xB1\xB2\xB3\xB4";
    writeFile(dir.file("b.y4m"), header + y + u + v);
    FileSource s(dir.file("b.y4m"), yuyv(4, 2), 200, true);
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    EXPECT_EQ(s.format(), yuyv(4, 2));
    const std::vector<uint8_t> expect = {1, 0xA1, 2, 0xB1, 3, 0xA2, 4, 0xB2, 5, 0xA3, 6, 0xB3, 7, 0xA4, 8, 0xB4};
    EXPECT_EQ(grab(s), expect);
}

TEST(FileSource, ReportsBadInputWithoutCrashing)
{
    TempDir dir;
    std::string error;
    FileSource missing(dir.file("nope.yuv"), yuyv(4, 2), 15, true);
    EXPECT_FALSE(missing.open(error));
    EXPECT_NE(error.find("nope.yuv"), std::string::npos);

    writeFile(dir.file("c.y4m"), std::string("YUV4MPEG2 W4 H2 F15:1 C444\nFRAME\n"));
    FileSource c444(dir.file("c.y4m"), yuyv(4, 2), 15, true);
    EXPECT_FALSE(c444.open(error));
    EXPECT_NE(error.find("444"), std::string::npos);

    writeFile(dir.file("raw.yuv"), std::string(64, 'x'));
    FileSource noSize(dir.file("raw.yuv"), FrameFormat(), 15, true);
    EXPECT_FALSE(noSize.open(error));

    writeFile(dir.file("tiny.yuv"), std::string(5, 'x'));
    FileSource tiny(dir.file("tiny.yuv"), yuyv(4, 2), 200, true);
    ASSERT_TRUE(tiny.open(error));
    FrameView v;
    EXPECT_EQ(tiny.next(v, 1000, error), ReadResult::Error) << "a looping file with no whole frame must not spin";

    writeFile(dir.file("bad.y4m"), std::string("YUV4MPEG2 W4 H2 F15:1\nNOTAFRAME\n01234567890123456789"));
    FileSource bad(dir.file("bad.y4m"), yuyv(4, 2), 200, true);
    ASSERT_TRUE(bad.open(error));
    EXPECT_EQ(bad.next(v, 1000, error), ReadResult::Error);
}

TEST(PipeSource, ReadsWholeFramesFromACommandAndEnds)
{
    PipeSource s("head -c 64 /dev/zero | tr '\\0' 'x'", yuyv(4, 4), 15); // two 32-byte frames
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    for (int i = 0; i < 2; ++i)
    {
        const auto f = grab(s);
        ASSERT_EQ(f.size(), 32u);
        EXPECT_EQ(f, std::vector<uint8_t>(32, 'x'));
    }
    FrameView v;
    ReadResult r;
    do { r = s.next(v, 2000, error); } while (r == ReadResult::Timeout);
    EXPECT_EQ(r, ReadResult::EndOfStream);
}

TEST(PipeSource, ADanglingPartialFrameIsNeverDelivered)
{
    PipeSource s("head -c 40 /dev/zero | tr '\\0' 'y'", yuyv(4, 4), 15); // one frame and 8 stray bytes
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    grab(s);
    FrameView v;
    ReadResult r;
    do { r = s.next(v, 2000, error); } while (r == ReadResult::Timeout);
    EXPECT_EQ(r, ReadResult::EndOfStream);
}

TEST(PipeSource, CloseStopsALongRunningCommandAndItsChildren)
{
    PipeSource s("sleep 30 & wait", yuyv(4, 4), 15);
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    FrameView v;
    EXPECT_EQ(s.next(v, 50, error), ReadResult::Timeout);
    const auto start = std::chrono::steady_clock::now();
    s.close();
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count(), 2000);
    s.close(); // second close is harmless
    EXPECT_EQ(s.next(v, 10, error), ReadResult::Error);
}

TEST(PipeSource, RefusesAnUnusableFormat)
{
    PipeSource s("true", FrameFormat(), 15);
    std::string error;
    EXPECT_FALSE(s.open(error));
}
