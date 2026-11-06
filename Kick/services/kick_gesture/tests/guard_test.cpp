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

#include "test_util.h"

#include <gtest/gtest.h>

#include <fstream>

using namespace kickgest;

namespace
{
    // Drives the guard once a second with a fixed CPU load (fraction of one core) and temperature.
    struct Rig
    {
        GuardLimits lim;
        ResourceGuard guard{lim};
        uint64_t ms = 100000;
        double cpuSeconds = 10.0;
        GuardStatus last;

        explicit Rig(const GuardLimits& l = GuardLimits()) : lim(l), guard(l) {}

        GuardStatus run(int seconds, double load, std::optional<double> temp = std::nullopt)
        {
            for (int i = 0; i < seconds; ++i)
            {
                ms += 1000;
                cpuSeconds += load;
                last = guard.update(ms, cpuSeconds, temp);
            }
            return last;
        }
    };
}

TEST(Guard, StaysNormalUnderTheSoftLimit)
{
    Rig r;
    const GuardStatus s = r.run(60, 0.3, 55.0);
    EXPECT_EQ(s.state, GuardState::Normal);
    EXPECT_EQ(s.rateDivisor, 1u);
    EXPECT_NEAR(s.cpu, 0.3, 0.02);
}

TEST(Guard, CpuAboveSoftHalvesTheRate)
{
    Rig r;
    r.run(10, 0.3);
    const GuardStatus s = r.run(8, 0.7);
    EXPECT_EQ(s.state, GuardState::Degraded);
    EXPECT_EQ(s.rateDivisor, 2u);
    EXPECT_STREQ(s.reason, "cpu_soft");
}

TEST(Guard, TemperatureAboveSoftHalvesTheRate)
{
    Rig r;
    const GuardStatus s = r.run(3, 0.2, 78.0);
    EXPECT_EQ(s.state, GuardState::Degraded);
    EXPECT_STREQ(s.reason, "temp_soft");
}

TEST(Guard, SustainedHardCpuDisablesButABlipDoesNot)
{
    Rig blip;
    blip.run(10, 0.3);
    blip.run(2, 1.5); // two seconds over the hard limit
    EXPECT_NE(blip.run(1, 0.3).state, GuardState::Disabled);

    Rig sustained;
    sustained.run(10, 0.3);
    // The first update that reports Disabled carries the reason.
    GuardStatus s;
    for (int i = 0; i < 20 && s.state != GuardState::Disabled; ++i) s = sustained.run(1, 1.2);
    EXPECT_EQ(s.state, GuardState::Disabled);
    EXPECT_EQ(s.rateDivisor, 0u);
    EXPECT_STREQ(s.reason, "cpu_hard");
}

TEST(Guard, HardTemperatureDisablesAtOnce)
{
    Rig r;
    r.run(5, 0.2, 50.0);
    const GuardStatus s = r.run(1, 0.2, 86.0);
    EXPECT_EQ(s.state, GuardState::Disabled);
    EXPECT_STREQ(s.reason, "temp_hard");
}

TEST(Guard, DisabledStaysOffUntilTheHoldPassesAndTheBoardCools)
{
    Rig r;
    r.run(5, 0.2, 86.0);
    ASSERT_EQ(r.last.state, GuardState::Disabled);
    // Cool again, but the hold (60 s) has not passed.
    EXPECT_EQ(r.run(30, 0.0, 60.0).state, GuardState::Disabled);
    // Hold passed but still warm: stays off.
    EXPECT_EQ(r.run(40, 0.0, 74.0).state, GuardState::Disabled);
    // Cool enough: comes back, degraded first.
    const GuardStatus s = r.run(2, 0.0, 60.0);
    EXPECT_EQ(s.state, GuardState::Degraded);
    // And to normal after recover_ms below the soft limits.
    EXPECT_EQ(r.run(40, 0.2, 60.0).state, GuardState::Normal);
}

TEST(Guard, RecoveryNeedsMarginAndTime)
{
    Rig r;
    r.run(8, 0.8);
    ASSERT_EQ(r.last.state, GuardState::Degraded);
    // Just below the soft limit is not enough: the margin is 0.10.
    EXPECT_EQ(r.run(60, 0.55).state, GuardState::Degraded);
    // Well below for recover_ms (30 s).
    EXPECT_EQ(r.run(20, 0.2).state, GuardState::Degraded);
    EXPECT_EQ(r.run(20, 0.2).state, GuardState::Normal);
}

