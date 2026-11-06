#!/usr/bin/env bash
# Copyright (c) 2024 MUSIC Tribe
#
# This file is part of Kick.
#
# Kick is free software: you can redistribute it and/or modify it under the
# terms of the GNU Affero General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# Kick is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
# more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with Kick. If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: AGPL-3.0-or-later
# /
# Starts the real kick-camera binary and drives it through its command line and control socket, with the test
# pattern as the source. Reads a frame back from shared memory with kick-camera-peek in a separate process.
#   cli_smoke.sh /path/to/kick-camera /path/to/kick-camera-peek
set -u
CAM="$1"
PEEK="$2"
DIR="$(mktemp -d /tmp/kcam-cli-XXXXXX)"
SOCK="$DIR/c.sock"
SHM="/kc-cli-$$"
fail() { echo "FAIL: $*" >&2; kill "${PID:-0}" 2>/dev/null; rm -rf "$DIR"; exit 1; }
want() { [[ "$1" == *"$2"* ]] || fail "expected '$2' in: $1"; }

"$CAM" --source pattern --socket "$SOCK" --shm "$SHM" --fps 30 2>"$DIR/log" &
PID=$!
for _ in $(seq 1 50); do [[ -S "$SOCK" ]] && break; sleep 0.1; done
[[ -S "$SOCK" ]] || fail "socket never appeared: $(cat "$DIR/log")"

status="$("$CAM" --socket "$SOCK" --ctl status)"
want "$status" '"state":"off"'
"$PEEK" --shm "$SHM" --wait-ms 200 >/dev/null && fail "got a frame while the camera was off"

[[ "$("$CAM" --socket "$SOCK" --ctl enable)" == ok ]] || fail "enable"
out="$("$PEEK" --shm "$SHM" --out "$DIR/f.pgm")" || fail "no frame after enable: $out"
want "$out" "320x240 YUYV"
[[ "$(head -c 2 "$DIR/f.pgm")" == P5 ]] || fail "peek did not write a PGM"
[[ "$(wc -c <"$DIR/f.pgm")" -ge $((320 * 240)) ]] || fail "PGM too small"

[[ "$("$CAM" --socket "$SOCK" --ctl preset dim_stage)" == ok ]] || fail "multi-word --ctl"
want "$("$CAM" --socket "$SOCK" --ctl status)" '"preset":"dim_stage"'
"$CAM" --socket "$SOCK" --ctl bogus >/dev/null 2>&1 && fail "unknown command should exit non-zero"

[[ "$("$CAM" --socket "$SOCK" --ctl disable)" == ok ]] || fail "disable"
"$PEEK" --shm "$SHM" --wait-ms 200 >/dev/null && fail "got a frame after disable"

kill -TERM "$PID"
for _ in $(seq 1 50); do kill -0 "$PID" 2>/dev/null || break; sleep 0.1; done
kill -0 "$PID" 2>/dev/null && fail "service did not stop on SIGTERM"
[[ ! -e "$SOCK" ]] || fail "socket file left behind"
"$PEEK" --shm "$SHM" --wait-ms 100 >/dev/null 2>&1 && fail "shared memory left behind"
rm -rf "$DIR"
echo "cli smoke ok"
