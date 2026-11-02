#!/usr/bin/env bash
# Builds the Kick app (KICK_EMU=ON) and the KICK Surface into <repo>/build (EMU_BUILD_DIR overrides).
# Usage: emu/build.sh [--release] [-j N]
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE=Debug
OUT="${EMU_BUILD_DIR:-build}"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
mkdir -p "$REPO/$OUT"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --release) BUILD_TYPE=Release ;;
        -j) shift; JOBS="$1" ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

for tool in cmake ninja; do
    command -v "$tool" >/dev/null || { echo "missing $tool (brew install $tool)" >&2; exit 1; }
done

echo "== Kick (KICK_EMU=ON, $BUILD_TYPE)"
cmake -S "$REPO/Kick" -B "$REPO/$OUT/emu" -G Ninja -DKICK_EMU=ON -DCMAKE_BUILD_TYPE="$BUILD_TYPE" >"$REPO/$OUT/emu-configure.log" 2>&1 \
    || { tail -30 "$REPO/$OUT/emu-configure.log"; exit 1; }
cmake --build "$REPO/$OUT/emu" -j "$JOBS"

echo "== KICK Surface"
cmake -S "$REPO/emu/surface" -B "$REPO/$OUT/surface" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" >"$REPO/$OUT/surface-configure.log" 2>&1 \
    || { tail -30 "$REPO/$OUT/surface-configure.log"; exit 1; }
cmake --build "$REPO/$OUT/surface" -j "$JOBS"

echo "built: Kick and KICK Surface under $REPO/$OUT"
