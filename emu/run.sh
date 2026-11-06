#!/usr/bin/env bash
# Runs Kick against the virtual control surface.
#
#   emu/run.sh                      Kick plus KICK Surface windows, until you close one
#   emu/run.sh --script file        drive the Surface from a script, exit with its result
#   emu/run.sh --root DIR           emulated device root (default ~/KickEmu)
#   emu/run.sh --no-surface         Kick only (set KICK_SERIAL_PORT yourself)
#   emu/run.sh --tcp PORT           Surface listens on TCP instead of opening a pty; Kick connects to it
#   emu/run.sh --camera             also start kick-camera (test pattern, camera off until "enable")
#   emu/run.sh --camera-enable      like --camera, and open the camera right away
#   emu/run.sh --camera-only        start only kick-camera, in the foreground (no Kick, no Surface)
#   emu/run.sh --firmware           run the control surface firmware (Firmware/kick-mcu, kick-mcu-sim) in place of the
#                                   Surface: headless, driven by --script (see Firmware/kick-mcu/sim/script_player.h)
#   emu/run.sh --gesture            gestures on, and kick-gesture replays a canned synthetic hand stream into the app
#                                   (no camera or model needed; it repeats until you quit)
#
# Environment: MCU_BUILD_DIR (build/mcu, where cmake built kick-mcu-sim), KICK_EMU_ROOT, KICK_EMU_CONTROL_PORT (50901), KICK_EMU_EVENT_PORT (50902),
# KICK_AUDIO_DEVICE (CoreAudio device name; default system output).
# Camera: KICK_CAMERA_SOURCE = pattern (default) | webcam (emu/tools/camera-webcam.sh) | file (KICK_CAMERA_FILE, raw
# YUYV 320x240 or Y4M). The service socket is $KICK_EMU_ROOT/run/kick-camera.sock and the frames are in shared
# memory /kick-camera; both are exported as KICK_CAMERA_SOCKET and KICK_CAMERA_SHM.
# Gestures: with no service running, hold G in the Surface and press 1 to 7 to send gesture events by hand (the Surface
# README section lists them), or use the "gesture" script command. KICK_GESTURE_PORT (default 50501) is the app's gesture
# server. KICK_EMU_GESTURES=1 turns the app's gesture switch on without the settings page; --gesture sets it for you.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${EMU_BUILD_DIR:-build}"
if [[ "$(uname)" == Darwin ]]; then
    KICK_BIN="$REPO/$OUT/emu/Kick.app/Contents/MacOS/Kick"
    SURFACE_REL="KICK Surface.app/Contents/MacOS/KICK Surface"
else
    KICK_BIN="$(find "$REPO/$OUT/emu/apps/kick" -type f -name Kick -perm -u+x 2>/dev/null | head -1)"
    SURFACE_REL="KICK Surface"
fi
# Single-config builds put the app under KickSurface_artefacts/<Config>/, older configures at the top.
SURFACE_BIN=""
for candidate in "$REPO"/$OUT/surface/KickSurface_artefacts/{Release,Debug,.}/"$SURFACE_REL"; do
    [[ -x "$candidate" ]] && { SURFACE_BIN="$candidate"; break; }
done

SCRIPT=""
USE_SURFACE=1
USE_FIRMWARE=0
TCP_PORT=""
CAMERA=0
CAMERA_ENABLE=0
CAMERA_ONLY=0
GESTURE=0
export KICK_EMU_ROOT="${KICK_EMU_ROOT:-$HOME/KickEmu}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --script) SCRIPT="$2"; shift ;;
        --root) export KICK_EMU_ROOT="$2"; shift ;;
        --no-surface) USE_SURFACE=0 ;;
        --firmware) USE_FIRMWARE=1 ;;
        --tcp) TCP_PORT="$2"; shift ;;
        --camera) CAMERA=1 ;;
        --camera-enable) CAMERA=1; CAMERA_ENABLE=1 ;;
        --camera-only) CAMERA=1; CAMERA_ONLY=1 ;;
        --gesture) GESTURE=1 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

MCU_SIM_BIN="$REPO/${MCU_BUILD_DIR:-build/mcu}/kick-mcu-sim"
if [[ "$USE_FIRMWARE" == 1 ]]; then
    [[ -x "$KICK_BIN" ]] || { echo "not built yet: run emu/build.sh" >&2; exit 1; }
    [[ -x "$MCU_SIM_BIN" ]] || { echo "kick-mcu-sim not built: cmake -S Firmware/kick-mcu -B ${MCU_BUILD_DIR:-build/mcu} -G Ninja && cmake --build ${MCU_BUILD_DIR:-build/mcu}" >&2; exit 1; }
    [[ "$USE_SURFACE" == 1 ]] || { echo "--firmware replaces the Surface; it cannot be combined with --no-surface" >&2; exit 2; }
