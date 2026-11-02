# Analog ’83 drum engine: architecture

A guide for developers working on, reviewing, or integrating the Analog ’83 engine in KICK.

## Contents

1. [What it is](#what-it-is)
2. [Design principles](#design-principles)
3. [Files](#files)
4. [Signal flow](#signal-flow)
5. [Using the engine](#using-the-engine)
6. [Threading and real-time safety](#threading-and-real-time-safety)
7. [The voices](#the-voices)
8. [The bus and the record chain](#the-bus-and-the-record-chain)
9. [Parameters](#parameters)
10. [Adding a voice](#adding-a-voice)
11. [Testing and performance](#testing-and-performance)
12. [Design notes: problems found and fixed](#design-notes-problems-found-and-fixed)
13. [Credits and provenance](#credits-and-provenance)

## What it is

Analog ’83 is a synthesized drum engine, inspired by early-’80s analog drum machines. Every sound is generated in real time from oscillators, filters and envelopes; there are no samples. This first version has a **kick** and a **closed/open hi-hat**.

An optional character chain (glue compression, tape saturation, tape EQ with wow and flutter, and a small room) gives it the sound of drums on a finished record rather than straight off the machine. Every stage of that chain can be set to 0 for a clean output.

**What it isn't:** a sequencer. Tempo and patterns belong to the host (on KICK, its own sequencer), which sends notes.

## Design principles

- **Sound only.** The engine takes notes and parameter values, and returns audio.
- **Self-contained.** Plain C++17 with no JUCE or KICK dependencies, so it can be tested on any desktop and wrapped by any host.
- **Real-time safe.** On the audio thread there are no locks, no memory allocation, and no denormal slow paths.
- **Deterministic.** The same notes and settings always produce the same audio, on every compiler and with KICK’s `-Ofast -ffast-math` flags, so the test suite can fingerprint the sound.
- **One source of truth for parameters.** A single table defines every knob’s label, range, default and unit.

## Files

| File | Contents |
|---|---|
| `Analog83Kit.h/.cpp` | The kit: owns the voices and parameters, receives notes, schedules them, and runs the mix bus. **The only class a host talks to.** |
| `Analog83Kick.h/.cpp` | Kick voice |
| `Analog83HiHat.h/.cpp` | Closed/open hi-hat voice |
| `Analog83Record.h/.cpp` | Character chain: room, wow and flutter, tape EQ, hiss; plus `Biquad` |
| `Analog83Common.h` | Shared DSP building blocks and real-time helpers |
| `README.md` | API summary and build instructions |
| `CREDITS.md` | Published techniques and values the engine uses |

Everything lives in the `a83` namespace.

## Signal flow

```
 notes (trigger / noteOn)
        |
        v
 +--------------+    +---------------+
 | Kick voice   |    | Hi-hat voice  |      each voice: oscillators -> filters ->
 +--------------+    +---------------+      envelope -> per-voice DRIVE -> LEVEL
        |                   |
        +---------+---------+---------------------------> processDry(): one dry
                  |                                         mono channel per voice
                  v
   mix: kick center, hats slightly right
                  |
   ROOM (hats only; off by default) ---+
                  |                     |
                  +<--------------------+
                  v
   BUS COMP -> BUS DRIVE -> RECORD (tape EQ, wow/flutter, hiss)
                  |
   peak limiter -> soft clip (ceiling -0.3 dBFS) ------> process(): stereo out
```

## Using the engine

```cpp
#include "Analog83Kit.h"

a83::Kit kit;                       // about 4 KB
kit.prepare(sampleRate);            // allocates everything; call before audio starts

// UI or any other thread
kit.setParam(a83::Param::KickDecay, 0.6f);
kit.trigger(42, 0.8f);              // closed hat, plays at the start of the next block
kit.setMode(a83::KitMode::Single, a83::VoiceId::Kick, 36);  // one chromatic voice

// Audio thread, once per block
kit.noteOn(36, 1.0f, 37);           // kick on sample 37 of this block
kit.process(left, right, numSamples);   // stereo; ADDS into the buffers
// or, for a host with its own mixer and effects:
float* outs[a83::kNumDryOutputs] = { kickOut, hatOut };
kit.processDry(outs, numSamples);   // one dry mono channel per voice; ADDS
```

### Notes

Notes are MIDI note numbers with velocity from 0 to 1.

- **Multi mode (default):** the General MIDI drum map. 35/36 play the kick, 42/44 the closed hat, 46 the open hat; other notes are ignored and return `false`.
- **Single mode:** every note plays one chosen voice, shifted by `note − rootNote` semitones from its TUNE. A kick across the pads becomes a tuned bass.

There are two ways in:

| Method | Thread | Timing |
|---|---|---|
| `trigger(note, velocity)` | Any thread, any number at once | At the start of the next block, as KICK’s samplers do |
| `noteOn(note, velocity, offset)` | Audio thread only | On an exact sample inside the next `process()` / `processDry()` call |

Both also accept a `VoiceId` instead of a note number.

### Outputs

Use **one** of these per kit:

- **`process()`** gives stereo with the full bus.
- **`processDry()`** gives one dry channel per voice (`DryOutput::Kick`, `DryOutput::Hat`). Each voice keeps its own DRIVE and LEVEL, but there's no pan, room, bus or tape. A `nullptr` output is skipped while its voice still advances.

## Threading and real-time safety

| Call | Thread |
|---|---|
| `prepare()` | Before audio starts (it allocates) |
| `setParam()`, `setMode()`, `getParam()`, `getHitCount()`, `renderPreview()` | UI thread |
| `trigger()` | Any thread, including several at once |
| `noteOn()`, `process()`, `processDry()` | Audio thread only |

**No locks.** Parameters are atomics, read once per block in `applyParams()` and applied only when something changed. Notes from other threads go through `MpscQueue`, a bounded lock-free multi-producer, single-consumer queue (Dmitry Vyukov’s design). Producers claim a slot with one compare-and-swap; the audio thread never waits. When the 64-slot queue is full, `trigger()` returns `false` rather than dropping the note silently.

**No allocation.** All memory (about 89 KB) is allocated in `prepare()`. The test suite counts heap allocations across 3,000 audio callbacks and requires zero.

**No denormals.** Decaying filters and reverb tails drift into the denormal float range, which can make x86 CPUs 10–100× slower, a classic cause of dropouts. `ScopedFlushDenormals` sets flush-to-zero for each audio block (FTZ/DAZ on x86, FZ on AArch64) and restores the previous mode afterward. Fast envelopes also snap to exactly zero below 1e-9, so the voices stay safe even when used outside the kit.

**Idle sleep.** Once both voices are silent and the bus has stayed below about −100 dBFS (measured before the tape hiss) for 0.25 s, the bus stops processing entirely. The next note wakes it with no added latency.

**Sample-accurate scheduling.** Each block, `runBlock()` sorts the notes scheduled with `noteOn()`, then renders in chunks that end exactly at each note, so every hit starts on its sample. `process()` and `processDry()` share this code path.

## The voices

Both voices follow the same interface: `prepare()`, parameter setters, `trigger(velocity, pitchSemitones)`, a `process(out, n)` that **adds** into `out`, and `isActive()`.

### Kick

A sine oscillator whose pitch is recomputed every sample:

```cpp
freq = baseHz * hitRatio * drift
     * (1 + punch * kMaxSweep * pitchEnv + kDroopAmount * droopEnv);
```

- **`baseHz`:** 50 Hz × TUNE. **`hitRatio`:** the per-hit pitch used in Single mode.
- **Punch:** at 100%, a hit starts at 3.5× the base pitch and drops into its note with a 10 ms time constant.
- **Droop:** the body starts 6% sharp and settles over about 50 ms.
- **Drift:** ±0.4% random pitch per hit, so no two hits are identical.
- **Beater:** a 6 ms low-passed pulse adds the attack thump. TONE controls the beater and the top end.
- **Then** tube/tape saturation (DRIVE), a tone lowpass (2–10 kHz), and a DC blocker.
- **The decay** uses `roundedTail()`, which eases the last 30 dB to zero along an S-curve.
- **On retrigger,** the previous sound crossfades out over 2 ms instead of being cut.

### Hi-hat

- **Six square-wave oscillators** at inharmonic frequencies (205.3, 304.4, 369.6, 522.7, 540 and 800 Hz) are summed. The clash between them is what makes the sound metallic.
- **Each square is band-limited with polyBLEP,** which smooths its edges over one sample. That gives about 15 dB less aliasing than naive squares.
- **The oscillator phases are never reset,** so each hit starts at a different point and sounds slightly different.
- **The chain:** a band-pass whose center TONE moves from 7 to 12 kHz, then saturation, then an envelope with a 0.3 ms attack (like an analog VCA opening, so there's no click), then a high-pass at 6.5 kHz.
- **Closed and open hats are one voice** with two decay times, so a closed hit chokes a ringing open hat.

## The bus and the record chain

| Stage | What it does | Default |
|---|---|---|
| Mix | Kick centered; hats left 0.78 / right 1.0 | |
| ROOM | Freeverb-style room (4 combs, 2 all-passes, 9 ms pre-delay). Only hats (and future snare, clap, percussion) are sent; the kick and low/mid toms stay dry. The send is band-limited to 250 Hz–9 kHz. Smoothed, and bypassed entirely at 0. | 0% (off) |
| BUS COMP | Stereo-linked compressor: 4 ms attack, 60 ms release. The detector runs every sample; the gain math runs every 8 samples and is interpolated in between. | 15% |
| BUS DRIVE | Tape saturation with a softened top | 15% |
| RECORD | Head bump (+3 dB at 72 Hz at 60%), rumble cut, top roll-off, wow (0.45 Hz) and flutter (6.2 Hz), and hiss (about −74 dBFS) | 60% |
| Mastering | Peak limiter (threshold 0.88, 1 ms attack, 80 ms release), then a soft clip (clean below 0.78, ceiling 0.97 ≈ −0.3 dBFS) | always on |

BUS COMP is kept light on purpose: at 50% it flattened velocity, so a half-velocity kick came out only 0.9 dB quieter, while adding almost no loudness. At 15%, a half-velocity kick is about 3 dB quieter at the same loudness.

## Parameters

All parameters are set through `Kit::setParam(Param, value)`, in the units shown. `paramInfo(Param)` returns each one's label, range, default and unit, for building a UI.

| Param | Range | Default |
|---|---|---|
| `KickTune` | −24 to +24 semitones | 0 |
| `KickDecay` | 0.01–2.0 s | 0.38 s |
| `KickPunch` | 0–100 | 72 |
| `KickTone` | 0–100 | 40 |
| `KickDrive` | 0–100% | 18% |
| `KickLevel` | −60 to +6 dB | 0 dB |
| `HatTune` | −12 to +12 semitones | 0 |
| `HatClosedDecay` | 0.01–0.3 s | 0.06 s |
| `HatOpenDecay` | 0.1–2.0 s | 0.45 s |
| `HatTone` | 0–100 | 50 |
| `HatDrive` | 0–100% | 18% |
| `HatLevel` | −60 to +6 dB | −3 dB |
| `BusDrive` | 0–100% | 15% |
| `BusComp` | 0–100% | 15% |
| `Record` | 0–100% | 60% |
| `Room` | 0–100% | 0% |

The 18% drive measures about 3% THD on a full-scale sine, mostly 2nd harmonic: warmth without audible break-up.

## Adding a voice

For example, a snare. Each step names where the change goes.

1. **Write the voice class,** `Analog83Snare.h/.cpp`, following the existing voices:
   - `prepare(sampleRate)` and parameter setters.
   - `trigger(velocity, pitchSemitones)`.
   - `process(out, n)`, which **adds** into `out`.
   - `isActive()`.

   Build it from the parts in `Analog83Common.h`. Don't allocate in `process()`. Snap fast envelopes to zero with `snapToZero()`, and give file-local constants voice-specific names (for example `kSnareSilence`): `objects.cpp` compiles files together, so two files with the same constant name clash.
2. **`Analog83Kit.h`:**
   - Add the voice to `VoiceId` and `DryOutput`, before `Count`.
   - Add its parameters to `Param`.
   - Add the voice member and a `kScratchSize` render buffer.
3. **`Analog83Kit.cpp`:**
   - Add matching rows to `kParamInfo`, **in the same order as `Param`**.
   - Add an `applyTo()` overload for the new voice.
   - Call its `prepare()` in `Kit::prepare()`.
   - Add a case in `fire()`.
   - Add its GM notes in `voiceForNote()` (snare 38/40, clap 39).
   - In `renderVoices()`: render it, add it to the mix with a pan, set its room send (hats, snare, clap and percussion use the room; kick and low/mid toms stay dry), and include it in the idle-sleep check.
   - Add it to `processDry()` and `renderPreview()`.
4. **Build:** add `#include "drumsynth/Analog83Snare.cpp"` to `objects.cpp`.
5. **Credits:** if you use a published technique or value, add it to `CREDITS.md`.
6. **Tests:** add a note-map check and a demo render. Listen first, then run `analog83_tests --update` to record its fingerprint.

## Testing and performance

The test tools live in the author's development repository, linked from the pull request, since they aren't part of KICK. Each builds on any desktop with CMake.

- **`analog83_tests`:** 27 checks.
  - The note map, chromatic pitch accuracy (within 3 cents), and dry-output timing and separation.
  - Four threads sending notes at once, with every accepted note fired exactly once.
  - Zero allocations across 3,000 audio callbacks.
  - Fingerprints of seven demo renders. Normal runs compare each render's level every 20 ms within 0.25 dB; `--strict` also requires bit-exact samples.
- **`analog83_bench`:** the cost of each stage and of the whole kit.
- **Also verified:** AddressSanitizer and ThreadSanitizer report nothing; all 27 checks pass with KICK's `-Ofast -ffast-math` flags; and the engine compiles as a single unity file the way `objects.cpp` builds it, with no collisions with KICK's macros.

**Performance,** measured on x86 at 48 kHz stereo with KICK's flags:

| Scenario | Cost |
|---|---|
| Demo beat | about 0.55% of one 2.8 GHz core (0.66% with ROOM at 100%) |
| Idle | about 0% (the bus sleeps) |
| Memory | 89 KB |

The estimate for one Cortex-A53 core (KICK's i.MX 8M Mini) is 2.5–4% for the demo beat. That's an estimate; it needs measuring on the hardware.

## Design notes: problems found and fixed

These came up during development and shaped the code, so they're worth knowing before changing it.

- **Denormal spikes.**
  - *Problem:* after a hit with the room on, the cost rose from about 143 to 1,821 ns per sample as tails decayed into denormal range. The kick's fast envelopes had the same problem.
  - *Fix:* `ScopedFlushDenormals` plus `snapToZero()`. The denormal case now costs 0.2 ns per sample.
- **A single-producer queue fed by many threads.**
  - *Problem:* the first trigger queue assumed one sending thread. With four threads sending, ThreadSanitizer reported a data race, and 34,169 of 80,000 events were lost or garbled. KICK sends notes from both its sequencer and live pads.
  - *Fix:* the multi-producer `MpscQueue`, which delivered 80,000 of 80,000.
- **A retrigger click.**
  - *Problem:* when a ringing kick was retriggered, its tone filter still held the old signal while the crossfade added it again, so the old sound was counted twice.
  - *Fix:* each hit now starts with clean filters, and only the crossfade carries the old sound.
- **Hits landing exactly on a whole sample.**
  - *Problem:* a hit that falls exactly on a sample in exact math (step 7 at 140 BPM is sample 36000) can compute as 35999.9999… and land a sample early, depending on compiler flags.
  - *Fix:* the engine places notes exactly where it's told; the test tools' timing math now uses a 1e-6 epsilon.
- **Compression flattening velocity.** Covered under [the bus](#the-bus-and-the-record-chain): BUS COMP now defaults to 15%.
- **Sample-rate dependence.** The room's damping was a per-sample constant, which made the room brighter at higher sample rates. It's now converted from its 48 kHz value, and the sound matches at 44.1, 48 and 96 kHz.

## Credits and provenance

This is original code, written for this module. It builds on published DSP techniques and values, all cited in [`CREDITS.md`](CREDITS.md), including Freeverb's public-domain room tuning. No proprietary code, firmware, schematics or samples were used. It was developed with substantial help from an AI assistant (Claude, by Anthropic) and reviewed and tested by the author.

Licensed under AGPL-3.0-or-later, like the rest of KICK.
