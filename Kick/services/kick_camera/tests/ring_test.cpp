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
#include "kick_camera/shm_ring.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>

using namespace kickcam;

namespace
{
    struct Block
    {
        explicit Block(size_t bytes) : size((bytes + 63) / 64 * 64), data(static_cast<uint8_t*>(std::aligned_alloc(64, size))) {}
        ~Block() { std::free(data); }
        size_t size;
        uint8_t* data;
    };

    FrameFormat yuyv(uint32_t w, uint32_t h) { return {w, h, w * 2, kFourccYuyv}; }

    std::vector<uint8_t> pattern(size_t n, uint64_t seed)
    {
        std::vector<uint8_t> v(n);
        for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(seed * 7 + i);
        return v;
    }

    // Ring over a heap block, with a writer and a reader attached.
    struct Fixture
    {
        explicit Fixture(uint32_t capacity = 4096) : block(ringBytes(capacity)) { EXPECT_TRUE(writer.init(block.data, block.size, capacity)); EXPECT_TRUE(reader.attach(block.data, block.size)); }
        Block block;
        RingWriter writer;
        RingReader reader;
    };
}

TEST(Ring, InitRejectsABlockThatIsTooSmallOrMisaligned)
{
    Block small(ringBytes(1024) - 64);
    RingWriter w;
    EXPECT_FALSE(w.init(small.data, small.size, 1024));
    Block ok(ringBytes(1024) + 64);
    EXPECT_FALSE(w.init(ok.data + 8, ok.size - 8, 1024));
    EXPECT_FALSE(w.init(ok.data, ok.size, 0));
}

TEST(Ring, ReaderRejectsMemoryThatIsNotARing)
{
    Block zeros(ringBytes(1024));
    std::memset(zeros.data, 0, zeros.size);
    RingReader r;
    EXPECT_FALSE(r.attach(zeros.data, zeros.size));

    Fixture f(1024);
    RingReader truncated;
    EXPECT_FALSE(truncated.attach(f.block.data, ringBytes(1024) - 64)) << "slots would run past the mapping";

    auto* header = reinterpret_cast<RingHeader*>(f.block.data);
    header->version.store(kRingVersion + 1);
    EXPECT_FALSE(r.attach(f.block.data, f.block.size));
    header->version.store(kRingVersion);
    header->slotStride.store(65);
    EXPECT_FALSE(r.attach(f.block.data, f.block.size));
}

TEST(Ring, NothingToReadUntilCapturingAndAFrameArrives)
{
    Fixture f;
    std::vector<uint8_t> out;
    FrameMeta meta;
    EXPECT_EQ(f.reader.readLatest(0, out, meta), RingReader::Result::NotCapturing);
    ASSERT_TRUE(f.writer.setFormat(yuyv(16, 8)));
    f.writer.setCapturing(true);
    EXPECT_TRUE(f.reader.capturing());
    EXPECT_EQ(f.reader.readLatest(0, out, meta), RingReader::Result::NotCapturing) << "no frame published yet";
}

TEST(Ring, PublishedFrameComesBackByteForByteWithItsMetadata)
{
    Fixture f;
    ASSERT_TRUE(f.writer.setFormat(yuyv(16, 8)));
    f.writer.setCapturing(true);
    const auto frame = pattern(16 * 8 * 2, 1);
    ASSERT_TRUE(f.writer.publish(frame.data(), frame.size(), 123456789));

    std::vector<uint8_t> out;
    FrameMeta meta;
    ASSERT_EQ(f.reader.readLatest(0, out, meta), RingReader::Result::Ok);
    EXPECT_EQ(out, frame);
    EXPECT_EQ(meta.frameNo, 1u);
    EXPECT_EQ(meta.timestampNs, 123456789u);
    EXPECT_EQ(meta.format, yuyv(16, 8));
    EXPECT_EQ(f.reader.readLatest(1, out, meta), RingReader::Result::NoNewFrame);
}

TEST(Ring, PayloadThatIsNotAMultipleOfEightSurvives)
{
    Fixture f;
    ASSERT_TRUE(f.writer.setFormat(yuyv(18, 3))); // 108 bytes
    f.writer.setCapturing(true);
    const auto frame = pattern(108, 5);
    ASSERT_TRUE(f.writer.publish(frame.data(), frame.size(), 1));
    std::vector<uint8_t> out;
    FrameMeta meta;
    ASSERT_EQ(f.reader.readLatest(0, out, meta), RingReader::Result::Ok);
    EXPECT_EQ(out, frame);
}

TEST(Ring, ReaderAlwaysGetsTheNewestFrameAfterTheRingWraps)
{
    Fixture f;
    ASSERT_TRUE(f.writer.setFormat(yuyv(16, 8)));
    f.writer.setCapturing(true);
    for (uint64_t n = 1; n <= 10; ++n)
    {
        const auto frame = pattern(256, n);
        ASSERT_TRUE(f.writer.publish(frame.data(), frame.size(), n));
    }
    std::vector<uint8_t> out;
    FrameMeta meta;
    ASSERT_EQ(f.reader.readLatest(0, out, meta), RingReader::Result::Ok);
    EXPECT_EQ(meta.frameNo, 10u);
    EXPECT_EQ(out, pattern(256, 10));
}

