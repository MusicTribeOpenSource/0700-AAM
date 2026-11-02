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

// Analog '83 drum engine: shared DSP helpers.
// Standalone C++17, no JUCE or KICK dependencies, so it builds anywhere.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
 #include <xmmintrin.h>
 #define A83_X86_FTZ 1
#endif

namespace a83 {

// Flushes denormal (tiny) floats to zero while in scope, then restores the
// previous mode. Decaying filters and reverb tails otherwise drift into
// denormal range, which can make x86 CPUs 10-100x slower: a silent CPU
// spike that causes dropouts. Put one at the top of every audio callback.
class ScopedFlushDenormals
{
public:
    ScopedFlushDenormals()
    {
#if defined(A83_X86_FTZ)
        previous = _mm_getcsr();
        _mm_setcsr((unsigned int) (previous | 0x8040u)); // FTZ (bit 15) + DAZ (bit 6)
#elif defined(__aarch64__) && !defined(_MSC_VER)
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        previous = fpcr;
        asm volatile("msr fpcr, %0" ::"r"(fpcr | (1ull << 24))); // FZ
#endif
    }

    ~ScopedFlushDenormals()
    {
#if defined(A83_X86_FTZ)
        _mm_setcsr((unsigned int) previous);
#elif defined(__aarch64__) && !defined(_MSC_VER)
        asm volatile("msr fpcr, %0" ::"r"(previous));
#endif
    }

    ScopedFlushDenormals(const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator=(const ScopedFlushDenormals&) = delete;

private:
    uint64_t previous = 0;
};

// Fast tanh (Pade 7/6 approximant, clamped). Max error 1.5e-5 (-97 dB) for
// |x| < 4 and 1e-4 (-80 dB) worst case: inaudible, and several times cheaper
// than std::tanh. Used wherever tanh runs per sample.
inline float fastTanh(float x)
{
    x = x < -4.97f ? -4.97f : (x > 4.97f ? 4.97f : x);
    const float x2 = x * x;
    const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    const float y = num / den;
    return y < -1.0f ? -1.0f : (y > 1.0f ? 1.0f : y);
}

// Snaps a decaying envelope to exactly zero once it's far below hearing,
// so voices stay denormal-safe even when used outside a guarded callback.
inline float snapToZero(float x) { return x < 1.0e-9f ? 0.0f : x; }

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 2.0f * kPi;

inline float clampf(float x, float lo, float hi) { return std::min(std::max(x, lo), hi); }
inline float dbToGain(float dB) { return std::pow(10.0f, dB / 20.0f); }
inline float semitonesToRatio(float st) { return std::pow(2.0f, st / 12.0f); }

// Per-sample multiplier for an exponential envelope that falls by 60 dB
// over `seconds`. Multiply the envelope by this once per sample.
inline float decayCoef(float seconds, double sampleRate)
{
    const double samples = std::max(1.0, (double) seconds * sampleRate);
    return (float) std::exp(std::log(0.001) / samples);
}

// Per-sample multiplier for an envelope with time constant `seconds`
// (falls to about 37% after that time). Used for fast pitch sweeps.
inline float timeConstantCoef(float seconds, double sampleRate)
{
    const double samples = std::max(1.0, (double) seconds * sampleRate);
    return (float) std::exp(-1.0 / samples);
}

// State variable filter (TPT / trapezoidal form, after Simper and Zavalishin;
// see CREDITS.md). One call gives lowpass,
// bandpass and highpass outputs. Stable at any cutoff below Nyquist.
class SVF
{
public:
    struct Out { float lp, bp, hp; };

