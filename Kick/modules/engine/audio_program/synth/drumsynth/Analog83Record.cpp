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

#include "Analog83Record.h"

namespace a83 {

// ---------------------------------------------------------------- Room

namespace {
// Delay times in ms: Freeverb's tuning (Jezar at Dreampoint, 2000, public
// domain), converted from samples at 44.1 kHz, so they scale correctly to any
// sample rate. The right side is offset by Freeverb's stereo spread. See CREDITS.md.
constexpr float kCombMs[4]    = { 25.3f, 26.9f, 29.0f, 30.7f };
constexpr float kAllPassMs[2] = { 12.6f, 10.0f };
constexpr float kStereoSpreadMs = 0.52f;
constexpr float kPreDelayMs = 9.0f;
constexpr float kRoomT60 = 0.55f; // seconds for the room to fade by 60 dB
constexpr float kRoomDamp = 0.2f; // slightly darker reflections, like a real room

size_t msToSamples(float ms, double sr) { return std::max<size_t>(1, (size_t) (ms * 0.001 * sr)); }
} // namespace

void Room::prepare(double sampleRate)
{
    for (int i = 0; i < kCombs; ++i)
    {
        for (int side = 0; side < 2; ++side)
        {
            auto& c = side == 0 ? combsL[(size_t) i] : combsR[(size_t) i];
            const float ms = kCombMs[i] + (side == 0 ? 0.0f : kStereoSpreadMs);
            c.buffer.assign(msToSamples(ms, sampleRate), 0.0f);
            c.index = 0;
            c.feedback = std::pow(10.0f, -3.0f * ms * 0.001f / kRoomT60);
            // kRoomDamp is tuned at 48 kHz; convert it so the room's
            // darkness (and tail length) is the same at any sample rate.
            c.damp = std::pow(kRoomDamp, (float) (48000.0 / sampleRate));
            c.filtered = 0.0f;
        }
    }
    for (int i = 0; i < kAllPasses; ++i)
    {
        allPassL[(size_t) i].buffer.assign(msToSamples(kAllPassMs[i], sampleRate), 0.0f);
        allPassR[(size_t) i].buffer.assign(msToSamples(kAllPassMs[i] + kStereoSpreadMs, sampleRate), 0.0f);
        allPassL[(size_t) i].index = allPassR[(size_t) i].index = 0;
    }
    preDelay.assign(msToSamples(kPreDelayMs, sampleRate), 0.0f);
    preDelayIndex = 0;
    sendHighPass.setup(Biquad::Type::HighPass, 250.0f, 0.7f, 0.0f, sampleRate);
    sendLowPass.setup(Biquad::Type::LowPass, 9000.0f, 0.7f, 0.0f, sampleRate);
    sendHighPass.reset();
    sendLowPass.reset();
}

void Room::reset()
{
    for (auto* combs : { &combsL, &combsR })
        for (auto& c : *combs) { std::fill(c.buffer.begin(), c.buffer.end(), 0.0f); c.filtered = 0.0f; }
    for (auto* aps : { &allPassL, &allPassR })
        for (auto& a : *aps) std::fill(a.buffer.begin(), a.buffer.end(), 0.0f);
    std::fill(preDelay.begin(), preDelay.end(), 0.0f);
    sendHighPass.reset();
    sendLowPass.reset();
}

void Room::process(float in, float& outL, float& outR)
{
    const float delayed = preDelay[preDelayIndex];
    preDelay[preDelayIndex] = sendLowPass.process(sendHighPass.process(in));
    if (++preDelayIndex >= preDelay.size()) preDelayIndex = 0;

    float l = 0.0f, r = 0.0f;
    for (int i = 0; i < kCombs; ++i)
    {
        l += combsL[(size_t) i].process(delayed);
        r += combsR[(size_t) i].process(delayed);
    }
    for (int i = 0; i < kAllPasses; ++i)
    {
        l = allPassL[(size_t) i].process(l);
        r = allPassR[(size_t) i].process(r);
    }
    outL = l * 0.25f;
    outR = r * 0.25f;
}

// ---------------------------------------------------------------- Wow & flutter

namespace {
constexpr float kBaseDelayMs = 4.0f;
// At depth 1.0: wow moves pitch about 0.12%, flutter about 0.05%.
constexpr float kWowHz = 0.45f,     kWowPitch = 0.0012f;
constexpr float kFlutterHz = 6.2f,  kFlutterPitch = 0.0005f;
constexpr size_t kWowBufferSize = 4096; // about 85 ms at 48 kHz, plenty
} // namespace

void WowFlutter::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    bufferL.assign(kWowBufferSize, 0.0f);
    bufferR.assign(kWowBufferSize, 0.0f);
    writeIndex = 0;
    wowLfo.setup(kWowHz, sampleRate);
    flutterLfo.setup(kFlutterHz, sampleRate);
    wowLfo.reset(0.0f);
    flutterLfo.reset(0.0f);
    setDepth(depth);
    driftFrom = 0.0f;
    driftTo = noise.next();
    driftPos = 0.0f;
    driftInc = 1.0f / (1.7f * (float) sampleRate);
}

