/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "kick_gesture/landmark_model.h"

#include <memory>
#include <string>

namespace kickgest
{
    struct OnnxModelOptions
    {
        std::string palmModelPath;     // palm_detection_mediapipe_2023feb[_int8].onnx
        std::string landmarkModelPath; // handpose_estimation_mediapipe_2023feb[_int8].onnx
        // One thread by default: the service shares four cores with the audio engine, and one inference thread is
        // already the budget (CPUQuota in the unit file). Idle threads do not spin (allow_spinning is off).
        int intraOpThreads = 1;
        float palmScoreThreshold = 0.5f;  // palm detector score (after sigmoid) a candidate needs
        float nmsIouThreshold = 0.3f;
        float handScoreThreshold = 0.5f;  // hand presence score the landmark model must reach for a hand to be returned
        int maxHands = 1;
    };

    // Time spent in the two networks for the last detect() call.
    struct OnnxStageTimes
    {
        double preprocessMs = 0.0; // letterbox and ROI warps
        double palmMs = 0.0;
        double landmarkMs = 0.0;
        int palmCandidates = 0; // boxes left after NMS
    };

    // MediaPipe palm detection (192 x 192) followed by hand landmarks (224 x 224 on a rotated ROI), run with ONNX Runtime on
    // the CPU. The geometry follows MediaPipe's hand landmark graph: the ROI is the palm box shifted toward the fingers
    // by half its height and scaled to 2.6 times its long side, rotated so the wrist-to-middle-finger-base axis is
    // vertical.
    class OnnxHandModel final : public ILandmarkModel
    {
    public:
        static std::unique_ptr<OnnxHandModel> create(const OnnxModelOptions& options, std::string& error);
        ~OnnxHandModel() override;

        const char* name() const override { return "onnx-mediapipe-hand"; }
        bool detect(const RgbImage& frame, std::vector<Hand>& out, std::string& error) override;

        const OnnxStageTimes& lastTimes() const;

    private:
        OnnxHandModel();
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
