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
#include "kick_camera/service.h"

#include "kick_camera/sources.h"
#include "kick_camera/v4l2_source.h"

#include <cstdio>
#include <sstream>
#include <sys/stat.h>

namespace kickcam
{
    namespace
    {
        void logLine(const std::string& s) { std::fprintf(stderr, "kick-camera: %s\n", s.c_str()); }

        void logUnsupported(const char* source, const ControlReport& report)
        {
            if (report.unsupported.empty()) return;
            std::string list;
            for (const auto& u : report.unsupported) list += (list.empty() ? "" : ", ") + u;
            logLine(std::string("controls not supported by ") + source + ": " + list);
        }

        std::string jsonEscape(const std::string& s)
        {
            std::string out;
            for (const char c : s)
            {
                if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
                else if (static_cast<unsigned char>(c) < 0x20) out.push_back(' ');
                else out.push_back(c);
            }
            return out;
        }

        FrameFormat requestedFormat(const Config& cfg)
        {
            // YUYV is the default for the backends that pick a format themselves; the stride is only a hint for them.
            FrameFormat f;
            f.width = cfg.width;
            f.height = cfg.height;
            f.fourcc = cfg.fourcc;
            f.stride = cfg.fourcc != 0 ? minStride(cfg.fourcc, cfg.width) : 0;
            return f;
        }
    }

    std::unique_ptr<FrameSource> makeSource(const Config& cfg, std::string& error)
    {
        FrameFormat f = requestedFormat(cfg);
        if (cfg.source == "v4l2")
        {
#ifdef __linux__
            return std::make_unique<V4L2Source>(cfg.device, f, cfg.fps);
#else
            error = "the v4l2 source only exists on Linux; use pattern, file or pipe here";
            return nullptr;
#endif
        }
        // The synthetic and file backends need a concrete format. Default to YUYV.
        if (f.fourcc == 0) f.fourcc = kFourccYuyv;
        f.stride = minStride(f.fourcc, f.width);
        if (cfg.source == "pattern") return std::make_unique<PatternSource>(f, cfg.fps);
        if (cfg.source == "file") return std::make_unique<FileSource>(cfg.file, f, cfg.fps, cfg.loop);
        if (cfg.source == "pipe") return std::make_unique<PipeSource>(cfg.pipeCommand, f, cfg.fps);
        error = "unknown source " + cfg.source;
        return nullptr;
    }

    CameraService::CameraService(Config cfg, SourceFactory factory) : cfg_(std::move(cfg)), factory_(std::move(factory))
    {
        if (!factory_)
        {
            factory_ = [](const Config& c) -> std::unique_ptr<FrameSource>
            {
                std::string error;
                auto s = makeSource(c, error);
                if (!s) logLine(error);
                return s;
            };
        }
        controls_.preset = cfg_.preset;
    }

    CameraService::~CameraService() { stop(); }

    bool CameraService::start(std::string& error)
    {
        std::string problem;
        if (!validate(cfg_, problem))
        {
            error = problem;
            return false;
        }
        const uint32_t capacity = ringCapacity(cfg_);
        if (!region_.create(cfg_.shmName, ringBytes(capacity), error)) return false;
        if (!ring_.init(region_.data(), region_.size(), capacity))
        {
            error = "ring does not fit its shared-memory block";
            region_.close();
            return false;
        }
        if (!server_.start(cfg_.socketPath, [this](const std::string& line) { return handleCommand(line); }, error))
        {
            region_.close();
            return false;
        }
        started_ = true;
        return true;
    }

    void CameraService::stop()
    {
        if (!started_) return;
        server_.stop(); // no more commands, so nothing races the shutdown below
        disable();
        region_.close();
        started_ = false;
    }

    bool CameraService::enable(std::string& error)
    {
        std::lock_guard<std::mutex> life(lifecycle_);
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (state_ == "streaming") return true;
        }
        if (capture_.joinable()) capture_.join(); // a thread that ended on its own (end of stream, error)

        std::unique_ptr<FrameSource> source = factory_(cfg_);
        if (!source)
        {
            error = "no frame source";
            std::lock_guard<std::mutex> lock(mu_);
            setStateLocked("error", error);
            return false;
        }
        // Raise the indicator before the device opens and lower it after the device closes, so it can never read
        // "off" while the hardware is on.
        ring_.setCapturing(true);
        std::string problem;
        if (!source->open(problem))
        {
            source->close();
            ring_.setCapturing(false);
            error = problem;
            std::lock_guard<std::mutex> lock(mu_);
            setStateLocked("error", error);
            return false;
        }
        const FrameFormat fmt = source->format();
        if (fmt.width > cfg_.maxWidth || fmt.height > cfg_.maxHeight || !ring_.setFormat(fmt))
        {
            source->close();
            ring_.setCapturing(false);
            error = "source produces " + std::to_string(fmt.width) + "x" + std::to_string(fmt.height) + " " + formatName(fmt.fourcc) +
                    ", which does not fit max_size " + std::to_string(cfg_.maxWidth) + "x" + std::to_string(cfg_.maxHeight);
            std::lock_guard<std::mutex> lock(mu_);
            setStateLocked("error", error);
            return false;
        }

