# kick-gesture

A small service that reads the camera frames the `kick-camera` service publishes and turns hand movements into the events described in `gesture-events.md`. It is plain C++17 with no JUCE. The only dependency is ONNX Runtime, which runs two MediaPipe networks.

## Pipeline

1. A frame reader takes the newest frame from the camera service's shared memory ring (`/kick-camera`). It only reads. If the camera service restarts, the reader reattaches. While the camera is off it does no inference.
2. `image.cpp` converts YUYV or NV12 to RGB and scales it to the network input with a letterbox.
3. `OnnxHandModel` runs palm detection (192x192, 2016 SSD anchors generated in code), builds a rotated square ROI around the best palm with the MediaPipe geometry (shift_y -0.5, scale 2.6, square_long), then runs the hand landmark network (224x224) on the warped ROI. The result is 21 landmarks, a hand score and a handedness score per hand.
4. `computeFeatures` turns landmarks into numbers measured in palm sizes: finger extension, thumb extension, pinch ratio, index reach, centre and speed. Everything downstream is independent of the camera distance and of the frame size.
5. `GestureEngine` runs one small state machine per gesture. It uses only frame timestamps, never the wall clock, so the same landmark stream always gives the same events.
6. Events go to the sinks. A `ResourceGuard` watches CPU and temperature and lowers the rate or stops inference.

Only the third step needs the models. Steps 4 and 5 take a `LandmarkFrame`, and `ILandmarkModel` is the interface between them, so the engine is tested without any model by feeding it synthetic hands.

## Thresholds

Every threshold is in `gesture.json`, installed at `/etc/kick-gesture/gesture.json`. `kick-gesture --print-config` prints the built-in values, and `tests/config_test.cpp` checks that the shipped file is equal to them. A key that is missing keeps its default. An unknown key or an out-of-range value is an error at start-up, not a silent default.

The values are a first guess from synthetic data and a handful of photos. They have not been tuned on recordings of people using the unit. The eval tooling below is how to tune them.

## Resource guard

The audio engine has priority over this service. The guard keeps it that way.

| Condition | Result |
|-----------|--------|
| CPU above `cpu_soft` (0.60 cores, averaged over a window) or temperature above `temp_soft_c` (75 C) | `degraded`, half the rate |
| CPU above `cpu_hard` (0.90) for 5 seconds, or temperature at or above `temp_hard_c` (85 C) | `disabled`, no inference, any open pinch is closed |
| Back below the soft limits for a settling time | `running` again |

CPU comes from `/proc/self/stat` with a `getrusage` fallback. Temperature comes from `/sys/class/thermal`. Both can be replaced in tests. The systemd unit adds a second layer: `Nice=15`, `CPUWeight=5`, `CPUQuota=100%` and `MemoryMax=192M`. Those numbers are untested on the unit.

## Privacy

The service reaches only this machine: it may connect to loopback (`IPAddressDeny=any`, `IPAddressAllow=127.0.0.0/8`, `RestrictAddressFamilies=AF_UNIX AF_INET`) and nothing else. That replaced `PrivateNetwork=yes` so it can reach the Kick app's local listener, and it is not yet tested on the unit. It does not open the camera, only the shared memory. It never writes a pixel. `--record` writes hand geometry only (21 points and two scores per frame), which is enough to replay a session and not enough to rebuild an image.

## Record and replay

```
kick-gesture --record /tmp/session.jsonl --events-stdout      # live, also write the landmark stream
kick-gesture --replay /tmp/session.jsonl                      # run it through the engine, events on stdout
kick-gesture --replay /tmp/session.jsonl --realtime           # keep the recorded timing
```

The file is JSON lines: a header line, then one line per frame. Coordinates are written with six decimals, and the live path rounds to six decimals before the engine sees them, so a replay gives exactly the live events.

## Evaluation

`tools/gesture-eval/gesture_eval.py` scores recordings against labels. Per gesture it reports precision and recall. For the clips with no labels it reports false triggers per 10 minutes. It exits 1 when recall is under 95 percent for any gesture or when false triggers are over 1 per 10 minutes. The label format is described at the top of the script.

```
kick-gesture-synth eval-set /tmp/ev                 # synthetic clips plus labels.json, 10.8 minutes of negatives
python3 tools/gesture-eval/gesture_eval.py /tmp/ev/labels.json --binary build/kick-gesture
```

The synthetic set runs in ctest (`gesture_eval_synthetic`). It proves the tool and the floors work. It does not say anything about real hands. The real number needs recordings of real people, with a talking-and-moving negative set that is longer than 10 minutes.

## Benchmark

```
kick-gesture --bench --bench-image hand.ppm --model-dir DIR [--bench-frames 200] [--bench-sizes 320x240,640x480]
```

It measures preprocess, palm detection and landmark time per frame for the int8 and fp32 sets and prints a Markdown table. The `hands found` column counts the frames that had a hand. Use an image with a hand in it, because the landmark stage only runs when palm detection finds a palm, so frames without a hand under-report the cost. `tools/prepare_test_images.py` converts a JPEG to the PPM it reads.

## Building on a desktop

```
cmake -S Kick/services/kick_gesture -B build/gesture
cmake --build build/gesture
ctest --test-dir build/gesture
```

The configure step downloads googletest, ONNX Runtime and the models, each checked against a SHA-256 (`cmake/pins.cmake`, `models/MODELS.md`). Without a network it warns and builds without the model backend, and the tests that need it are not registered. Use `-DKICK_GESTURE_ORT_ROOT=` and `-DKICK_GESTURE_MODEL_DIR=` to point at files you already have, `-DKICK_GESTURE_FETCH=OFF` to forbid downloads, and `-DKICK_GESTURE_SANITIZE=thread` or `address` for a sanitizer build.

## Demo without a camera

```
kick-gesture-synth demo | kick-gesture --replay /dev/stdin
```

`demo` writes a landmark stream of synthetic hands doing each gesture once. It exists so the event contract can be seen without a model or a camera.

## On the unit

The Buildroot package is `Linux/buildroot/package/kick-gesture`. It downloads the aarch64 ONNX Runtime archive and the int8 models as hash-checked extra downloads, builds the service with downloads off, and installs the binary, `libonnxruntime.so`, the models, the config files and the systemd unit. The package has not been built yet. Nothing here has run on the i.MX8M.
