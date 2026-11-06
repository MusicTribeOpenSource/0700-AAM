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

#include <cstring>
#include <new>
#include <thread>

namespace kickcam
{
    static_assert(std::atomic<uint64_t>::is_always_lock_free, "the ring needs lock-free 64-bit atomics");
    static_assert(std::atomic<uint32_t>::is_always_lock_free, "the ring needs lock-free 32-bit atomics");
    static_assert(sizeof(std::atomic<uint64_t>) == 8, "the payload is copied as 64-bit words");
    static_assert(sizeof(RingSlot) == 64, "the payload must start on a cache line");
    static_assert(kRingSlots >= 2, "a reader needs a slot the writer is not in");

    namespace
    {
        constexpr uint32_t alignUp(uint32_t v, uint32_t a) { return (v + a - 1) / a * a; }

        uint32_t slotStrideFor(uint32_t capacity) { return static_cast<uint32_t>(sizeof(RingSlot)) + alignUp(capacity, 64); }

        std::atomic<uint64_t>* payloadWords(const RingSlot* slot)
        {
            return reinterpret_cast<std::atomic<uint64_t>*>(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(slot)) + sizeof(RingSlot));
        }

        void storeBytes(std::atomic<uint64_t>* dst, const uint8_t* src, size_t n)
        {
            size_t i = 0;
            for (; i + 8 <= n; i += 8)
            {
                uint64_t w;
                std::memcpy(&w, src + i, 8);
                dst[i / 8].store(w, std::memory_order_relaxed);
            }
            if (i < n)
            {
                uint64_t w = 0;
                std::memcpy(&w, src + i, n - i);
                dst[i / 8].store(w, std::memory_order_relaxed);
            }
        }

