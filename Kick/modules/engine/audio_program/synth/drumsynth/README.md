# drumsynth: Analog ’83 synthesized drum engine

A synthesized drum engine inspired by early-’80s analog drum machines (issue #16). Every sound is generated in real time; there are no samples. This first version has a kick plus closed and open hi-hats.

Destination in KICK: `Kick/modules/engine/objects/drumsynth/`, next to `sampler/`.

## Design

- **Sound only.** Tempo and patterns stay with KICK’s own sequencer, which triggers notes. The engine has no clock of its own.
- **Self-contained C++17.** No JUCE or KICK dependencies, so it can be wrapped in an `AudioObject` (for example behind `MTProgramType::Plugin`) in a follow-up PR.
- **Real-time safe.** There are no locks and no allocations on the audio thread; all memory (about 89 KB) is allocated in `prepare()`. Denormals are flushed on x86 and AArch64, and the bus sleeps when nothing is sounding.
- **Unity-build safe.** The files compile cleanly when included together into one translation unit, as `objects.cpp` does.

## Files

| File | Contents |
|---|---|
| `Analog83Kit.h/.cpp` | The kit: voices, parameters, note scheduling, stereo bus. **Start here.** |
| `Analog83Kick.h/.cpp` | Kick voice |
| `Analog83HiHat.h/.cpp` | Closed/open hi-hat voice (closed chokes open) |
| `Analog83Record.h/.cpp` | Optional character chain: room, tape EQ, wow and flutter |
| `Analog83Common.h` | Shared DSP: filters, saturator, compressor, limiter, denormal guard |
| `ARCHITECTURE.md` | How the engine works: signal flow, threading, voices, bus, parameters, how to add a voice, design notes |
| `CREDITS.md` | Published techniques and values the engine uses, with citations |

## API

```cpp
a83::Kit kit;                    // about 4 KB
kit.prepare(sampleRate);         // allocates; call before audio starts

// UI thread
kit.setParam(a83::Param::KickDecay, 0.6f);
kit.trigger(a83::VoiceId::Kick, 1.0f);              // pad press, plays next block
kit.trigger(42, 1.0f);                              // same, by MIDI note
kit.setMode(a83::KitMode::Single, a83::VoiceId::Kick, 36); // one chromatic voice

// Audio thread, once per block
kit.noteOn(36, 0.8f, offset);    // sample-accurate; offset is the sample in this block
kit.process(left, right, numSamples);              // stereo, with the character bus
// ...or, for a host with its own mixer and effects:
float* outs[a83::kNumDryOutputs] = { kickOut, hatOut };
kit.processDry(outs, numSamples);                  // one dry mono channel per voice
```

### Notes and modes

- **Multi (default):** the full kit. Notes follow the General MIDI drum map: 35/36 kick, 42/44 closed hat, 46 open hat. Other notes are ignored.
- **Single:** one voice played chromatically, for single-voice Plugin programs. The root note plays at the voice's TUNE; every note above or below shifts it by semitones, so a kick across the pads becomes a playable tuned bass.

The `VoiceId` versions of `noteOn()` and `trigger()` play a voice directly in either mode.

### Two ways out

| Method | Output | When to use it |
|---|---|---|
| `process()` | Stereo, with pan, ROOM, BUS COMP, BUS DRIVE, RECORD and the limiter | Standalone use, previews, or a single-program "character" sound |
| `processDry()` | One dry mono channel per voice (`DryOutput::Kick`, `DryOutput::Hat`), with each voice's own DRIVE and LEVEL only | A host with its own mixer and effects, like KICK |

Use one of them per kit. Both share the same sample-accurate note scheduling.

### Threading

- **`trigger()`** is safe from any thread, including several at once (KICK sends notes from both its sequencer and live pads). It's a lock-free multi-producer queue, tested under ThreadSanitizer, and returns false if the queue is full.
- **`setParam()`, `setMode()`, `getHitCount()`** are safe from the UI thread.
- **`noteOn()`, `process()` and `processDry()`** are audio thread only.
- **`renderPreview()`** draws a waveform from the UI thread using its own voices.

## Adding it to the build

Follow how `sampler/` is listed:

- **`objects.h`:** add `#include "drumsynth/Analog83Kit.h"`.
- **`objects.cpp`:** add each `.cpp`:

  ```cpp
  #include "drumsynth/Analog83Kick.cpp"
  #include "drumsynth/Analog83HiHat.cpp"
  #include "drumsynth/Analog83Record.cpp"
  #include "drumsynth/Analog83Kit.cpp"
  ```

Format with the `.clang-format` in `Kick/` before committing. Every file already carries KICK's AGPL-3.0-or-later header.

To play it as a Plugin program (`MTProgramType::Plugin`), see `integration/README.md` in the author's fork, a draft for the follow-up PR.

## Testing

A WAV renderer, a CPU benchmark and a JUCE preview app live in the author’s fork (they are dev tools, not part of KICK). They drive the kit through `noteOn()` exactly as KICK’s sequencer will.