TEST(Ring, WrongSizedFramesAndOversizedFormatsAreRefused)
{
    Fixture f(1024);
    EXPECT_FALSE(f.writer.publish(nullptr, 0, 0)) << "no format set yet";
    ASSERT_TRUE(f.writer.setFormat(yuyv(16, 8)));
    const std::vector<uint8_t> shortFrame(255, 0);
    EXPECT_FALSE(f.writer.publish(shortFrame.data(), shortFrame.size(), 0));
    EXPECT_FALSE(f.writer.setFormat(yuyv(32, 32))) << "2048 bytes in a 1024 byte slot";
    EXPECT_FALSE(f.writer.setFormat({15, 8, 30, kFourccYuyv})) << "odd width";
    EXPECT_FALSE(f.writer.setFormat({16, 8, 32, 0x12345678})) << "unknown fourcc";
}

TEST(Ring, ChangingFormatDropsTheOldFrame)
{
    Fixture f;
    ASSERT_TRUE(f.writer.setFormat(yuyv(16, 8)));
    f.writer.setCapturing(true);
    const auto a = pattern(256, 1);
    f.writer.publish(a.data(), a.size(), 1);
    ASSERT_TRUE(f.writer.setFormat({16, 8, 16, kFourccNv12}));
    std::vector<uint8_t> out;
    FrameMeta meta;
    EXPECT_EQ(f.reader.readLatest(0, out, meta), RingReader::Result::NotCapturing) << "latest was reset with the format";
    const auto b = pattern(16 * 8 * 3 / 2, 2);
    ASSERT_TRUE(f.writer.publish(b.data(), b.size(), 2));
    ASSERT_EQ(f.reader.readLatest(0, out, meta), RingReader::Result::Ok);
    EXPECT_EQ(meta.format.fourcc, kFourccNv12);
    EXPECT_EQ(out, b);
    EXPECT_EQ(meta.frameNo, 2u) << "frame numbers keep counting across a format change";
}

TEST(Ring, ClearZeroesEveryPixelAndTurnsCapturingOff)
{
    Fixture f;
    ASSERT_TRUE(f.writer.setFormat(yuyv(16, 8)));
    f.writer.setCapturing(true);
    const std::vector<uint8_t> loud(256, 0xAB);
    for (int i = 0; i < 5; ++i) f.writer.publish(loud.data(), loud.size(), 1);
    f.writer.clear();

    EXPECT_FALSE(f.reader.capturing());
    std::vector<uint8_t> out;
    FrameMeta meta;
    EXPECT_EQ(f.reader.readLatest(0, out, meta), RingReader::Result::NotCapturing);
    for (size_t i = 0; i < f.block.size; ++i) ASSERT_NE(f.block.data[i], 0xAB) << "pixel byte left at offset " << i;
}

TEST(Ring, ASlotLeftMidWriteIsReportedAsTornNotReturned)
{
    Fixture f;
    ASSERT_TRUE(f.writer.setFormat(yuyv(16, 8)));
    f.writer.setCapturing(true);
    const auto frame = pattern(256, 3);
    f.writer.publish(frame.data(), frame.size(), 1);

    auto* header = reinterpret_cast<RingHeader*>(f.block.data);
    const uint64_t n = header->latest.load();
    auto* slot = reinterpret_cast<RingSlot*>(f.block.data + header->slotsOffset.load() + (n % kRingSlots) * header->slotStride.load());
    slot->seq.fetch_add(1); // odd: the writer is "inside" this slot

    std::vector<uint8_t> out;
    FrameMeta meta;
    EXPECT_EQ(f.reader.readLatest(0, out, meta), RingReader::Result::Torn);
}

// One thread publishes as fast as it can while another reads. Every frame a reader accepts must be internally
// consistent: its bytes follow the pattern for the frame number the ring reports. Run it under ThreadSanitizer
// (-DKICK_CAMERA_SANITIZE=thread) to check the seqlock has no data race.
class RingConcurrency : public testing::TestWithParam<FrameFormat> {};

TEST_P(RingConcurrency, ReaderNeverSeesATornFrame)
{
    const FrameFormat fmt = GetParam();
    const size_t bytes = frameBytes(fmt);
    Fixture f(8192);
    ASSERT_TRUE(f.writer.setFormat(fmt));
    f.writer.setCapturing(true);

    constexpr uint64_t kFrames = 4000;
    std::atomic<bool> done{false};
    std::thread writer(
        [&]
        {
            for (uint64_t n = 1; n <= kFrames; ++n)
            {
                const auto frame = pattern(bytes, n);
                f.writer.publish(frame.data(), frame.size(), n * 1000);
            }
            done = true;
        });

    uint64_t last = 0, accepted = 0, torn = 0, mismatches = 0, backwards = 0;
    std::vector<uint8_t> out;
    FrameMeta meta;
    while (!done || last < kFrames)
    {
        const auto r = f.reader.readLatest(last, out, meta);
        if (r == RingReader::Result::Ok)
        {
            ++accepted;
            if (meta.frameNo <= last) ++backwards;
            if (out != pattern(bytes, meta.frameNo) || meta.timestampNs != meta.frameNo * 1000) ++mismatches;
            last = meta.frameNo;
        }
        else if (r == RingReader::Result::Torn) ++torn;
        if (done && r == RingReader::Result::NoNewFrame) break;
    }
    writer.join();
    EXPECT_EQ(mismatches, 0u);
    EXPECT_EQ(backwards, 0u);
    EXPECT_GT(accepted, 0u);
    RecordProperty("accepted", static_cast<int>(accepted));
    RecordProperty("torn_retries_exhausted", static_cast<int>(torn));
}

INSTANTIATE_TEST_SUITE_P(Formats, RingConcurrency,
                         testing::Values(FrameFormat{64, 32, 128, kFourccYuyv}, FrameFormat{18, 3, 36, kFourccYuyv}, FrameFormat{32, 16, 32, kFourccNv12}));
