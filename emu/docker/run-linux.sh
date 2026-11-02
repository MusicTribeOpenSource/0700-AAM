#!/usr/bin/env bash
# Builds the emulator in Docker and runs the smoke test under Xvfb.
#   emu/docker/run-linux.sh [build|test|shell]    (default: build then test)
# Build output goes to build-linux/ in the repo so the host build/ directory is left alone.
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE=kick-emu-linux
MODE="${1:-all}"

docker build -q -t "$IMAGE" "$REPO/emu/docker" >/dev/null
run() { docker run --rm -v "$REPO":/src -w /src "$IMAGE" bash -lc "$1"; }

if [[ "$MODE" == shell ]]; then
    docker run --rm -it -v "$REPO":/src -w /src "$IMAGE" bash
    exit
fi

if [[ "$MODE" == build || "$MODE" == all ]]; then
    run 'EMU_BUILD_DIR=build-linux emu/build.sh'
fi
if [[ "$MODE" == test || "$MODE" == all ]]; then
    run 'EMU_BUILD_DIR=build-linux xvfb-run -a -s "-screen 0 1600x1000x24" emu/run.sh --script emu/tests/smoke.script'
fi
