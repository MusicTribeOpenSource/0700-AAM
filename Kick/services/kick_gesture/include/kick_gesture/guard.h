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

#include <cstdint>
#include <optional>
#include <string>

// CPU and temperature guard. The gesture service shares four cores with the audio engine, so when it uses too
// much CPU or the board runs hot it slows down first and switches itself off if that is not enough. It never
// waits for the audio engine to complain.
//
//   Normal   -> Degraded  CPU above cpu_soft (averaged over the window) or temperature above temp_soft
//   Degraded -> Normal    CPU and temperature below their soft limits minus a margin for recover_ms
//   any      -> Disabled  temperature at or above temp_hard, or CPU above cpu_hard for hard_hold_ms
//   Disabled -> Degraded  after disabled_hold_ms, once the temperature is below temp_soft minus the margin
//
// While Degraded the service runs at half rate. While Disabled it does no inference and keeps no camera
// interest; it reports its state and tries again by itself.
namespace kickgest
{
    struct GuardLimits
    {
        double cpuSoft = 0.60; // fraction of one core, averaged over windowMs
        double cpuHard = 0.90;
        double tempSoftC = 75.0;
        double tempHardC = 85.0;
        uint32_t windowMs = 5000;
        uint32_t hardHoldMs = 5000;      // CPU has to stay above cpuHard this long before the service disables itself
        uint32_t recoverMs = 30000;
        uint32_t disabledHoldMs = 60000;
        double cpuRecoverMargin = 0.10;
        double tempRecoverMarginC = 5.0;
    };

    enum class GuardState
    {
        Normal,
        Degraded,
        Disabled
    };
    const char* guardStateName(GuardState s);

    struct GuardStatus
    {
        GuardState state = GuardState::Normal;
        uint32_t rateDivisor = 1; // 1 normal, 2 degraded, 0 disabled
        double cpu = 0.0;         // fraction of one core over the window, 0 until a second of data exists
        std::optional<double> tempC;
        const char* reason = "";  // why the state is what it is: "", cpu_soft, cpu_hard, temp_soft, temp_hard, recovering, cooling
    };

    class ResourceGuard
    {
    public:
        explicit ResourceGuard(const GuardLimits& limits) : lim_(limits) {}

        // nowMs is a monotonic millisecond clock, cpuSeconds the cumulative CPU time of this process, tempC the
        // hottest thermal zone when known. Call about once a second.
        GuardStatus update(uint64_t nowMs, double cpuSeconds, std::optional<double> tempC);

        GuardState state() const { return state_; }

    private:
        struct Sample
        {
            uint64_t ms;
            double cpuSeconds;
        };
        double cpuFraction(uint64_t nowMs, double cpuSeconds);

        GuardLimits lim_;
        GuardState state_ = GuardState::Normal;
        static constexpr int kMaxSamples = 64;
        Sample samples_[kMaxSamples] = {};
        int sampleCount_ = 0;
        int sampleHead_ = 0;
        bool hardOver_ = false;
        uint64_t hardSince_ = 0;
        bool recovering_ = false;
        uint64_t recoverSince_ = 0;
        uint64_t disabledAt_ = 0;
    };

    // Where the readings come from. The service takes this so tests can feed it numbers.
    class IResourceReader
    {
    public:
        virtual ~IResourceReader() = default;
        virtual double cpuSeconds() = 0;
        virtual std::optional<double> tempC() = 0;
    };

    // Cumulative user plus system CPU seconds from the text of /proc/self/stat. nullopt when the text is malformed.
    // The command name is in parentheses and may contain spaces and parentheses, so fields are counted from the last ')'.
    std::optional<double> parseProcStatCpuSeconds(const std::string& text, long ticksPerSecond);

    // Whole-process CPU seconds: /proc/self/stat on Linux, getrusage elsewhere.
    double readProcessCpuSeconds();

    // The hottest temperature under <dir>/thermal_zone*/temp (millidegrees Celsius), or nullopt when there is none.
    // Readings outside -40 to 150 degrees are ignored as garbage.
    std::optional<double> readMaxThermalC(const std::string& dir = "/sys/class/thermal");

    class SystemResourceReader final : public IResourceReader
    {
    public:
        explicit SystemResourceReader(std::string thermalDir = "/sys/class/thermal") : thermalDir_(std::move(thermalDir)) {}
        double cpuSeconds() override { return readProcessCpuSeconds(); }
        std::optional<double> tempC() override { return readMaxThermalC(thermalDir_); }

    private:
        std::string thermalDir_;
    };
}
