#!/usr/bin/env bash
# Builds the emulator in Docker and runs the smoke test under Xvfb.
#   emu/docker/run-linux.sh [build|test|shell]    (default: build then test)
# Build output goes to build-linux/ in the repo so the host build/ directory is left alone.
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE=kick-emu-linux
MODE="${1:-all}"

docker build -q -t "$IMAGE" "$REPO/emu/docker" >/dev/null
run() { docker run --rm -e JOBS="${JOBS:-3}" -v "$REPO":/src -w /src "$IMAGE" bash -lc "$1"; }

if [[ "$MODE" == shell ]]; then
    docker run --rm -it -v "$REPO":/src -w /src "$IMAGE" bash
    exit
fi

if [[ "$MODE" == build || "$MODE" == all ]]; then
    run 'EMU_BUILD_DIR=build-linux emu/build.sh --release -j ${JOBS:-3}'
fi
if [[ "$MODE" == test || "$MODE" == all ]]; then
    # Xvfb is started by hand: xvfb-run waits forever for the server's ready signal in this image.
    run 'Xvfb :99 -screen 0 1600x1000x24 >/dev/null 2>&1 & sleep 2; export DISPLAY=:99 EMU_BUILD_DIR=build-linux KICK_EMU_ROOT=/tmp/kickemu; emu/run.sh --script emu/tests/smoke.script; rc=$?; cp -f /tmp/kickemu/*.png /src/build-linux/ 2>/dev/null; tail -n 20 /tmp/kickemu/kick.log /tmp/kickemu/surface.log 2>/dev/null; exit $rc'
fi
