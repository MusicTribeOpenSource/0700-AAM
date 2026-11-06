# Gesture control in the Kick app

This is the app half of the gesture feature. `kick-camera` captures frames and `kick-gesture` turns them into events (see `camera.md` and `Kick/services/kick_gesture/docs/gesture-events.md`). The app listens for those events and turns them into front panel input.

Nothing here has run on a KICK. Everything below was exercised in the emulator, which sends gestures from a script, from keys, or from a replayed synthetic hand stream. See "Not verified on hardware" at the end.

## The path of one gesture

```
kick-gesture ---- TCP 127.0.0.1:50501, JUCE InterprocessConnection framing ----> GestureIpcServer (Kick/modules/gesture)
                                                                                      |  GestureSignals::gestureReceived (message thread)
                                                                                      v
                                        KickGestureAdapter (Kick/apps/kick) -> GestureRouter (plain C++) -> HWMsg_t
                                                                                      |
                                                              DclSignals::hardwareInputTriggered, the same entry the panel uses
```

I made the transport the app's own IPC module, with message type 500 for an event and 501 for a status line. The service's JSON line sits unchanged under the key `event`. The wire format is written down in `gesture-events.md`. `gesture_ipc_test` in `emu/tests/gesture_ipc/` runs the service's sink against the real server, so the two sides are checked against each other and not only against my reading of both.

The router is a pure function of the event, the settings and a small `AppState`, so it has plain gtest coverage in `Kick/modules/gesture/tests/` with no JUCE.

## What each gesture does

The defaults:

| Gesture | Action |
|---|---|
| swipe left | previous bank, wrapping from A to H |
| swipe right | next bank, wrapping from H to A |
| open palm held | Play (the Play button) |
| fist | Stop (the Stop button) |
| pinch | Q-Link knob 1, 32 encoder steps over the full pinch travel |
| pinch start, pinch end | nothing |

A gesture becomes the same packets the panel sends: a bank or transport action is a button press and release (`[id, 1, 80, 100]` then `[id, 0, 80, 100]`), and a knob is a Q-Link packet `[knob, direction, steps]`. Nothing in the app can tell a gesture from a hand on the panel.

## The gate

Before an action runs, three checks apply, in this order:

1. Gestures must be on. They are off by default. The switch is on the Camera page in Preferences, and `KICK_EMU_GESTURES=1` forces it on in the emulator.
2. The event's confidence must reach the sensitivity setting. Sensitivity runs from 1 to 10 and sets a minimum confidence of 0.96 at 1, 0.80 at 5 and 0.60 at 10.
3. While anything is recording or overdubbing, no gesture may touch transport or erase. That covers Play, Stop, Record, Overdub, Play Start and Erase, and every Transport action. Bank selection and knob turns stay allowed because neither one ends a take.

The third rule exists because a hand moving near a camera mid-take is the worst moment for a stray fist to stop a recording.

## Pinch smoothing

The pinch value from the service is the thumb to index distance, and a real hand jitters. Sending that straight to a knob would zipper it. The router puts the value through a one-pole low-pass (time constant 400 ms at sensitivity 1 down to 40 ms at 10) and a slew limit of 3 range units a second, on the event clock and not the wall clock. It carries the fractional step between events, so a slow pinch still reaches the end of the travel and a still hand with a jittering reading nets out to nothing: no frame moves the knob by more than one step.

A pinch starts from the value at `pinch_start`. If the gate blocks a `pinch_change`, the next allowed one sets a new reference and does not turn the knob, so closing the gate mid-pinch and opening it again does not jump the knob.

## The mapping format

Mappings are stored in the preferences file under `GESTURE`, `gesturemapping`, as one entry per gesture:

```
swipe_left=bank:0:-1:1;swipe_right=bank:0:1:1;palm_hold=transport:0:0:1;fist=transport:1:0:1;pinch_start=none:0:0:1;pinch_change=parameter:1:1:32;pinch_end=none:0:0:1
```

Each entry is `name=kind:id:direction:scale`.

