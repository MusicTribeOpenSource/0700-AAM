#!/usr/bin/env bash
# Builds and runs the gesture IPC integration test: kick-gesture's sink against the app's real JUCE IPC server.
# Needs the JUCE checkout emu/build.sh fetched. Output goes to <repo>/build/gesture-ipc (EMU_BUILD_DIR overrides the base).
# Usage: emu/tests/run-gesture-ipc-test.sh
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$REPO/${EMU_BUILD_DIR:-build}/gesture-ipc"

for tool in cmake ninja; do
    command -v "$tool" >/dev/null || { echo "missing $tool" >&2; exit 1; }
done

cmake -S "$REPO/emu/tests/gesture_ipc" -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE=Debug >"$OUT.configure.log" 2>&1 \
    || { tail -30 "$OUT.configure.log"; exit 1; }
cmake --build "$OUT" -j "$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

BIN="$(find "$OUT" -type f -name 'GestureIpcTest' -path '*_artefacts*' -perm -u+x | head -1)"
[[ -n "$BIN" ]] || { echo "built binary not found under $OUT" >&2; exit 1; }
exec "$BIN"
