#!/usr/bin/env bash
# Boots the Kick rootfs in QEMU with the KICK Surface on the second UART.
# SCAFFOLD: untested. Needs qemu-system-aarch64 (brew install qemu) and an image from build-image.sh.
#
#   emu/qemu/run-qemu.sh [image-dir]
#
# Serial layout: UART0 = console on stdio, UART1 = control surface over TCP 127.0.0.1:50910.
# The Surface listens on that port and QEMU connects to it as a client.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMG="${1:-$REPO/build/qemu-image}"
PORT=50910

[[ -f "$IMG/Image" && -f "$IMG/rootfs.ext4" ]] || { echo "missing $IMG/Image or rootfs.ext4: run emu/qemu/build-image.sh" >&2; exit 1; }
command -v qemu-system-aarch64 >/dev/null || { echo "install qemu: brew install qemu" >&2; exit 1; }

SURFACE_BIN="$REPO/build/surface/KickSurface_artefacts/KICK Surface.app/Contents/MacOS/KICK Surface"
"$SURFACE_BIN" --root "${KICK_EMU_ROOT:-$HOME/KickEmu}" --tcp "$PORT" >/dev/null 2>&1 &
SURFACE_PID=$!
trap 'kill $SURFACE_PID 2>/dev/null || true' EXIT
sleep 1

exec qemu-system-aarch64 \
    -machine virt,highmem=on -accel hvf -cpu host -smp 4 -m 2048 \
    -kernel "$IMG/Image" \
    -append "root=/dev/vda rw console=ttyAMA0" \
    -drive file="$IMG/rootfs.ext4",format=raw,if=none,id=hd0 -device virtio-blk-device,drive=hd0 \
    -serial stdio \
    -serial tcp:127.0.0.1:$PORT \
    -device virtio-gpu-pci -display cocoa \
    -audiodev coreaudio,id=snd0 -device virtio-sound-pci,audiodev=snd0 \
    -device virtio-net-device,netdev=n0 -netdev user,id=n0
