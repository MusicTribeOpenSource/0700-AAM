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

#ifdef __linux__

#include "kick_camera/frame_source.h"

#include <functional>
#include <vector>

#include <sys/types.h>

namespace kickcam
{
    // The system calls V4L2Source makes, so a test can stand in for the kernel. system() is the real thing.
    struct V4l2Ops
    {
        std::function<int(const char* path, int flags)> open;
        std::function<int(int fd)> close;
        std::function<int(int fd, unsigned long request, void* arg)> ioctl;
        // Returns nullptr on failure (the real implementation maps MAP_FAILED to nullptr).
        std::function<void*(size_t length, int fd, off_t offset)> mmap;
        std::function<int(void* addr, size_t length)> munmap;
        // 1 readable, 0 timed out, -1 error with errno set.
        std::function<int(int fd, int timeoutMs)> poll;

        static V4l2Ops system();
    };

    // Single-plane V4L2 capture with mmap buffers (VIDIOC_REQBUFS / QBUF / DQBUF). One backend covers the MIPI CSI
    // sensor and a USB UVC webcam: they are both /dev/videoN capture devices, so the device path is the only difference.
    //
    // Multi-planar capture devices (V4L2_CAP_VIDEO_CAPTURE_MPLANE) are refused with a clear message. Whether the
    // i.MX8M Mini capture node is single- or multi-planar is not known until the unit exists.
    class V4L2Source : public FrameSource
    {
    public:
        // requested.fourcc 0 tries YUYV, then NV12. The driver may adjust the size; the adjusted size is what gets published.
        V4L2Source(std::string device, FrameFormat requested, uint32_t fps, V4l2Ops ops = V4l2Ops::system());
        ~V4L2Source() override;

        bool open(std::string& error) override;
        void close() override;
        FrameFormat format() const override { return format_; }
        uint32_t fps() const override { return fps_; }
        ReadResult next(FrameView& out, int timeoutMs, std::string& error) override;
        const char* name() const override { return "v4l2"; }
        uint64_t droppedFrames() const override { return dropped_; }

        bool setAutoExposure(bool on) override;
        bool setExposureAbsolute(int value) override;
        bool getExposureAbsolute(int& value) override;
        bool setKeepFrameRate(bool on) override;
        bool setAutoWhiteBalance(bool on) override;

    private:
        struct Mapping
        {
            void* ptr = nullptr;
            size_t length = 0;
        };
        int xioctl(unsigned long request, void* arg);
        bool queue(unsigned index);
        bool setControl(unsigned id, int value);

        std::string device_;
        FrameFormat requested_;
        FrameFormat format_;
        uint32_t fps_;
        V4l2Ops ops_;
        int fd_ = -1;
        bool streaming_ = false;
        std::vector<Mapping> maps_;
        int held_ = -1;
        bool haveSequence_ = false;
        uint32_t lastSequence_ = 0;
        uint64_t dropped_ = 0;
    };
}

#endif
