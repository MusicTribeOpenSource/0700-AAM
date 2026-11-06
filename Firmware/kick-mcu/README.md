# kick-mcu

Firmware for the MCU on the KICK's control surface, written as portable C++17 and run on the desktop against a simulated board. The MCU scans the pads, buttons, encoders and touch strip and drives the LEDs. The Kick app on the i.MX8M talks to it over `/dev/ttymxc0` (230400 baud, 8N1, no flow control).

The firmware has a transport layer (framing, CRC, the receive parser, the transmit queue and the connection handshake), an input layer, an output layer for the LEDs, and a system layer for the version, tempo, shutdown and the firmware update. The hardware is unreleased and the MCU part is not public, so nothing here has run on a real KICK. The wire format below was read from the host code and checked against the emulator, where the unmodified host talks to the Surface.

```
cmake -S Firmware/kick-mcu -B build/mcu -G Ninja
cmake --build build/mcu
ctest --test-dir build/mcu --output-on-failure
```

Add `-DKICK_SANITIZE=ON` for ASan and UBSan. The tests fetch googletest v1.15.2 through FetchContent; pass `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path>` to build offline.

## Wire format

```
'M' 'T' | type | dir | cmd | size | payload[size-1] | crc8 | trailer (MCU to host only)
```

- `type`: 0 SYSTEM, 1 BUTTON, 2 LED, 3 PAD, 4 KNOBS, 5 MIDI, 6 CV, 7 SLIDER.
- `dir`: 0 request, 1 response. The host only sends requests and ignores responses.
- `size` is the payload length plus one, because it counts the CRC byte.
- `crc8` is polynomial 0x31, initial value 0xFF, no reflection, no final xor, over every byte from `'M'` through the last payload byte.

### The size mismatch in issue #2

`PktMgr` writes `size = payload + 1`, so a frame is `size + 6` bytes. The host receive loop in `input_events.cpp` reads `size + 7`. The extra byte is a trailer: the host hands the first `size + 6` bytes to `PktMgr` and drops the last one unread.

So the two directions differ:

