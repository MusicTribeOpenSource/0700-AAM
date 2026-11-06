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

# Everything that is downloaded, with the hash it must have. Nothing here is committed to the repository.
# models/MODELS.md documents the same pins and tools/check_pins.py fails when the two disagree.

# ONNX Runtime prebuilt release (MIT licence), one archive per supported host.
set(KICK_GESTURE_ORT_VERSION "1.24.4")
set(KICK_GESTURE_ORT_URL_BASE "https://github.com/microsoft/onnxruntime/releases/download/v${KICK_GESTURE_ORT_VERSION}")
set(KICK_GESTURE_ORT_SHA256_osx-arm64 "93787795f47e1eee369182e43ed51b9e5da0878ab0346aecf4258979b8bba989")
set(KICK_GESTURE_ORT_SHA256_linux-x64 "3a211fbea252c1e66290658f1b735b772056149f28321e71c308942cdb54b747")
set(KICK_GESTURE_ORT_SHA256_linux-aarch64 "866109a9248d057671a039b9d725be4bd86888e3754140e6701ec621be9d4d7e")

# MediaPipe palm detection and hand landmark models converted to ONNX by the OpenCV Zoo (Apache-2.0).
# They are served from Git LFS, so the URL is the media host at a pinned commit.
set(KICK_GESTURE_ZOO_COMMIT "48b10fa505aeb51fbd127fc60a5c660040e823b7")
set(KICK_GESTURE_ZOO_URL_BASE "https://media.githubusercontent.com/media/opencv/opencv_zoo/${KICK_GESTURE_ZOO_COMMIT}/models")
set(KICK_GESTURE_MODEL_FILES
    palm_detection_mediapipe_2023feb_int8.onnx
    handpose_estimation_mediapipe_2023feb_int8.onnx
    palm_detection_mediapipe_2023feb.onnx
    handpose_estimation_mediapipe_2023feb.onnx)
set(KICK_GESTURE_MODEL_URL_palm_detection_mediapipe_2023feb_int8.onnx "palm_detection_mediapipe/palm_detection_mediapipe_2023feb_int8.onnx")
set(KICK_GESTURE_MODEL_URL_handpose_estimation_mediapipe_2023feb_int8.onnx "handpose_estimation_mediapipe/handpose_estimation_mediapipe_2023feb_int8.onnx")
set(KICK_GESTURE_MODEL_URL_palm_detection_mediapipe_2023feb.onnx "palm_detection_mediapipe/palm_detection_mediapipe_2023feb.onnx")
set(KICK_GESTURE_MODEL_URL_handpose_estimation_mediapipe_2023feb.onnx "handpose_estimation_mediapipe/handpose_estimation_mediapipe_2023feb.onnx")
set(KICK_GESTURE_MODEL_SHA256_palm_detection_mediapipe_2023feb_int8.onnx "9f014de96ef5b6816b3eb9a5fed21a7371ef0f104ea440aa19ce9129fe2af5f6")
set(KICK_GESTURE_MODEL_SHA256_handpose_estimation_mediapipe_2023feb_int8.onnx "e97bc1fb83b641954d33424c82b6ade719d0f73250bdb91710ecfd5f7b47e321")
set(KICK_GESTURE_MODEL_SHA256_palm_detection_mediapipe_2023feb.onnx "78ff51c38496b7fc8b8ebdb6cc8c1abb02fa6c38427c6848254cdaba57fcce7c")
set(KICK_GESTURE_MODEL_SHA256_handpose_estimation_mediapipe_2023feb.onnx "db0898ae717b76b075d9bf563af315b29562e11f8df5027a1ef07b02bef6d81c")

# Photos of hands used only by the model-backed tests (MediaPipe test data, Apache-2.0). Converted to PPM by
# tools/prepare_test_images.py, which the tests need and the service does not.
set(KICK_GESTURE_TESTIMG_URL_BASE "https://storage.googleapis.com/mediapipe-assets")
set(KICK_GESTURE_TESTIMG_FILES victory.jpg fist.jpg pointing_up.jpg thumb_up.jpg)
set(KICK_GESTURE_TESTIMG_SHA256_victory.jpg "84cb8853e3df614e0cb5c93a25e3e2f38ea5e4f92fd428ee7d867ed3479d5764")
set(KICK_GESTURE_TESTIMG_SHA256_fist.jpg "43fa1cabf3f90d574accc9a56986e2ee48638ce59fc65af1846487f73bb2ef24")
set(KICK_GESTURE_TESTIMG_SHA256_pointing_up.jpg "ecf8ca2611d08fa25948a4fc10710af9120e88243a54da6356bacea17ff3e36e")
set(KICK_GESTURE_TESTIMG_SHA256_thumb_up.jpg "5d673c081ab13b8a1812269ff57047066f9c33c07db5f4178089e8cb3fdc0291")
