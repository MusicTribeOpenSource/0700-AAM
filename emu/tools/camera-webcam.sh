#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Runs kick-camera on a desktop webcam, in the foreground. The camera stays off until something sends "enable".
#
#   emu/tools/camera-webcam.sh [options] [-- kick-camera options]
#     --index N         macOS: avfoundation video device index (default 0). Use --list to see them.
#     --size WxH        frame size handed to the service (default 320x240)
#     --fps N           frame rate handed to the service (default 15)
#     --list            macOS: list the avfoundation devices and exit
#     --print-command   print the ffmpeg pipeline the service would run, then exit
#
#   macOS: ffmpeg reads the webcam through avfoundation and writes raw YUYV to the service's pipe. The service
#   starts ffmpeg on "enable" and stops it on "disable", so the webcam (and its light) is only on while enabled.
#   The first run asks for camera permission for the terminal app.
#   Linux: no ffmpeg needed. The service reads the UVC device directly (V4L2), DEVICE=/dev/videoN, default /dev/video0.
#
# Environment: EMU_BUILD_DIR (build), CAMERA_INPUT_FPS (30, what the webcam is asked for before the service
# resamples it to --fps: many Mac cameras refuse 15), KICK_CAMERA_FFMPEG_INPUT (replaces the avfoundation input,
# for example "-re -f lavfi -i testsrc=size=640x480:rate=30" to try the pipeline without a webcam; -re paces a
# synthetic input, a real camera paces itself).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${EMU_BUILD_DIR:-build}"
INDEX=0
SIZE=320x240
FPS=15
PRINT=0
LIST=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --index) INDEX="$2"; shift ;;
        --size) SIZE="$2"; shift ;;
        --fps) FPS="$2"; shift ;;
        --list) LIST=1 ;;
        --print-command) PRINT=1 ;;
        --) shift; break ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done
W="${SIZE%x*}"
H="${SIZE#*x}"

if [[ "$(uname)" == Darwin ]]; then
    command -v ffmpeg >/dev/null || { echo "ffmpeg not found (brew install ffmpeg)" >&2; exit 1; }
    if [[ "$LIST" == 1 ]]; then
        ffmpeg -hide_banner -f avfoundation -list_devices true -i "" 2>&1 | grep -E "AVFoundation|\[[0-9]+\]" || true
        exit 0
    fi
    INPUT="${KICK_CAMERA_FFMPEG_INPUT:--f avfoundation -framerate ${CAMERA_INPUT_FPS:-30} -i ${INDEX}:none}"
    PIPE_COMMAND="exec ffmpeg -hide_banner -loglevel error $INPUT -vf fps=$FPS,scale=$W:$H -pix_fmt yuyv422 -f rawvideo -"
    [[ "$PRINT" == 1 ]] && { echo "$PIPE_COMMAND"; exit 0; }
    SOURCE_ARGS=(--source pipe --pipe-command "$PIPE_COMMAND")
else
    [[ "$LIST" == 1 ]] && { ls /dev/video* 2>/dev/null || echo "no /dev/video* nodes"; exit 0; }
    [[ "$PRINT" == 1 ]] && { echo "(Linux reads ${DEVICE:-/dev/video0} directly, no ffmpeg pipeline)"; exit 0; }
    SOURCE_ARGS=(--source v4l2 --device "${DEVICE:-/dev/video0}")
fi

BIN="$REPO/$OUT/camera/kick-camera"
if [[ ! -x "$BIN" ]]; then
    cmake -S "$REPO/Kick/services/kick_camera" -B "$REPO/$OUT/camera" -DKICK_CAMERA_BUILD_TESTS=OFF >/dev/null
    cmake --build "$REPO/$OUT/camera" >/dev/null
fi
exec "$BIN" "${SOURCE_ARGS[@]}" --size "$SIZE" --fps "$FPS" "$@"
