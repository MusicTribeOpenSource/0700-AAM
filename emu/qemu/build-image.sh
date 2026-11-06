#!/usr/bin/env bash
# Builds the aarch64 Linux image for QEMU inside Docker (Buildroot does not build natively on macOS).
# SCAFFOLD: untested. Expect to iterate on the fragment (kernel, weston, JUCE runtime libs).
#
#   emu/qemu/build-image.sh
#
# Output: build/qemu-image/{Image,rootfs.ext4}
# Kick for the VM is cross-built separately (Kick/ with the Buildroot toolchain) and copied into
# the overlay as /usr/bin/KT_Kick before this runs. That cross build is not scripted yet.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$REPO/build/qemu-image"
mkdir -p "$OUT"

command -v docker >/dev/null || { echo "docker not found" >&2; exit 1; }

docker run --rm -v "$REPO:/work" -w /work/Linux/buildroot debian:12 bash -euxc '
    apt-get update
    apt-get install -y build-essential git wget cpio unzip rsync bc file python3 libncurses-dev libssl-dev
    # Start from the real defconfig, drop the i.MX-specific kernel and boot lines, merge the fragment.
    grep -v -E "^BR2_(LINUX_KERNEL_CUSTOM|LINUX_KERNEL_PATCH|LINUX_KERNEL_USE_CUSTOM|LINUX_KERNEL_INTREE_DTS|LINUX_KERNEL_IMAGEGZ|ROOTFS_POST|TARGET_UBOOT)" configs/KTKick_defconfig > /tmp/base_defconfig
    cat /tmp/base_defconfig /work/emu/qemu/KTKick_emu.fragment > configs/KTKick_emu_defconfig
    make KTKick_emu_defconfig
    make -j"$(nproc)"
    cp output/images/Image output/images/rootfs.ext4 /work/build/qemu-image/
'
echo "image in $OUT"
