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

// Analog '83 hi-hat voice (closed and open).
//
// Six detuned square-wave oscillators at inharmonic ratios make a metallic
// tone. It's band-passed, driven through a tube/tape-style saturator (like
// an analog VCA pushed a little hard), shaped by a decay envelope with a
// rounded-off tail, then high-passed. Closed and open hats share ONE voice, so a closed hit
// automatically chokes a ringing open hat, like the classic machines.
#pragma once

#include "Analog83Common.h"

namespace a83 {

class HiHat
{
public:
    void prepare(double sampleRate);

    void setTune(float semitones);        // -12..+12
    void setClosedDecay(float seconds);   // 0.01..0.3
    void setOpenDecay(float seconds);     // 0.1..2.0
    void setTone(float amount);           // 0..1, darker to brighter
    void setDrive(float amount);          // 0..1, saturation (0.18 = warm)
    void setLevel(float dB);              // -60..+6

    // pitchSemitones shifts this hit from the TUNE setting (for chromatic
    // playing); clamped to +/-24.
    void triggerClosed(float velocity, float pitchSemitones = 0.0f);
    void triggerOpen(float velocity, float pitchSemitones = 0.0f);

    // ADDS the voice into `out` (does not clear it).
    void process(float* out, int numSamples);

    bool isActive() const { return active; }

private:
    static constexpr int kNumOscillators = 6;

    void updateOscillators();
    void updateFilters();
    void start(float velocity, float decaySeconds, float pitchSemitones);

    double sampleRate = 48000.0;

    float tuneSemitones = 0.0f;
    float closedDecay = 0.06f;
    float openDecay = 0.45f;
    float tone = 0.5f;
    float levelGain = 1.0f;     // smoothed, follows levelTarget (no zipper noise)
    float levelTarget = 1.0f;
    float levelSmoothing = 0.0f;

    float phases[kNumOscillators] = {};
    float increments[kNumOscillators] = {};
    float hitSemitones = 0.0f; // this hit's pitch offset

    SVF bandpass;
    SVF highpass;
    Saturator drive;

    bool active = false;
    float ampEnv = 0.0f;
    float attack = 1.0f, attackInc = 1.0f; // short VCA opening, ramps 0 -> 1
    float ampCoef = 0.0f;
    float velocityGain = 1.0f;
};

} // namespace a83