elif [[ "$CAMERA_ONLY" == 0 ]]; then
    [[ -x "$KICK_BIN" && -n "$SURFACE_BIN" ]] || { echo "not built yet: run emu/build.sh" >&2; exit 1; }
fi

# --gesture switches the app's gesture control on (the settings page does the same on a real unit).
[[ "$GESTURE" == 1 ]] && export KICK_EMU_GESTURES=1

mkdir -p "$KICK_EMU_ROOT"
[[ "$CAMERA_ONLY" == 1 || -d "$KICK_EMU_ROOT/data/sounds/Emu Kit" ]] || python3 "$REPO/emu/tools/make_samples.py" "$KICK_EMU_ROOT"

PIDS=()
cleanup() {
    for pid in "${PIDS[@]:-}"; do kill "$pid" 2>/dev/null || true; done
}
trap cleanup EXIT INT TERM

# kick-camera: the camera capture service from Kick/services/kick_camera, on the host instead of the unit.
# It builds in a few seconds (plain C++, no JUCE) into $OUT/camera.
start_camera() {
    export KICK_CAMERA_SOCKET="$KICK_EMU_ROOT/run/kick-camera.sock"
    export KICK_CAMERA_SHM="/kick-camera"
    mkdir -p "$KICK_EMU_ROOT/run"
    local bin="$REPO/$OUT/camera/kick-camera"
    if [[ ! -x "$bin" ]]; then
        cmake -S "$REPO/Kick/services/kick_camera" -B "$REPO/$OUT/camera" -DKICK_CAMERA_BUILD_TESTS=OFF >/dev/null
        cmake --build "$REPO/$OUT/camera" >/dev/null
    fi
    local args=(--socket "$KICK_CAMERA_SOCKET" --shm "$KICK_CAMERA_SHM")
    case "${KICK_CAMERA_SOURCE:-pattern}" in
        pattern) args+=(--source pattern) ;;
        file) args+=(--source file --file "${KICK_CAMERA_FILE:?set KICK_CAMERA_FILE for KICK_CAMERA_SOURCE=file}") ;;
        webcam) args=("$REPO/emu/tools/camera-webcam.sh" "--" "${args[@]}") ;;
        *) echo "unknown KICK_CAMERA_SOURCE: ${KICK_CAMERA_SOURCE}" >&2; exit 2 ;;
    esac
    [[ "$CAMERA_ENABLE" == 1 ]] && args+=(--enable)
    if [[ "${KICK_CAMERA_SOURCE:-pattern}" == webcam ]]; then
        "${args[@]}" >"$KICK_EMU_ROOT/camera.log" 2>&1 &
    else
        "$bin" "${args[@]}" >"$KICK_EMU_ROOT/camera.log" 2>&1 &
    fi
    CAMERA_PID=$!
    PIDS+=("$CAMERA_PID")
    for _ in $(seq 1 50); do
        [[ -S "$KICK_CAMERA_SOCKET" ]] && break
        kill -0 "$CAMERA_PID" 2>/dev/null || { cat "$KICK_EMU_ROOT/camera.log" >&2; exit 1; }
        sleep 0.1
    done
    echo "kick-camera pid $CAMERA_PID, socket $KICK_CAMERA_SOCKET, log $KICK_EMU_ROOT/camera.log (camera is off until: kick-camera --socket '$KICK_CAMERA_SOCKET' --ctl enable)"
}

