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
#include "kick_gesture/guard.h"

#include <dirent.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace kickgest
{
    const char* guardStateName(GuardState s)
    {
        switch (s)
        {
            case GuardState::Normal: return "running";
            case GuardState::Degraded: return "degraded";
            default: return "disabled";
        }
    }

    double ResourceGuard::cpuFraction(uint64_t nowMs, double cpuSeconds)
    {
        samples_[sampleHead_] = {nowMs, cpuSeconds};
        sampleHead_ = (sampleHead_ + 1) % kMaxSamples;
        if (sampleCount_ < kMaxSamples) ++sampleCount_;
        // Oldest sample that is still inside the window.
        const Sample* oldest = nullptr;
        for (int i = 0; i < sampleCount_; ++i)
        {
            const Sample& s = samples_[(sampleHead_ - 1 - i + 2 * kMaxSamples) % kMaxSamples];
            if (nowMs - s.ms > lim_.windowMs) break;
            oldest = &s;
        }
        if (oldest == nullptr || nowMs - oldest->ms < 1000) return 0.0;
        const double dt = static_cast<double>(nowMs - oldest->ms) / 1000.0;
        const double used = cpuSeconds - oldest->cpuSeconds;
        return used > 0.0 ? used / dt : 0.0;
    }

    GuardStatus ResourceGuard::update(uint64_t nowMs, double cpuSeconds, std::optional<double> tempC)
    {
        GuardStatus out;
        out.cpu = cpuFraction(nowMs, cpuSeconds);
        out.tempC = tempC;
        const double temp = tempC.value_or(-1000.0);
        const bool haveTemp = tempC.has_value();

        const bool tempHard = haveTemp && temp >= lim_.tempHardC;
        const bool cpuHard = out.cpu > lim_.cpuHard;
        if (cpuHard)
        {
            if (!hardOver_)
            {
                hardOver_ = true;
                hardSince_ = nowMs;
            }
        }
        else
            hardOver_ = false;
        const bool cpuHardHeld = hardOver_ && nowMs - hardSince_ >= lim_.hardHoldMs;

        const bool anySoft = out.cpu > lim_.cpuSoft || (haveTemp && temp > lim_.tempSoftC);
        const bool allCool = out.cpu < lim_.cpuSoft - lim_.cpuRecoverMargin && (!haveTemp || temp < lim_.tempSoftC - lim_.tempRecoverMarginC);

        if (state_ != GuardState::Disabled && (tempHard || cpuHardHeld))
        {
            state_ = GuardState::Disabled;
            disabledAt_ = nowMs;
            recovering_ = false;
            hardOver_ = false;
            out.reason = tempHard ? "temp_hard" : "cpu_hard";
        }
        else if (state_ == GuardState::Disabled)
        {
            const bool cooled = !haveTemp || temp < lim_.tempSoftC - lim_.tempRecoverMarginC;
            if (nowMs - disabledAt_ >= lim_.disabledHoldMs && cooled)
            {
                state_ = GuardState::Degraded;
                recovering_ = false;
                out.reason = "recovering";
            }
            else
                out.reason = cooled ? "cooling" : "temp_hard";
        }
        else if (state_ == GuardState::Normal)
        {
            if (anySoft)
            {
                state_ = GuardState::Degraded;
                recovering_ = false;
                out.reason = (haveTemp && temp > lim_.tempSoftC) ? "temp_soft" : "cpu_soft";
            }
        }
        else // Degraded
        {
            if (allCool)
            {
                if (!recovering_)
                {
                    recovering_ = true;
                    recoverSince_ = nowMs;
                }
                if (nowMs - recoverSince_ >= lim_.recoverMs)
                {
                    state_ = GuardState::Normal;
                    recovering_ = false;
                }
                else
                    out.reason = "recovering";
            }
            else
            {
                recovering_ = false;
                out.reason = (haveTemp && temp > lim_.tempSoftC) ? "temp_soft" : "cpu_soft";
            }
        }
        out.state = state_;
        out.rateDivisor = state_ == GuardState::Normal ? 1u : (state_ == GuardState::Degraded ? 2u : 0u);
        return out;
    }

    std::optional<double> parseProcStatCpuSeconds(const std::string& text, long ticksPerSecond)
    {
        if (ticksPerSecond <= 0) return std::nullopt;
        const size_t close = text.rfind(')');
        if (close == std::string::npos) return std::nullopt;
        // After ") " the fields are: state(3) ppid(4) pgrp(5) session(6) tty(7) tpgid(8) flags(9) minflt(10) cminflt(11)
        // majflt(12) cmajflt(13) utime(14) stime(15)
        std::istringstream in(text.substr(close + 1));
        std::string field;
        long long utime = 0, stime = 0;
        for (int i = 3; i <= 15; ++i)
        {
            if (!(in >> field)) return std::nullopt;
            if (i == 14 || i == 15)
            {
                char* end = nullptr;
                const long long v = std::strtoll(field.c_str(), &end, 10);
                if (end == field.c_str() || *end != '\0' || v < 0) return std::nullopt;
                (i == 14 ? utime : stime) = v;
            }
        }
        return static_cast<double>(utime + stime) / static_cast<double>(ticksPerSecond);
    }

    double readProcessCpuSeconds()
    {
#if defined(__linux__)
        std::ifstream f("/proc/self/stat");
        if (f)
        {
            std::stringstream ss;
            ss << f.rdbuf();
            const auto v = parseProcStatCpuSeconds(ss.str(), ::sysconf(_SC_CLK_TCK));
            if (v) return *v;
        }
#endif
        rusage ru{};
        if (::getrusage(RUSAGE_SELF, &ru) != 0) return 0.0;
        return static_cast<double>(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) + static_cast<double>(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1e6;
    }

    std::optional<double> readMaxThermalC(const std::string& dir)
    {
        DIR* d = ::opendir(dir.c_str());
        if (d == nullptr) return std::nullopt;
        std::optional<double> best;
        while (const dirent* e = ::readdir(d))
        {
            if (std::strncmp(e->d_name, "thermal_zone", 12) != 0) continue;
            std::ifstream f(dir + "/" + e->d_name + "/temp");
            long long milli = 0;
            if (!(f >> milli)) continue;
            const double c = static_cast<double>(milli) / 1000.0;
            if (c < -40.0 || c > 150.0) continue;
            if (!best || c > *best) best = c;
        }
        ::closedir(d);
        return best;
    }
}
