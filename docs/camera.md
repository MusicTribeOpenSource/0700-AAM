# Camera capture service

`kick-camera` is the capture half of the gesture feature. It opens the camera, pulls frames at a fixed low cost, and hands them to other processes. It knows nothing about gestures. Recognition is a separate service that reads what this one publishes.

Source: `Kick/services/kick_camera/`. Plain C++17, no JUCE, no libraries beyond libc and libstdc++, so it adds little to the image (a stripped aarch64 build is about 130 KB).

Nothing here has run on a KICK. The kernel config shows an OV5640 on MIPI CSI-2 and a UVC driver, and everything else is inferred from that. See "Not verified on hardware" at the end.

## How it fits together

```
 camera (V4L2 node, webcam, file, pipe)
        |
   FrameSource            one backend per input, all return YUYV or NV12
        |
   capture thread  ---->  shared memory ring  /kick-camera   (3 slots, seqlock, read-only for readers)
        ^
 control socket  /run/kick-camera.sock   enable, disable, status, lock_exposure, ...
```

The service starts with the camera closed. A client sends `enable` and the device is opened. `disable` closes it again.

## Why shared memory for frames, and a Unix socket for control

I picked shared memory over sending frames down a socket for four reasons.

- **No copy per reader.** A 320x240 YUYV frame is 150 KB, 2.3 MB/s at 15 fps. Over a socket the service would copy that once per reader and the kernel would copy it again. With shared memory the service writes once and each reader copies only when it wants a frame.
- **Readers cannot block the writer.** Readers map the memory read-only and only ever load from it. A reader that stalls, crashes or is slow misses frames and that is all. A socket with a slow reader either blocks the writer or forces a drop policy I would have to write and test.
- **No network.** The unit file gives this service no network namespace at all (`PrivateNetwork=yes`, `IPAddressDeny=any`). Shared memory and a path-based Unix socket work without one.
- **Latest frame wins.** Gesture recognition wants the newest frame, not a queue of old ones. A ring with "newest complete frame" matches that directly.

The control channel is a Unix stream socket because commands are rare, small and need an answer. It is mode 0600, so only root can drive it.

The cost of this choice: a reader must follow the seqlock protocol below. That is about 30 lines, and `RingReader` in `shm_ring.h` already does it.

## The frame ring

Defined in `Kick/services/kick_camera/include/kick_camera/shm_ring.h`. The header file is the reference. In short:

- POSIX shared memory object `/kick-camera` (configurable). Mode 0600, so only the owning user can map it. kick-gesture and the Kick app run as the same user.
- One `RingHeader` (64 bytes), then 3 slots. Each slot is a 64-byte `RingSlot` followed by the pixel payload.
- The ring is sized for the largest allowed frame (`max_size`, default 640x480 YUYV, 600 KB per slot, about 1.8 MB in total), so changing the frame size never resizes the memory.

Header fields a reader cares about:

| Field | Meaning |
|---|---|
| `magic`, `version` | `'KCAM'` and 1. Check both before trusting anything else. |
| `capturing` | 1 while the camera device is open. This is what the on-screen indicator follows. |
| `width`, `height`, `stride`, `fourcc`, `payloadBytes` | Current frame format. `fourcc` is the V4L2 value for `YUYV` or `NV12`. NV12 has a luma plane of `stride * height` bytes, then an interleaved UV plane of `stride * height / 2`. |
| `latest` | Number of the newest complete frame, 0 when there is none. |
| `formatGen` | Odd while the writer changes the format fields. |

Timestamps are `CLOCK_MONOTONIC` nanoseconds, so a reader compares them with its own monotonic clock. The age of a frame is `now - timestamp`.

### Reading a frame

The writer works on frame `n` in slot `n % 3`. A reader:

1. Loads `latest` (acquire). If it is 0 or equals the number it saw last, there is nothing new.
2. Reads the format fields and `formatGen`. If `formatGen` is odd, the writer is changing format, so retry.
3. Loads the slot's `seq`. If it is odd, the writer is inside that slot, so retry.
4. Copies the payload.
5. Loads `seq` and `frameNo` again. If `seq` changed, or `frameNo` is not `n`, or `formatGen` changed, the copy is torn, so throw it away and retry from step 1.

With three slots the writer only gets back to a reader's slot after producing `n + 3`, so a reader has two to three frame periods (130 to 200 ms at 15 fps) to finish a copy. A reader that copies 150 KB in well under a millisecond practically never retries. `RingReader::readLatest` does all of this and gives up with `Torn` after 8 attempts.

