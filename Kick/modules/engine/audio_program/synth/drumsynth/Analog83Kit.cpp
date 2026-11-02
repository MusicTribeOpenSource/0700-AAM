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

#include "Analog83Kit.h"

#include <cmath>

namespace a83 {

namespace {

// label, min, max, default, unit. Defaults match the kit page mockup.
const ParamInfo kParamInfo[kNumParams] = {
    { "TUNE",  -24.0f, 24.0f,  0.0f,  Unit::Semitones }, // KickTune
    { "DECAY",  0.01f,  2.0f,  0.38f, Unit::Seconds },   // KickDecay
    { "PUNCH",  0.0f, 100.0f, 72.0f,  Unit::Percent },   // KickPunch
    { "TONE",   0.0f, 100.0f, 40.0f,  Unit::Percent },   // KickTone
    { "DRIVE",  0.0f, 100.0f, 18.0f,  Unit::Percent },   // KickDrive
    { "LEVEL", -60.0f,  6.0f,  0.0f,  Unit::Decibels },  // KickLevel
    { "TUNE",  -12.0f, 12.0f,  0.0f,  Unit::Semitones }, // HatTune
    { "DECAY",  0.01f,  0.3f,  0.06f, Unit::Seconds },   // HatClosedDecay
    { "DECAY",  0.1f,   2.0f,  0.45f, Unit::Seconds },   // HatOpenDecay
    { "TONE",   0.0f, 100.0f, 50.0f,  Unit::Percent },   // HatTone
    { "DRIVE",  0.0f, 100.0f, 18.0f,  Unit::Percent },   // HatDrive
    { "LEVEL", -60.0f,  6.0f, -3.0f,  Unit::Decibels },  // HatLevel (sits ~20 dB under the kick, as before)
    { "BUS DRIVE", 0.0f, 100.0f, 15.0f, Unit::Percent }, // BusDrive
    { "BUS COMP",  0.0f, 100.0f, 15.0f, Unit::Percent }, // BusComp: 15% keeps velocity accents (half velocity ~3 dB quieter)
    { "RECORD",    0.0f, 100.0f, 60.0f, Unit::Percent }, // Record
    { "ROOM",      0.0f, 100.0f,  0.0f, Unit::Percent }, // Room: off by default (dry)
};

// Mix placement, like a typical record: kick in the centre, hats a little right.
constexpr float kHatLeftGain = 0.78f, kHatRightGain = 1.0f;

// ROOM is a dry-to-room knob, off by default. Room sends per voice:
//   into the room: hats, snare, clap and percussion (rim, cowbell, congas,
//                  clave, shaker, cymbal)
//   kept dry:      kick, low tom and mid tom, so the low end stays tight
// Only the kick and hats exist so far; the other voices get their send when
// they're built.
constexpr float kKickRoomSend = 0.0f, kHatRoomSend = 1.0f;
// Measured on a closed hat, room tail 150-400 ms after the hit vs the hit:
// 10% about -33 dB (a hint), 25% about -25 dB, 100% about -13 dB (wet,
// late-80s style). The kick is never touched by the room.
constexpr float kRoomReturn = 3.1f;

constexpr float kBusInputGain = 1.0f; // voices arrive normalised near -1 dBFS


float valueOf(const std::array<float, kNumParams>& v, Param p) { return v[(size_t) p]; }

} // namespace

const ParamInfo& paramInfo(Param p) { return kParamInfo[(int) p]; }

Kit::Kit()
{
    for (int i = 0; i < kNumParams; ++i)
        params[(size_t) i].store(kParamInfo[i].defaultValue);
    for (auto& h : hitCounts)
        h.store(0);
}

void Kit::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    kick.prepare(sampleRate);
    hat.prepare(sampleRate);
    busComp.prepare(sampleRate);
    for (auto& dc : busDc) dc.prepare(sampleRate);
    room.prepare(sampleRate);
    roomSmoothing = timeConstantCoef(0.02f, sampleRate);
    roomGain = 0.0f;
    roomRunning = false;
    busAsleep = false;
    silentSamples = 0;
    record.prepare(sampleRate);
    limiter.prepare(sampleRate);
    forceApply = true;
    applyParams();
}

