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

// Analog '83 kit: the sound engine. It owns the voices, their parameters
// and the stereo bus. It is sound only: tempo and patterns come from the
// host's sequencer (on KICK, its own sequencer), which triggers notes.
//
// Threading:
//   - Any thread, any number of threads at once: trigger() (pad presses or
//     host notes; play at the start of the next audio block).
//   - UI thread: setParam(), setMode(), getHitCount(), renderPreview().
//   - Audio thread: noteOn() (sample-accurate hits from a sequencer),
//     process().
//   No locks are used, so the audio thread never waits on the UI.
#pragma once

#include "Analog83HiHat.h"
#include "Analog83Kick.h"
#include "Analog83Record.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <vector>

namespace a83 {

enum class Param
{
    KickTune, KickDecay, KickPunch, KickTone, KickDrive, KickLevel,
    HatTune, HatClosedDecay, HatOpenDecay, HatTone, HatDrive, HatLevel,
    BusDrive, BusComp, Record, Room,
    Count
};

enum class Unit { Semitones, Seconds, Percent, Decibels };

struct ParamInfo
{
    const char* label;
    float min, max, defaultValue;
    Unit unit;
};

const ParamInfo& paramInfo(Param p);
constexpr int kNumParams = (int) Param::Count;

enum class VoiceId { Kick, ClosedHat, OpenHat, Count };
constexpr int kNumVoices = (int) VoiceId::Count;

// General MIDI drum notes the kit answers to (Multi mode).
namespace gm {
constexpr int Kick = 36, KickAlt = 35, ClosedHat = 42, PedalHat = 44, OpenHat = 46;
}

// Multi: the full kit, notes pick voices by the GM drum map.
// Single: one voice played chromatically; rootNote plays at the voice's TUNE.
enum class KitMode { Multi, Single };

// One dry mono channel per voice for processDry(). Closed and open hats share
// one voice (so closed chokes open), so they share one output.
enum class DryOutput { Kick, Hat, Count };
constexpr int kNumDryOutputs = (int) DryOutput::Count;

// MSVC warns (C4324) that the queue, and the Kit that contains it, were
// padded because of alignas(64). That padding is intended: it keeps the
// producer and consumer counters on separate cache lines.
#if defined(_MSC_VER)
 #pragma warning(push)
 #pragma warning(disable : 4324)
#endif

// Bounded multi-producer / single-consumer queue (Dmitry Vyukov's design;
// see CREDITS.md).
// Any number of threads may push() at once (KICK sends notes from both its
// sequencer and live pads); only the audio thread may pop(). push() never
// blocks the audio thread, and pop() never waits. Capacity must be a power
// of two. When full, push() returns false and the event is dropped.
template <typename T, size_t Capacity>
class MpscQueue
{
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    MpscQueue()
    {
        for (size_t i = 0; i < Capacity; ++i)
            cells[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool push(const T& item)
    {
        size_t pos = enqueuePos.load(std::memory_order_relaxed);
        for (;;)
        {
            Cell& cell = cells[pos & (Capacity - 1)];
            const size_t seq = cell.sequence.load(std::memory_order_acquire);
            const auto diff = (std::ptrdiff_t) seq - (std::ptrdiff_t) pos;
            if (diff == 0)
            {
                if (enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    cell.data = item;
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            }
            else if (diff < 0)
            {
                return false; // full
            }
            else
            {
                pos = enqueuePos.load(std::memory_order_relaxed);
            }
        }
    }

    bool pop(T& item) // audio thread only
    {
        const size_t pos = dequeuePos.load(std::memory_order_relaxed);
        Cell& cell = cells[pos & (Capacity - 1)];
        const size_t seq = cell.sequence.load(std::memory_order_acquire);
        if ((std::ptrdiff_t) seq - (std::ptrdiff_t) (pos + 1) < 0)
            return false; // empty (or a push still finishing; it arrives next block)
        item = cell.data;
        cell.sequence.store(pos + Capacity, std::memory_order_release);
        dequeuePos.store(pos + 1, std::memory_order_relaxed);
        return true;
    }

private:
    struct Cell
    {
        std::atomic<size_t> sequence { 0 };
        T data {};
    };

    std::array<Cell, Capacity> cells;
    alignas(64) std::atomic<size_t> enqueuePos { 0 }; // separate cache lines: producers
    alignas(64) std::atomic<size_t> dequeuePos { 0 }; // and the consumer don't contend
};

class Kit
{
public:
    Kit();

    // Call before audio starts, and whenever the sample rate changes.
    void prepare(double sampleRate);

    // ---- UI thread
    void setParam(Param p, float value);
    float getParam(Param p) const;
    // Pad press or host note, from any thread (several at once is fine):
    // plays at the start of the next audio block. Returns false if the
    // queue is full (64 pending events) and the press was dropped.
    bool trigger(VoiceId v, float velocity);
    // Same, by MIDI note (follows the current mode). Returns false if the
    // note doesn't map to a voice.
    bool trigger(int midiNote, float velocity);

    // Switch between the full kit and one chromatic voice. UI thread; takes
    // effect from the next note.
    void setMode(KitMode mode, VoiceId singleVoice = VoiceId::Kick, int rootNote = gm::Kick);
    KitMode getMode() const { return (KitMode) modeValue.load(); }

    // GM drum map lookup (Multi mode). Returns false for unmapped notes.
    static bool voiceForNote(int midiNote, VoiceId& voice);

    // ---- Audio thread
    // Sample-accurate hit for a sequencer: sampleOffset is the sample inside
    // the NEXT process() call where the hit starts (0 = first sample;
    // out-of-range offsets are clamped). Up to kMaxNotesPerBlock per call;
    // returns false if the block is full.
    static constexpr int kMaxNotesPerBlock = 128;
    bool noteOn(VoiceId v, float velocity, int sampleOffset);
    // Same, by MIDI note (follows the current mode). Returns false if the
    // note doesn't map to a voice or the block is full.
    bool noteOn(int midiNote, float velocity, int sampleOffset);

    // ---- Any thread
    // Increments every time a voice is triggered (trigger() or noteOn()).
    // The UI polls this to flash pads.
    uint32_t getHitCount(VoiceId v) const { return hitCounts[(size_t) v].load(); }

    // Renders one hit of a voice with the current settings, for drawing a
    // waveform. Uses its own voices, so it's safe to call from the UI thread.
    std::vector<float> renderPreview(VoiceId v, double seconds, double previewRate = 48000.0) const;

    // ---- Audio thread
    // ADDS the stereo kit into `left` and `right` (does not clear them).
    void process(float* left, float* right, int numSamples);

    // Alternative to process() for hosts with their own mixer and effects
    // (like KICK): one DRY mono channel per voice, indexed by DryOutput.
    // Each voice keeps its own DRIVE and LEVEL; there is no pan, room, bus
    // or tape. ADDS into the buffers; a nullptr output is skipped. Use either
    // process() or processDry() for a given kit, not both.
    void processDry(float* const* outputs, int numSamples);

private:
    struct TriggerEvent { VoiceId voice; float velocity; float semitones; };

    void applyParams();
    void fire(VoiceId v, float velocity, float semitones);
    bool resolveNote(int midiNote, VoiceId& voice, float& semitones) const;
    template <typename Render>
    void runBlock(int numSamples, Render&& render);
    void renderVoices(float* left, float* right, int numSamples);

    static void applyTo(Kick& kick, const std::array<float, kNumParams>& values);
    static void applyTo(HiHat& hat, const std::array<float, kNumParams>& values);

    double sampleRate = 48000.0;
    Kick kick;
    HiHat hat;

    // Kit bus (stereo): voices + room -> glue compressor -> tape saturation
    // -> record stage (tape EQ, wow/flutter, hiss) -> peak limiter -> soft clip
    Room room;
    float roomGain = 0.0f;      // smoothed, follows roomTarget
    float roomTarget = 0.0f;    // from the ROOM knob
    float roomSmoothing = 0.0f; // per-sample coefficient, ~20 ms
    bool roomRunning = false;   // false = room skipped entirely (saves CPU)

    // Idle sleep: once every voice has finished and the bus has been silent
    // (below about -100 dBFS, room tail included) for 0.25 s, the whole bus
    // stops processing. The next hit wakes it instantly.
    bool busAsleep = false;
    int silentSamples = 0;
    BusCompressor busComp;
    Saturator busTape;
    std::array<OnePoleLowpass, 2> busTapeTone;
    std::array<DcBlocker, 2> busDc;
    RecordChain record;
    PeakLimiter limiter;

    std::array<std::atomic<float>, kNumParams> params;
    std::array<float, kNumParams> applied {};
    bool forceApply = true;

    MpscQueue<TriggerEvent, 64> triggers; // pad presses / host notes from any thread
    std::array<std::atomic<uint32_t>, kNumVoices> hitCounts;

    // Notes scheduled by noteOn() for the next process() call (audio thread only)
    struct ScheduledNote { int offset; VoiceId voice; float velocity; float semitones; };
    std::array<ScheduledNote, kMaxNotesPerBlock> scheduled {};
    int numScheduled = 0;

    // Mode (written by the UI thread, read when notes arrive)
    std::atomic<int> modeValue { (int) KitMode::Multi };
    std::atomic<int> singleVoiceValue { (int) VoiceId::Kick };
    std::atomic<int> rootNoteValue { gm::Kick };

    static constexpr int kScratchSize = 256;
    std::array<float, kScratchSize> kickBuffer {}, hatBuffer {};
};

#if defined(_MSC_VER)
 #pragma warning(pop)
#endif

} // namespace a83
