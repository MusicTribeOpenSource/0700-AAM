# KICK emulator

Runs the real Kick app on macOS against a virtual control surface, before the hardware exists. Kick itself is unmodified apart from `KICK_EMU` guards that route hardware access to stand-ins.

Two parts:

- **Kick (`-DKICK_EMU=ON`)** is the normal app target built for macOS. The modules the public release lacks (`AudIO_juce`, `AudIO_constants`, `juce_serialport`, `PktMgr`) are clean-room stand-ins in `emu/modules/`. The `kick_emu` module holds the app-side hardware layer and a control socket.
- **KICK Surface** (`emu/surface/`) is a separate JUCE app that plays the MCU: pads, buttons, Q-link knobs, encoder, touch strip, LEDs, CV meters, SD and USB slots. It talks to Kick over a pty using the PktMgr serial protocol.

Stage 2 (not built yet) is a QEMU aarch64 VM running the Buildroot rootfs with the same Surface attached over TCP (`--tcp`).

## Build and run

```bash
emu/build.sh            # Kick + Surface into build/emu and build/surface
emu/run.sh              # opens both windows
emu/run.sh --root DIR   # device root, default ~/KickEmu
emu/run.sh --script emu/tests/smoke.script   # headless run, exit code = result
```

Needs Xcode, cmake, ninja, python3. The first build fetches JUCE 7.0.12 and other deps through CPM and applies `emu/patches/juce_7.0.12_macos15_sdk.patch`.

`run.sh` generates 16 synthesized one-shots in `<root>/data/sounds/Emu Kit` on first run. Kick's demo projects are empty, so assign samples to pads from the Kick UI before expecting sound.

## Device root

Everything Kick expects at a Linux path lives under `KICK_EMU_ROOT`:

| Device path | Emulated as |
|---|---|
| `/dev/ttymxc0` | symlink to the Surface pty |
| `/mnt/sdcard`, `/mnt/flash` | `<root>/mnt/...`, shown when the SD/USB toggles are on |
| `/dev/mmcblk1`, `/dev/sda` | marker files the Surface creates for SD / USB presence |
| `/home/Kick/{projects,logs,prefs}` | `<root>/home/Kick/...` |
| `/data/sounds` | `<root>/data/sounds` (factory samples) |
| `/sys/devices/soc0/serial_number` | serial, default `KICKEMU0001` |
| I2C bus 3, addr 0x0C | virtual CV DAC, drives the Surface CV meters |
| backlight, `reboot`, `poweroff` | forwarded to the Surface as events |

## Surface

Mouse: click pads and buttons, drag knobs, scroll or drag the encoder, drag the touch strip. The velocity and pressure sliders set what pad hits send.

Keyboard: pads `z x c v / a s d f / q w e r / 1 2 3 4` (bottom row first), space = Play, return = Enter, arrows = Knob CCW/CW, esc = Back, tab = Stop, shift = Shift, up/down = encoder.

Script commands (`--script`, `#` comments): `wait ms`, `pad 1-16 [vel] [holdMs]`, `button name|id [holdMs]`, `knob 0|1-16 steps` (negative = CCW), `slider 0-100`, `sd in|out`, `usb in|out`, `snapshot file.png`, `expect connected`, `expect pad n colourId|off`, `expect led name on|off`, `expect cv ch minVolts`, `quit [code]`.

## Control socket

Kick listens on `127.0.0.1:50901` (`KICK_EMU_CONTROL_PORT`), one command per line:

```
ping
snapshot <file.png>
tap <x> <y> [holdMs]
drag <x1> <y1> <x2> <y2> [holdMs]
quit
```

Taps go straight to the component under the point. Injecting through the window peer fails on macOS unless the window is topmost.

The Surface listens on UDP `50902` (`KICK_EMU_EVENT_PORT`) for `cv ch raw volts`, `backlight N`, `power reboot|off`, `app started|stopped`.

## Serial protocol

230400 8N1, frames `'M' 'T' type dir cmd size data[size-1] crc8`. CRC-8 poly 0x31, init 0xFF over every prior byte. `size` is data length + 1. Frames to the app carry one trailing `'\n'` because the app's reader takes `size+7` bytes. Frames from the app have no trailer. A frame with no data hangs the app parser, so the Surface never sends one.

Types: SYSTEM 0, BUTTON 1, LED 2, PAD 3, KNOBS 4, MIDI 5, CV 6, SLIDER 7. Handshake: app sends COMM_INIT[1], Surface answers SW_VER[1,0] and status[1], app sends COMM_INIT[2], Surface sends status[2], app answers status[2], link is live. The Surface heartbeats status[2] about every 3 s.

## Assumptions and limits

- The stand-in modules are written from how Kick calls them, not from the closed originals. `AudIO_constants` values are guesses sized to work.
- Audio uses the default CoreAudio output at 48 kHz, 256 frames.
- The camera is a placeholder rectangle. Kick has no camera code.
- Pad aftertouch is sent but the app ignores it.
- Pad LEDs only light when the app sets them (some modes need a loaded program).
- Wifi, firmware update, and resource-update paths are Linux-only and not emulated.
- Stage 2 (QEMU + Buildroot) is not started.
