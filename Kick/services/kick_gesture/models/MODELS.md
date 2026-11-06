# Models and runtime

Nothing listed here is committed to the repository. The build downloads each file and refuses it when its SHA-256 differs from the value below. `cmake/pins.cmake` holds the same values and `tools/check_pins.py` fails when the two files disagree.

## ONNX Runtime

Version 1.24.4, MIT licence, Microsoft. Prebuilt release archives from https://github.com/microsoft/onnxruntime/releases/tag/v1.24.4.

| Host | Archive | SHA-256 |
|------|---------|---------|
| osx-arm64 | `onnxruntime-osx-arm64-1.24.4.tgz` | `93787795f47e1eee369182e43ed51b9e5da0878ab0346aecf4258979b8bba989` |
| linux-x64 | `onnxruntime-linux-x64-1.24.4.tgz` | `3a211fbea252c1e66290658f1b735b772056149f28321e71c308942cdb54b747` |
| linux-aarch64 | `onnxruntime-linux-aarch64-1.24.4.tgz` | `866109a9248d057671a039b9d725be4bd86888e3754140e6701ec621be9d4d7e` |

The aarch64 archive is the one the i.MX8M Buildroot image uses.

## Hand models

MediaPipe palm detection and hand landmark models converted to ONNX by the OpenCV Zoo, Apache-2.0 licence, at commit `48b10fa505aeb51fbd127fc60a5c660040e823b7` of https://github.com/opencv/opencv_zoo. The files are in Git LFS, so the download URL is on `media.githubusercontent.com`.

| File | Size | SHA-256 |
|------|------|---------|
| `palm_detection_mediapipe_2023feb_int8.onnx` | 1.1 MB | `9f014de96ef5b6816b3eb9a5fed21a7371ef0f104ea440aa19ce9129fe2af5f6` |
| `handpose_estimation_mediapipe_2023feb_int8.onnx` | 1.1 MB | `e97bc1fb83b641954d33424c82b6ade719d0f73250bdb91710ecfd5f7b47e321` |
| `palm_detection_mediapipe_2023feb.onnx` | 3.7 MB | `78ff51c38496b7fc8b8ebdb6cc8c1abb02fa6c38427c6848254cdaba57fcce7c` |
| `handpose_estimation_mediapipe_2023feb.onnx` | 3.9 MB | `db0898ae717b76b075d9bf563af315b29562e11f8df5027a1ef07b02bef6d81c` |

The int8 pair is the default. The fp32 pair is there for the parity test and as a fallback when the int8 pair loses too much accuracy on a target.

Palm detection takes a 192x192 RGB input. The 2016 SSD anchors are generated in code (`palm_anchors.cpp`) and match the zoo's anchors to 2e-8. Hand landmark takes a 224x224 RGB crop of a rotated square ROI and returns 21 landmarks, a hand score and a handedness score.

## Test photos

Used only by the model-backed tests. MediaPipe test assets, Apache-2.0, from https://storage.googleapis.com/mediapipe-assets. `tools/prepare_test_images.py` converts them to PPM, which needs OpenCV or Pillow. The service does not need them.

| File | SHA-256 |
|------|---------|
| `victory.jpg` | `84cb8853e3df614e0cb5c93a25e3e2f38ea5e4f92fd428ee7d867ed3479d5764` |
| `fist.jpg` | `43fa1cabf3f90d574accc9a56986e2ee48638ce59fc65af1846487f73bb2ef24` |
| `pointing_up.jpg` | `ecf8ca2611d08fa25948a4fc10710af9120e88243a54da6356bacea17ff3e36e` |
| `thumb_up.jpg` | `5d673c081ab13b8a1812269ff57047066f9c33c07db5f4178089e8cb3fdc0291` |

## Updating a pin

Change the value in `cmake/pins.cmake`, change it here, run `python3 tools/check_pins.py`. A model swap also needs `tools/make_reference.py` to regenerate `testdata/reference_landmarks.json`, and the `onnx_model_test` bounds to be re-checked.
