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
#include "kick_camera/config.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace kickcam
{
    namespace
    {
        bool parseUint(const std::string& s, uint32_t& out)
        {
            if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos || s.size() > 9) return false;
            out = static_cast<uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
            return true;
        }

        bool parseSize(const std::string& s, uint32_t& w, uint32_t& h)
        {
            const size_t x = s.find('x');
            return x != std::string::npos && parseUint(s.substr(0, x), w) && parseUint(s.substr(x + 1), h);
        }

        bool parseBool(const std::string& s, bool& out)
        {
            if (s == "1" || s == "true" || s == "yes" || s == "on") { out = true; return true; }
            if (s == "0" || s == "false" || s == "no" || s == "off") { out = false; return true; }
            return false;
        }

        std::string trim(const std::string& s)
        {
            const size_t a = s.find_first_not_of(" \t\r");
            if (a == std::string::npos) return "";
            const size_t b = s.find_last_not_of(" \t\r");
            return s.substr(a, b - a + 1);
        }
    }

    bool applySetting(Config& cfg, const std::string& key, const std::string& value, std::string& error)
    {
        auto bad = [&](const char* what) { error = "bad value for " + key + ": " + what + " (" + value + ")"; return false; };
        if (key == "source")
        {
            if (value != "v4l2" && value != "file" && value != "pattern" && value != "pipe") return bad("expected v4l2, file, pattern or pipe");
            cfg.source = value;
        }
        else if (key == "device") cfg.device = value;
        else if (key == "file") cfg.file = value;
        else if (key == "pipe_command") cfg.pipeCommand = value;
        else if (key == "size") { if (!parseSize(value, cfg.width, cfg.height)) return bad("expected WIDTHxHEIGHT"); }
        else if (key == "max_size") { if (!parseSize(value, cfg.maxWidth, cfg.maxHeight)) return bad("expected WIDTHxHEIGHT"); }
        else if (key == "fps") { if (!parseUint(value, cfg.fps)) return bad("expected a number"); }
        else if (key == "format")
        {
            if (value == "auto") cfg.fourcc = 0;
            else if (!parseFormatName(value, cfg.fourcc)) return bad("expected auto, yuyv or nv12");
        }
        else if (key == "loop") { if (!parseBool(value, cfg.loop)) return bad("expected true or false"); }
        else if (key == "shm") cfg.shmName = value;
        else if (key == "socket") cfg.socketPath = value;
        else if (key == "preset") { if (!parsePreset(value, cfg.preset)) return bad("expected lit_room or dim_stage"); }
        else if (key == "debug_dump") cfg.debugDumpDir = value;
        else if (key == "debug_dump_max") { if (!parseUint(value, cfg.debugDumpMax)) return bad("expected a number"); }
        else if (key == "enable") { if (!parseBool(value, cfg.enableOnStart)) return bad("expected true or false"); }
        else
        {
            error = "unknown setting: " + key;
            return false;
        }
        return true;
    }

    bool loadConfigFile(const std::string& path, Config& cfg, std::string& error)
    {
        std::ifstream in(path);
        if (!in)
        {
            error = "cannot read config file " + path;
            return false;
        }
        std::string line;
        int lineNo = 0;
        while (std::getline(in, line))
        {
            ++lineNo;
            const size_t hash = line.find('#');
            if (hash != std::string::npos) line.erase(hash);
            line = trim(line);
            if (line.empty()) continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                error = path + ":" + std::to_string(lineNo) + ": expected key=value";
                return false;
            }
            std::string inner;
            if (!applySetting(cfg, trim(line.substr(0, eq)), trim(line.substr(eq + 1)), inner))
            {
                error = path + ":" + std::to_string(lineNo) + ": " + inner;
                return false;
            }
        }
        return true;
    }

    uint32_t ringCapacity(const Config& cfg)
    {
        return cfg.maxWidth * cfg.maxHeight * 2; // YUYV is the larger of the two formats
    }

    bool validate(const Config& cfg, std::string& error)
    {
        if (cfg.maxWidth == 0 || cfg.maxHeight == 0 || cfg.maxWidth > 1920 || cfg.maxHeight > 1080)
        {
            error = "max_size must be between 1x1 and 1920x1080";
            return false;
        }
        if (cfg.width == 0 || cfg.height == 0 || cfg.width > cfg.maxWidth || cfg.height > cfg.maxHeight)
        {
            error = "size " + std::to_string(cfg.width) + "x" + std::to_string(cfg.height) + " is outside max_size";
            return false;
        }
        if ((cfg.width & 1u) != 0 || (cfg.height & 1u) != 0)
        {
            error = "size must be even in both directions";
            return false;
        }
        if (cfg.fps == 0 || cfg.fps > 60)
        {
            error = "fps must be between 1 and 60";
            return false;
        }
        if (cfg.source == "file" && cfg.file.empty())
        {
            error = "source=file needs file=PATH";
            return false;
        }
        if (cfg.shmName.empty() || cfg.shmName[0] != '/' || cfg.shmName.find('/', 1) != std::string::npos)
        {
            error = "shm must look like /name";
            return false;
        }
        if (cfg.socketPath.empty() || cfg.socketPath.size() >= 104)
        {
            error = "socket path is empty or longer than a socket address allows";
            return false;
        }
        return true;
    }
}
