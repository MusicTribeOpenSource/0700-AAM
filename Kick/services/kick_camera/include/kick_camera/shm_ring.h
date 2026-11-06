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

#include "kick_camera/types.h"

#include <atomic>
#include <vector>

// A single-writer, many-reader frame ring over a block of memory (POSIX shared memory in production,
// plain heap memory in tests). Readers only ever load from it, so a reader can neither block the writer
// nor corrupt it, and a reader that stalls just misses frames.
//
// Layout: one RingHeader, then kRingSlots slots. Each slot is a RingSlot followed by the pixel payload.
//
// Consistency is a seqlock per slot. For frame number n (1, 2, 3, ...) the writer uses slot n % kRingSlots:
//   1. slot.seq goes to an odd value, 2. the payload, frameNo and timestamp are written,
//   3. slot.seq goes to the next even value, 4. header.latest is set to n.
// A reader loads latest, copies that slot, and keeps the copy only if slot.seq was even and unchanged and
// slot.frameNo is still n. With three slots a reader copying frame n is only disturbed once the writer has
// produced n + 3, so at 15 fps a reader has two to three frame periods (130 to 200 ms) to finish a copy.
//
// The pixel payload is accessed as relaxed 64-bit atomics on both sides. That keeps the seqlock free of data
// races in the C++ memory model (and quiet under ThreadSanitizer) at no real cost: a 320x240 YUYV frame is
// 19200 words per copy.
//
// Format changes use a second seqlock (formatGen, odd while the writer changes the format fields).
//
// Timestamps are CLOCK_MONOTONIC nanoseconds, so a reader compares them with its own monotonic clock.

namespace kickcam
{
    constexpr uint32_t kRingMagic = makeFourcc('K', 'C', 'A', 'M');
    constexpr uint32_t kRingVersion = 1;
    constexpr uint32_t kRingSlots = 3;

    struct alignas(64) RingHeader
    {
        std::atomic<uint32_t> magic;       // written last by RingWriter::init, so a reader never sees a half-built header
        std::atomic<uint32_t> version;     // kRingVersion
        std::atomic<uint32_t> slotCount;   // kRingSlots
        std::atomic<uint32_t> slotStride;  // bytes from one slot to the next
        std::atomic<uint32_t> slotsOffset; // bytes from the start of the ring to slot 0
        std::atomic<uint32_t> capacity;    // payload bytes available in each slot
        std::atomic<uint32_t> capturing;   // 1 while the camera device is open. The on-screen indicator follows this.
        std::atomic<uint32_t> formatGen;   // odd while the format fields below are being rewritten
        std::atomic<uint32_t> width;
        std::atomic<uint32_t> height;
        std::atomic<uint32_t> stride;
        std::atomic<uint32_t> fourcc;
        std::atomic<uint32_t> payloadBytes; // bytes of pixel data in a frame of the current format
        std::atomic<uint64_t> latest;       // number of the newest complete frame, 0 when there is none
    };

    struct alignas(64) RingSlot
    {
        std::atomic<uint64_t> seq; // odd while the writer is inside this slot
        std::atomic<uint64_t> frameNo;
        std::atomic<uint64_t> timestampNs;
    };

    // Bytes needed for a ring whose slots hold up to payloadCapacity bytes.
    size_t ringBytes(uint32_t payloadCapacity);

    class RingWriter
    {
    public:
        // Builds the ring in [base, base + bytes). base must be 64-byte aligned (mmap and malloc'd blocks of
        // 64 or more bytes from aligned_alloc are). Returns false when the block is too small.
        bool init(void* base, size_t bytes, uint32_t payloadCapacity);

        // Changes the published format. False when the format is unsupported or does not fit the capacity.
        // Clears the latest frame so no reader mixes two formats.
        bool setFormat(const FrameFormat& f);

        void setCapturing(bool on);

        // Copies one frame in. size must equal frameBytes() of the current format. False otherwise.
        bool publish(const uint8_t* data, size_t size, uint64_t timestampNs);

        // Marks the ring as not capturing, drops the latest frame and zeroes every slot, so no pixels outlive
        // the camera being switched off.
        void clear();

        uint64_t framesPublished() const { return counter_; }

    private:
        RingSlot* slot(uint32_t index) const;
        RingHeader* header_ = nullptr;
        uint8_t* base_ = nullptr;
        uint32_t payloadBytes_ = 0;
        uint64_t counter_ = 0;
    };

    struct FrameMeta
    {
        FrameFormat format;
        uint64_t frameNo = 0;
        uint64_t timestampNs = 0;
    };

    class RingReader
    {
    public:
        enum class Result
        {
            Ok,           // out and meta hold a new frame
            NoNewFrame,   // the newest frame is the one named by lastSeen
            NotCapturing, // the camera is off or has not produced a frame yet
            Torn          // the writer kept overwriting the slot for every retry; try again
        };

        // Validates the header and every offset against bytes. False for anything that is not a ring this
        // build understands.
        bool attach(const void* base, size_t bytes);

        bool capturing() const;

        // Copies the newest frame into out unless it is the one numbered lastSeen. Frame numbers only grow within
        // one run of the service; a reader that sees the number go down should treat it as a restart.
        Result readLatest(uint64_t lastSeen, std::vector<uint8_t>& out, FrameMeta& meta) const;

    private:
        const RingHeader* header_ = nullptr;
        const uint8_t* base_ = nullptr;
        uint32_t slotStride_ = 0;
        uint32_t slotsOffset_ = 0;
        uint32_t capacity_ = 0;
    };
}