void WowFlutter::setDepth(float amount)
{
    depth = clampf(amount, 0.0f, 1.0f);
    // A sine of amplitude A seconds at frequency f moves pitch by 2*pi*f*A,
    // so the amplitude for a wanted pitch change is pitch / (2*pi*f).
    const float sr = (float) sampleRate;
    wowAmp = depth * kWowPitch / (kTwoPi * kWowHz) * sr;
    flutterAmp = depth * kFlutterPitch / (kTwoPi * kFlutterHz) * sr;
    baseDelay = kBaseDelayMs * 0.001f * sr;
}

// 4-point cubic (Hermite) interpolation: keeps the highs intact, unlike
// simple linear interpolation, which dulls hats and cymbals.
float WowFlutter::read(const std::vector<float>& buffer, float delaySamples) const
{
    float pos = (float) writeIndex - delaySamples;
    while (pos < 0.0f) pos += (float) kWowBufferSize;
    const size_t i1 = (size_t) pos % kWowBufferSize;
    const size_t i0 = (i1 + kWowBufferSize - 1) % kWowBufferSize;
    const size_t i2 = (i1 + 1) % kWowBufferSize;
    const size_t i3 = (i1 + 2) % kWowBufferSize;
    const float t = pos - std::floor(pos);
    const float y0 = buffer[i0], y1 = buffer[i1], y2 = buffer[i2], y3 = buffer[i3];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

void WowFlutter::process(float& left, float& right)
{
    bufferL[writeIndex] = left;
    bufferR[writeIndex] = right;

    // Smooth random drift (smoothstep between random points, so it never
    // jumps or jitters)
    driftPos += driftInc;
    if (driftPos >= 1.0f)
    {
        driftPos -= 1.0f;
        driftFrom = driftTo;
        driftTo = noise.next();
    }
    const float s = driftPos * driftPos * (3.0f - 2.0f * driftPos);
    const float drift = driftFrom + (driftTo - driftFrom) * s;

    const float delay = baseDelay
                        + wowAmp * (wowLfo.next() + 0.5f * drift)
                        + flutterAmp * flutterLfo.next();

    left = read(bufferL, delay);
    right = read(bufferR, delay);

    if (++writeIndex >= kWowBufferSize) writeIndex = 0;
}

// ---------------------------------------------------------------- Record chain

void RecordChain::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    wow.prepare(sampleRate);
    for (auto& f : headBump) f.reset();
    for (auto& f : cleanUp) f.reset();
    for (auto& f : topEnd) f.reset();
    for (auto& f : hissTone) { f.setup(9000.0f, sampleRate); f.reset(); }
    setAmount(amount);
}

void RecordChain::setAmount(float a)
{
    amount = clampf(a, 0.0f, 1.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        // Head bump: the low-end lift tape machines are known for (+3 dB at 60%)
        headBump[(size_t) ch].setup(Biquad::Type::Peak, 72.0f, 0.9f, 5.0f * amount, sampleRate);
        // Clean-up below the bump, like a mastering high-pass
        cleanUp[(size_t) ch].setup(Biquad::Type::HighPass, 24.0f + 10.0f * amount, 0.7f, 0.0f, sampleRate);
        // Softer top: 20 kHz at 0%, about 16 kHz at 60% (hats stay crisp)
        topEnd[(size_t) ch].setup(Biquad::Type::LowPass, 20000.0f - 7000.0f * amount, 0.707f, 0.0f, sampleRate);
    }
    wow.setDepth(amount);
    // Measured noise floor: about -74 dBFS RMS at 60%, -70 at 100% (faint, like a
    // well-recorded tape; the hiss filter and mastering take ~9 dB off this value)
    hissGain = amount > 0.0f ? dbToGain(-71.0f + 10.0f * amount) : 0.0f;
}

void RecordChain::process(float& left, float& right)
{
    if (amount <= 0.0f)
        return;

    float* ch[2] = { &left, &right };
    for (size_t i = 0; i < 2; ++i)
    {
        float x = *ch[i];
        x = headBump[i].process(x);
        x = cleanUp[i].process(x);
        x = topEnd[i].process(x);
        *ch[i] = x;
    }

    wow.process(left, right);

    left += hissTone[0].process(hissL.next()) * hissGain;
    right += hissTone[1].process(hissR.next()) * hissGain;
}

} // namespace a83