    void setup(float cutoffHz, float q, double sampleRate)
    {
        const float fc = clampf(cutoffHz, 20.0f, 0.45f * (float) sampleRate);
        g = std::tan(kPi * fc / (float) sampleRate);
        k = 1.0f / std::max(q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() { ic1 = ic2 = 0.0f; }

    Out process(float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, x - k * v1 - v2 };
    }

private:
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

// ---------------------------------------------------------------- Analog flavour

// "Rounds off" the end of a decay. Above `knee` the envelope is untouched;
// below it the level eases to zero along a smooth S-curve, so tails fade
// out cleanly instead of hanging at a low level and then stopping.
inline float roundedTail(float env, float knee = 0.03f)
{
    if (env >= knee)
        return env;
    const float t = env / knee;
    return env * t * t * (3.0f - 2.0f * t);
}

// Tube/tape-style saturation. amount 0..1.
// Measured on a full-scale sine: 18% adds about 3% distortion, mostly the
// warm 2nd harmonic, with no audible break-up; 50% is clearly driven and
// 100% is dirty. A bias makes the curve asymmetric like tubes and tape,
// which is where the even harmonics come from. Quiet signals pass at unity
// gain while loud peaks are gently rounded.
class Saturator
{
public:
    void setAmount(float a)
    {
        amount = clampf(a, 0.0f, 1.0f);
        drive = amount * 2.2f;
        bias = amount * 0.8f;
        offset = fastTanh(bias); // same curve as process(), so silence stays exactly 0
        norm = drive > 0.0f ? 1.0f / (drive * (1.0f - offset * offset)) : 1.0f;
    }

    float process(float x) const
    {
        if (amount < 0.001f)
            return x;
        return (fastTanh(drive * x + bias) - offset) * norm;
    }

private:
    float amount = 0.0f, drive = 0.0f, bias = 0.0f, offset = 0.0f, norm = 1.0f;
};

// Removes the small DC offset that asymmetric saturation creates (the
// classic one-pole, one-zero DC blocker; see CREDITS.md).
class DcBlocker
{
public:
    void prepare(double sampleRate, float cutoffHz = 5.0f)
    {
        r = 1.0f - kTwoPi * cutoffHz / (float) sampleRate;
    }

    void reset() { x1 = y1 = 0.0f; }

    float process(float x)
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    float r = 0.9995f, x1 = 0.0f, y1 = 0.0f;
};

// Gentle 6 dB/octave lowpass, used to soften the top end like tape.
class OnePoleLowpass
{
public:
    // Works down to very low cutoffs (0.01 Hz), so it can also smooth slow
    // control signals such as tape drift.
    void setup(float cutoffHz, double sampleRate)
    {
        const float fc = clampf(cutoffHz, 0.01f, 0.45f * (float) sampleRate);
        a = 1.0f - std::exp(-kTwoPi * fc / (float) sampleRate);
    }

    void reset() { z = 0.0f; }

    float process(float x)
    {
        z += a * (x - z);
        return z;
    }

private:
    float a = 1.0f, z = 0.0f;
};

// Punchy "glue" compressor for the kit bus. amount 0..1 sets ratio,
// threshold and make-up gain together. The 4 ms attack lets each hit's
// first transient through, and the quick 60 ms release brings the body
// back up fast, so drums sound both punchy and fat. Tuned by measuring
// loudness, peak-to-average ratio and kick body on the demo pattern.
class BusCompressor
{
public:
    void prepare(double sampleRate)
    {
        attackCoef = (float) std::exp(-1.0 / (0.004 * sampleRate));
        releaseCoef = (float) std::exp(-1.0 / (0.060 * sampleRate));
        envelope = 0.0f;
        countdown = 0;
        gain = 1.0f;
        gainStep = 0.0f;
    }

    void setAmount(float a)
    {
        const float amt = clampf(a, 0.0f, 1.0f);
        ratio = 1.0f + 3.0f * amt;              // 1:1 .. 4:1
        thresholdDb = -6.0f - 16.0f * amt;      // -6 .. -22 dB
        makeupDb = -thresholdDb * (1.0f - 1.0f / ratio) * 0.8f;
    }

    float process(float x) { return x * gainFor(std::abs(x)); }

    // Linear gain (including make-up) for a detector level. For stereo,
    // pass the louder of the two channels and apply the gain to both, so
    // the stereo image doesn't shift.
    // Returns the gain to apply to both channels (stereo-linked).
    // The detector runs every sample; the dB gain math (log and power) runs
    // every kControlRate samples and the gain glides linearly in between.
    // That's 8x less work and still far finer than the 4 ms attack.
    float gainFor(float level)
    {
        const float coef = level > envelope ? attackCoef : releaseCoef;
        envelope = level + coef * (envelope - level);

        if (--countdown <= 0)
        {
            countdown = kControlRate;
            const float target = computeGain(envelope);
            gainStep = (target - gain) / (float) kControlRate;
        }
        gain += gainStep;
        return gain;
    }

private:
    float computeGain(float env) const
    {
        const float envDb = 20.0f * std::log10(std::max(env, 1.0e-6f));
        const float over = envDb - thresholdDb;
        float reductionDb = 0.0f;
        if (over > kKneeDb * 0.5f)
            reductionDb = (1.0f / ratio - 1.0f) * over;
        else if (over > -kKneeDb * 0.5f)
        {
            const float k = over + kKneeDb * 0.5f;
            reductionDb = (1.0f / ratio - 1.0f) * k * k / (2.0f * kKneeDb);
        }
        return dbToGain(reductionDb + makeupDb);
    }

    static constexpr int kControlRate = 8;
    int countdown = 0;
    float gain = 1.0f, gainStep = 0.0f;
    static constexpr float kKneeDb = 6.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f, envelope = 0.0f;
    float ratio = 1.0f, thresholdDb = 0.0f, makeupDb = 0.0f;
};

// Mastering-style peak limiter (stereo-linked). Turns the level down
// smoothly when peaks go over the threshold, instead of reshaping the
// waveform, so loud kicks stay clean. 1 ms attack, 80 ms release.
class PeakLimiter
{
public:
    void prepare(double sampleRate)
    {
        attackCoef = (float) std::exp(-1.0 / (0.001 * sampleRate));
        releaseCoef = (float) std::exp(-1.0 / (0.080 * sampleRate));
        gain = 1.0f;
    }

    // Returns the gain to apply to both channels.
    float gainFor(float level)
    {
        const float target = level > kThreshold ? kThreshold / level : 1.0f;
        const float coef = target < gain ? attackCoef : releaseCoef;
        gain = target + coef * (gain - target);
        return gain;
    }

private:
    static constexpr float kThreshold = 0.88f; // just under the soft clipper's knee
    float attackCoef = 0.0f, releaseCoef = 0.0f, gain = 1.0f;
};

// Final safety stage, like tape hitting its limit: clean below 0.9, then
// rounds off smoothly so the output never goes past 0.97 (about -0.3 dBFS).
// No harsh digital clipping.
inline float softClip(float x)
{
    constexpr float kKnee = 0.9f, kCeiling = 0.97f;
    const float ax = std::abs(x);
    if (ax <= kKnee)
        return x;
    const float shaped = kKnee + (kCeiling - kKnee) * fastTanh((ax - kKnee) / (kCeiling - kKnee));
    return x < 0.0f ? -shaped : shaped;
}

// Standard 2nd-order filter (RBJ "Audio EQ Cookbook" formulas; see CREDITS.md).
class Biquad
{
public:
    enum class Type { LowPass, HighPass, Peak };

    void setup(Type type, float freqHz, float q, float gainDb, double sampleRate)
    {
        const float f = clampf(freqHz, 10.0f, 0.45f * (float) sampleRate);
        const float w0 = kTwoPi * f / (float) sampleRate;
        const float cosw = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * std::max(q, 0.05f));
        const float A = std::pow(10.0f, gainDb / 40.0f);
        float a0 = 1.0f;

        switch (type)
        {
            case Type::LowPass:
                b0 = (1.0f - cosw) * 0.5f; b1 = 1.0f - cosw; b2 = b0;
                a0 = 1.0f + alpha; a1 = -2.0f * cosw; a2 = 1.0f - alpha;
                break;
            case Type::HighPass:
                b0 = (1.0f + cosw) * 0.5f; b1 = -(1.0f + cosw); b2 = b0;
                a0 = 1.0f + alpha; a1 = -2.0f * cosw; a2 = 1.0f - alpha;
                break;
            case Type::Peak:
                b0 = 1.0f + alpha * A; b1 = -2.0f * cosw; b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A; a1 = -2.0f * cosw; a2 = 1.0f - alpha / A;
                break;
        }
        b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    }

    void reset() { z1 = z2 = 0.0f; }

    float process(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};

// Sine LFO by rotating a unit vector each sample: a few multiplies instead
// of std::sin. Renormalised every 4096 samples so the level never drifts.
class SineLfo
{
public:
    void setup(float hz, double sampleRate)
    {
        const double w = 2.0 * 3.14159265358979323846 * hz / sampleRate;
        rotC = (float) std::cos(w);
        rotS = (float) std::sin(w);
    }

    void reset(float phase01 = 0.0f)
    {
        c = std::cos(kTwoPi * phase01);
        s = std::sin(kTwoPi * phase01);
        count = 0;
    }

    float next()
    {
        const float out = s;
        const float nc = c * rotC - s * rotS;
        s = s * rotC + c * rotS;
        c = nc;
        if (++count >= 4096)
        {
            count = 0;
            const float g = 1.0f / std::sqrt(c * c + s * s);
            c *= g;
            s *= g;
        }
        return out;
    }

private:
    float rotC = 1.0f, rotS = 0.0f, c = 1.0f, s = 0.0f;
    int count = 0;
};

// Cheap white noise, -1..+1 (xorshift32, Marsaglia 2003; see CREDITS.md).
class Noise
{
public:
    explicit Noise(uint32_t seed = 0x1983A83u) : state(seed ? seed : 1u) {}

    float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }

private:
    uint32_t state;
};

} // namespace a83
