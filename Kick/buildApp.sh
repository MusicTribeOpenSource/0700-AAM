#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
set -euo pipefail

build_type="${1:-Release}"
if (( $# )); then shift; fi
case "$build_type" in
    Debug|Release|RelWithDebInfo|MinSizeRel) ;;
    *) printf 'Unsupported build type: %s\n' "$build_type" >&2; exit 2 ;;
esac
: "${TOOLCHAIN_FILE:?Set TOOLCHAIN_FILE to your Buildroot SDK toolchainfile.cmake}"
if [[ ! -f "$TOOLCHAIN_FILE" ]]; then
    printf 'Toolchain file not found: %s\n' "$TOOLCHAIN_FILE" >&2
    exit 2
fi
kick_source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
kick_build_dir="$kick_source_dir/Build/$build_type"
cmake -S "$kick_source_dir" -B "$kick_build_dir" -G Ninja \
    "-DCMAKE_BUILD_TYPE=$build_type" "-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE" \
    -DKICK_ENABLE_HARDWARE=ON "$@"
cmake --build "$kick_build_dir" --target Kick --parallel "${KICK_BUILD_JOBS:-2}"
printf 'KICK target executable: %s/bin/Kick\n' "$kick_build_dir"
