# KICK CLAP host

A C++17 CLAP host core, a reusable JUCE session/editor, and a KICK Plugin-program
adapter. The core has no JUCE, audio-device, or hardware dependency. The vendored
CLAP headers are MIT licensed; new host code follows KICK's AGPL-3.0-or-later license.
The exact SDK revision is recorded in `third_party/clap/upstream.json`.

## Build and test the core

No dependency downloads or proprietary KICK components are needed:

```sh
cmake -S Kick -B /tmp/kick-clap -DKICK_CLAP_HOST_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/kick-clap --parallel
ctest --test-dir /tmp/kick-clap --output-on-failure
```

Alternatively configure `Kick/clap_host` directly. Install the scanner into the
target image with `cmake --install <build> --prefix /usr`. It must be built for the
same target as KICK. Do not run cross-compiled fixtures on the build host; execute
CTest on the board or through an explicitly configured emulator.

## Build the standalone host and JUCE integration tests

The standalone app uses the same session, project-state, controls, and rendering
code as the KICK integration. Supply a local JUCE 7.0.12 or JUCE 8 checkout:

```sh
cmake -S Kick/clap_host -B /tmp/kick-clap-ui \
  -DKICK_CLAP_BUILD_JUCE=ON -DKICK_CLAP_BUILD_PLAYER=ON \
  -DKICK_CLAP_JUCE_PATH=/path/to/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/kick-clap-ui --parallel
ctest --test-dir /tmp/kick-clap-ui --output-on-failure
```

`kick-clap-player` provides an on-screen MIDI keyboard and audio/MIDI device
settings. The scanner is copied beside the executable, including inside macOS's
development app bundle. The JUCE session test can also load a real plugin:

```sh
/tmp/kick-clap-ui/tests/kick-clap-session-tests /path/to/instrument.clap plugin.unique.id
```

That test renders notes offline and verifies opaque plugin state survives project
XML serialization/restoration. It does not open an audio device. The full KICK
application still needs the release's missing platform dependencies documented
in `../README.md`, including the AudIO JUCE-module glue and `juce_serialport`.

## Use in KICK

1. Create a **Plugin** program, assign it to a track, and press its **Edit** button.
2. **Scan** finds instruments in `CLAP_PATH`, `~/.clap`, `/usr/lib/clap`,
   `/usr/local/lib/clap`, and `/opt/kick/plugins/clap` on Linux. **Add folder**
   adds a persistent custom search folder. macOS development builds use its
   standard CLAP locations too.
3. Choose an instrument and press **Load**. Every descriptor in a bundle has its
   own identity. Linux hardware requires Linux AArch64 binaries and compatible
   runtime libraries; macOS/Windows or Linux x86 binaries do not work there.
4. Edit or search the generated parameter controls. Assign parameters to the four
   knob menus. KICK's physical knobs control those assignments while its dialog is
   active, through the common monolithic/split GUI hardware route. Consumed knob
   events do not also edit screen/Q-Link controls. Stepped parameters move by
   whole encoder ticks in both directions. Mappings are stored in the project.
5. Save the project. Plugin path, exact plugin ID, opaque CLAP state, and knob
   assignments are stored in the program's `CLAP` ValueTree child. Duplicating a
   program first snapshots the live state. Explicit save, autosave, and project
   close/switch share a message-thread preparation barrier before writing XML or
   destroying programs. Missing plugins retain their saved identity and state; selecting the same plugin ID at a new path relinks it.

An instrument without the state extension can play, but cannot provide reliable
project recall; saving reports failure rather than pretending parameters are a
complete substitute for plugin state. A failed snapshot or disk write leaves the
project dirty and preserves the previous saved file. Save errors identify the
program/plugin in a global warning and status indicator; failed close/switch
keeps the current project open for retry or plugin removal. Normal application
quit also checks saving before destroying windows. **Panic** resets held notes.
**Remove** explicitly clears the plugin assignment and saved state.

Set `KICK_CLAP_SCANNER` to an absolute worker executable path when it is installed
somewhere other than beside KICK or in `/usr/bin` / `/usr/local/bin`.
`kick-clap-scan [directories or .clap paths...]` is also a diagnostic CLI.
Scanner errors are shown in the page's count, tooltips, and tappable **Scan errors** dialog.

## Processing and lifecycle

- Main-thread operations: library loading, discovery results, metadata queries,
  plugin activation/deactivation, restart handling, state, timers, file descriptors,
  UI, and destruction. Entry initialization/deinitialization is shared per binary
  across instances, and plugins are selected by their stable ID.
- The host-owned render path never acquires a blocking mutex, allocates memory, reads
  files, or performs library loading. Fixed-size queues accept multiple MIDI
  producers. Events are stably ordered, then rebased when blocks exceed the
  plugin's negotiated maximum size. Parameter cookies are refreshed on rescan.
