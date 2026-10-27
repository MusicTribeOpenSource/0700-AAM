# Kick

Application software for the BE 0700-AAM, written in C++ with [JUCE](https://juce.com).

## License

Kick is licensed under the GNU Affero General Public License v3.0 or later, see [LICENSE](LICENSE).
Bundled third-party code keeps its own license (see below).

## Building

The released sibling `AudIO` and `ComProtocol` sources are integrated locally.
AGPL replacements provide the missing JUCE device/recorder/DSP bridge and POSIX
serial port. No private platform repository or `juce_serialport` download is needed.
The full app uses **JUCE 7.0.12**; the independent [CLAP host](clap_host/README.md)
also supports JUCE 8.

```sh
cmake -S Kick -B /tmp/kick-build -DKICK_JUCE_PATH=/path/to/JUCE-7.0.12 \
  -DCMAKE_BUILD_TYPE=Debug -DTIMESTAMP=OFF \
  -DKICK_BUILD_PLATFORM_TESTS=ON -DKICK_CLAP_BUILD_TESTS=ON
cmake --build /tmp/kick-build --parallel 2
ctest --test-dir /tmp/kick-build --output-on-failure
```

Omit `KICK_JUCE_PATH` to fetch the pinned JUCE tag through CPM. On Linux, CMake
applies the touchscreen patch to an owned copy under the build directory, leaving
the supplied source/cache unchanged. Remove that owned copy to refresh it after
editing an external JUCE checkout. `KICK_AUDIO_SOURCE` and `KICK_PROTOCOL_SOURCE`
can override the sibling directories.

Ubuntu dependencies: `libasound2-dev libjack-jackd2-dev libx11-dev libxext-dev libxinerama-dev
libxrandr-dev libxcursor-dev libxi-dev libfreetype-dev libfontconfig1-dev
libgl1-mesa-dev`. Headless GUI tests also need `xvfb xauth`; run CTest with
`xvfb-run -a`. The build includes `Kick`, `KickUserInterface`, `KickDeviceControl`,
`KickAudioEngine`, `KickAudioModel` and `KickMain`. Platform tests cover serial
framing/PTY I/O, recorder/looper behavior, fixed-block audio adaptation, and a real
KICK application CLAP project create/duplicate/save/reopen/render/shutdown cycle.

Desktop builds default to `KICK_ENABLE_HARDWARE=OFF`. Linux board builds must opt
in with `-DKICK_ENABLE_HARDWARE=ON`; this enables board serial defaults, I2C, USB,
network/system settings and firmware operations. `KICK_NO_HARDWARE=1` disables
these board operations at runtime. `KICK_SERIAL_DEVICE` selects a serial endpoint;
`KICK_NO_SERIAL_DEVICE=1` disables it. `KICK_NO_AUDIO_DEVICE=1` disables audio-device
startup for offline application tests. These settings do not emulate the board.

`KICK_RESOURCE_ROOT` selects a directory containing `assets`, `templates` and
`demos`. `KICK_DATA_ROOT` redirects writable preferences, projects, logs and
sounds. Without these variables the application retains its legacy paths. Tests
use a temporary data root and never modify a user's projects or board settings.

The JUCE audio bridge adapts arbitrary driver buffer sizes to the graph pool's
fixed block size (default 480 samples). It adds **one graph block of pipeline
latency**, 10 ms at 48 kHz, in addition to device/plugin latency. The callback uses
preallocated storage; driver restart and recorder rate conversion run outside DSP.
Software bus counts/default ranges are declared in
`../AudIO/AudIO/Includes/Constants.h`; they are replacement defaults, not confirmed
physical board specifications. Factory samples, fonts and SoundFont data remain
absent; load content you have rights to use.

For cross compilation, build an SDK from the companion Buildroot tree
(`make KTKick_defconfig && make sdk`) and pass its
`share/buildroot/toolchainfile.cmake` as `CMAKE_TOOLCHAIN_FILE`.
See `how-to/Kick_Compilation.txt`. Desktop and emulator tests cannot certify board
boot, touch/serial wiring, ALSA latency/xruns or third-party AArch64 plugins. Use
the CLAP benchmark tools on the target before setting supported voice/buffer limits.
Playback process isolation/recovery, graph latency compensation and a sample-timed
sequencer automation path remain separate host upgrades; playback is currently
in process. See the CLAP host README for the exact supported contract.

## Removed from this release

Material that could not be redistributed was removed. The code may still refer to it by path:

- Audio samples (factory content and metronome / percussion sounds).
- The Myriad Pro font. Supply any font you have the rights to.
- The prebuilt aarch64 SDK tarball.
- `modules/3rdParty/juce_serialport`, which has no license.

## Third-party code included

| Component | Location | License |
|---|---|---|
| CPM.cmake | `get_cpm.cmake` | MIT |
| cmake_timestamp (Kalle Raiskila) | `timestamp/` | see `timestamp/license.txt` |
| dywapitchtrack (Antoine Schmitt) | `modules/3rdParty/dywapitchtrack/src` | MIT |
| fpm | `modules/3rdParty/fpm` | MIT |
| juce-toys, JUCE natvis (Jim Credland) | `modules/3rdParty/juce-toys`, `juce.natvis` | MIT |
| magic_enum | `modules/3rdParty/magic_enum` | MIT |
| melatonin_parameters | `modules/3rdParty/melatonin_parameters` | MIT |
| moodycamel ConcurrentQueue | `modules/3rdParty/moodycamel` | see its `LICENSE.md` |
| NanoLog | `modules/3rdParty/NanoLog` | MIT |
| signal | `modules/3rdParty/signal` | MIT |
| dRowAudio waveform | `modules/3rdParty/waveform` | MIT |
| smbPitchShift (Stephan M. Bernsee) | `modules/engine/objects/sampler/smbPitchShift` | Wide Open License |
| stftPitchShift (Juergen Hock) | `modules/engine/objects/sampler/stftPitchShift` | MIT |
| Gaussian blur (Roland Rabien) | `modules/kick_ui_controls/source/kick_utils_blur.h` | BSD 3-Clause |
| ttymidi (Thiago Teixeira) | `apps/midiPort/src/ttymidi.c` | GPL-3.0-or-later |
