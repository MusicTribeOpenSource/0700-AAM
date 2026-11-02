/*
 * Copyright (c) 2026 FlowSphereAI
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

// Analog '83 "record" stage: what happens to a drum machine between the
// machine and the record. The drums play into a small room, then go through
// the mix bus and onto tape:
//   - Room: a short, dark stereo room/plate (hats and snare send more than
//     the kick, like a real mix)
//   - Tape EQ: the low-end "head bump", a clean-up high-pass and a softer top
//   - Wow and flutter: the slight, slow speed wobble of a tape machine
//   - Noise floor: a very quiet tape hiss
#pragma once

#include "Analog83Common.h"

#include <array>
#include <vector>

namespace a83 {

// Small stereo room: a compact version of Jezar's public-domain Freeverb
// (parallel damped combs into series all-passes, slightly different per side
// for width), with Freeverb's delay tuning. See CREDITS.md.
class Room
{
public:
    void prepare(double sampleRate);
    void reset();

    // Mono in, stereo wet out (written, not added).
    void process(float in, float& outL, float& outR);

private:
    struct Comb
    {
        std::vector<float> buffer;
        size_t index = 0;
        float feedback = 0.0f, damp = 0.0f, filtered = 0.0f;

        float process(float x)
        {
            const float out = buffer[index];
            filtered = out * (1.0f - damp) + filtered * damp;
            buffer[index] = x + filtered * feedback;
            if (++index >= buffer.size()) index = 0;
            return out;
        }
    };

    struct AllPass
    {
        std::vector<float> buffer;
        size_t index = 0;

        float process(float x)
        {
            const float delayed = buffer[index];
            buffer[index] = x + delayed * 0.5f;
            if (++index >= buffer.size()) index = 0;
            return delayed - x;
        }
    };

    static constexpr int kCombs = 4;
    static constexpr int kAllPasses = 2;
    std::array<Comb, kCombs> combsL, combsR;
    std::array<AllPass, kAllPasses> allPassL, allPassR;
    std::vector<float> preDelay;
    size_t preDelayIndex = 0;
    // The room hears about 250 Hz - 9 kHz of whatever is sent: no low end
    // (keeps the mix clean) and a slightly darkened top, like a real room.
    Biquad sendHighPass, sendLowPass;
};

// Tape speed wobble: a short delay line whose delay time drifts slowly
// (wow, about 0.5 Hz) and quickly (flutter, about 6 Hz).
class WowFlutter
{
public:
    void prepare(double sampleRate);
    void setDepth(float amount); // 0..1; 0.6 is a well-kept machine
    void process(float& left, float& right);

private:
    float read(const std::vector<float>& buffer, float delaySamples) const;

    double sampleRate = 48000.0;
    std::vector<float> bufferL, bufferR;
    size_t writeIndex = 0;
    float depth = 0.0f;
    SineLfo wowLfo, flutterLfo;
    float wowAmp = 0.0f, flutterAmp = 0.0f, baseDelay = 0.0f; // in samples, set in setDepth()
    // Slow random drift: glides smoothly to a new random point every ~1.7 s
    float driftFrom = 0.0f, driftTo = 0.0f, driftPos = 0.0f, driftInc = 0.0f;
    Noise noise { 0x7A9E5u };
};

// The whole print chain for the stereo kit bus.
class RecordChain
{
public:
    void prepare(double sampleRate);
    void setAmount(float amount); // 0..1 (RECORD knob)
    void process(float& left, float& right);

private:
    double sampleRate = 48000.0;
    float amount = 0.0f;
    float hissGain = 0.0f;
    std::array<Biquad, 2> headBump, cleanUp, topEnd;
    WowFlutter wow;
    Noise hissL { 0x51A5u }, hissR { 0x2B3Fu };
    std::array<OnePoleLowpass, 2> hissTone;
};

} // namespace a83
