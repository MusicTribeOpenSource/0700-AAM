# Gesture events

This is the contract between `kick-gesture` and whatever listens to it, normally the Kick app. The machine-readable form is `gesture-event.schema.json` in this directory. `tools/validate_events.py` checks a stream against it and `tests/cli_test.py` runs that check on every build.

The service writes one JSON object per line. There are two kinds of line, a gesture event and a status message. A line never spans two lines and never contains a raw newline.

## Gesture events

```
{"type":"pinch_change","value":0.412,"hand":"left","confidence":0.931,"t_ms":48210}
```

| Field | Type | Meaning |
|-------|------|---------|
| `type` | string | One of the seven names below. |
| `value` | number 0 to 1 | Pinch aperture. Present on `pinch_start`, `pinch_change` and `pinch_end`, absent on the rest. |
| `hand` | `left`, `right` or `unknown` | The hand that made the gesture, as the person using the Kick sees it. See the handedness note. |
| `confidence` | number 0 to 1 | Mean hand score over the frames that made the gesture. Never below `min_event_confidence` (0.6). |
| `t_ms` | integer | Milliseconds on the clock of the camera frame that completed the gesture. It is a monotonic clock. It is not wall-clock time and it is not comparable between runs. |

Numbers are written with three decimals.

### The seven types

| Type | When it fires | Rate limit |
|------|---------------|------------|
| `swipe_left` | The hand moves left across the frame fast and mostly straight, with at least three fingers extended. Left is the user's left. | 800 ms between swipes of either direction |
| `swipe_right` | Same, to the right. | 800 ms, shared with `swipe_left` |
| `palm_hold` | An open palm (four fingers and the thumb extended) stays still for `palm_hold.hold_ms` (600 ms). Fires once per hold. The hand has to leave the pose for `exit_ms` before it can fire again. | 1500 ms |
| `fist` | A closed hand stays still for `fist.hold_ms` (350 ms). Fires once per hold. | 1500 ms |
| `pinch_start` | Thumb tip and index tip come together (ratio at or below `pinch.on`) and stay there for `pinch.start_ms`. | 300 ms |
| `pinch_change` | While a pinch is active and the aperture has moved by at least `pinch.deadband` since the last `pinch_change`. | 50 ms, so 20 per second at most |
| `pinch_end` | The aperture goes above `pinch.off` for `pinch.end_ms`, or the pinch ends for one of the reasons below. | none, because every `pinch_start` has to get one |

### Guarantees

These hold for every stream the service writes, and `engine_test` checks them on random input.

1. Every `pinch_start` is followed by exactly one `pinch_end` before the next `pinch_start`.
2. `pinch_change` only appears between a `pinch_start` and its `pinch_end`.
3. A pinch also ends, with a `pinch_end`, when the hand is lost for longer than `tracking.lost_ms`, when the frame clock goes backwards, when the camera is turned off, when the resource guard disables the service, and when the service shuts down in an orderly way.
4. While a pinch is starting or active, `swipe_left`, `swipe_right`, `palm_hold` and `fist` are suppressed. A pinch is not a swipe that happens to end up in the same place.
5. `t_ms` never goes down inside one session.
6. No event is produced for a frame that has no hand, or whose best hand scores below `tracking.min_hand_score` (0.8).

A listener that connects in the middle of a pinch sees `pinch_change` and `pinch_end` without the `pinch_start`. The socket does not replay history.

### Pinch value

`value` is an aperture, not a distance. With `ratio` as the distance from thumb tip to index tip divided by the palm size:

```
value = clamp((ratio - pinch.min) / (pinch.off - pinch.min), 0, 1)
```

0 is thumb and index touching. 1 is at the release threshold. A fully open hand also reads 1. The distance is measured in palm sizes, so the value does not depend on how far the hand is from the camera.

A pinch needs two things: the ratio at or below `pinch.on`, and the index tip at least `pose.pinch_min_reach` (1.2) palm sizes from the wrist. The second condition keeps a closed fist, where the thumb tip rests against the index finger, from reading as a pinch.

### Handedness

`hand` is the model's handedness guess, flipped for the mirror image when `tracking.flip_x` is true, and swapped again when `tracking.swap_handedness` is true. Both are true by default, which matches the preview a person sees of themselves. The classifier is the weakest output of the landmark network. The app must not rely on `hand` to tell two people apart and must not treat `unknown` as an error. Nothing in the engine depends on it except the value written in this field.

## Status messages

```
{"type":"status","state":"degraded","reason":"cpu_soft","rate_hz":6.0,"cpu":0.64,"temp_c":71.0,"t_ms":48000}
```

| Field | Meaning |
|-------|---------|
| `state` | `running`, `degraded`, `disabled`, `idle`, `no_camera` or `error`. |
| `reason` | Optional. Why the state is not `running`. |
| `rate_hz` | The current inference rate. |
| `cpu` | CPU use of this process averaged over the guard window, in cores (1.0 is one full core). It reads 0 until a second of data exists. |
| `temp_c` | Board temperature in degrees Celsius. Absent when the board has no thermal zone. |
| `t_ms` | Same clock as the events when the camera is on. |

A status line is written at every change of `state` or `reason` and at least every 5 seconds (`status_interval_ms`).

| State | Reason | Meaning |
|-------|--------|---------|
| `running` | none | Normal rate. |
| `degraded` | `cpu_soft`, `temp_soft`, `recovering` | The rate is halved. |
| `disabled` | `cpu_hard`, `temp_hard`, `cooling` | No inference. Open gestures are closed with a `pinch_end` first. |
| `idle` | `camera_off` | The camera service is up and the camera is closed. The service does nothing and costs almost no CPU. |
| `no_camera` | `camera_service_missing` | There is no camera shared memory to read. The service keeps looking. |
| `error` | `model_failed`, `unsupported_frame` | The models would not load or the camera sends a pixel format the service cannot read. Needs a person. |

## Transport

The Kick app listens on TCP port 50501 of 127.0.0.1 and the service connects to it (`events_ipc = 127.0.0.1:50501`). The app side is `GestureIpcServer` in `Kick/modules/gesture`. The framing is the one JUCE's `InterprocessConnection` uses: the magic number `0xf2b49e2c` and the payload length, both 32 bit little endian, then the payload. The payload is JSON.

The service first sends a REGISTER message (`message_type` 1, `connection_id` 500, `connection_name` `kick-gesture`), then one message per line it writes:

```json
{"message_id":12,"originator":500,"message_type":500,"transaction_type":30,"event":{"type":"swipe_left","hand":"right","confidence":0.97,"t_ms":48210}}
```

`message_type` 500 carries a gesture event and 501 a status line. The service's own line is nested under `event`, unchanged, so everything in this document about the fields still holds.

The socket is non-blocking. When the app is not there or does not read, events are dropped, not queued. A stale gesture delivered late is worse than no gesture. While the app is not listening the service retries with a growing delay, 1 s doubling up to 30 s, and starts again at 1 s after a connect. Nothing authenticates the sender, so any local process can talk to the port; it listens on loopback only and gestures are off until the user turns them on.

The older sinks still work: a Unix stream socket (`events_socket`), a file (`events_file`) and standard output (`events_stdout`). More than one sink can be on at once.
