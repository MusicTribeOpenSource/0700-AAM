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

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace kickcam
{
    namespace
    {
        // Sleeps until dueNs (monotonic) but never longer than timeoutMs. Returns false when the timeout cut it short.
        bool sleepUntil(uint64_t dueNs, int timeoutMs)
        {
            const uint64_t now = monotonicNs();
            if (dueNs <= now) return true;
            const uint64_t wait = dueNs - now;
            const uint64_t limit = static_cast<uint64_t>(std::max(timeoutMs, 0)) * 1000000ull;
            if (wait > limit)
            {
                std::this_thread::sleep_for(std::chrono::nanoseconds(limit));
                return false;
            }
            std::this_thread::sleep_for(std::chrono::nanoseconds(wait));
            return true;
        }

        // Moves the due time forward one frame, or re-bases it on now if the consumer fell more than a frame behind
        // so a slow tick is not followed by a burst.
        uint64_t advance(uint64_t due, uint32_t fps)
        {
            const uint64_t period = 1000000000ull / std::max<uint32_t>(fps, 1);
            const uint64_t now = monotonicNs();
            due += period;
            return due < now ? now : due;
        }
    }

    // ---------------------------------------------------------------- PatternSource

    PatternSource::PatternSource(FrameFormat f, uint32_t fps) : format_(f), fps_(fps) {}

    void PatternSource::render(const FrameFormat& f, uint64_t index, uint8_t* dst)
    {
        const size_t total = frameBytes(f);
        std::memset(dst, 0, total);
        const uint32_t w = f.width, h = f.height;
        const uint32_t squareX = w > 32 ? static_cast<uint32_t>((index * 8) % (w - 16)) : 0;
        const uint32_t squareY = h / 2 >= 8 ? h / 2 - 8 : 0;
        auto luma = [&](uint32_t x, uint32_t y) -> uint8_t
        {
            if (w > 32 && h >= 16 && x >= squareX && x < squareX + 16 && y >= squareY && y < squareY + 16) return 235;
            return static_cast<uint8_t>(w > 1 ? x * 255 / (w - 1) : 0);
        };
        if (f.fourcc == kFourccYuyv)
        {
            for (uint32_t y = 0; y < h; ++y)
            {
                uint8_t* row = dst + static_cast<size_t>(y) * f.stride;
                for (uint32_t x = 0; x < w; x += 2)
                {
                    row[x * 2 + 0] = luma(x, y);
                    row[x * 2 + 1] = 128;
                    row[x * 2 + 2] = luma(x + 1, y);
                    row[x * 2 + 3] = 128;
                }
            }
        }
        else if (f.fourcc == kFourccNv12)
        {
            for (uint32_t y = 0; y < h; ++y)
            {
                uint8_t* row = dst + static_cast<size_t>(y) * f.stride;
                for (uint32_t x = 0; x < w; ++x) row[x] = luma(x, y);
            }
            std::memset(dst + static_cast<size_t>(h) * f.stride, 128, static_cast<size_t>(h / 2) * f.stride);
        }
    }

    bool PatternSource::open(std::string& error)
    {
        if (frameBytes(format_) == 0)
        {
            error = "pattern: unsupported frame format";
            return false;
        }
        buffer_.assign(frameBytes(format_), 0);
        index_ = 0;
        nextDueNs_ = monotonicNs();
        opened_ = true;
        return true;
    }

    void PatternSource::close()
    {
        opened_ = false;
        buffer_.clear();
        buffer_.shrink_to_fit();
    }

    ReadResult PatternSource::next(FrameView& out, int timeoutMs, std::string& error)
    {
        if (!opened_)
        {
            error = "pattern: not open";
            return ReadResult::Error;
        }
        if (!sleepUntil(nextDueNs_, timeoutMs)) return ReadResult::Timeout;
        render(format_, index_++, buffer_.data());
        nextDueNs_ = advance(nextDueNs_, fps_);
        out.data = buffer_.data();
        out.size = buffer_.size();
        out.timestampNs = monotonicNs();
        return ReadResult::Frame;
    }

    // ---------------------------------------------------------------- FileSource

    FileSource::FileSource(std::string path, FrameFormat requested, uint32_t fps, bool loop)
        : path_(std::move(path)), format_(requested), fps_(fps), loop_(loop)
    {
    }

    FileSource::~FileSource() { close(); }

    namespace
    {
        bool readLine(std::FILE* f, std::string& line)
        {
            line.clear();
            int c;
            while ((c = std::fgetc(f)) != EOF)
            {
                if (c == '\n') return true;
                line.push_back(static_cast<char>(c));
                if (line.size() > 512) return false;
            }
            return false;
        }
    }

    bool FileSource::open(std::string& error)
    {
        close();
        file_ = std::fopen(path_.c_str(), "rb");
        if (file_ == nullptr)
        {
            error = "file: cannot open " + path_ + ": " + std::strerror(errno);
            return false;
        }
        char sig[9] = {};
        if (std::fread(sig, 1, 9, file_) == 9 && std::memcmp(sig, "YUV4MPEG2", 9) == 0)
        {
            std::rewind(file_);
            std::string header;
            if (!readLine(file_, header))
            {
                error = "file: unreadable Y4M header in " + path_;
                close();
                return false;
            }
            uint32_t w = 0, h = 0, fpsNum = 0, fpsDen = 1;
            std::string colour = "420";
            size_t pos = header.find(' ');
            while (pos != std::string::npos && pos < header.size())
            {
                const size_t end = header.find(' ', pos + 1);
                const std::string token = header.substr(pos + 1, end == std::string::npos ? std::string::npos : end - pos - 1);
                if (!token.empty())
                {
                    if (token[0] == 'W') w = static_cast<uint32_t>(std::strtoul(token.c_str() + 1, nullptr, 10));
                    else if (token[0] == 'H') h = static_cast<uint32_t>(std::strtoul(token.c_str() + 1, nullptr, 10));
                    else if (token[0] == 'C') colour = token.substr(1);
                    else if (token[0] == 'F')
                    {
                        const size_t colon = token.find(':');
                        if (colon != std::string::npos)
                        {
                            fpsNum = static_cast<uint32_t>(std::strtoul(token.c_str() + 1, nullptr, 10));
                            fpsDen = static_cast<uint32_t>(std::strtoul(token.c_str() + colon + 1, nullptr, 10));
                        }
                    }
                }
                pos = end;
            }
            if (colour.rfind("420", 0) == 0) y4m422_ = false;
            else if (colour == "422") y4m422_ = true;
            else
            {
                error = "file: Y4M colourspace C" + colour + " is not supported (use 420 or 422)";
                close();
                return false;
            }
            format_.width = w;
            format_.height = h;
            format_.fourcc = y4m422_ ? kFourccYuyv : kFourccNv12;
            format_.stride = minStride(format_.fourcc, w);
            if (frameBytes(format_) == 0)
            {
                error = "file: Y4M size " + std::to_string(w) + "x" + std::to_string(h) + " is not usable";
                close();
                return false;
            }
            if (fps_ == 0 && fpsNum != 0 && fpsDen != 0) fps_ = std::max<uint32_t>(1, fpsNum / fpsDen);
            y4m_ = true;
            dataStart_ = std::ftell(file_);
            scratch_.assign(static_cast<size_t>(w) * h * (y4m422_ ? 2 : 3) / (y4m422_ ? 1 : 2), 0);
        }
        else
        {
            y4m_ = false;
            if (frameBytes(format_) == 0)
            {
                error = "file: raw input needs a valid size and format (yuyv or nv12)";
                close();
                return false;
            }
            std::rewind(file_);
            dataStart_ = 0;
        }
        if (fps_ == 0) fps_ = 15;
        buffer_.assign(frameBytes(format_), 0);
        nextDueNs_ = monotonicNs();
        return true;
    }

    void FileSource::close()
    {
        if (file_ != nullptr) std::fclose(file_);
        file_ = nullptr;
        buffer_.clear();
        scratch_.clear();
    }

    bool FileSource::readFrame(std::string& error, bool& endOfFile)
    {
        endOfFile = false;
        if (!y4m_)
        {
            const size_t got = std::fread(buffer_.data(), 1, buffer_.size(), file_);
            if (got == buffer_.size()) return true;
            endOfFile = true;
            return false;
        }
        std::string line;
        if (!readLine(file_, line))
        {
            endOfFile = true;
            return false;
        }
        if (line.rfind("FRAME", 0) != 0)
        {
            error = "file: Y4M frame marker missing in " + path_;
            return false;
        }
        if (std::fread(scratch_.data(), 1, scratch_.size(), file_) != scratch_.size())
        {
            endOfFile = true;
            return false;
        }
        const uint32_t w = format_.width, h = format_.height;
        if (y4m422_)
        {
            const uint8_t* y = scratch_.data();
            const uint8_t* u = y + static_cast<size_t>(w) * h;
            const uint8_t* v = u + static_cast<size_t>(w / 2) * h;
            for (uint32_t row = 0; row < h; ++row)
            {
                uint8_t* dst = buffer_.data() + static_cast<size_t>(row) * format_.stride;
                for (uint32_t x = 0; x < w; x += 2)
                {
                    dst[x * 2 + 0] = y[static_cast<size_t>(row) * w + x];
                    dst[x * 2 + 1] = u[static_cast<size_t>(row) * (w / 2) + x / 2];
                    dst[x * 2 + 2] = y[static_cast<size_t>(row) * w + x + 1];
                    dst[x * 2 + 3] = v[static_cast<size_t>(row) * (w / 2) + x / 2];
                }
            }
        }
        else
        {
            const uint8_t* y = scratch_.data();
            const uint8_t* u = y + static_cast<size_t>(w) * h;
            const uint8_t* v = u + static_cast<size_t>(w / 2) * (h / 2);
            for (uint32_t row = 0; row < h; ++row)
                std::memcpy(buffer_.data() + static_cast<size_t>(row) * format_.stride, y + static_cast<size_t>(row) * w, w);
            uint8_t* uvPlane = buffer_.data() + static_cast<size_t>(h) * format_.stride;
            for (uint32_t row = 0; row < h / 2; ++row)
            {
                uint8_t* dst = uvPlane + static_cast<size_t>(row) * format_.stride;
                for (uint32_t x = 0; x < w / 2; ++x)
                {
                    dst[x * 2] = u[static_cast<size_t>(row) * (w / 2) + x];
                    dst[x * 2 + 1] = v[static_cast<size_t>(row) * (w / 2) + x];
                }
            }
        }
        return true;
    }

    ReadResult FileSource::next(FrameView& out, int timeoutMs, std::string& error)
    {
        if (file_ == nullptr)
        {
            error = "file: not open";
            return ReadResult::Error;
        }
        if (!sleepUntil(nextDueNs_, timeoutMs)) return ReadResult::Timeout;

        bool eof = false;
        bool ok = readFrame(error, eof);
        if (!ok && eof && loop_)
        {
            std::fseek(file_, dataStart_, SEEK_SET);
            ok = readFrame(error, eof);
            if (!ok && eof)
            {
                error = "file: " + path_ + " holds no complete frame";
                return ReadResult::Error;
            }
        }
        if (!ok) return eof ? ReadResult::EndOfStream : ReadResult::Error;

        nextDueNs_ = advance(nextDueNs_, fps_);
        out.data = buffer_.data();
        out.size = buffer_.size();
        out.timestampNs = monotonicNs();
        return ReadResult::Frame;
    }

    // ---------------------------------------------------------------- PipeSource

    PipeSource::PipeSource(std::string command, FrameFormat f, uint32_t fps) : command_(std::move(command)), format_(f), fps_(fps) {}

    PipeSource::~PipeSource() { close(); }

    bool PipeSource::open(std::string& error)
    {
        close();
        if (frameBytes(format_) == 0)
        {
            error = "pipe: needs a valid size and format (yuyv or nv12)";
            return false;
        }
        buffer_.assign(frameBytes(format_), 0);
        filled_ = 0;
        if (command_.empty())
        {
            fd_ = STDIN_FILENO;
            return true;
        }

        int fds[2];
        if (::pipe(fds) != 0)
        {
            error = std::string("pipe: pipe(): ") + std::strerror(errno);
            return false;
        }
        const pid_t child = ::fork();
        if (child < 0)
        {
            error = std::string("pipe: fork(): ") + std::strerror(errno);
            ::close(fds[0]);
            ::close(fds[1]);
            return false;
        }
        if (child == 0)
        {
            ::setpgid(0, 0);
            ::dup2(fds[1], STDOUT_FILENO);
            ::close(fds[0]);
            ::close(fds[1]);
            const int devNull = ::open("/dev/null", O_RDONLY);
            if (devNull >= 0) ::dup2(devNull, STDIN_FILENO);
            ::execl("/bin/sh", "sh", "-c", command_.c_str(), static_cast<char*>(nullptr));
            ::_exit(127);
        }
        ::setpgid(child, child);
        ::close(fds[1]);
        fd_ = fds[0];
        pid_ = child;
        return true;
    }

    void PipeSource::close()
    {
        if (pid_ > 0)
        {
            ::kill(-pid_, SIGTERM);
            if (fd_ >= 0) ::close(fd_);
            fd_ = -1;
            int status = 0;
            bool reaped = false;
            for (int i = 0; i < 100 && !reaped; ++i)
            {
                if (::waitpid(pid_, &status, WNOHANG) == pid_) reaped = true;
                else std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (!reaped)
            {
                ::kill(-pid_, SIGKILL);
                ::waitpid(pid_, &status, 0);
            }
            pid_ = -1;
        }
        else if (fd_ > STDIN_FILENO)
        {
            ::close(fd_);
        }
        fd_ = -1;
        filled_ = 0;
        buffer_.clear();
    }

    ReadResult PipeSource::next(FrameView& out, int timeoutMs, std::string& error)
    {
        if (fd_ < 0)
        {
            error = "pipe: not open";
            return ReadResult::Error;
        }
        struct pollfd p = {fd_, POLLIN, 0};
        const int rc = ::poll(&p, 1, timeoutMs);
        if (rc == 0) return ReadResult::Timeout;
        if (rc < 0)
        {
            if (errno == EINTR) return ReadResult::Timeout;
            error = std::string("pipe: poll(): ") + std::strerror(errno);
            return ReadResult::Error;
        }
        const ssize_t n = ::read(fd_, buffer_.data() + filled_, buffer_.size() - filled_);
        if (n == 0) return ReadResult::EndOfStream;
        if (n < 0)
        {
            if (errno == EINTR || errno == EAGAIN) return ReadResult::Timeout;
            error = std::string("pipe: read(): ") + std::strerror(errno);
            return ReadResult::Error;
        }
        filled_ += static_cast<size_t>(n);
        if (filled_ < buffer_.size()) return ReadResult::Timeout;
        filled_ = 0;
        out.data = buffer_.data();
        out.size = buffer_.size();
        out.timestampNs = monotonicNs();
        return ReadResult::Frame;
    }
}
