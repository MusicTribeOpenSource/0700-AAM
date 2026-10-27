# AudIO

AudIO is the audio engine library used by Kick. It is a patchable graph of audio objects (oscillators,
ADSR, mixer, playback and recorder, SoundFont synth, more than 300 Airwindows effects) with buses,
chains and a small DAW layer. It can be used as a JUCE module (`AudIO/AudIO.h`) or on its own through
CMake. See `AudIO/Example/` for usage.

## License

AudIO is licensed under the GNU Affero General Public License v3.0 or later, see [LICENSE](LICENSE).
Bundled third-party code keeps its own license.

## KICK JUCE integration

`juce_modules.cmake` registers the AGPL `AudIO_constants` and `AudIO_juce` modules.
`src/audio_core.cpp` compiles the released core/Airwindows once; the JUCE adapter
adds an audio device, recorder/looper with overdub and one-level undo/redo, WAV
export, ADSR and stereo filter adapters. The device uses a fixed-block pipeline
with one graph block of added latency. Capture/preview and device adaptation are
covered by `Kick/tests/platform`; the actual application is built in CI.

The constants bridge supplies software defaults needed by the released UI; it
does not recover the missing private library's exact hardware configuration.
See [KICK build instructions](../Kick/README.md) for configuration and limitations.

## Removed from this release

- The embedded General MIDI SoundFont (`AWE32_1MB.h`, copyright E-mu Systems). It is now an empty stub,
  so supply your own `.sf2`.
- The demo drum samples in `AudIO/Samples/`.

## Third-party code included

| Component | Location | License |
|---|---|---|
| Airwindows (Chris Johnson) | `AudIO/airwindows` | MIT |
| AudioFile (Adam Stark) | `AudIO/3rdparty/AudioFile.h` | MIT |
| parallel-hashmap | `AudIO/3rdparty/phmap` | Apache-2.0 |
| smbPitchShift (Stephan M. Bernsee) | `AudIO/3rdparty/smbPitchShift.h` | Wide Open License |
| TinySoundFont / TinyMidiLoader (Bernhard Schelling) | `AudIO/Objects/SoundFontSynthesis/tsf.h`, `AudIO/Example/Synth/tml.h` | MIT |
| minisdl_audio (from TinySoundFont, based on SDL) | `AudIO/Devices/minisdl_audio.*` | zlib |
| Dear ImGui, ImGuiFileDialog, GLFW, stb | `AudIO/Example/Synth/Thirdparty` | MIT / zlib / public domain (see each folder) |
