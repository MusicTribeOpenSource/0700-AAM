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
"""Regenerates testdata/reference_landmarks.json: what the OpenCV Zoo's own Python pipeline (cv2.dnn, float32 models)
finds in the test photos. The C++ backend is checked against these numbers, so a geometry mistake in the port shows up
as a number, not as a feeling.

Needs opencv-python and numpy. Fetches the two zoo scripts at the pinned commit into a temporary directory.

    make_reference.py <dir with *.jpg> <dir with the fp32 .onnx models> > testdata/reference_landmarks.json
"""
import json
import pathlib
import sys
import tempfile
import urllib.request

ZOO_COMMIT = "48b10fa505aeb51fbd127fc60a5c660040e823b7"
RAW = f"https://raw.githubusercontent.com/opencv/opencv_zoo/{ZOO_COMMIT}/models"


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    import cv2 as cv
    import numpy as np

    imgs = pathlib.Path(argv[1])
    models = pathlib.Path(argv[2])
    work = pathlib.Path(tempfile.mkdtemp(prefix="kg-ref-"))
    for rel in ("palm_detection_mediapipe/mp_palmdet.py", "handpose_estimation_mediapipe/mp_handpose.py"):
        (work / pathlib.Path(rel).name).write_bytes(urllib.request.urlopen(f"{RAW}/{rel}").read())
    sys.path.insert(0, str(work))
    from mp_handpose import MPHandPose
    from mp_palmdet import MPPalmDet

    palm = MPPalmDet(str(models / "palm_detection_mediapipe_2023feb.onnx"), nmsThreshold=0.3, scoreThreshold=0.5)
    hand = MPHandPose(str(models / "handpose_estimation_mediapipe_2023feb.onnx"), confThreshold=0.5)
    out = {"zoo_commit": ZOO_COMMIT, "images": {}}
    for jpg in sorted(imgs.glob("*.jpg")):
        img = cv.imread(str(jpg))
        h, w = img.shape[:2]
        palms = palm.infer(img)
        entry = {"width": w, "height": h, "found": False}
        if len(palms):
            best = max(palms, key=lambda p: p[-1])
            res = hand.infer(img, best)
            if res is not None:
                lm = res[4:67].reshape(21, 3)
                entry = {
                    "width": w,
                    "height": h,
                    "found": True,
                    "palm_score": round(float(best[-1]), 4),
                    "score": round(float(res[-1]), 4),
                    "right_prob": round(float(res[-2]), 4),
                    "landmarks": [[round(float(x) / w, 5), round(float(y) / h, 5)] for x, y, _ in lm],
                }
        out["images"][jpg.stem] = entry
    # One image per line keeps the file reviewable.
    lines = [f'  {json.dumps(k)}: {json.dumps(v, separators=(",", ":"))}' for k, v in out["images"].items()]
    print('{\n "zoo_commit": ' + json.dumps(out["zoo_commit"]) + ',\n "images": {\n' + ",\n".join(lines) + "\n }\n}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
