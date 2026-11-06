#!/usr/bin/env python3
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
"""Fails when cmake/pins.cmake and models/MODELS.md disagree.

Every SHA-256 pinned in pins.cmake must appear in MODELS.md on a row that names the same file. The ONNX Runtime version
and the OpenCV Zoo commit in pins.cmake must appear in MODELS.md too. MODELS.md must not list a hash that pins.cmake lacks.

Usage: check_pins.py [PINS_CMAKE MODELS_MD]     defaults to the files next to this script
"""
import os
import re
import sys

HEX = re.compile(r"\b[0-9a-f]{64}\b")


def main(argv):
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    pins_path = argv[0] if len(argv) == 2 else os.path.join(root, "cmake", "pins.cmake")
    doc_path = argv[1] if len(argv) == 2 else os.path.join(root, "models", "MODELS.md")
    with open(pins_path) as f:
        pins = f.read()
    with open(doc_path) as f:
        doc_lines = f.read().splitlines()

    errors = []
    # set(KICK_GESTURE_<KIND>_SHA256_<name> "<hash>")
    pinned = {}
    for m in re.finditer(r'set\(KICK_GESTURE_\w*?SHA256_(\S+)\s+"([0-9a-f]{64})"\)', pins):
        pinned[m.group(1)] = m.group(2)
    if not pinned:
        errors.append("no SHA256 pins found in " + pins_path)

    version = re.search(r'KICK_GESTURE_ORT_VERSION\s+"([^"]+)"', pins)
    commit = re.search(r'KICK_GESTURE_ZOO_COMMIT\s+"([0-9a-f]{40})"', pins)
    doc_text = "\n".join(doc_lines)
    if not version or version.group(1) not in doc_text:
        errors.append("ONNX Runtime version in pins.cmake is not in MODELS.md")
    if not commit or commit.group(1) not in doc_text:
        errors.append("OpenCV Zoo commit in pins.cmake is not in MODELS.md")

    for name, digest in sorted(pinned.items()):
        # ORT pins are named by host (osx-arm64), the archive file in the doc contains the host name.
        rows = [l for l in doc_lines if digest in l]
        if not rows:
            errors.append(f"{name}: hash {digest} is missing from MODELS.md")
        elif not any(name in l for l in rows):
            errors.append(f"{name}: hash {digest} is in MODELS.md but not on a row naming {name}")

    for l in doc_lines:
        for digest in HEX.findall(l):
            if digest not in pinned.values():
                errors.append(f"MODELS.md lists {digest} which pins.cmake does not pin")

    for e in errors:
        print("check_pins: " + e)
    if errors:
        return 1
    print(f"check_pins: {len(pinned)} pins match MODELS.md")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