void Kit::setParam(Param p, float value)
{
    const auto& info = paramInfo(p);
    params[(size_t) p].store(clampf(value, info.min, info.max));
}

float Kit::getParam(Param p) const { return params[(size_t) p].load(); }

bool Kit::trigger(VoiceId v, float velocity) { return triggers.push({ v, velocity, 0.0f }); }

bool Kit::trigger(int midiNote, float velocity)
{
    VoiceId v {};
    float semitones = 0.0f;
    return resolveNote(midiNote, v, semitones) && triggers.push({ v, velocity, semitones });
}

void Kit::setMode(KitMode mode, VoiceId singleVoice, int rootNote)
{
    singleVoiceValue.store((int) singleVoice);
    rootNoteValue.store(std::max(0, std::min(127, rootNote)));
    modeValue.store((int) mode);
}

bool Kit::voiceForNote(int midiNote, VoiceId& voice)
{
    switch (midiNote)
    {
        case gm::Kick:
        case gm::KickAlt:   voice = VoiceId::Kick;      return true;
        case gm::ClosedHat:
        case gm::PedalHat:  voice = VoiceId::ClosedHat; return true;
        case gm::OpenHat:   voice = VoiceId::OpenHat;   return true;
        default:            return false;
    }
}

bool Kit::resolveNote(int midiNote, VoiceId& voice, float& semitones) const
{
    if ((KitMode) modeValue.load(std::memory_order_relaxed) == KitMode::Single)
    {
        // One voice across the keys: the root note plays at the voice's TUNE
        voice = (VoiceId) singleVoiceValue.load(std::memory_order_relaxed);
        semitones = (float) (midiNote - rootNoteValue.load(std::memory_order_relaxed));
        return true;
    }
    semitones = 0.0f;
    return voiceForNote(midiNote, voice);
}

void Kit::applyTo(Kick& k, const std::array<float, kNumParams>& v)
{
    k.setTune(valueOf(v, Param::KickTune));
    k.setDecay(valueOf(v, Param::KickDecay));
    k.setPunch(valueOf(v, Param::KickPunch) / 100.0f);
    k.setTone(valueOf(v, Param::KickTone) / 100.0f);
    k.setDrive(valueOf(v, Param::KickDrive) / 100.0f);
    k.setLevel(valueOf(v, Param::KickLevel));
}

void Kit::applyTo(HiHat& h, const std::array<float, kNumParams>& v)
{
    h.setTune(valueOf(v, Param::HatTune));
    h.setClosedDecay(valueOf(v, Param::HatClosedDecay));
    h.setOpenDecay(valueOf(v, Param::HatOpenDecay));
    h.setTone(valueOf(v, Param::HatTone) / 100.0f);
    h.setDrive(valueOf(v, Param::HatDrive) / 100.0f);
    h.setLevel(valueOf(v, Param::HatLevel));
}

void Kit::applyParams()
{
    std::array<float, kNumParams> current {};
    bool changed = forceApply;
    for (int i = 0; i < kNumParams; ++i)
    {
        current[(size_t) i] = params[(size_t) i].load(std::memory_order_relaxed);
        changed = changed || current[(size_t) i] != applied[(size_t) i];
    }
    if (! changed)
        return;

    applyTo(kick, current);
    applyTo(hat, current);

    const float tape = valueOf(current, Param::BusDrive) / 100.0f;
    busTape.setAmount(tape);
    for (auto& lp : busTapeTone) lp.setup(18000.0f - tape * 6000.0f, sampleRate); // more drive, softer top
    busComp.setAmount(valueOf(current, Param::BusComp) / 100.0f);
    record.setAmount(valueOf(current, Param::Record) / 100.0f);
    roomTarget = valueOf(current, Param::Room) / 100.0f * kRoomReturn;
    applied = current;
    forceApply = false;
}