- A per-instance atomic gate excludes state/reconfiguration work from processing.
  The callback returns silence while the gate is held; a long plugin state save or
  restart can therefore interrupt sound. The control thread never waits for an
  audio callback that may already have stopped. Dirty opaque state is snapshotted
  on the message thread in coalesced 250 ms windows (failed snapshots retry after
  two seconds), even with the editor closed. These automatic snapshots use the
  same gate and can also interrupt sound. Exclusive stop/reset/flush scopes
  are marked as CLAP's **symbolic audio thread**, as explicitly permitted by the
  thread-check specification. They never run concurrently with processing.
- Input/output port layouts are queried rather than assumed stereo. The core
  supports up to 16 audio ports and 64 channels in each direction, with flattened
  planar routing, silent missing inputs, scratch storage for unused outputs, and
  mono-to-stereo duplication for a mono instrument on KICK's stereo bus.
- Native CLAP notes and MIDI note events are translated according to input-port
  dialects. Core callers can provide timestamped parameter/expression events.
  KICK's legacy tick-thread MIDI path has **no intra-buffer timestamps**: it enters
  a bounded queue and is applied at the next block boundary. Adding CLAP does not
  make that upstream sequencer sample-accurate.
- Tempo/beat position and play/stop state are supplied from the assigned KICK
  track. The audio session reads a coherent transport tuple without blocking
  and interpolates between tick updates. Loop/time-signature and recording flags are not yet advertised. A program shared by multiple tracks
  receives their latest transport update; use separate programs for independent
  transport contexts.
- Host extensions include thread-check, parameters, state dirty notifications,
  audio/note ports, latency notifications, timers, POSIX file descriptors, tail
  notifications, and bounded log handling. Main-thread callbacks and restart/
  process/flush requests are serviced by the message loop.
- Idle instruments returning `CLAP_PROCESS_SLEEP` skip DSP until an event, input
  signal, wake request, or transport transition (including Play/Stop, tempo and
  seek). Normal beat progression does not itself wake an idle plugin. Invalid
  process statuses and nonfinite mapped output silence and fault that instance. Event overflow is counted and triggers a reset
  to avoid stuck notes. Per-instance callback-time, drop and error counters are
  available without locks. No thread-pool extension is advertised.

## Deliberate limits

The KICK page hosts **instruments** with generic touchscreen controls. The core
also processes audio effects (tested with aliased input/output), but CLAP effects
are not yet exposed in KICK's existing Airwindows insert browser. Native plugin
editor embedding, timeline parameter-automation recording, MIDI/SysEx routing
from plugin outputs, per-note-expression editing, and delay compensation across
the KICK graph are not implemented. Latency is queried and displayed, not silently
treated as compensated. Plugin output events are bounded and retained by the
core; the KICK session currently drains them after parameter telemetry updates.

Discovery runs each binary in a separate executable with a deadline, output-size
limit, cancellation and crash detection. This is crash isolation, not a security
sandbox. Playback runs in-process: an arbitrary plugin segmentation fault can
still terminate the host. Processing exceptions, process error statuses and nonfinite
audio are contained; native memory corruption requires future process isolation.

## Verification

CTest uses actual shared-library fixtures, not mocked plugin calls. Coverage
includes exact descriptor selection, scan crashes/hangs, multiple instances of a
binary, lifecycle/thread checks, MIDI/native note conversion, frame offsets,
oversized blocks, mono/stereo and effect routing, parameters/gestures, queued
parameter state saving, opaque state recall, concurrent control/rendering,
bounded-event overflow/panic, idle sleep/wake, process/start/activation failures,
and a render-time C++ allocation guard. Optional JUCE tests cover MIDI/audio,
transport-start wake-up without MIDI, continuous/stepped macro edits, missing
plugin relinking, duplicate/recall, and the actual KickXML autosave/close paths.
Persistence tests cover failed/unsupported state, preserved disk/opaque snapshots,
message-thread marshaling, dirty/error notification, and successful retry. Common
hardware-contract tests cover exclusive dialog ownership, pass-through, lifetime,
worker dispatch, and the actual split GUI IPC/keyboard producers. Test fixtures
are never installed into the target plugin directory.

Enable `-DKICK_CLAP_SANITIZE=ON` for ASan/UBSan on the core, consumers, and all
fixture MODULE libraries. The render guard supplies scalar/array, sized, aligned,
and nothrow allocation/deallocation pairs. `.github/workflows/clap-host.yml` runs
Linux release and default-settings GCC/Clang sanitizer suites, plus JUCE 7/8
integration suites with a headless display. No allocator-mismatch suppression is
used. Local AppleClang 15 ASan runtime initialization currently aborts even in a
minimal empty executable; that environment failure does not clear Linux ASan.

Actual hardware boot, ALSA round-trip latency, xrun behavior, and performance with
Linux AArch64 plugins must be measured on the target. Desktop results do not
establish a safe voice count or buffer size for the i.MX8M Mini.