| kind | id | direction | scale |
|---|---|---|---|
| `none` | unused | unused | unused |
| `parameter` | Q-Link knob 1 to 16 | +1 or -1 | steps per gesture, or for `pinch_change` steps over a full pinch |
| `button` | panel button 0 to 52 | unused | unused |
| `transport` | 0 Play, 1 Stop, 2 Record, 3 Overdub | unused | unused |
| `bank` | bank 0 (A) to 7 (H) when direction is 0 | 0 for an absolute bank, +1 or -1 for relative | unused |

An empty or missing text means the defaults. A bad entry is skipped and logged, and that gesture keeps its default. `pinch_change` takes only `none` or `parameter`, because it is continuous.

The Camera page cycles each gesture through `actionChoices()` in `gesture_mapping.cpp`. It leaves out Record and Overdub on purpose: one wrong gesture should not be able to start a take. The text format can still hold them for someone who edits the file.

## How to add a gesture action

1. Add the action to the `ActionKind` enum in `Kick/modules/gesture/core/gesture_mapping.h` if it is a new kind, or reuse `button` for any panel button.
2. Teach `validateAction`, `describeAction` and `toString`/`fromString` about it in `gesture_mapping.cpp`, and add it to `actionChoices` if the page should offer it.
3. Handle it in `GestureRouter::handle` in `gesture_router.cpp`. Return a `Command` of type `ButtonTap` or `KnobSteps`. If the action must not run during a take, add it to `blockedWhileRecording` in `gesture_gate.cpp`.
4. Add cases to the tests in `Kick/modules/gesture/tests/`. They build with plain CMake: `cmake -S Kick/modules/gesture/tests -B build/gesture-core && cmake --build build/gesture-core && ctest --test-dir build/gesture-core`.
5. If it needs a new panel number, add it to `panel` in `gesture_mapping.h` and a `static_assert` against `DeviceControlContracts` in `Kick/apps/kick/kick_gesture_adapter.h`.

To add a new gesture kind, the event contract in `gesture-events.md` comes first, then `GestureKind` in `Kick/modules/contracts/gesture/gesture_event.h`.

## The camera switch and the indicator

The switch on the Camera page asks `kick-camera` to `enable` or `disable` over its control socket (`KICK_CAMERA_SOCKET`, `/run/kick-camera.sock` by default) and then follows the service's own `capturing` flag. A tap the service refuses snaps back. The switch is not stored: the camera never turns itself on after a restart.

While the camera is capturing, `KickHome` shows a red CAM badge in the top right corner of every screen.

`CameraService` polls `status` about once a second. While the service does not answer, it retries after 1 s, then 2 s, doubling up to 30 s, and the page says "Camera service not running". That is the normal state on a unit or emulator without `kick-camera`. The app works the same with or without it, and `emu/tests/smoke.script` passes with both services stopped.

## Security notes

The app's gesture server listens on loopback only and does not authenticate the sender, so any local process can inject gestures. Gestures are off by default and the gate rules above still apply. What a rogue sender can trigger is whatever the mappings hold, by default a bank change, Play, Stop or a turn of knob 1. It cannot start a take, because Record and Overdub are not on the page's list and Play and Stop are blocked while anything records.

The `kick-gesture.service` unit lost `PrivateNetwork=yes` so it can reach the app on loopback. It still has `IPAddressDeny=any` with `IPAddressAllow=127.0.0.0/8`, and `AF_INET` is the only family added to `AF_UNIX`. I have not tested that on a unit.

## Not verified on hardware

- No camera, frame, gesture or latency number comes from a KICK. The emulator proves the plumbing.
- Where the CAM badge sits on the real 800x480 display, and whether it covers anything on a screen I did not open, is unchecked.
- The systemd unit's loopback allow list is untested. Check it with `systemd-analyze security kick-gesture` and a real connection.
- Whether the app's recording state is accurate from the looper and recorder getters on every screen, and for the sequencer. The gate reads `isLooperRecording`, `isLooperOverdubbing`, `isRecorderRunning` and `isRecorderOverdubbing`. It does not read sequencer record state.
- Gesture quality with real hands, lighting and a real camera is not measured here. The synthetic stream only exercises the pipeline.
