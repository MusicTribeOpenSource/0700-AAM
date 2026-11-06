#!/usr/bin/env bash
# Runs emu/tests/gesture.script against Kick and the Surface with gestures forced on, then checks the app log for the
# panel input the gestures produced. Needs emu/build.sh first. Takes about a minute.
# Usage: emu/tests/run-gesture-test.sh
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export KICK_EMU_ROOT="${KICK_EMU_ROOT:-$(mktemp -d "${TMPDIR:-/tmp}/kick-gesture-test.XXXXXX")}"
export KICK_EMU_GESTURES=1
# A private port so a Kick that is already running does not catch the test's gestures.
export KICK_GESTURE_PORT="${KICK_GESTURE_PORT:-50599}"

# Pass 1: gestures forced on.
"$REPO/emu/run.sh" --script "$REPO/emu/tests/gesture.script"

LOG="$KICK_EMU_ROOT/kick.log"
fail=0
expect_log() {
    if grep -qF "$1" "$LOG"; then
        echo "PASS  $2"
    else
        echo "FAIL  $2 (no '$1' in $LOG)"
        fail=1
    fi
}
expect_log "Data[0] From DCL:45 cmd From DCL : 0 type From DCL : 1" "palm_hold pressed Play (button 45)"
expect_log "Data[0] From DCL:44 cmd From DCL : 0 type From DCL : 1" "fist pressed Stop (button 44)"
expect_log "Data[0] From DCL:1 cmd From DCL : 1 type From DCL : 4" "pinch turned Q-Link knob 1"

# Pass 2: gestures off, the default. The same swipe must do nothing.
unset KICK_EMU_GESTURES
export KICK_EMU_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/kick-gesture-test.XXXXXX")"
"$REPO/emu/run.sh" --script "$REPO/emu/tests/gesture-off.script" && echo "PASS  a swipe changes nothing while gestures are off" || { echo "FAIL  a swipe changed the bank while gestures were off"; fail=1; }
exit "$fail"