| Direction | Trailer | Payload limit |
|---|---|---|
| MCU to host | one byte after the CRC, `'\n'` (what the Surface sends) | 19 bytes (set by the host's 20-byte buffer) |
| host to MCU | none | 57 bytes (the receive buffer), 33 in practice |

Without the trailer the host swallows the first byte of the next frame. The payload limit comes from the host's `BUFFER_RX_SIZE` of 20: `PushByteToRxFifo` writes `size - 1` bytes into that buffer with no check against its length, so 21 bytes overrun it. By my reading of the code 20 would still fit; I cap at 19 to leave a byte of margin, and nothing the MCU sends is longer than a few bytes. `encodeFrame` refuses anything over 19.

The answer is pinned in three places: the doc comment in `frame_codec.h`, `FrameCodec.McuToHostFrameCarriesATrailerAndHostToMcuDoesNot`, and `PktMgrCrossCheck.HostPktMgrReadsFramesTheFirmwareEncodes`, which feeds the host's real `PktMgr` the firmware's bytes minus the trailer.

### What the host cannot do

These constrain the MCU, and the design follows from them:

- The host cannot resync. It reads whatever is in the serial buffer, and if the buffer holds less than one whole frame it throws everything away. The MCU therefore sends each frame as one contiguous write and only starts a frame when the UART has room for all of it.
- `PktMgr` loses a frame with `size == 1` (no payload): it waits for a data byte that never comes. The MCU parser accepts such frames, and the MCU never sends one.
- `PktMgr` keeps its partial frame in a `static std::vector`. Line noise corrupts its CRC and nothing clears it. That is host code and is not changed here.

## Layout

```
include/kick_mcu/contract.h      wire constants and enums, checked against the host headers
include/kick_mcu/bsp/bsp.h       board interfaces: IUart, IClock, and the ones later layers use
include/kick_mcu/link/           frame_codec, rx_parser, tx_queue, session, link, spsc_ring
include/kick_mcu/input/          debounce, velocity_curve, pads, encoders, strip, event_sink
include/kick_mcu/output/         led_state, commands, colours, blink, current_budget, led_renderer
include/kick_mcu/system/         crc32, flash_layout, image, boot_control, updater, self_check, bpm, shutdown, system
include/kick_mcu/app/            scan_loop, firmware (the main loop that ties the link to the inputs and the outputs)
src/                             implementations
bsp/sim/                         desktop UART, clock, fake panel and a byte-paced 230400 baud UART model
sim/                             kick-mcu-sim: the firmware on the desktop, plus the script player
tests/                           googletest suites
tests/host_updater/             the host's real MCUUpdate against the firmware (opt-in)
../../tools/mcu-image-header.py  stamps the 64-byte image header onto a hex or binary
```

The core has no vendor headers, no heap use and is built with `-fno-exceptions -fno-rtti`. Only `bsp/<mcu>/` changes when the board arrives.

### `frame_codec`

CRC-8 and `encodeFrame`. The wire direction picks the trailer and the payload limit.

### `rx_parser`

A byte-at-a-time state machine over one 64-byte frame buffer. It never blocks and does bounded work per byte.

- Bytes outside a frame are dropped until `'M' 'T'`. `'M' 'M' 'T'` works.
- A bad CRC, a `size` of 0, or a payload over the 57-byte buffer rejects the frame. Every byte of the rejected frame after its first is then scanned again for a new `'M' 'T'`, so a real frame that started inside the bad one is not lost. This also covers truncation: a short frame absorbs the start of the next one, fails its CRC, and the re-scan finds the next one.
- `expire()` abandons a partial frame the same way. `Link::poll()` calls it after 10 ms with no byte, which flushes a frame stuck waiting for bytes that will not arrive.
- It does not inspect `type`, `dir` or `cmd`.
- Counters: frames, CRC errors, size errors, garbage bytes, re-scans, timeouts.

### `tx_queue`

Fixed rings in four lanes, drained strictly in this order: `System`, `Button`, `PadKnob`, `Aftertouch`. A pad storm can queue faster than 230400 baud drains, and with one FIFO a button press would wait behind it or be dropped when it filled. With lanes the storm loses its own lowest lane first.

- A full lane drops the new frame and counts it, except `Aftertouch`, which drops the oldest because the newest pressure is the one worth sending.
- Order holds inside a lane. A button can overtake an earlier pad event, and aftertouch can trail its pad's release. The input layer avoids the second by not queueing aftertouch for a released pad.
- `pump()` writes a frame only if `IUart::writableBytes()` covers all of it, and stops at a blocked high lane rather than sending a smaller frame from a lower one.
- Worst-case button wait is the frame already in flight plus whatever is in the UART FIFO. The BSP chooses that FIFO depth, so keep it small; at 32 bytes it is 1.4 ms of line. The storm test uses 32.

Depths are 8, 32, 64 and 16 frames, 28 bytes each, about 3.4 KB. They are constants in `tx_queue.h`.

### `session`

The handshake, driven by the host (read from `input_events.cpp`, confirmed against the emulator):

```
host                                        MCU
 |  COMM_INIT [1]                            |
 |------------------------------------------>|   state: Connected
 |            GET_SW_VER [major, minor]      |
 |<------------------------------------------|
 |            CHECK_COMM_STATUS [1]          |
 |<------------------------------------------|
 |  COMM_INIT [2]                            |
 |------------------------------------------>|
 |            CHECK_COMM_STATUS [2]          |
 |<------------------------------------------|
 |  CHECK_COMM_STATUS [2]                    |
 |------------------------------------------>|   state: Streaming
 |            CHECK_COMM_STATUS [2]          |   then every 3 s
 |<------------------------------------------|
```

The `[2]` stage is not in `dcl_contracts.h`. It is what the host sends and expects.

- `COMM_INIT [1]` restarts the sequence from any state and discards event frames left over from the last session.
- `COMM_INIT [0]` and `RECONNECT` drop to `NotConnected`. The host never sends `RECONNECT`; the MCU handles it anyway. The MCU does not start a handshake of its own, since the host is the one that opens the line.
- Until the state is `Streaming`, `Link::send()` refuses event frames and counts them. System frames are always allowed.
- Frames with `dir == response` are ignored.
- There is no link-loss timeout. The host defines none, and nothing says it answers every heartbeat, so the MCU keeps streaming until told otherwise.
- Everything the session does not consume (LED, pad, slider, BPM, shutdown, update commands) goes to a pass-through handler for the layers above.

The version it reports defaults to 1.0, which is what the Surface reports. The host reads two bytes, `[major, minor]`.

### `link`

Owns the receive ring, parser, queue and session, and wires them to an `IUart` and an `IClock`.

- The UART receive interrupt calls one function that pushes a byte into a lock-free single-producer ring. Nothing else runs in the interrupt.
- `poll()` runs on the main loop: drain the ring into the parser, apply the inter-byte timeout, run the heartbeat, pump the queue. It never waits, so the scan loop can call it every pass.
- The ring holds 255 bytes, about 11 ms of line. `poll()` has to run at least that often or bytes are lost and counted in `rxOverruns`.

## Input layer

`ScanLoop::tick()` reads every input once and queues what changed. `Firmware::step()` runs one tick and then `Link::poll()`, and the main loop calls it every 250 us. Nothing in the input layer waits on the UART, allocates, or reads a clock other than the one it is handed.

Event payloads, as `input_events.cpp` decodes them:

| Input | Frame | Payload |
|---|---|---|
| Button | `BTN_STATUS` | `[code, state]`, state 1 down and 0 up |
| Pad | `PAD_STATE` | `[pad, state, velocity, aftertouch]` |
| Jog wheel (knob 0) | `KNOB_ROTARY` | `[0, dir, steps]` |
| Knobs 1 to 16 | `KNOB_QLINK` | `[knob, dir, steps]` |
| Touch strip | slider | `[value]`, 1 to 100, 0 for a lift |

### Buttons

`Debouncer` is an integrator per key. A key counts as pressed after `pressUs` of net closed time (default 1 ms) and as released after `releaseUs` of net open time (default 4 ms). The release window is longer because a late release costs nothing a player can feel. If a tick arrives late, the integration step is capped at half the shorter window, so a stalled main loop cannot turn one bounce into a full press. A stall therefore slows the debounce down instead of speeding it up.

The scan loop compares the debounced state with what the host was last told. A key that differs gets a frame, SHIFT first and then in code order. A refused frame is kept as a difference and retried on the next tick, so a SHIFT release is never lost. A press and release that both happen while the queue refuses cancel out.

### Pads

A strike starts when a reading reaches `onThreshold`. The peak over the next `velocityWindowUs` (default 1 ms) goes through a 128-entry velocity curve (`linear`, `soft` or `hard`, or a table of the caller's own) and becomes the velocity. The pad-down frame goes out when the window ends, or earlier if the pad releases first, with the peak seen so far. The release threshold is below the on threshold so noise cannot chatter a pad.

Aftertouch is off until the host sends `PAD_SET_AFTER_TOUCH`. It is sent as `PAD_STATE` with state 2. Most host pad handlers compare the state with 1 and 0 only, so state 2 does not retrigger a note there, but two do react: the MIDI control page (`kick_MidiControlTabs.cpp`) sends a note-off for any state that is not 1, and Pad Perform (`kick_PadPerform.cpp`) hands the engine an empty MIDI message. The host turns aftertouch on from those same screens, so both see these frames. This is an open decision, not a settled one. Pressure in the first 30 ms after a strike is ignored, a new value must differ from the last sent by 2, and each pad sends at most 50 frames a second. Aftertouch is never queued for a released pad. It uses the lowest queue lane, so a storm of it never delays a button.

### Encoders

`EncoderBank` decodes Gray-code quadrature with a transition table, so a skipped or bounced transition is counted instead of misread. `transitionsPerStep` (default 4) says how many transitions make one detent. Steps in the same direction within one tick leave as one frame with a step count. A reversal inside a tick flushes the steps so far, and if the queue refuses that, the two directions net out.

### Strip

A new position is sent when it differs from the last one sent by `deadband` percent. The lift (value 0) is sent after the strip reads untouched for `liftDebounceUs`, so one dropped reading mid-drag does not end the touch.

### Link state

While the session is not streaming, every input is still read and tracked, but its events are discarded. A key or pad held when the link comes up is not announced, and its release is not sent, so the host never sees half a press.

### Config

All thresholds and times live in `ScanConfig` (`DebounceConfig`, `PadConfig`, `EncoderConfig`, `StripConfig`). The defaults are placeholders. The sensor part, the key matrix and the encoder part are not known until the board exists.

## Output layer

The host's LED, PAD and SLIDER packets land in a double-buffered table (`led_state.h`). `CommandDecoder` (`commands.h`) applies one packet at a time to the back buffer and commits it only if the whole packet checked out, so a multi-pad update appears atomically. The refresh task (`LedRefresh` in `led_renderer.h`) draws the front buffer into an `ILedDriver` every 10 ms and is the only thing that touches the hardware. The buffer is safe for one execution context; if the packet handler and the refresh run in different interrupt levels on the real MCU, the swap needs a critical section, and `led_state.h` says so.

Every command and its payload layout is documented in `commands.h`, taken from the host senders in `Kick/modules/contracts/dcl/dcl_contracts_helper.cpp`. A packet that is too short or has a length that does not fit its command is counted as malformed and changes nothing; an id past the table is counted as out of range; an unknown command is counted as unknown. All three counters are in `CommandStats` and none of them crash or write.

- Colours: 18 `PadColourIds` entries in `colours.h`, copied from the Surface, adjustable with `ColourTable::set`. Gamma defaults to 2.2 and is adjustable from 1 to 4.
- Blink: one `BlinkClock` (500 ms period) drives every blinking pad and button, so they stay in phase.
- Current: `current_budget.h` caps the estimated LED current. The arithmetic is in `docs/mcu-led-current-budget.md`. Its inputs are assumptions.
- Radio groups: the host never turns an old bank LED off. The firmware clears the others in a group (banks, Play/Stop, Full/Half Level), as the Surface does. Whether the real MCU does is not known.

### Comparing with the Surface

Both programs replay a recorded host session and write the final LED table as one line of JSON:

```bash
build/mcu/kick-mcu-sim --replay emu/tests/traces/leds.trace --dump-leds -
"build/surface/KickSurface_artefacts/Release/KICK Surface.app/Contents/MacOS/KICK Surface" --replay emu/tests/traces/leds.trace --dump-leds -
emu/tools/compare-leds.sh            # runs both and diffs
```

`--dump-leds FILE` also works on a live run and writes the table at exit. The JSON holds the 53 button states, each pad's `[state, colour, offColour, velocity]` and the strip percent. The firmware keeps more than that (button colours, the strip mode), which the Surface does not model, so those are not compared.

`leds.trace` is `leds-live.trace` (RX frames recorded from the unmodified host, boot-time pad clears and strip frames) plus synthetic frames from `emu/tools/make-led-trace.py`. The emulated host does not send button LED, blink, multi-colour or step frames on its own, so a purely recorded trace could not exercise them. The synthetic frames use the layouts from the host's senders. `leds.surface.json` is what the Surface produced for it.

Where the Surface and the firmware differ on purpose: the Surface accepts a payload shorter than the layout and the firmware rejects it, and the Surface's handling of BLINK_SINGLE, SET_ALL and SLIDER_INDIVIDUAL payloads is a guess where the host never sends them. The trace avoids those.

## System layer

`System` (`system/system.h`) takes the SYSTEM frames the session leaves alone: `GET_SW_VER` (a host request is answered with a 15-byte report: version, image status, `FLASH_VALID`, the payload CRC-32 computed from flash, revision and flags), `BPM` (stored; the blink clock follows it only when `blinkFollowsBpm` is set), `SHUTDOWN` (acknowledge, LEDs off, no more input events, then a power-off request that is a stub until the board exists) and `SW_UPDATE`.

The handshake's `GET_SW_VER` reply carries `[major, minor]` from the running image's header when it verifies, and the build version otherwise. The boot-time self check runs once and never touches the update path.

The update is an A/B scheme with a trial boot: the MCU writes only the slot that is not running, checks the whole image by reading it back, arms a trial with an atomic boot-control record, and the new image has to complete the host handshake before it is confirmed. A reset before that returns to the old image. The protocol, the flash map, the failure matrix and what is not done about image signing are in `docs/mcu-update-protocol.md`.

Build an image the update path accepts with `tools/mcu-image-header.py stamp`.

## kick-mcu-sim

The firmware, with the fake panel from `bsp/sim/`, on the desktop and headless. It serves the same pty or TCP line as the emulator Surface, so Kick connects to it unchanged. Frames leave at the moment a UART at 230400 baud would have finished sending them, and always whole.

```bash
cmake -S Firmware/kick-mcu -B build/mcu && cmake --build build/mcu
build/mcu/kick-mcu-sim --root ~/KickEmu --script emu/tests/firmware-input.script --host-log ~/KickEmu/kick.log
```

Options: `--root DIR`, `--tcp PORT`, `--script FILE`, `--host-log FILE`, `--curve linear|soft|hard`, `--tick-us N`, `--dump-leds FILE` (write the LED table at exit, `-` for stdout), `--replay TRACE` (play a recorded session into the firmware with no serial line, then exit), `--flash FILE` (keep the MCU flash in a file between runs; a reset from an update restarts the firmware in process), `--factory-hex FILE` (program slot A with a stamped image when the flash file is new), `--boot-report FILE` (write the boot decision as JSON, `-` for stdout) and `--inspect` (print the slots and the boot-control record from `--flash`, then exit). Without `--script` it runs until killed. `emu/run.sh --firmware` starts Kick against the built binary; see `emu/README.md`.

Script commands, one per line, `#` starts a comment:

| Command | Effect |
|---|---|
| `wait <ms>` | pause |
| `press <button> [holdMs]` | button by name (`"Track Mix"`, `+`, `Shift`) or code 0 to 54; hold defaults to 60 |
| `pad <1-16> [velocity] [holdMs] [aftertouch]` | a strike that reads as `velocity` through the linear curve |
| `turn <0\|1-16> <steps> [intervalUs]` | positive is clockwise; 0 is the jog wheel |
| `strip <1-100> [holdMs]` | touch, hold, lift |
| `aftertouch on\|off` | what the host's `PAD_SET_AFTER_TOUCH` does |
| `expect streaming [timeoutMs]` | wait for the handshake |
| `expect led <button> on\|off [timeoutMs]` | wait for a button LED in the firmware's table |
| `expect pad <1-16> <colourId\|off> [timeoutMs]` | wait for a pad to show a colour (`off` is dark or Trans) |
| `mark-host` | remember how much of the host log exists now |
| `expect host "<text>" [count] [timeoutMs]` | wait for the host log, after the last mark, to hold `<text>` on at least `count` lines |
| `quit [code]` | stop with this exit code |

`button`, `knob` and `slider` are accepted as aliases for `press`, `turn` and `strip`. The whole script is parsed before anything runs, so a typo fails on its line number up front. A key release debounces for longer than a press, so two presses with no `wait` between them can reach the host out of order.

`emu/tests/firmware-input.script` runs against the unmodified host and checks its log lines (`Data[0] : N Cmd : C type : T`) for a button, a pad with aftertouch, the jog wheel, a Q-Link knob and the strip. `emu/tests/firmware-leds.script` checks the pad clear the host sends at startup against the firmware's table.

## Latency

Measured in `tests/latency_test.cpp` on a virtual clock and a UART modelled byte by byte at 230400 baud. "Decision to wire" runs from the start of the tick that decided the event to the moment the last bit of the frame, trailer included, leaves the UART. "Contact to wire" adds the debounce or velocity window and the wait for the next tick.

| Scenario | Decision to wire | Contact to wire |
|---|---|---|
| Button, quiet line | 534 us | 1284 us |
| Pad, quiet line | 621 us | 1621 us |
| Button during a saturated pad storm (worst of 60) | 1326 us | 2076 us |

Decision to wire is under the 2 ms budget in every scenario, and the test asserts it. The storm figure for contact to wire goes slightly over 2 ms because the 1 ms debounce is included. 16 pads struck at once serialize on the line: the last one leaves about 8.3 ms after the first.

These numbers hold only under the test's assumptions, which are requirements on the BSP:

- The main loop ticks every 250 us.
- The scan costs 100 us of CPU between sampling and queuing. This is a guess. The clock is virtual, so CPU time is not measured.
- The UART transmit FIFO is 24 bytes. A larger FIFO adds to the worst case a button can wait behind a frame already committed.
- The debounce and velocity windows are the defaults above.

## Tests

| Suite | What it covers |
|---|---|
| `FrameCodec` | CRC against bytes captured from the emulator, trailer, limits, round trip for every payload length |
| `RxParserTest` | valid, truncated at every point, bad CRC, garbage between frames, back-to-back, bit flips, size 0 and oversize, a real frame inside a rejected one, `expire()`, 200k bytes of noise under ASan |
| `TxQueue` | priority order, whole-frame writes, overflow policy and counters |
| `SessionTest` | each handshake step, heartbeat timing including clock wrap, reconnect, restart, pass-through |
| `LinkTest` | the above through raw UART bytes, inter-byte timeout, ring overflow |
| `Storm` | 16 pads at 1 kHz aftertouch plus strikes and buttons for 3 s on a 230400 baud line: 429 of 429 button frames, 2400 of 2400 pad events, 45029 aftertouch frames dropped and counted |
| `TraceReplay` | `emu/tests/traces/smoke.trace` through the parser and the full link |
| `NoAlloc` | a counting `operator new` sees zero allocations across the receive path and the whole link; `static_assert`s that the types are plain storage |
| `Debounce`, `VelocityCurve`, `Pads`, `Encoders`, `Strip` | each input on its own: bounce rejection, stall handling, curve monotonicity and endpoints, strike windows, aftertouch rules, Gray-code decoding including skips, coalescing and reversal, strip deadband and lift |
| `ScanLoop` | button order with SHIFT first, retry on refusal, suppression until the link streams, held keys at connect |
| `Latency` | the table above, plus the 16-pad serialization |
| `Sim` | the script player: every command, parse errors with line numbers, `expect host` |
| `Decode`, `LedState`, `Colours`, `BlinkClock`, `CurrentBudget`, `Render`, `LedRig`, `LedReplay` | every `LedCmd`, `PadCmd` and `SliderCmd` with representative and malformed payloads, the double buffer, colour and gamma tables, blink phase, the current cap, rendering, the firmware's whole output path, and the replay against the Surface's table (set `KICK_SURFACE_BIN` to also run the Surface live) |
| `Contract`, `PktMgrCrossCheck` | every enum in `contract.h` equals the host's (`static_assert`), and the host's own `PktMgr.cpp` is compiled in and run against the firmware codec in both directions, including a button, pad, knob and strip frame produced by the real scan loop |
| `Crc32`, `ImageHeader`, `VerifyImage`, `SimFlash`, `BootRig` | the CRC check value, every header field and corruption, a good, erased and flipped-bit image, NOR flash rules and power cuts, the boot-control records (alternation, damage, wraparound, trial and revert, a cut at every byte of a record write) |
| `UpdateRig`, `UpdatePowerCut` | the update state machine: happy path, bad CRC, truncation, a lying header, a dropped ACK, a repeated or conflicting record, a mid-transfer reset, refusals; and a sweep that cuts power at every flash byte operation of an update plus confirm and checks a verified image always boots |
| `SysRig`, `SystemContract`, `ImageTool` | version, extended report, BPM, shutdown, trial confirm, an update carried by the real link, the contract enums against the host headers, and the image tool's output through the C++ checker |
| `HostRig` (opt-in, `-DKICK_MCU_HOST_UPDATER=ON`) | the host's unmodified `MCUUpdate` against the firmware: a full update with trial boot and confirm, a bad CRC, a dropped ACK, and power cuts in the transfer and in the boot-control write. One process per scenario. See `docs/mcu-update-protocol.md` |

`emu/tests/traces/smoke.trace` was captured by running `KICK_EMU_TRACE=emu/tests/traces/smoke.trace emu/run.sh --script emu/tests/smoke.script` against the unmodified host. The format is described in `emu/README.md`. The replay test checks that the parser yields the frames the Surface decoded, that every frame the Surface sent re-encodes to the same bytes, and that the firmware answers the handshake byte for byte as the Surface does.

## Not verified on hardware

- Baud rate, framing and the handshake are read from the host code and exercised only against the emulator Surface.
- Interrupt and DMA behaviour of the real UART: ring depth, poll interval, TX FIFO size and the button latency that follows from it.
- That the host really tolerates the 10 ms inter-byte timeout and the `'\n'` trailer on silicon the way it does in the emulator. The trailer rule comes from the host's receive loop, not from a captured trace of a real unit.
- The MCU part, its clock and its UART peripheral.
- The key matrix, pad sensors, encoders and touch strip: every threshold, debounce time and the encoder transitions per detent are placeholders.
- Scan CPU time on the target. The latency table assumes 100 us.
- Velocity feel. The curves are checked for shape, not played.
- That the host does something sensible with aftertouch frames (state 2). Reading the code, the MIDI control page turns one into a note-off and Pad Perform forwards an empty MIDI message. The framing needs a decision, ideally from the real MCU's behavior.
- LED colours as they look, and the gamma that matches them. The colour table is the Surface's, not tuned on pads.
- The LED driver chips, their bus and their pin mapping: the renderer talks to an `ILedDriver`, and the only implementation is the simulator's.
- The current budget. Every number is an assumption (see `docs/mcu-led-current-budget.md`).
- Whether the real MCU clears the other LEDs in a radio group, which the firmware assumes.
- The strip LED count and what the host's SLIDER_INDIVIDUAL and level modes should look like.
- The blink look at 500 ms and the velocity-to-brightness mapping of step pads.
- Everything in `docs/mcu-update-protocol.md` under "Not verified on hardware": the flash map and timing, how a real part behaves on a power cut mid-write, the bootloader split, the reset and power-off calls, and the host's real pacing of DATA frames.
- Image signing. The update path checks a CRC-32, which catches corruption and not tampering.
- The host updater harness on Linux. It is built and run on macOS only.
