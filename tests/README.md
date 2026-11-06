# Automated tests

Four tiers, all driven by CTest and labeled so you can run any slice.

| Tier | Label | What it covers |
|------|-------|----------------|
| unit | `unit` | PktMgr framing and CRC, audio utils, containers (SPSC queue, MemoryPool, RampedParameter), DSP (panner, LFO, ADSR) |
| integration | `integration` | AudIO graph rendering (design, bus, chain, meters, bypass), PktMgr byte streams |
| ui | `ui` | Kick controls driven with synthesized mouse and knob input, rendered off-screen |
| e2e | `e2e` | Panel wire bytes through PktMgr and IPC JSON into real controls and back |

## Run

```bash
cmake -S tests -B build/tests -G Ninja
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

UI and e2e tiers pull JUCE 7.0.12 and are off by default (macOS only for now):

```bash
cmake -S tests -B build/tests-ui -G Ninja -DKICK_UI_TESTS=ON
cmake --build build/tests-ui
ctest --test-dir build/tests-ui -L "ui|e2e" --output-on-failure
```

Run one tier with `ctest -L unit`, `-L integration`, `-L ui` or `-L e2e`.

## Options

- `KICK_SANITIZE` builds with ASan and UBSan.
- `KICK_TSAN` builds with ThreadSanitizer.
- `KICK_COVERAGE` adds gcov instrumentation.
- `KICK_UI_TESTS` enables the JUCE based ui and e2e tiers.

## Not covered

The `Filter` template has `override` on virtuals that do not exist, so it cannot be instantiated.

## Layout

- `support/` shared harnesses (PktMgr FIFO harness, audio pool, unity build of AudIO)
- `unit/`, `integration/`, `ui/` test sources
- `ui/stubs/` stand-ins for private modules missing from the public release
