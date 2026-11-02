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

// Analog '83 kick voice.
//
// Modelled on how classic early-'80s analog kicks behave (not copied from
// any schematic): a sine-like resonance that starts a little sharp and
// drops into its note, a soft "beater" thump on the attack, a slight pitch
// droop through the body, tiny per-hit drift like an analog oscillator,
// tube/tape-style drive, and a rounded-off tail.
#pragma once

#include "Analog83Common.h"

namespace a83 {

class Kick
{
public:
    // Call once before use, and again if the sample rate changes.
    void prepare(double sampleRate);

    // Parameters. Safe to call at any time, including while playing.
    void setTune(float semitones);   // -24..+24, 0 = about 50 Hz
    void setDecay(float seconds);    // 0.01..2.0
    void setPunch(float amount);     // 0..1, depth of the pitch drop
    void setTone(float amount);      // 0..1, dark to bright beater and top end
    void setDrive(float amount);     // 0..1, tube/tape saturation (0.18 = warm)
    void setLevel(float dB);         // -60..+6

    // Start a hit. velocity is 0..1 (use 1.0 for an accent).
    // pitchSemitones shifts this hit from the TUNE setting (for playing the
    // kick chromatically, as a tuned bass); clamped to +/-36.
    void trigger(float velocity, float pitchSemitones = 0.0f);

    // ADDS the voice into `out` (does not clear it), so several voices can
    // be mixed into one buffer.
    void process(float* out, int numSamples);

    bool isActive() const { return active; }

private:
    void updateCoefficients();

    double sampleRate = 48000.0;

    // Parameter values
    float tuneSemitones = 0.0f;
    float decaySeconds = 0.45f;
    float punch = 0.6f;
    float tone = 0.4f;
    float levelGain = 1.0f;     // smoothed, follows levelTarget (no zipper noise)
    float levelTarget = 1.0f;
    float levelSmoothing = 0.0f;

    // Derived values
    float baseHz = 50.0f;
    float ampCoef = 0.0f, pitchCoef = 0.0f, droopCoef = 0.0f;
    float beaterCoef = 0.0f, declickCoef = 0.0f;

    // Running state
    bool active = false;
    float phase = 0.0f;
    float ampEnv = 0.0f, pitchEnv = 0.0f, droopEnv = 0.0f, beaterEnv = 0.0f;
    float drift = 1.0f;
    float hitRatio = 1.0f; // this hit's pitch offset as a frequency ratio
    float velocityGain = 1.0f;
    float lastOut = 0.0f, residual = 0.0f, residualEnv = 0.0f;

    Saturator drive;
    OnePoleLowpass toneFilter;
    SVF beaterFilter;
    DcBlocker dcBlocker;
    Noise noise;
};

} // namespace a83