        FrameSource* raw = source.get();
        {
            std::lock_guard<std::mutex> lock(mu_);
            source_ = std::move(source);
            format_ = fmt;
            fps_ = raw->fps();
            frames_ = 0;
            lastFrameNs_ = 0;
            dropped_ = 0;
            dumped_ = 0;
            const ControlReport report = applyPreset(*raw, controls_.preset, fps_, controls_);
            logUnsupported(raw->name(), report);
            setStateLocked("streaming", "");
        }
        if (!cfg_.debugDumpDir.empty())
            logLine("debug dump is ON: up to " + std::to_string(cfg_.debugDumpMax) + " frames are written to " + cfg_.debugDumpDir);
        stopCapture_ = false;
        capture_ = std::thread([this, raw] { captureLoop(raw); });
        return true;
    }

    void CameraService::disable()
    {
        std::lock_guard<std::mutex> life(lifecycle_);
        stopCapture_ = true;
        if (capture_.joinable()) capture_.join();
        std::lock_guard<std::mutex> lock(mu_);
        if (source_)
        {
            source_->close();
            source_.reset();
        }
        ring_.clear();
        controls_.exposureLocked = false;
        controls_.wbLocked = false;
        setStateLocked("off", "");
    }

    void CameraService::setStateLocked(const char* state, const std::string& message)
    {
        state_ = state;
        error_ = message;
    }

    void CameraService::finishFromCaptureThread(const char* state, const std::string& message)
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (source_)
        {
            source_->close();
            source_.reset();
        }
        ring_.clear();
        controls_.exposureLocked = false;
        controls_.wbLocked = false;
        setStateLocked(state, message);
        logLine(std::string("capture stopped: ") + state + (message.empty() ? "" : " (" + message + ")"));
    }

    void CameraService::dumpFrame(const FrameView& v)
    {
        if (cfg_.debugDumpDir.empty() || dumped_ >= cfg_.debugDumpMax) return;
        char name[64];
        std::snprintf(name, sizeof(name), "/kick-camera-%06u.yuv", static_cast<unsigned>(dumped_.load()));
        std::FILE* f = std::fopen((cfg_.debugDumpDir + name).c_str(), "wb");
        if (f == nullptr)
        {
            if (!dumpWarned_) logLine("debug dump: cannot write to " + cfg_.debugDumpDir);
            dumpWarned_ = true;
            dumped_ = cfg_.debugDumpMax; // stop trying
            return;
        }
        std::fwrite(v.data, 1, v.size, f);
        std::fclose(f);
        ++dumped_;
    }

    void CameraService::captureLoop(FrameSource* source)
    {
        FrameView view;
        std::string error;
        while (!stopCapture_)
        {
            const ReadResult r = source->next(view, 200, error);
            if (stopCapture_) return;
            if (r == ReadResult::Timeout) continue;
            if (r == ReadResult::EndOfStream)
            {
                finishFromCaptureThread("ended", "end of stream");
                return;
            }
            if (r == ReadResult::Error)
            {
                finishFromCaptureThread("error", error);
                return;
            }
            if (!ring_.publish(view.data, view.size, view.timestampNs))
            {
                finishFromCaptureThread("error", "source delivered a frame of the wrong size");
                return;
            }
            ++frames_;
            lastFrameNs_ = monotonicNs();
            dropped_ = source->droppedFrames();
            dumpFrame(view);
        }
    }

    std::string CameraService::statusJson()
    {
        std::lock_guard<std::mutex> lock(mu_);
        const bool streaming = state_ == "streaming";
        const uint64_t last = lastFrameNs_;
        std::ostringstream o;
        o << "{\"state\":\"" << state_ << "\""
          << ",\"capturing\":" << (streaming ? "true" : "false")
          << ",\"source\":\"" << jsonEscape(cfg_.source) << "\""
          << ",\"preset\":\"" << presetName(controls_.preset) << "\""
          << ",\"exposure_locked\":" << (controls_.exposureLocked ? "true" : "false")
          << ",\"wb_locked\":" << (controls_.wbLocked ? "true" : "false")
          << ",\"debug_dump\":" << (cfg_.debugDumpDir.empty() ? "false" : "true");
        if (streaming)
        {
            o << ",\"width\":" << format_.width << ",\"height\":" << format_.height << ",\"stride\":" << format_.stride
              << ",\"format\":\"" << formatName(format_.fourcc) << "\",\"fps\":" << fps_ << ",\"frames\":" << frames_.load()
              << ",\"dropped\":" << dropped_.load() << ",\"frame_age_ms\":";
            if (last == 0) o << "null";
            else o << (monotonicNs() - last) / 1000000ull;
        }
        o << ",\"error\":\"" << jsonEscape(error_) << "\"}";
        return o.str();
    }

    std::string CameraService::handleCommand(const std::string& line)
    {
        std::istringstream in(line);
        std::string verb, arg, extra;
        in >> verb >> arg >> extra;
        std::string error;

        if (verb == "status" && arg.empty()) return statusJson();
        if (verb == "enable" && arg.empty()) return enable(error) ? "ok" : "err " + error;
        if (verb == "disable" && arg.empty())
        {
            disable();
            return "ok";
        }
        if (verb == "preset" && !arg.empty() && extra.empty())
        {
            Preset p;
            if (!parsePreset(arg, p)) return "err unknown preset " + arg + " (lit_room or dim_stage)";
            std::lock_guard<std::mutex> lock(mu_);
            if (source_)
            {
                const ControlReport report = applyPreset(*source_, p, fps_, controls_);
                logUnsupported(source_->name(), report);
            }
            controls_.preset = p;
            return "ok";
        }
        if ((verb == "lock_exposure" || verb == "unlock_exposure" || verb == "lock_wb" || verb == "unlock_wb") && arg.empty())
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (!source_ || state_ != "streaming") return "err camera is off";
            bool ok;
            if (verb == "lock_exposure") ok = lockExposure(*source_, controls_, error);
            else if (verb == "unlock_exposure") ok = unlockExposure(*source_, fps_, controls_, error);
            else if (verb == "lock_wb") ok = lockWhiteBalance(*source_, controls_, error);
            else ok = unlockWhiteBalance(*source_, controls_, error);
            return ok ? "ok" : "err " + error;
        }
        return "err unknown command";
    }
}
