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
"""Converts the JPEG test photos fetched by CMake into binary PPM files (P6) that the C++ tests read.

The tests have no JPEG decoder on purpose, so this is the one place that needs OpenCV or Pillow. Run it once per
build directory, or let ctest's fixture do it (tests/CMakeLists.txt).

    prepare_test_images.py [--if-possible] <dir with *.jpg>

With --if-possible the script exits 0 and converts nothing when neither OpenCV nor Pillow is installed. The model tests
that need the photos skip themselves when the PPM files are missing, so a machine without either library still gets a green
run, with those tests reported as skipped.
"""
import pathlib
import sys


def load_rgb(path):
    try:
        import cv2

        bgr = cv2.imread(str(path), cv2.IMREAD_COLOR)
        if bgr is None:
            raise SystemExit(f"cannot read {path}")
        h, w = bgr.shape[:2]
        return w, h, cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).tobytes()
    except ImportError:
        from PIL import Image

        im = Image.open(path).convert("RGB")
        return im.width, im.height, im.tobytes()


def have_imaging_library():
    for name in ("cv2", "PIL"):
        try:
            __import__(name)
            return True
        except ImportError:
            pass
    return False


def main(argv):
    args = argv[1:]
    if_possible = "--if-possible" in args
    args = [a for a in args if a != "--if-possible"]
    if len(args) != 1:
        print(__doc__)
        return 2
    if if_possible and not have_imaging_library():
        print("neither cv2 nor PIL is installed: no PPM files written, the photo tests will skip")
        return 0
    d = pathlib.Path(args[0])
    jpgs = sorted(d.glob("*.jpg"))
    if not jpgs:
        print(f"no .jpg files in {d}", file=sys.stderr)
        return 1
    for jpg in jpgs:
        w, h, rgb = load_rgb(jpg)
        out = jpg.with_suffix(".ppm")
        out.write_bytes(f"P6\n{w} {h}\n255\n".encode() + rgb)
        print(f"{out.name}: {w}x{h}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
