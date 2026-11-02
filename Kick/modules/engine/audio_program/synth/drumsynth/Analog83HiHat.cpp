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

#include "Analog83HiHat.h"

namespace a83 {

namespace {
// Inharmonic oscillator frequencies in Hz. These ratios are what make the
// tone sound metallic rather than like a chord. Widely published values for
// the classic six-oscillator analog hi-hat; see CREDITS.md.
constexpr float kOscHz[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
// PolyBLEP (after Valimaki & Huovilainen, 2007; see CREDITS.md): smooths each edge of a square wave over one sample, removing
// most of the aliasing (harsh, out-of-tune fold-back) of a naive square.
inline float polyBlep(float t, float dt)
{
    if (t < dt)       { t /= dt;          return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

constexpr float kHatNormalise = 3.17f; // filters remove a lot of level; peaks near -1 dBFS at 0 dB (measured)
constexpr float kAttackTime = 0.0003f; // 0.3 ms, about how fast an analog VCA opens
constexpr float kHatSilence = 0.0001f;
} // namespace

void HiHat::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    levelSmoothing = timeConstantCoef(0.005f, sampleRate);
    levelGain = levelTarget;
    bandpass.reset();
    highpass.reset();
    drive.setAmount(0.18f);
    updateOscillators();
    updateFilters();
}

void HiHat::setTune(float semitones)      { tuneSemitones = clampf(semitones, -12.0f, 12.0f); updateOscillators(); }
void HiHat::setClosedDecay(float seconds) { closedDecay = clampf(seconds, 0.01f, 0.3f); }
void HiHat::setOpenDecay(float seconds)   { openDecay = clampf(seconds, 0.1f, 2.0f); }
void HiHat::setTone(float amount)         { tone = clampf(amount, 0.0f, 1.0f); updateFilters(); }
void HiHat::setDrive(float amount)        { drive.setAmount(amount); }
void HiHat::setLevel(float dB)            { levelTarget = dbToGain(clampf(dB, -60.0f, 6.0f)); }

void HiHat::updateOscillators()
{
    const float ratio = semitonesToRatio(tuneSemitones + hitSemitones);
    for (int i = 0; i < kNumOscillators; ++i)
        increments[i] = kOscHz[i] * ratio / (float) sampleRate;
}

void HiHat::updateFilters()
{
    const float centre = 7000.0f + tone * 5000.0f; // 7 kHz dark .. 12 kHz bright
    bandpass.setup(centre, 1.2f, sampleRate);
    highpass.setup(6500.0f, 0.7f, sampleRate);
}

void HiHat::triggerClosed(float velocity, float pitchSemitones) { start(velocity, closedDecay, pitchSemitones); }
void HiHat::triggerOpen(float velocity, float pitchSemitones)   { start(velocity, openDecay, pitchSemitones); }

void HiHat::start(float velocity, float decaySeconds, float pitchSemitones)
{
    const float semis = clampf(pitchSemitones, -24.0f, 24.0f);
    if (semis != hitSemitones)
    {
        hitSemitones = semis;
        updateOscillators(); // only when the pitch actually changes
    }

    // Retriggering the same voice is what chokes an open hat.
    if (! active)
        levelGain = levelTarget; // fresh hit: start at the knob's level
    active = true;
    ampEnv = 1.0f;
    attack = 0.0f;
    attackInc = 1.0f / std::max(1.0f, kAttackTime * (float) sampleRate);
    ampCoef = decayCoef(decaySeconds, sampleRate);
    velocityGain = clampf(velocity, 0.0f, 1.0f);
}

void HiHat::process(float* out, int numSamples)
{
    if (! active)
        return;

    // Oscillator phases are never reset on a new hit, so each hit starts at
    // a different point and sounds slightly different, like free-running
    // analog oscillators.
    const float hitGain = velocityGain * kHatNormalise;

    for (int i = 0; i < numSamples; ++i)
    {
        float metal = 0.0f;
        for (int o = 0; o < kNumOscillators; ++o)
        {
            const float dt = increments[o];
            float t = phases[o] + dt;
            if (t >= 1.0f)
                t -= 1.0f;
            phases[o] = t;
            float half = t + 0.5f;
            if (half >= 1.0f)
                half -= 1.0f;
            metal += (t < 0.5f ? 1.0f : -1.0f) + polyBlep(t, dt) - polyBlep(half, dt);
        }
        metal *= (1.0f / kNumOscillators);

        const float driven = drive.process(bandpass.process(metal).bp * 2.0f) * 0.5f;
        attack = std::min(1.0f, attack + attackInc);
        levelGain = levelTarget + levelSmoothing * (levelGain - levelTarget);
        out[i] += highpass.process(driven * roundedTail(ampEnv) * attack).hp * hitGain * levelGain;

        ampEnv *= ampCoef;
        if (ampEnv < kHatSilence)
        {
            active = false;
            break;
        }
    }
}

} // namespace a83
