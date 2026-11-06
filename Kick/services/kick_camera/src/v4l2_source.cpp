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
#ifdef __linux__

#include "kick_camera/v4l2_source.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace kickcam
{
    V4l2Ops V4l2Ops::system()
    {
        V4l2Ops ops;
        ops.open = [](const char* path, int flags) { return ::open(path, flags); };
        ops.close = [](int fd) { return ::close(fd); };
        ops.ioctl = [](int fd, unsigned long request, void* arg) { return ::ioctl(fd, request, arg); };
        ops.mmap = [](size_t length, int fd, off_t offset) -> void*
        {
            void* p = ::mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
            return p == MAP_FAILED ? nullptr : p;
        };
        ops.munmap = [](void* addr, size_t length) { return ::munmap(addr, length); };
        ops.poll = [](int fd, int timeoutMs)
        {
            struct pollfd p = {fd, POLLIN, 0};
            const int rc = ::poll(&p, 1, timeoutMs);
            return rc < 0 ? -1 : (rc == 0 ? 0 : 1);
        };
        return ops;
    }

    namespace
    {
        constexpr unsigned kBufferCount = 4;
        constexpr unsigned kMinBuffers = 2;
    }

    V4L2Source::V4L2Source(std::string device, FrameFormat requested, uint32_t fps, V4l2Ops ops)
        : device_(std::move(device)), requested_(requested), fps_(fps), ops_(std::move(ops))
    {
    }

    V4L2Source::~V4L2Source() { close(); }

    int V4L2Source::xioctl(unsigned long request, void* arg)
    {
        int rc;
        do { rc = ops_.ioctl(fd_, request, arg); } while (rc < 0 && errno == EINTR);
        return rc;
    }

    bool V4L2Source::queue(unsigned index)
    {
        v4l2_buffer b;
        std::memset(&b, 0, sizeof(b));
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = index;
        return xioctl(VIDIOC_QBUF, &b) == 0;
    }

    bool V4L2Source::open(std::string& error)
    {
        close();
        fd_ = ops_.open(device_.c_str(), O_RDWR | O_NONBLOCK);
        if (fd_ < 0)
        {
            error = "v4l2: cannot open " + device_ + ": " + std::strerror(errno);
            return false;
        }
        auto fail = [&](const std::string& message)
        {
            error = message;
            close();
            return false;
        };

        v4l2_capability cap;
        std::memset(&cap, 0, sizeof(cap));
        if (xioctl(VIDIOC_QUERYCAP, &cap) != 0) return fail("v4l2: " + device_ + " is not a V4L2 device: " + std::strerror(errno));
        const uint32_t caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) != 0 ? cap.device_caps : cap.capabilities;
        if ((caps & V4L2_CAP_VIDEO_CAPTURE) == 0)
        {
            return fail("v4l2: " + device_ + " is not a single-plane capture device" +
                        ((caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) != 0 ? " (multi-planar capture is not supported yet)" : ""));
        }
        if ((caps & V4L2_CAP_STREAMING) == 0) return fail("v4l2: " + device_ + " does not support streaming I/O");

        const uint32_t candidates[2] = {requested_.fourcc != 0 ? requested_.fourcc : kFourccYuyv, kFourccNv12};
        const int candidateCount = requested_.fourcc != 0 ? 1 : 2;
        bool chosen = false;
        for (int i = 0; i < candidateCount && !chosen; ++i)
        {
            v4l2_format fmt;
            std::memset(&fmt, 0, sizeof(fmt));
            fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            fmt.fmt.pix.width = requested_.width;
            fmt.fmt.pix.height = requested_.height;
            fmt.fmt.pix.pixelformat = candidates[i];
            fmt.fmt.pix.field = V4L2_FIELD_ANY;
            if (xioctl(VIDIOC_S_FMT, &fmt) != 0 || fmt.fmt.pix.pixelformat != candidates[i]) continue;

            format_.width = fmt.fmt.pix.width;
            format_.height = fmt.fmt.pix.height;
            format_.fourcc = fmt.fmt.pix.pixelformat;
            const uint32_t minimum = minStride(format_.fourcc, format_.width);
            format_.stride = fmt.fmt.pix.bytesperline >= minimum ? fmt.fmt.pix.bytesperline : minimum;
            const size_t needed = frameBytes(format_);
            if (needed == 0 || fmt.fmt.pix.sizeimage < needed) continue;
            chosen = true;
        }
        if (!chosen)
        {
            return fail("v4l2: " + device_ + " gave neither " + (requested_.fourcc != 0 ? formatName(requested_.fourcc) : "YUYV or NV12") +
                        " at " + std::to_string(requested_.width) + "x" + std::to_string(requested_.height));
        }

        v4l2_streamparm parm;
        std::memset(&parm, 0, sizeof(parm));
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = fps_ == 0 ? 15 : fps_;
        if (xioctl(VIDIOC_S_PARM, &parm) == 0 && parm.parm.capture.timeperframe.numerator != 0 && parm.parm.capture.timeperframe.denominator != 0)
            fps_ = parm.parm.capture.timeperframe.denominator / parm.parm.capture.timeperframe.numerator;
        // If the driver refuses S_PARM the requested rate stays in fps_ as a claim, not a measurement.

        v4l2_requestbuffers req;
        std::memset(&req, 0, sizeof(req));
        req.count = kBufferCount;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (xioctl(VIDIOC_REQBUFS, &req) != 0) return fail(std::string("v4l2: REQBUFS failed: ") + std::strerror(errno));
        if (req.count < kMinBuffers) return fail("v4l2: the driver granted fewer than 2 buffers");

        maps_.assign(req.count, Mapping());
        for (unsigned i = 0; i < req.count; ++i)
        {
            v4l2_buffer b;
            std::memset(&b, 0, sizeof(b));
            b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            b.memory = V4L2_MEMORY_MMAP;
            b.index = i;
            if (xioctl(VIDIOC_QUERYBUF, &b) != 0) return fail(std::string("v4l2: QUERYBUF failed: ") + std::strerror(errno));
            void* p = ops_.mmap(b.length, fd_, static_cast<off_t>(b.m.offset));
            if (p == nullptr) return fail(std::string("v4l2: mmap of buffer failed: ") + std::strerror(errno));
            maps_[i].ptr = p;
            maps_[i].length = b.length;
        }
        for (unsigned i = 0; i < req.count; ++i)
            if (!queue(i)) return fail(std::string("v4l2: QBUF failed: ") + std::strerror(errno));

        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(VIDIOC_STREAMON, &type) != 0) return fail(std::string("v4l2: STREAMON failed: ") + std::strerror(errno));
        streaming_ = true;
        held_ = -1;
        haveSequence_ = false;
        dropped_ = 0;
        return true;
    }

    void V4L2Source::close()
    {
        if (fd_ >= 0)
        {
            if (streaming_)
            {
                int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                xioctl(VIDIOC_STREAMOFF, &type);
            }
            for (Mapping& m : maps_)
                if (m.ptr != nullptr) ops_.munmap(m.ptr, m.length);
            if (!maps_.empty())
            {
                v4l2_requestbuffers req;
                std::memset(&req, 0, sizeof(req));
                req.count = 0;
                req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                req.memory = V4L2_MEMORY_MMAP;
                xioctl(VIDIOC_REQBUFS, &req);
            }
            ops_.close(fd_);
        }
        fd_ = -1;
        streaming_ = false;
        maps_.clear();
        held_ = -1;
    }

    ReadResult V4L2Source::next(FrameView& out, int timeoutMs, std::string& error)
    {
        if (fd_ < 0 || !streaming_)
        {
            error = "v4l2: not streaming";
            return ReadResult::Error;
        }
        // The previous frame's buffer goes back to the driver now, which is why a FrameView lives until the next call.
        if (held_ >= 0)
        {
            const unsigned index = static_cast<unsigned>(held_);
            held_ = -1;
            if (!queue(index))
            {
                error = std::string("v4l2: QBUF failed: ") + std::strerror(errno);
                return ReadResult::Error;
            }
        }

        const int ready = ops_.poll(fd_, timeoutMs);
        if (ready == 0) return ReadResult::Timeout;
        if (ready < 0)
        {
            if (errno == EINTR) return ReadResult::Timeout;
            error = std::string("v4l2: poll failed: ") + std::strerror(errno);
            return ReadResult::Error;
        }

        v4l2_buffer b;
        std::memset(&b, 0, sizeof(b));
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        if (xioctl(VIDIOC_DQBUF, &b) != 0)
        {
            if (errno == EAGAIN) return ReadResult::Timeout;
            error = std::string("v4l2: DQBUF failed: ") + std::strerror(errno);
            return ReadResult::Error;
        }
        if (b.index >= maps_.size())
        {
            error = "v4l2: the driver returned an unknown buffer index";
            return ReadResult::Error;
        }

        if (haveSequence_)
        {
            const uint32_t gap = b.sequence - lastSequence_ - 1; // unsigned wrap is intended
            if (gap != 0 && gap < (1u << 30)) dropped_ += gap;
        }
        haveSequence_ = true;
        lastSequence_ = b.sequence;

        const size_t expected = frameBytes(format_);
        if ((b.flags & V4L2_BUF_FLAG_ERROR) != 0 || b.bytesused < expected || maps_[b.index].length < expected)
        {
            ++dropped_;
            if (!queue(b.index))
            {
                error = std::string("v4l2: QBUF failed: ") + std::strerror(errno);
                return ReadResult::Error;
            }
            return ReadResult::Timeout;
        }

        held_ = static_cast<int>(b.index);
        out.data = static_cast<const uint8_t*>(maps_[b.index].ptr);
        out.size = expected;
        out.timestampNs = (b.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) == V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC
                              ? static_cast<uint64_t>(b.timestamp.tv_sec) * 1000000000ull + static_cast<uint64_t>(b.timestamp.tv_usec) * 1000ull
                              : monotonicNs();
        return ReadResult::Frame;
    }

    bool V4L2Source::setControl(unsigned id, int value)
    {
        if (fd_ < 0) return false;
        v4l2_control c;
        std::memset(&c, 0, sizeof(c));
        c.id = id;
        c.value = value;
        return xioctl(VIDIOC_S_CTRL, &c) == 0;
    }

    bool V4L2Source::setAutoExposure(bool on)
    {
        if (!on) return setControl(V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL);
        // UVC cameras call automatic exposure "aperture priority", most sensor drivers call it "auto". Try both.
        return setControl(V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_APERTURE_PRIORITY) || setControl(V4L2_CID_EXPOSURE_AUTO, V4L2_EXPOSURE_AUTO);
    }

    bool V4L2Source::setExposureAbsolute(int value) { return setControl(V4L2_CID_EXPOSURE_ABSOLUTE, value); }

    bool V4L2Source::getExposureAbsolute(int& value)
    {
        if (fd_ < 0) return false;
        v4l2_control c;
        std::memset(&c, 0, sizeof(c));
        c.id = V4L2_CID_EXPOSURE_ABSOLUTE;
        if (xioctl(VIDIOC_G_CTRL, &c) != 0) return false;
        value = c.value;
        return true;
    }

    // V4L2_CID_EXPOSURE_AUTO_PRIORITY: 0 keeps the frame rate constant, 1 lets the sensor stretch exposure and drop the rate.
    bool V4L2Source::setKeepFrameRate(bool on) { return setControl(V4L2_CID_EXPOSURE_AUTO_PRIORITY, on ? 0 : 1); }

    bool V4L2Source::setAutoWhiteBalance(bool on) { return setControl(V4L2_CID_AUTO_WHITE_BALANCE, on ? 1 : 0); }
}

#endif