# kick-gesture without the ONNX backend: enough for --replay, which needs no model. Plain C++, builds in seconds into $OUT/gesture-svc.
start_gesture_service() {
    local dir="$REPO/$OUT/gesture-svc" port="${KICK_GESTURE_PORT:-50501}"
    if [[ ! -x "$dir/kick-gesture" || ! -x "$dir/kick-gesture-synth" ]]; then
        cmake -S "$REPO/Kick/services/kick_gesture" -B "$dir" -DKICK_GESTURE_WITH_ONNX=OFF -DKICK_GESTURE_FETCH=OFF \
            -DKICK_GESTURE_BUILD_TESTS=OFF -DKICK_CAMERA_BUILD_TESTS=OFF >"$REPO/$OUT/gesture-svc-configure.log" 2>&1 \
            || { tail -20 "$REPO/$OUT/gesture-svc-configure.log" >&2; exit 1; }
        cmake --build "$dir" --target kick-gesture kick-gesture-synth >"$REPO/$OUT/gesture-svc-build.log" 2>&1 \
            || { tail -20 "$REPO/$OUT/gesture-svc-build.log" >&2; exit 1; }
    fi
    "$dir/kick-gesture-synth" demo >"$KICK_EMU_ROOT/gesture-demo.jsonl"
    # Wait for the app's gesture server, then replay the stream in real time, again and again.
    (
        for _ in $(seq 1 600); do
            (exec 3<>"/dev/tcp/127.0.0.1/$port") 2>/dev/null && break
            sleep 0.2
        done
        while true; do
            "$dir/kick-gesture" --replay "$KICK_EMU_ROOT/gesture-demo.jsonl" --realtime --events-ipc "127.0.0.1:$port" || true
            sleep 4
        done
    ) >"$KICK_EMU_ROOT/gesture.log" 2>&1 &
    PIDS+=("$!")
    echo "kick-gesture replay loop pid $!, log $KICK_EMU_ROOT/gesture.log"
}

if [[ "$CAMERA" == 1 ]]; then
    rm -f "$KICK_EMU_ROOT/run/kick-camera.sock"
    start_camera
    if [[ "$CAMERA_ONLY" == 1 ]]; then
        wait "$CAMERA_PID" || true
        exit 0
    fi
fi

if [[ "$USE_SURFACE" == 1 ]]; then
    rm -f "$KICK_EMU_ROOT/dev/ttymxc0" "$KICK_EMU_ROOT/surface.port"
    SURFACE_ARGS=(--root "$KICK_EMU_ROOT")
    [[ -n "$SCRIPT" ]] && SURFACE_ARGS+=(--script "$SCRIPT")
    [[ -n "$TCP_PORT" ]] && SURFACE_ARGS+=(--tcp "$TCP_PORT")
    if [[ "$USE_FIRMWARE" == 1 ]]; then
        # The firmware simulator reads Kick's log to check what the host decoded (`expect host` in a script).
        : >"$KICK_EMU_ROOT/kick.log"
        "$MCU_SIM_BIN" "${SURFACE_ARGS[@]}" --host-log "$KICK_EMU_ROOT/kick.log" >"$KICK_EMU_ROOT/surface.log" 2>&1 &
    else
        "$SURFACE_BIN" "${SURFACE_ARGS[@]}" >"$KICK_EMU_ROOT/surface.log" 2>&1 &
    fi
    SURFACE_PID=$!
    PIDS+=("$SURFACE_PID")

    if [[ -z "$TCP_PORT" ]]; then
        for _ in $(seq 1 50); do
            [[ -e "$KICK_EMU_ROOT/dev/ttymxc0" ]] && break
            kill -0 "$SURFACE_PID" 2>/dev/null || { cat "$KICK_EMU_ROOT/surface.log" >&2; exit 1; }
            sleep 0.1
        done
        export KICK_SERIAL_PORT="$KICK_EMU_ROOT/dev/ttymxc0"
    else
        # Kick sends its first message as soon as it connects, so the listener has to exist by then.
        for _ in $(seq 1 50); do
            [[ -e "$KICK_EMU_ROOT/surface.port" ]] && break
            kill -0 "$SURFACE_PID" 2>/dev/null || { cat "$KICK_EMU_ROOT/surface.log" >&2; exit 1; }
            sleep 0.1
        done
        export KICK_SERIAL_PORT="tcp://127.0.0.1:$TCP_PORT"
    fi
fi

# Kick resolves ./assets, ./demos and ./templates against its working directory.
cd "$REPO/$OUT/emu"
"$KICK_BIN" >"$KICK_EMU_ROOT/kick.log" 2>&1 &
KICK_PID=$!
PIDS+=("$KICK_PID")
echo "Kick pid $KICK_PID, logs in $KICK_EMU_ROOT/kick.log and surface.log"

if [[ "$GESTURE" == 1 ]]; then
    start_gesture_service
fi

if [[ -n "$SCRIPT" && "$USE_SURFACE" == 1 ]]; then
    # A script ends the run: its exit status is the test result.
    set +e
    wait "$SURFACE_PID"
    STATUS=$?
    set -e
    exit "$STATUS"
fi

wait "$KICK_PID" || true
