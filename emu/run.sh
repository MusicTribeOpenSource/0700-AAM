#!/usr/bin/env bash
# Runs Kick against the virtual control surface.
#
#   emu/run.sh                      Kick plus KICK Surface windows, until you close one
#   emu/run.sh --script file        drive the Surface from a script, exit with its result
#   emu/run.sh --root DIR           emulated device root (default ~/KickEmu)
#   emu/run.sh --no-surface         Kick only (set KICK_SERIAL_PORT yourself)
#   emu/run.sh --tcp PORT           Surface listens on TCP for QEMU instead of opening a pty
#
# Environment: KICK_EMU_ROOT, KICK_EMU_CONTROL_PORT (50901), KICK_EMU_EVENT_PORT (50902),
# KICK_AUDIO_DEVICE (CoreAudio device name; default system output).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KICK_BIN="$REPO/build/emu/Kick.app/Contents/MacOS/Kick"
# Single-config builds put the app under KickSurface_artefacts/<Config>/, older configures at the top.
SURFACE_BIN=""
for candidate in "$REPO"/build/surface/KickSurface_artefacts/{Release,Debug,.}/"KICK Surface.app/Contents/MacOS/KICK Surface"; do
    [[ -x "$candidate" ]] && { SURFACE_BIN="$candidate"; break; }
done

SCRIPT=""
USE_SURFACE=1
TCP_PORT=""
export KICK_EMU_ROOT="${KICK_EMU_ROOT:-$HOME/KickEmu}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --script) SCRIPT="$2"; shift ;;
        --root) export KICK_EMU_ROOT="$2"; shift ;;
        --no-surface) USE_SURFACE=0 ;;
        --tcp) TCP_PORT="$2"; shift ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

[[ -x "$KICK_BIN" && -n "$SURFACE_BIN" ]] || { echo "not built yet: run emu/build.sh" >&2; exit 1; }

mkdir -p "$KICK_EMU_ROOT"
[[ -d "$KICK_EMU_ROOT/data/sounds/Emu Kit" ]] || python3 "$REPO/emu/tools/make_samples.py" "$KICK_EMU_ROOT"

PIDS=()
cleanup() {
    for pid in "${PIDS[@]:-}"; do kill "$pid" 2>/dev/null || true; done
}
trap cleanup EXIT INT TERM

if [[ "$USE_SURFACE" == 1 ]]; then
    rm -f "$KICK_EMU_ROOT/dev/ttymxc0"
    SURFACE_ARGS=(--root "$KICK_EMU_ROOT")
    [[ -n "$SCRIPT" ]] && SURFACE_ARGS+=(--script "$SCRIPT")
    [[ -n "$TCP_PORT" ]] && SURFACE_ARGS+=(--tcp "$TCP_PORT")
    "$SURFACE_BIN" "${SURFACE_ARGS[@]}" >"$KICK_EMU_ROOT/surface.log" 2>&1 &
    SURFACE_PID=$!
    PIDS+=("$SURFACE_PID")

    if [[ -z "$TCP_PORT" ]]; then
        for _ in $(seq 1 50); do
            [[ -e "$KICK_EMU_ROOT/dev/ttymxc0" ]] && break
            kill -0 "$SURFACE_PID" 2>/dev/null || { cat "$KICK_EMU_ROOT/surface.log" >&2; exit 1; }
            sleep 0.1
        done
        export KICK_SERIAL_PORT="$KICK_EMU_ROOT/dev/ttymxc0"
    fi
fi

# Kick resolves ./assets, ./demos and ./templates against its working directory.
cd "$REPO/build/emu"
"$KICK_BIN" >"$KICK_EMU_ROOT/kick.log" 2>&1 &
KICK_PID=$!
PIDS+=("$KICK_PID")
echo "Kick pid $KICK_PID, logs in $KICK_EMU_ROOT/kick.log and surface.log"

if [[ -n "$SCRIPT" && "$USE_SURFACE" == 1 ]]; then
    # A script ends the run: its exit status is the test result.
    set +e
    wait "$SURFACE_PID"
    STATUS=$?
    set -e
    exit "$STATUS"
fi

wait "$KICK_PID" || true