void Kit::fire(VoiceId v, float velocity, float semitones)
{
    switch (v)
    {
        case VoiceId::Kick:      kick.trigger(velocity, semitones); break;
        case VoiceId::ClosedHat: hat.triggerClosed(velocity, semitones); break;
        case VoiceId::OpenHat:   hat.triggerOpen(velocity, semitones); break;
        case VoiceId::Count:     return;
    }
    busAsleep = false; // wake the bus instantly
    silentSamples = 0;
    hitCounts[(size_t) v].fetch_add(1, std::memory_order_relaxed);
}


void Kit::renderVoices(float* left, float* right, int numSamples)
{
    if (busAsleep)
        return; // nothing sounding: costs almost nothing while idle

    std::fill(kickBuffer.begin(), kickBuffer.begin() + numSamples, 0.0f);
    std::fill(hatBuffer.begin(), hatBuffer.begin() + numSamples, 0.0f);
    kick.process(kickBuffer.data(), numSamples);
    hat.process(hatBuffer.data(), numSamples);
    float blockPeak = 0.0f; // bus level before the tape hiss, for idle sleep

    for (int i = 0; i < numSamples; ++i)
    {
        const float k = kickBuffer[(size_t) i];
        const float h = hatBuffer[(size_t) i];

        // Mix: kick centre, hats slightly right, plus the room
        float l = k + h * kHatLeftGain;
        float r = k + h * kHatRightGain;
        float wetL = 0.0f, wetR = 0.0f;
        roomGain = roomTarget + roomSmoothing * (roomGain - roomTarget);
        if (roomTarget > 0.0f || roomGain > 1.0e-4f)
        {
            room.process(k * kKickRoomSend + h * kHatRoomSend, wetL, wetR);
            roomRunning = true;
        }
        else if (roomRunning)
        {
            room.reset(); // knob back at 0: clear the tail so it can't reappear later
            roomRunning = false;
            roomGain = 0.0f;
        }
        l = (l + wetL * roomGain) * kBusInputGain;
        r = (r + wetR * roomGain) * kBusInputGain;

        // Stereo-linked glue compression
        const float g = busComp.gainFor(std::max(std::abs(l), std::abs(r)));
        l *= g;
        r *= g;

        // Tape saturation and softened top
        l = busDc[0].process(busTapeTone[0].process(busTape.process(l)));
        r = busDc[1].process(busTapeTone[1].process(busTape.process(r)));

        blockPeak = std::max(blockPeak, std::max(std::abs(l), std::abs(r)));

        // On to the record: tape EQ, wow and flutter, noise floor
        record.process(l, r);

        // Mastering: smooth peak limiting, then a soft clip for stray peaks
        const float lim = limiter.gainFor(std::max(std::abs(l), std::abs(r)));
        l *= lim;
        r *= lim;

        left[i] += softClip(l);
        right[i] += softClip(r);
    }

    constexpr float kSleepLevel = 1.0e-5f; // about -100 dBFS
    if (! kick.isActive() && ! hat.isActive() && blockPeak < kSleepLevel)
    {
        silentSamples += numSamples;
        if (silentSamples >= (int) (0.25 * sampleRate))
            busAsleep = true;
    }
    else
    {
        silentSamples = 0;
    }
}

bool Kit::noteOn(VoiceId v, float velocity, int sampleOffset)
{
    if (numScheduled >= kMaxNotesPerBlock)
        return false;
    scheduled[(size_t) numScheduled++] = { std::max(0, sampleOffset), v, velocity, 0.0f };
    return true;
}