The payload is stored and loaded as relaxed 64-bit atomics on both sides. That keeps the protocol free of data races in the C++ memory model, so ThreadSanitizer stays quiet, and it costs nothing measurable at this frame size.

A reader that sees `latest` go down should treat it as a service restart (frame numbers count up within one run only).

`kick-camera-peek` (built next to the service) is a working example: it maps the ring read-only in a separate process and writes the luma plane as a PGM.

## Control commands

One command per line, one reply line. `kick-camera --socket PATH --ctl COMMAND` sends one.

| Command | Effect |
|---|---|
| `enable` | Opens the camera and starts publishing. Replies `ok` or `err <reason>`. Does nothing if already streaming. |
| `disable` | Stops capture, closes the device, zeroes every pixel in the ring. |
| `status` | One line of JSON: `state` (`off`, `streaming`, `ended`, `error`), `capturing`, `source`, `width`, `height`, `stride`, `format`, `fps`, `frames`, `dropped`, `frame_age_ms`, `preset`, `exposure_locked`, `wb_locked`, `debug_dump`, `error`. The stream fields are only present while streaming. |
| `lock_exposure`, `unlock_exposure` | Freeze exposure at the value the sensor is using, or go back to what the preset says. |
| `lock_wb`, `unlock_wb` | Turn automatic white balance off or on. |
| `preset lit_room` or `preset dim_stage` | Choose the exposure and white-balance starting point. |

`ended` means a finite source ran out or a pipe closed. `error` means the device failed or refused to open, with the reason in `error`. In both cases the device is already closed and `enable` can be sent again.

## Exposure and white balance

Auto exposure that keeps adjusting while a hand moves wrecks gesture detection, so the service starts from a preset and can freeze the sensor.

| Preset | Exposure | Frame rate in low light | White balance |
|---|---|---|---|
| `lit_room` | automatic | held fixed (`V4L2_CID_EXPOSURE_AUTO_PRIORITY` = 0) | automatic |
| `dim_stage` | manual, 5/6 of a frame period (555 units of 100 us at 15 fps) | held fixed | automatic |

The controls used are `V4L2_CID_EXPOSURE_AUTO`, `V4L2_CID_EXPOSURE_ABSOLUTE`, `V4L2_CID_EXPOSURE_AUTO_PRIORITY` and `V4L2_CID_AUTO_WHITE_BALANCE`. For "automatic exposure" the service tries aperture priority first (what UVC cameras call it) and plain auto second (what sensor drivers call it). A control the driver does not have is skipped and listed once in the log, not treated as an error.

`lock_exposure` reads the current `EXPOSURE_ABSOLUTE`, switches to manual, and writes the same value back. `lock_wb` switches automatic white balance off, which on most sensors leaves the last gains in place.

The `dim_stage` number is my estimate. I chose 5/6 of the period so the shutter stays open as long as it can without costing frames. It has not been compared to a real stage. Treat both presets as starting points to tune on the unit.

## Privacy

- **Off by default.** After start-up the device is closed. Only `enable` opens it. `enable=true` in the config opens it at start-up, and the shipped config does not set it.
- **Indicator.** `capturing` in the ring header is raised just before the device opens and lowered just after it closes. The UI should draw its camera indicator from that flag, so it cannot show "off" while the device is open. It can read "on" for a few milliseconds around an open that fails.
- **Pixels do not outlive the camera.** `disable`, and any source failure, closes the device and zeroes all slots.
- **No frames on disk.** Frames are written only if `debug_dump` names a directory, and then at most `debug_dump_max` (default 100). The service logs a line when that mode is on, and `status` shows `"debug_dump":true`.
- **No network.** The systemd unit uses `PrivateNetwork=yes`, `IPAddressDeny=any` and `RestrictAddressFamilies=AF_UNIX`. It also has `ProtectSystem=strict` with write access to `/run` and `/dev/shm` only, and a device policy that allows video nodes only.

## Configuration

`/etc/kick-camera.conf` (key=value, `#` comments), overridden by command-line flags of the same name (`--debug-dump DIR`). Defaults:

| Key | Default | Notes |
|---|---|---|
| `source` | `v4l2` | `v4l2`, `file`, `pattern`, `pipe` |
| `device` | `/dev/video0` | placeholder until the unit's real node is known |
| `size` | `320x240` | up to `max_size` |
| `fps` | `15` | 1 to 60 |
| `format` | `auto` | `auto` tries YUYV, then NV12; or `yuyv`, `nv12` |
| `max_size` | `640x480` | fixes the ring's slot size |
| `preset` | `lit_room` | `dim_stage` |
| `shm`, `socket` | `/kick-camera`, `/run/kick-camera.sock` | |
| `file`, `loop` | | file source only |
| `pipe_command` | | pipe source only: a shell command that writes raw frames to stdout. Empty reads stdin. |
| `debug_dump`, `debug_dump_max` | off, 100 | see Privacy |
| `enable` | `false` | open the camera at start-up |

