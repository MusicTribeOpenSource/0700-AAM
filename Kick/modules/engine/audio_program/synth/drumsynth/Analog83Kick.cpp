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

#include "Analog83Kick.h"

namespace a83 {

namespace {
constexpr float kBaseHz = 50.0f;          // pitch at tune = 0
constexpr float kMaxSweep = 2.5f;         // punch 1.0 starts at 3.5x the base pitch
constexpr float kPitchTimeConst = 0.010f; // seconds; fast initial drop
constexpr float kDroopAmount = 0.06f;     // body starts 6% sharp...
constexpr float kDroopTimeConst = 0.05f;  // ...and settles over ~50 ms
constexpr float kBeaterTime = 0.006f;     // seconds, the attack thump
constexpr float kDeclickTime = 0.002f;    // crossfade when retriggering
constexpr float kDriftAmount = 0.004f;    // +/-0.4% pitch per hit
constexpr float kKickNormalise = 0.96f;       // peaks near -1 dBFS at level 0 dB (measured)
constexpr float kKickSilence = 0.0001f;
} // namespace

void Kick::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    levelSmoothing = timeConstantCoef(0.005f, sampleRate);
    levelGain = levelTarget;
    beaterFilter.setup(1800.0f, 0.6f, sampleRate);
    beaterFilter.reset();
    dcBlocker.prepare(sampleRate);
    drive.setAmount(0.18f);
    updateCoefficients();
}

void Kick::setTune(float semitones)  { tuneSemitones = clampf(semitones, -24.0f, 24.0f); updateCoefficients(); }
void Kick::setDecay(float seconds)   { decaySeconds = clampf(seconds, 0.01f, 2.0f); updateCoefficients(); }
void Kick::setPunch(float amount)    { punch = clampf(amount, 0.0f, 1.0f); }
void Kick::setTone(float amount)     { tone = clampf(amount, 0.0f, 1.0f); updateCoefficients(); }
void Kick::setDrive(float amount)    { drive.setAmount(amount); }
void Kick::setLevel(float dB)        { levelTarget = dbToGain(clampf(dB, -60.0f, 6.0f)); }

void Kick::updateCoefficients()
{
    baseHz = kBaseHz * semitonesToRatio(tuneSemitones);
    ampCoef = decayCoef(decaySeconds, sampleRate);
    pitchCoef = timeConstantCoef(kPitchTimeConst, sampleRate);
    droopCoef = timeConstantCoef(kDroopTimeConst, sampleRate);
    beaterCoef = decayCoef(kBeaterTime, sampleRate);
    declickCoef = decayCoef(kDeclickTime, sampleRate);
    toneFilter.setup(2000.0f + tone * 8000.0f, sampleRate); // 2 kHz dark .. 10 kHz bright
}

void Kick::trigger(float velocity, float pitchSemitones)
{
    hitRatio = semitonesToRatio(clampf(pitchSemitones, -36.0f, 36.0f));
    // Fade out whatever was still sounding instead of cutting it (no click).
    residual = lastOut;
    residualEnv = 1.0f;

    if (! active)
        levelGain = levelTarget; // fresh hit: start at the knob's level
    active = true;
    phase = 0.0f;
    ampEnv = pitchEnv = droopEnv = beaterEnv = 1.0f;
    // Start the new hit's filters clean; the crossfade above handles the old sound.
    beaterFilter.reset(); // beater rises over a fraction of a millisecond, like a real one
    toneFilter.reset();
    dcBlocker.reset();
    drift = 1.0f + noise.next() * kDriftAmount;
    velocityGain = clampf(velocity, 0.0f, 1.0f);
}

void Kick::process(float* out, int numSamples)
{
    if (! active)
        return;

    const float invSr = 1.0f / (float) sampleRate;
    const float beaterAmount = 0.12f + tone * 0.18f;
    const float hitGain = velocityGain * kKickNormalise;

    for (int i = 0; i < numSamples; ++i)
    {
        // Resonance: drops quickly into its note, then droops slightly
        const float freq = baseHz * hitRatio * drift * (1.0f + punch * kMaxSweep * pitchEnv + kDroopAmount * droopEnv);
        phase += freq * invSr;
        if (phase >= 1.0f)
            phase -= 1.0f;
        const float body = std::sin(kTwoPi * phase) * roundedTail(ampEnv);

        // Beater: a short, soft, low-passed thump (mostly felt, not heard)
        const float beater = beaterFilter.process(noise.next() * 0.5f + 1.0f).lp * beaterEnv * beaterAmount;

        float y = drive.process(body + beater);
        y = toneFilter.process(y);
        y = dcBlocker.process(y);
        levelGain = levelTarget + levelSmoothing * (levelGain - levelTarget);
        y *= hitGain * levelGain;

        // Crossfade from the previous hit, if any
        y += residual * residualEnv;
        residualEnv *= declickCoef;

        out[i] += y;
        lastOut = y;

        ampEnv *= ampCoef;
        pitchEnv = snapToZero(pitchEnv * pitchCoef);   // these decay much faster than the
        droopEnv = snapToZero(droopEnv * droopCoef);   // body; snapping keeps them out of
        beaterEnv = snapToZero(beaterEnv * beaterCoef); // denormal range (CPU spikes)
        residualEnv = snapToZero(residualEnv);

        if (ampEnv < kKickSilence && residualEnv < kKickSilence)
        {
            active = false;
            lastOut = 0.0f;
            break;
        }
    }
}

} // namespace a83
