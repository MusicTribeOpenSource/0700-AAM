#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Replays a recorded host session (KICK_EMU_TRACE) into the Surface and into the firmware and
# diffs the final LED tables. Exit 0 when they match.
# Usage: emu/tools/compare-leds.sh [trace]   (default emu/tests/traces/leds.trace)
# Environment: MCU_BUILD_DIR (build/mcu), SURFACE_BIN (the built KICK Surface binary).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TRACE="${1:-$REPO/emu/tests/traces/leds.trace}"
SIM="${MCU_BUILD_DIR:-$REPO/build/mcu}/kick-mcu-sim"
SURFACE="${SURFACE_BIN:-$REPO/build/surface/KickSurface_artefacts/Release/KICK Surface.app/Contents/MacOS/KICK Surface}"

[[ -f "$TRACE" ]] || { echo "trace not found: $TRACE" >&2; exit 2; }
[[ -x "$SIM" ]] || { echo "missing $SIM (cmake -S Firmware/kick-mcu -B build/mcu && cmake --build build/mcu)" >&2; exit 2; }
[[ -x "$SURFACE" ]] || { echo "missing Surface binary: $SURFACE (set SURFACE_BIN, or run emu/build.sh)" >&2; exit 2; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

"$SURFACE" --replay "$TRACE" --dump-leds "$WORK/surface.json" 2>/dev/null
"$SIM" --replay "$TRACE" --dump-leds "$WORK/firmware.json" 2>/dev/null

if diff -u "$WORK/surface.json" "$WORK/firmware.json"; then
    echo "LED tables match: $(basename "$TRACE")"
else
    echo "LED tables differ: $(basename "$TRACE")" >&2
    exit 1
fi