The service refuses a size that the device adjusts to something larger than `max_size`.

## Frame sources

All of them return YUYV or NV12.

- **`v4l2`**: single-plane capture with mmap buffers (`REQBUFS`, `QBUF`, `DQBUF`), four buffers, format negotiated per the config. The MIPI CSI sensor and a USB UVC webcam are both `/dev/videoN` capture nodes, so one backend covers both and `device` is the only difference. A multi-planar capture node is refused with a clear message. Linux only. Frames lost to the driver (sequence gaps, buffers flagged in error) show up as `dropped` in `status`.
- **`file`**: loops a Y4M file (4:2:0 becomes NV12, 4:2:2 becomes YUYV, size and rate from the header) or a raw YUYV/NV12 file at `size`. Paced at `fps`.
- **`pattern`**: a test card (a gradient with a bright square that moves across), no input needed. Used by the tests and the emulator.
- **`pipe`**: raw frames on a pipe. With `pipe_command` the service runs the command when enabled and kills it when disabled, so a helper like ffmpeg, and the webcam light with it, only runs while the camera is enabled. Without a command it reads stdin. The producer sets the pace, so a synthetic producer needs its own throttle (`ffmpeg -re`).

## Running it on a desktop

```bash
emu/run.sh --camera-only                     # test pattern, camera off until enabled
kick-camera --socket ~/KickEmu/run/kick-camera.sock --ctl enable   # the binary is build/camera/kick-camera
build/camera/kick-camera-peek --out frame.pgm                      # see what a reader sees
```

`emu/run.sh --camera` starts the service next to Kick and the Surface, `--camera-enable` opens the camera right away. `KICK_CAMERA_SOURCE` selects the input: `pattern` (default), `file` (with `KICK_CAMERA_FILE`) or `webcam`.

- **macOS webcam:** `KICK_CAMERA_SOURCE=webcam emu/run.sh --camera-only`, or run `emu/tools/camera-webcam.sh` directly. ffmpeg reads the webcam through avfoundation and the service resamples to 320x240 at 15 fps. `--list` shows the device indexes. The first run asks for camera permission for the terminal app.
- **Linux webcam:** the service reads the UVC node directly. Set `DEVICE=/dev/video2` if it is not `/dev/video0`.

## Tests

```bash
cmake -S Kick/services/kick_camera -B build/camera-test && cmake --build build/camera-test && ctest --test-dir build/camera-test
cmake -S Kick/services/kick_camera -B build/camera-tsan -DKICK_CAMERA_SANITIZE=thread   # same suite under ThreadSanitizer
cmake --build build/camera-tsan && ctest --test-dir build/camera-tsan
```

The suite covers the ring (including a writer and a reader on separate threads, run under TSan, and a check that a half-written slot is reported as torn), every source, the exposure logic, config parsing, the service (camera off by default, enable and disable, device failures, no files written without the debug flag, socket permissions, concurrent clients), the real binary driven from a shell with a reader in another process, and on Linux the V4L2 backend against a stand-in kernel. The stand-in checks my call sequence and bookkeeping. It cannot show that a real driver accepts those calls.

## Buildroot

`Linux/buildroot/package/kick-camera/` is a `cmake-package` under a new "Kick" menu. It builds `Kick/services/kick_camera` from this repository (local site, no download), installs `/usr/bin/kick-camera`, `/etc/kick-camera.conf` and the systemd unit, and enables the unit when the init system is systemd. `KTKick_defconfig` selects it.

The unit is installed by the package, not through the rootfs overlay, so it exists only in images that select the package.

## Not verified on hardware

Everything below needs a unit or a Variscite dev kit with an OV5640 module.

- Which `/dev/videoN` and `/dev/v4l-subdevN` nodes exist, and whether the capture node is single-plane (supported) or multi-planar (refused with a message).
- Whether the driver accepts YUYV or NV12 at 320x240, the requested frame rate, and the exposure and white-balance controls above. The OV5640 driver in the kernel config may behave differently from UVC.
- Sensor orientation and field of view.
- CPU use (target: under 5 percent of one core at 320x240 15 fps), a one-hour run with stable memory and no dropped buffers, and audio underrun counts with the camera running.
- `CPUAffinity=3` in the unit assumes the audio threads are not pinned to core 3. I do not know how they are pinned.
- The macOS avfoundation path was checked for its command line only. It was run against a synthetic ffmpeg input, not a real webcam.