bool Kit::noteOn(int midiNote, float velocity, int sampleOffset)
{
    VoiceId v {};
    float semitones = 0.0f;
    if (numScheduled >= kMaxNotesPerBlock || ! resolveNote(midiNote, v, semitones))
        return false;
    scheduled[(size_t) numScheduled++] = { std::max(0, sampleOffset), v, velocity, semitones };
    return true;
}

// Shared by process() and processDry(): applies parameters, fires pad
// presses and scheduled notes, and calls render(pos, length) for chunks
// that end exactly at each note, so every hit starts on its sample.
template <typename Render>
void Kit::runBlock(int numSamples, Render&& render)
{
    const ScopedFlushDenormals noDenormals; // see Analog83Common.h
    applyParams();

    // Pad presses from the UI play at the start of the block
    TriggerEvent e {};
    while (triggers.pop(e))
        fire(e.voice, e.velocity, e.semitones);

    // Sequencer notes: clamp into this block and sort by offset (a short
    // insertion sort; usually only a few notes, often already in order)
    for (int i = 0; i < numScheduled; ++i)
        scheduled[(size_t) i].offset = std::min(scheduled[(size_t) i].offset, std::max(0, numSamples - 1));
    for (int i = 1; i < numScheduled; ++i)
    {
        const auto note = scheduled[(size_t) i];
        int j = i - 1;
        while (j >= 0 && scheduled[(size_t) j].offset > note.offset)
        {
            scheduled[(size_t) j + 1] = scheduled[(size_t) j];
            --j;
        }
        scheduled[(size_t) (j + 1)] = note;
    }

    int pos = 0, next = 0;
    while (pos < numSamples)
    {
        while (next < numScheduled && scheduled[(size_t) next].offset <= pos)
        {
            const auto& n = scheduled[(size_t) next];
            fire(n.voice, n.velocity, n.semitones);
            ++next;
        }

        int end = std::min(numSamples, pos + kScratchSize);
        if (next < numScheduled)
            end = std::min(end, scheduled[(size_t) next].offset);

        render(pos, end - pos);
        pos = end;
    }
    numScheduled = 0;
}

void Kit::process(float* left, float* right, int numSamples)
{
    runBlock(numSamples, [&](int pos, int length) { renderVoices(left + pos, right + pos, length); });
}

void Kit::processDry(float* const* outputs, int numSamples)
{
    float* kickOut = outputs[(size_t) DryOutput::Kick];
    float* hatOut = outputs[(size_t) DryOutput::Hat];

    runBlock(numSamples, [&](int pos, int length)
    {
        // Voices write straight into the outputs: no bus, nothing to mix.
        // A skipped (nullptr) output still advances its voice, via scratch.
        auto renderVoice = [&](auto& voice, float* out, std::array<float, kScratchSize>& scratch)
        {
            if (out != nullptr)
            {
                voice.process(out + pos, length);
            }
            else if (voice.isActive())
            {
                std::fill(scratch.begin(), scratch.begin() + length, 0.0f);
                voice.process(scratch.data(), length);
            }
        };
        renderVoice(kick, kickOut, kickBuffer);
        renderVoice(hat, hatOut, hatBuffer);
    });
}

std::vector<float> Kit::renderPreview(VoiceId v, double seconds, double previewRate) const
{
    std::array<float, kNumParams> current {};
    for (int i = 0; i < kNumParams; ++i)
        current[(size_t) i] = params[(size_t) i].load();

    std::vector<float> out((size_t) std::max(1.0, seconds * previewRate), 0.0f);

    if (v == VoiceId::Kick)
    {
        Kick k;
        k.prepare(previewRate);
        applyTo(k, current);
        k.trigger(1.0f);
        k.process(out.data(), (int) out.size());
    }
    else
    {
        HiHat h;
        h.prepare(previewRate);
        applyTo(h, current);
        if (v == VoiceId::OpenHat) h.triggerOpen(1.0f);
        else                       h.triggerClosed(1.0f);
        h.process(out.data(), (int) out.size());
    }
    return out;
}

} // namespace a83
