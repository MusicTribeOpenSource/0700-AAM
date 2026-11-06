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

#include "kick_camera/frame_source.h"

#include <cstdio>
#include <vector>

namespace kickcam
{
    // A synthetic test card: a horizontal gradient with a bright square that moves right and wraps. Frame
    // numbering starts at 0. Used by the emulator and the tests, and it works anywhere.
    class PatternSource : public FrameSource
    {
    public:
        PatternSource(FrameFormat f, uint32_t fps);
        bool open(std::string& error) override;
        void close() override;
        FrameFormat format() const override { return format_; }
        uint32_t fps() const override { return fps_; }
        ReadResult next(FrameView& out, int timeoutMs, std::string& error) override;
        const char* name() const override { return "pattern"; }

        // Fills dst (frameBytes(f) long) with frame number index of the pattern. Exposed so tests can compare.
        static void render(const FrameFormat& f, uint64_t index, uint8_t* dst);

    private:
        FrameFormat format_;
        uint32_t fps_;
        bool opened_ = false;
        uint64_t index_ = 0;
        uint64_t nextDueNs_ = 0;
        std::vector<uint8_t> buffer_;
    };

    // Plays a video file in a loop, paced at the requested fps. Two kinds of file:
    //  - Y4M (recognised by its "YUV4MPEG2" signature): 4:2:0 becomes NV12, 4:2:2 becomes YUYV. The file's own
    //    size is used and the requested size is ignored.
    //  - anything else is raw frames in the requested format and size, back to back.
    class FileSource : public FrameSource
    {
    public:
        FileSource(std::string path, FrameFormat requested, uint32_t fps, bool loop);
        ~FileSource() override;
        bool open(std::string& error) override;
        void close() override;
        FrameFormat format() const override { return format_; }
        uint32_t fps() const override { return fps_; }
        ReadResult next(FrameView& out, int timeoutMs, std::string& error) override;
        const char* name() const override { return "file"; }

    private:
        bool readFrame(std::string& error, bool& endOfFile);
        std::string path_;
        FrameFormat format_;
        uint32_t fps_;
        bool loop_;
        std::FILE* file_ = nullptr;
        bool y4m_ = false;
        bool y4m422_ = false;
        long dataStart_ = 0;
        uint64_t nextDueNs_ = 0;
        std::vector<uint8_t> buffer_;
        std::vector<uint8_t> scratch_;
    };

    // Raw frames on a pipe, back to back. Without a command the pipe is this process's stdin. With a command the
    // source starts it with /bin/sh -c on open() and stops it on close(), so a webcam helper such as ffmpeg only
    // runs, and the camera light is only on, while the service is enabled.
    class PipeSource : public FrameSource
    {
    public:
        PipeSource(std::string command, FrameFormat f, uint32_t fps);
        ~PipeSource() override;
        bool open(std::string& error) override;
        void close() override;
        FrameFormat format() const override { return format_; }
        uint32_t fps() const override { return fps_; }
        ReadResult next(FrameView& out, int timeoutMs, std::string& error) override;
        const char* name() const override { return "pipe"; }

    private:
        std::string command_;
        FrameFormat format_;
        uint32_t fps_;
        int fd_ = -1;
        int pid_ = -1;
        size_t filled_ = 0;
        std::vector<uint8_t> buffer_;
    };
}
