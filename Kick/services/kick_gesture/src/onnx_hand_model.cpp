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
#include "kick_gesture/onnx_hand_model.h"

#include "kick_gesture/palm_anchors.h"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numeric>

namespace kickgest
{
    namespace
    {
        constexpr int kPalmSize = 192;
        constexpr int kHandSize = 224;
        constexpr int kPalmAnchors = 2016;
        constexpr float kPi = 3.14159265358979f;
        constexpr float kRoiScale = 2.6f;
        constexpr float kRoiShiftY = -0.5f;

        struct Candidate
        {
            float cx, cy, w, h; // box in source pixels (edge coordinates)
            float kp[7][2];     // palm keypoints in source pixels
            float score;
        };

        float iou(const Candidate& a, const Candidate& b)
        {
            const float ax0 = a.cx - a.w / 2, ax1 = a.cx + a.w / 2, ay0 = a.cy - a.h / 2, ay1 = a.cy + a.h / 2;
            const float bx0 = b.cx - b.w / 2, bx1 = b.cx + b.w / 2, by0 = b.cy - b.h / 2, by1 = b.cy + b.h / 2;
            const float iw = std::max(0.f, std::min(ax1, bx1) - std::max(ax0, bx0));
            const float ih = std::max(0.f, std::min(ay1, by1) - std::max(ay0, by0));
            const float inter = iw * ih;
            const float uni = a.w * a.h + b.w * b.h - inter;
            return uni > 0.f ? inter / uni : 0.f;
        }