TEST(Guard, NoTemperatureReadingIsNotAProblem)
{
    Rig r;
    EXPECT_EQ(r.run(60, 0.2, std::nullopt).state, GuardState::Normal);
    EXPECT_FALSE(r.last.tempC.has_value());
}

TEST(Guard, FirstSecondReportsZeroCpu)
{
    GuardLimits l;
    ResourceGuard g(l);
    const GuardStatus s = g.update(1000, 5.0, std::nullopt);
    EXPECT_EQ(s.cpu, 0.0);
}

TEST(ProcStat, ParsesUserPlusSystemTicks)
{
    // pid (comm) state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime ...
    const std::string line = "1234 (kick-gesture) S 1 1234 1234 0 -1 4194560 100 0 0 0 250 50 0 0 20 0 4 0 12345 1000000 500 ...";
    const auto v = parseProcStatCpuSeconds(line, 100);
    ASSERT_TRUE(v.has_value());
    EXPECT_DOUBLE_EQ(*v, 3.0);
}

TEST(ProcStat, CommandNamesWithSpacesAndParenthesesAreHandled)
{
    const std::string line = "1234 (evil) name) R 1 1 1 0 -1 0 0 0 0 0 100 100 0 0";
    const auto v = parseProcStatCpuSeconds(line, 100);
    ASSERT_TRUE(v.has_value());
    EXPECT_DOUBLE_EQ(*v, 2.0);
}

TEST(ProcStat, RejectsMalformedText)
{
    EXPECT_FALSE(parseProcStatCpuSeconds("", 100).has_value());
    EXPECT_FALSE(parseProcStatCpuSeconds("1234 no parens here", 100).has_value());
    EXPECT_FALSE(parseProcStatCpuSeconds("1 (x) S 1 1 1 0 -1 0 0 0 0 0 abc 5", 100).has_value());
    EXPECT_FALSE(parseProcStatCpuSeconds("1 (x) S 1 1 1", 100).has_value());
    EXPECT_FALSE(parseProcStatCpuSeconds("1 (x) S 1 1 1 0 -1 0 0 0 0 0 1 1", 0).has_value());
}

TEST(ProcessCpu, IsMonotonicAndGrowsWithWork)
{
    const double a = readProcessCpuSeconds();
    volatile double sink = 0;
    for (long i = 0; i < 30000000; ++i) sink += static_cast<double>(i) * 1e-9;
    const double b = readProcessCpuSeconds();
    EXPECT_GE(b, a);
    EXPECT_GT(b, 0.0);
}

TEST(Thermal, ReadsTheHottestZoneAndIgnoresGarbage)
{
    kicktest::TempDir dir;
    auto zone = [&](const std::string& name, const std::string& content) {
        ::mkdir((dir.path() + "/" + name).c_str(), 0700);
        std::ofstream(dir.path() + "/" + name + "/temp") << content;
    };
    EXPECT_FALSE(readMaxThermalC(dir.path()).has_value());
    zone("thermal_zone0", "48000\n");
    zone("thermal_zone1", "61500\n");
    zone("thermal_zone2", "not a number\n");
    zone("thermal_zone3", "999000\n"); // 999 C: a broken sensor
    zone("cooling_device0", "99000\n"); // not a thermal zone
    const auto t = readMaxThermalC(dir.path());
    ASSERT_TRUE(t.has_value());
    EXPECT_DOUBLE_EQ(*t, 61.5);
    EXPECT_FALSE(readMaxThermalC("/nonexistent-dir-for-kick-gesture").has_value());
    // The zone directories are left behind for TempDir to unlink non-recursively, so clean them here.
    for (const char* z : {"thermal_zone0", "thermal_zone1", "thermal_zone2", "thermal_zone3", "cooling_device0"})
    {
        ::unlink((dir.path() + "/" + z + "/temp").c_str());
        ::rmdir((dir.path() + "/" + z).c_str());
    }
}