        void loadBytes(const std::atomic<uint64_t>* src, uint8_t* dst, size_t n)
        {
            size_t i = 0;
            for (; i + 8 <= n; i += 8)
            {
                const uint64_t w = src[i / 8].load(std::memory_order_relaxed);
                std::memcpy(dst + i, &w, 8);
            }
            if (i < n)
            {
                const uint64_t w = src[i / 8].load(std::memory_order_relaxed);
                std::memcpy(dst + i, &w, n - i);
            }
        }
    }

    size_t ringBytes(uint32_t payloadCapacity)
    {
        return sizeof(RingHeader) + static_cast<size_t>(kRingSlots) * slotStrideFor(payloadCapacity);
    }

    bool RingWriter::init(void* base, size_t bytes, uint32_t payloadCapacity)
    {
        if (base == nullptr || payloadCapacity == 0 || reinterpret_cast<uintptr_t>(base) % 64 != 0) return false;
        if (bytes < ringBytes(payloadCapacity)) return false;

        std::memset(base, 0, ringBytes(payloadCapacity));
        base_ = static_cast<uint8_t*>(base);
        header_ = new (base_) RingHeader();
        payloadBytes_ = 0;
        counter_ = 0;
        for (uint32_t i = 0; i < kRingSlots; ++i) new (base_ + sizeof(RingHeader) + static_cast<size_t>(i) * slotStrideFor(payloadCapacity)) RingSlot();

        header_->version.store(kRingVersion, std::memory_order_relaxed);
        header_->slotCount.store(kRingSlots, std::memory_order_relaxed);
        header_->slotStride.store(slotStrideFor(payloadCapacity), std::memory_order_relaxed);
        header_->slotsOffset.store(static_cast<uint32_t>(sizeof(RingHeader)), std::memory_order_relaxed);
        header_->capacity.store(payloadCapacity, std::memory_order_relaxed);
        header_->magic.store(kRingMagic, std::memory_order_release);
        return true;
    }

    RingSlot* RingWriter::slot(uint32_t index) const
    {
        return reinterpret_cast<RingSlot*>(base_ + header_->slotsOffset.load(std::memory_order_relaxed) +
                                           static_cast<size_t>(index) * header_->slotStride.load(std::memory_order_relaxed));
    }

    bool RingWriter::setFormat(const FrameFormat& f)
    {
        const size_t bytes = frameBytes(f);
        if (header_ == nullptr || bytes == 0 || bytes > header_->capacity.load(std::memory_order_relaxed)) return false;

        const uint32_t gen = header_->formatGen.load(std::memory_order_relaxed);
        header_->formatGen.store(gen + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        header_->latest.store(0, std::memory_order_relaxed);
        header_->width.store(f.width, std::memory_order_relaxed);
        header_->height.store(f.height, std::memory_order_relaxed);
        header_->stride.store(f.stride, std::memory_order_relaxed);
        header_->fourcc.store(f.fourcc, std::memory_order_relaxed);
        header_->payloadBytes.store(static_cast<uint32_t>(bytes), std::memory_order_relaxed);
        header_->formatGen.store(gen + 2, std::memory_order_release);
        payloadBytes_ = static_cast<uint32_t>(bytes);
        return true;
    }

    void RingWriter::setCapturing(bool on)
    {
        if (header_ != nullptr) header_->capturing.store(on ? 1u : 0u, std::memory_order_release);
    }

    bool RingWriter::publish(const uint8_t* data, size_t size, uint64_t timestampNs)
    {
        if (header_ == nullptr || payloadBytes_ == 0 || size != payloadBytes_ || data == nullptr) return false;

        const uint64_t frameNo = ++counter_;
        RingSlot* s = slot(static_cast<uint32_t>(frameNo % kRingSlots));
        const uint64_t seq = s->seq.load(std::memory_order_relaxed);
        s->seq.store(seq + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        storeBytes(payloadWords(s), data, size);
        s->frameNo.store(frameNo, std::memory_order_relaxed);
        s->timestampNs.store(timestampNs, std::memory_order_relaxed);
        s->seq.store(seq + 2, std::memory_order_release);
        header_->latest.store(frameNo, std::memory_order_release);
        return true;
    }

    void RingWriter::clear()
    {
        if (header_ == nullptr) return;
        header_->capturing.store(0, std::memory_order_release);
        header_->latest.store(0, std::memory_order_release);
        const uint32_t capacity = header_->capacity.load(std::memory_order_relaxed);
        const std::vector<uint8_t> zeros(capacity, 0);
        for (uint32_t i = 0; i < kRingSlots; ++i)
        {
            RingSlot* s = slot(i);
            const uint64_t seq = s->seq.load(std::memory_order_relaxed);
            s->seq.store(seq + 1, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_release);
            storeBytes(payloadWords(s), zeros.data(), zeros.size());
            s->frameNo.store(0, std::memory_order_relaxed);
            s->timestampNs.store(0, std::memory_order_relaxed);
            s->seq.store(seq + 2, std::memory_order_release);
        }
    }

    bool RingReader::attach(const void* base, size_t bytes)
    {
        header_ = nullptr;
        if (base == nullptr || bytes < sizeof(RingHeader) || reinterpret_cast<uintptr_t>(base) % 64 != 0) return false;
        const auto* h = static_cast<const RingHeader*>(base);
        if (h->magic.load(std::memory_order_acquire) != kRingMagic || h->version.load(std::memory_order_relaxed) != kRingVersion) return false;

        const uint32_t count = h->slotCount.load(std::memory_order_relaxed);
        const uint32_t stride = h->slotStride.load(std::memory_order_relaxed);
        const uint32_t offset = h->slotsOffset.load(std::memory_order_relaxed);
        const uint32_t capacity = h->capacity.load(std::memory_order_relaxed);
        if (count != kRingSlots || capacity == 0 || offset < sizeof(RingHeader) || offset % 64 != 0 || stride % 64 != 0) return false;
        if (stride < sizeof(RingSlot) + static_cast<size_t>(capacity)) return false;
        if (static_cast<size_t>(offset) + static_cast<size_t>(count) * stride > bytes) return false;

        base_ = static_cast<const uint8_t*>(base);
        header_ = h;
        slotStride_ = stride;
        slotsOffset_ = offset;
        capacity_ = capacity;
        return true;
    }

    bool RingReader::capturing() const { return header_ != nullptr && header_->capturing.load(std::memory_order_acquire) != 0; }

    RingReader::Result RingReader::readLatest(uint64_t lastSeen, std::vector<uint8_t>& out, FrameMeta& meta) const
    {
        if (header_ == nullptr) return Result::NotCapturing;
        constexpr int kAttempts = 8;
        for (int attempt = 0; attempt < kAttempts; ++attempt)
        {
            if (header_->capturing.load(std::memory_order_acquire) == 0) return Result::NotCapturing;

            const uint32_t gen = header_->formatGen.load(std::memory_order_acquire);
            if ((gen & 1u) != 0) { std::this_thread::yield(); continue; }

            const uint64_t n = header_->latest.load(std::memory_order_acquire);
            if (n == 0) return Result::NotCapturing;
            if (n == lastSeen) return Result::NoNewFrame;

            FrameFormat f;
            f.width = header_->width.load(std::memory_order_relaxed);
            f.height = header_->height.load(std::memory_order_relaxed);
            f.stride = header_->stride.load(std::memory_order_relaxed);
            f.fourcc = header_->fourcc.load(std::memory_order_relaxed);
            const uint32_t payload = header_->payloadBytes.load(std::memory_order_relaxed);
            if (payload == 0 || payload > capacity_) { std::this_thread::yield(); continue; }

            const auto* s = reinterpret_cast<const RingSlot*>(base_ + slotsOffset_ + static_cast<size_t>(n % kRingSlots) * slotStride_);
            const uint64_t s1 = s->seq.load(std::memory_order_acquire);
            if ((s1 & 1u) != 0) { std::this_thread::yield(); continue; }

            out.resize(payload);
            loadBytes(payloadWords(s), out.data(), payload);
            const uint64_t frameNo = s->frameNo.load(std::memory_order_relaxed);
            const uint64_t ts = s->timestampNs.load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            const uint64_t s2 = s->seq.load(std::memory_order_relaxed);
            const uint32_t gen2 = header_->formatGen.load(std::memory_order_relaxed);
            if (s1 != s2 || frameNo != n || gen != gen2) continue;

            meta.format = f;
            meta.frameNo = n;
            meta.timestampNs = ts;
            return Result::Ok;
        }
        return Result::Torn;
    }
}