        double msSince(std::chrono::steady_clock::time_point t0)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        }
    }

    struct OnnxHandModel::Impl
    {
        OnnxModelOptions opt;
        Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "kick-gesture"};
        std::unique_ptr<Ort::Session> palm;
        std::unique_ptr<Ort::Session> landmark;
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::string palmIn, lmIn;
        std::vector<std::string> palmOutNames, lmOutNames;
        int palmBoxOut = 0; // index of the output with 18 values per anchor
        std::vector<float> palmInput;
        std::vector<float> handInput;
        OnnxStageTimes times;

        static std::unique_ptr<Ort::Session> open(Ort::Env& env, const std::string& path, int threads, std::string& inName, std::vector<std::string>& outNames)
        {
            Ort::SessionOptions so;
            so.SetIntraOpNumThreads(threads);
            so.SetInterOpNumThreads(1);
            so.SetExecutionMode(ORT_SEQUENTIAL);
            so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            so.AddConfigEntry("session.intra_op.allow_spinning", "0");
            auto s = std::make_unique<Ort::Session>(env, path.c_str(), so);
            Ort::AllocatorWithDefaultOptions alloc;
            inName = s->GetInputNameAllocated(0, alloc).get();
            outNames.clear();
            for (size_t i = 0; i < s->GetOutputCount(); ++i) outNames.emplace_back(s->GetOutputNameAllocated(i, alloc).get());
            return s;
        }

        bool run(Ort::Session& s, const std::string& inName, const std::vector<std::string>& outNames, float* data, int side, std::vector<Ort::Value>& outputs)
        {
            const std::array<int64_t, 4> shape{1, side, side, 3};
            Ort::Value input = Ort::Value::CreateTensor<float>(mem, data, static_cast<size_t>(side) * static_cast<size_t>(side) * 3u, shape.data(), shape.size());
            const char* inNames[] = {inName.c_str()};
            std::vector<const char*> outPtrs;
            for (const std::string& n : outNames) outPtrs.push_back(n.c_str());
            outputs = s.Run(Ort::RunOptions{nullptr}, inNames, &input, 1, outPtrs.data(), outPtrs.size());
            return true;
        }
    };

    OnnxHandModel::OnnxHandModel() : impl_(std::make_unique<Impl>()) {}
    OnnxHandModel::~OnnxHandModel() = default;

    const OnnxStageTimes& OnnxHandModel::lastTimes() const { return impl_->times; }

    std::unique_ptr<OnnxHandModel> OnnxHandModel::create(const OnnxModelOptions& options, std::string& error)
    {
        std::unique_ptr<OnnxHandModel> m(new OnnxHandModel());
        Impl& d = *m->impl_;
        d.opt = options;
        d.opt.maxHands = std::max(1, d.opt.maxHands);
        d.opt.intraOpThreads = std::max(1, d.opt.intraOpThreads);
        try
        {
            d.palm = Impl::open(d.env, options.palmModelPath, d.opt.intraOpThreads, d.palmIn, d.palmOutNames);
            d.landmark = Impl::open(d.env, options.landmarkModelPath, d.opt.intraOpThreads, d.lmIn, d.lmOutNames);

            if (d.palmOutNames.size() != 2)
            {
                error = "palm model should have 2 outputs, found " + std::to_string(d.palmOutNames.size());
                return nullptr;
            }
            // The boxes output is [1, 2016, 18] and the scores output is [1, 2016, 1]. Find them by shape, not by name.
            d.palmBoxOut = -1;
            for (size_t i = 0; i < 2; ++i)
            {
                const auto shape = d.palm->GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
                if (shape.size() == 3 && shape[1] == kPalmAnchors && shape[2] == 18) d.palmBoxOut = static_cast<int>(i);
            }
            if (d.palmBoxOut < 0)
            {
                error = "palm model has no [1, 2016, 18] output; this is not the expected MediaPipe palm detector";
                return nullptr;
            }
            if (d.lmOutNames.size() != 4)
            {
                error = "landmark model should have 4 outputs (landmarks, score, handedness, world landmarks), found " + std::to_string(d.lmOutNames.size());
                return nullptr;
            }
            d.palmInput.assign(static_cast<size_t>(kPalmSize) * kPalmSize * 3, 0.f);
            d.handInput.assign(static_cast<size_t>(kHandSize) * kHandSize * 3, 0.f);
        }
        catch (const Ort::Exception& e)
        {
            error = std::string("ONNX Runtime: ") + e.what();
            return nullptr;
        }
        return m;
    }

    bool OnnxHandModel::detect(const RgbImage& frame, std::vector<Hand>& out, std::string& error)
    {
        out.clear();
        Impl& d = *impl_;
        d.times = OnnxStageTimes();
        if (frame.w < 8 || frame.h < 8 || frame.px.size() < static_cast<size_t>(frame.w) * static_cast<size_t>(frame.h) * 3u)
        {
            error = "frame is empty or smaller than 8 x 8";
            return false;
        }
        try
        {
            auto t0 = std::chrono::steady_clock::now();

            // Palm detector input: the frame letterboxed into 192 x 192, black bars on the short side.
            const float W = static_cast<float>(frame.w);
            const float H = static_cast<float>(frame.h);
            const float ratio = std::min(kPalmSize / W, kPalmSize / H);
            const float padX = (kPalmSize - W * ratio) / 2.f;
            const float padY = (kPalmSize - H * ratio) / 2.f;
            Affine letterbox;
            letterbox.a = 1.f / ratio;
            letterbox.e = 1.f / ratio;
            letterbox.c = (0.5f - padX) / ratio - 0.5f;
            letterbox.f = (0.5f - padY) / ratio - 0.5f;
            warpToFloat(frame, letterbox, kPalmSize, kPalmSize, d.palmInput.data());
            d.times.preprocessMs += msSince(t0);

            t0 = std::chrono::steady_clock::now();
            std::vector<Ort::Value> palmOut;
            d.run(*d.palm, d.palmIn, d.palmOutNames, d.palmInput.data(), kPalmSize, palmOut);
            d.times.palmMs = msSince(t0);

            // Read what the networks returned, not what create() was told to expect.
            const size_t boxIdx = static_cast<size_t>(d.palmBoxOut);
            if (palmOut.size() != 2 || palmOut[boxIdx].GetTensorTypeAndShapeInfo().GetElementCount() < static_cast<size_t>(kPalmAnchors) * 18u
                || palmOut[1 - boxIdx].GetTensorTypeAndShapeInfo().GetElementCount() < static_cast<size_t>(kPalmAnchors))
            {
                error = "palm model returned fewer values than the anchor table";
                return false;
            }
            const float* boxes = palmOut[static_cast<size_t>(d.palmBoxOut)].GetTensorData<float>();
            const float* scores = palmOut[static_cast<size_t>(1 - d.palmBoxOut)].GetTensorData<float>();
            const std::vector<Anchor>& anchors = palmAnchors();

            std::vector<Candidate> cands;
            for (int i = 0; i < kPalmAnchors; ++i)
            {
                float logit = scores[i];
                if (!std::isfinite(logit)) continue;
                logit = std::max(-100.f, std::min(100.f, logit));
                const float score = 1.f / (1.f + std::exp(-logit));
                if (score < d.opt.palmScoreThreshold) continue;
                const float* b = boxes + static_cast<size_t>(i) * 18;
                const Anchor& a = anchors[static_cast<size_t>(i)];
                Candidate c;
                c.score = score;
                // Model space is the 192 x 192 input; edge coordinates, then undo the letterbox.
                const float mcx = (a.x + b[0] / kPalmSize) * kPalmSize;
                const float mcy = (a.y + b[1] / kPalmSize) * kPalmSize;
                c.cx = (mcx - padX) / ratio;
                c.cy = (mcy - padY) / ratio;
                c.w = b[2] / ratio;
                c.h = b[3] / ratio;
                for (int k = 0; k < 7; ++k)
                {
                    const float kx = (a.x + b[4 + 2 * k] / kPalmSize) * kPalmSize;
                    const float ky = (a.y + b[5 + 2 * k] / kPalmSize) * kPalmSize;
                    c.kp[k][0] = (kx - padX) / ratio;
                    c.kp[k][1] = (ky - padY) / ratio;
                }
                if (!(c.w > 1.f && c.h > 1.f)) continue;
                cands.push_back(c);
            }
            std::sort(cands.begin(), cands.end(), [](const Candidate& x, const Candidate& y) { return x.score > y.score; });
            std::vector<Candidate> kept;
            for (const Candidate& c : cands)
            {
                bool suppressed = false;
                for (const Candidate& k : kept)
                    if (iou(c, k) > d.opt.nmsIouThreshold)
                    {
                        suppressed = true;
                        break;
                    }
                if (!suppressed) kept.push_back(c);
                if (static_cast<int>(kept.size()) >= d.opt.maxHands) break;
            }
            d.times.palmCandidates = static_cast<int>(kept.size());

            for (const Candidate& c : kept)
            {
                // ROI as in MediaPipe: rotate so wrist -> middle finger base points up, shift toward the fingers, scale.
                const float dx = c.kp[2][0] - c.kp[0][0];
                const float dy = c.kp[2][1] - c.kp[0][1];
                float rot = kPi / 2.f - std::atan2(-dy, dx);
                rot -= 2.f * kPi * std::floor((rot + kPi) / (2.f * kPi));
                const float cs = std::cos(rot);
                const float sn = std::sin(rot);
                const float roiCx = c.cx - c.h * kRoiShiftY * sn;
                const float roiCy = c.cy + c.h * kRoiShiftY * cs;
                const float side = std::max(c.w, c.h) * kRoiScale;

                // dst pixel centre (u, v) -> position in the ROI -> source edge coordinates -> pixel centre coordinates.
                const float k = side / kHandSize;
                Affine roi;
                roi.a = cs * k;
                roi.b = -sn * k;
                roi.d = sn * k;
                roi.e = cs * k;
                const float u0 = 0.5f - kHandSize / 2.f; // local offset of pixel (0, 0)
                roi.c = roiCx + (cs * u0 - sn * u0) * k - 0.5f;
                roi.f = roiCy + (sn * u0 + cs * u0) * k - 0.5f;

                t0 = std::chrono::steady_clock::now();
                warpToFloat(frame, roi, kHandSize, kHandSize, d.handInput.data());
                d.times.preprocessMs += msSince(t0);

                t0 = std::chrono::steady_clock::now();
                std::vector<Ort::Value> lmOut;
                d.run(*d.landmark, d.lmIn, d.lmOutNames, d.handInput.data(), kHandSize, lmOut);
                d.times.landmarkMs += msSince(t0);

                if (lmOut.size() != 4 || lmOut[0].GetTensorTypeAndShapeInfo().GetElementCount() < static_cast<size_t>(kNumLandmarks) * 3
                    || lmOut[1].GetTensorTypeAndShapeInfo().GetElementCount() < 1 || lmOut[2].GetTensorTypeAndShapeInfo().GetElementCount() < 1)
                {
                    error = "landmark model returned fewer values than 21 points";
                    return false;
                }
                const float* lm = lmOut[0].GetTensorData<float>();
                const float conf = lmOut[1].GetTensorData<float>()[0];
                const float handed = lmOut[2].GetTensorData<float>()[0];
                if (!std::isfinite(conf) || conf < d.opt.handScoreThreshold) continue;

                Hand h;
                h.score = std::max(0.f, std::min(1.f, conf));
                h.rightProb = std::isfinite(handed) ? std::max(0.f, std::min(1.f, handed)) : 0.5f;
                for (int i = 0; i < kNumLandmarks; ++i)
                {
                    const float lx = (lm[3 * i] - kHandSize / 2.f) * k;
                    const float ly = (lm[3 * i + 1] - kHandSize / 2.f) * k;
                    const float lz = lm[3 * i + 2] * k;
                    Point3 p;
                    p.x = (roiCx + cs * lx - sn * ly) / W;
                    p.y = (roiCy + sn * lx + cs * ly) / H;
                    p.z = lz / W;
                    h.lm[static_cast<size_t>(i)] = p;
                }
                out.push_back(h);
            }
            std::sort(out.begin(), out.end(), [](const Hand& x, const Hand& y) { return x.score > y.score; });
            return true;
        }
        catch (const Ort::Exception& e)
        {
            error = std::string("ONNX Runtime: ") + e.what();
            out.clear();
            return false;
        }
    }
}
