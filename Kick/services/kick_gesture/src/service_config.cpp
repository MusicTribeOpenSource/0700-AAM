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
#include "kick_gesture/service_config.h"
#include "kick_gesture/sink.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>

namespace kickgest
{
    namespace
    {
        std::string trim(const std::string& s)
        {
            size_t a = 0, b = s.size();
            while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
            while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
            return s.substr(a, b - a);
        }

        bool toDouble(const std::string& v, double lo, double hi, double& out)
        {
            char* end = nullptr;
            errno = 0;
            const double d = std::strtod(v.c_str(), &end);
            if (end == v.c_str() || *end != '\0' || errno != 0 || !std::isfinite(d) || d < lo || d > hi) return false;
            out = d;
            return true;
        }

        bool toBool(const std::string& v, bool& out)
        {
            if (v == "1" || v == "true" || v == "yes" || v == "on")
            {
                out = true;
                return true;
            }
            if (v == "0" || v == "false" || v == "no" || v == "off")
            {
                out = false;
                return true;
            }
            return false;
        }
    }

    bool isServiceFlag(const std::string& key) { return key == "events_stdout"; }

    bool applyServiceSetting(const std::string& rawKey, const std::string& value, ServiceConfig& c, std::string& error)
    {
        std::string key = rawKey;
        for (char& ch : key)
            if (ch == '-') ch = '_';
        double d = 0;
        auto bad = [&](const char* what) {
            error = "bad value \"" + value + "\" for " + key + " (" + what + ")";
            return false;
        };
        if (key == "shm")
        {
            if (value.size() < 2 || value[0] != '/' || value.find('/', 1) != std::string::npos) return bad("a POSIX shared memory name like /kick-camera");
            c.shm = value;
        }
        else if (key == "rate_hz")
        {
            if (!toDouble(value, 1.0, 30.0, d)) return bad("1 to 30");
            c.rateHz = d;
        }
        else if (key == "model_dir") c.modelDir = value;
        else if (key == "model_set")
        {
            if (value != "int8" && value != "fp32") return bad("int8 or fp32");
            c.modelSet = value;
        }
        else if (key == "threads")
        {
            if (!toDouble(value, 1, 4, d) || d != std::floor(d)) return bad("1 to 4");
            c.threads = static_cast<int>(d);
        }
        else if (key == "gesture_config") c.gestureConfig = value;
        else if (key == "events_stdout")
        {
            bool b = false;
            if (!toBool(value, b)) return bad("true or false");
            c.eventsStdout = b;
        }
        else if (key == "events_socket") c.eventsSocket = value;
        else if (key == "events_ipc")
        {
            std::string host;
            uint16_t port = 0;
            if (!value.empty() && !parseIpcEndpoint(value, host, port)) return bad("host:port with a dotted IPv4 host, for example 127.0.0.1:50501");
            c.eventsIpc = value;
        }
        else if (key == "events_file") c.eventsFile = value;
        else if (key == "record") c.record = value;
        else if (key == "status_interval_ms")
        {
            if (!toDouble(value, 500, 3600000, d) || d != std::floor(d)) return bad("500 to 3600000");
            c.statusIntervalMs = static_cast<uint32_t>(d);
        }
        else if (key == "cpu_soft")
        {
            if (!toDouble(value, 0.05, 4.0, d)) return bad("0.05 to 4");
            c.guard.cpuSoft = d;
        }
        else if (key == "cpu_hard")
        {
            if (!toDouble(value, 0.05, 4.0, d)) return bad("0.05 to 4");
            c.guard.cpuHard = d;
        }
        else if (key == "temp_soft_c")
        {
            if (!toDouble(value, 20, 150, d)) return bad("20 to 150");
            c.guard.tempSoftC = d;
        }
        else if (key == "temp_hard_c")
        {
            if (!toDouble(value, 20, 150, d)) return bad("20 to 150");
            c.guard.tempHardC = d;
        }
        else
        {
            error = "unknown setting \"" + rawKey + "\"";
            return false;
        }
        if (!(c.guard.cpuSoft < c.guard.cpuHard))
        {
            error = "cpu_soft must be below cpu_hard";
            return false;
        }
        if (!(c.guard.tempSoftC < c.guard.tempHardC))
        {
            error = "temp_soft_c must be below temp_hard_c";
            return false;
        }
        return true;
    }

    bool loadServiceConfigFile(const std::string& path, ServiceConfig& cfg, std::string& error)
    {
        std::ifstream in(path);
        if (!in)
        {
            error = "cannot read " + path;
            return false;
        }
        std::string line;
        int n = 0;
        while (std::getline(in, line))
        {
            ++n;
            const size_t hash = line.find('#');
            if (hash != std::string::npos) line.erase(hash);
            line = trim(line);
            if (line.empty()) continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                error = path + ":" + std::to_string(n) + ": expected key = value";
                return false;
            }
            std::string e;
            if (!applyServiceSetting(trim(line.substr(0, eq)), trim(line.substr(eq + 1)), cfg, e))
            {
                error = path + ":" + std::to_string(n) + ": " + e;
                return false;
            }
        }
        return true;
    }
}
