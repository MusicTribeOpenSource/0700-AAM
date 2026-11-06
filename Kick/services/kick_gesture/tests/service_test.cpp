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
#include "stream.h"

#include "kick_gesture/service.h"

#include "test_util.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <thread>

using namespace kickgest;

namespace
{
    class ScriptedModel final : public ILandmarkModel
    {
    public:
        const char* name() const override { return "scripted"; }
        bool detect(const RgbImage& frame, std::vector<Hand>& out, std::string& error) override
        {
            ++calls;
            lastW = frame.w;
            lastH = frame.h;
            out.clear();
            if (fail)
            {
                error = "scripted failure";
                return false;
            }
            if (hasPose) out.push_back(synthHand(pose, static_cast<float>(frame.w) / static_cast<float>(frame.h), true));
            return true;
        }
        SynthPose pose;
        bool hasPose = false;
        bool fail = false;
        int calls = 0;
        int lastW = 0, lastH = 0;
    };

    class FakeResources final : public IResourceReader
    {
    public:
        double cpuSeconds() override { return cpu; }
        std::optional<double> tempC() override { return temp; }
        double cpu = 0.0;
        std::optional<double> temp;
    };

    constexpr uint64_t kMs = 1000000ull;

    // A camera ring in this process, a scripted model, and a clock the test drives.
    struct Rig
    {
        static constexpr uint32_t kW = 64, kH = 48;

        Rig(double rateHz = 15.0)
        {
            kickcam::FrameFormat f;
            f.width = kW;
            f.height = kH;
            f.stride = kW * 2;
            f.fourcc = kickcam::kFourccYuyv;
            const size_t bytes = kickcam::ringBytes(static_cast<uint32_t>(kickcam::frameBytes(f)));
            mem = static_cast<uint8_t*>(std::aligned_alloc(64, (bytes + 63) / 64 * 64));
            writer.init(mem, bytes, static_cast<uint32_t>(kickcam::frameBytes(f)));
            writer.setFormat(f);
            frame.assign(kickcam::frameBytes(f), 100);
            reader = std::make_unique<RingFrameReader>(mem, bytes);
            ServiceOptions o;
            o.rateHz = rateHz;
            o.statusIntervalMs = 5000;
            o.idleCheckMs = 100;
            service = std::make_unique<GestureService>(&model, GestureConfig(), o, GuardLimits(), &resources, reader.get(), std::vector<IEventSink*>{&sink});
        }
        ~Rig() { std::free(mem); }

        void camera(bool on) { writer.setCapturing(on); }
        // Runs the clock for `ms`, publishing a camera frame every 33 ms while `publish` is set.
        void run(uint64_t ms, bool publish = true)
        {
            const uint64_t end = now + ms * kMs;
            for (; now < end; now += 5 * kMs)
            {
                if (publish && now >= nextFrame)
                {
                    writer.publish(frame.data(), frame.size(), now);
                    nextFrame = now + 33 * kMs;
                }
                resources.cpu += cpuRate * 0.005;
                service->step(now);
            }
        }
        std::vector<std::string> lines(const char* needle) const
        {
            std::vector<std::string> v;
            for (const std::string& l : sink.lines())
                if (l.find(needle) != std::string::npos) v.push_back(l);
            return v;
        }

        uint8_t* mem = nullptr;
        kickcam::RingWriter writer;
        std::vector<uint8_t> frame;
        std::unique_ptr<RingFrameReader> reader;
        ScriptedModel model;
        FakeResources resources;
        MemorySink sink;
        std::unique_ptr<GestureService> service;
        uint64_t now = 1000 * kMs;
        uint64_t nextFrame = 0;
        double cpuRate = 0.2;
    };
}

TEST(Service, TurnsCameraFramesIntoEvents)
{
    Rig r;
    r.camera(true);
    r.model.hasPose = true;
    r.model.pose = synthFist(0.5f, 0.5f);
    r.run(3000);
    EXPECT_EQ(r.lines("\"type\":\"fist\"").size(), 1u);
    EXPECT_GE(r.lines("\"state\":\"running\"").size(), 1u);
    EXPECT_EQ(r.model.lastW, static_cast<int>(Rig::kW));
    EXPECT_GT(r.service->stats().framesProcessed, 30u);
}

TEST(Service, RunsAtTheConfiguredRateNotTheCameraRate)
{
    Rig r(10.0);
    r.camera(true);
    r.run(10000); // the camera delivers 30 fps
    const double perSecond = static_cast<double>(r.model.calls) / 10.0;
    EXPECT_NEAR(perSecond, 10.0, 1.5);
}

TEST(Service, CameraOffClosesAnOpenPinchAndReportsIdle)
{
    Rig r;
    r.camera(true);
    r.model.hasPose = true;
    r.model.pose = synthPinch(0.5f, 0.5f, 0.2f);
    r.run(1500);
    ASSERT_EQ(r.lines("pinch_start").size(), 1u);
    ASSERT_EQ(r.lines("pinch_end").size(), 0u);

    r.writer.clear(); // what kick-camera does when it is told to disable
    r.run(500, false);
    EXPECT_EQ(r.lines("pinch_end").size(), 1u);
    EXPECT_EQ(r.lines("\"state\":\"idle\"").size(), 1u);
    const int calls = r.model.calls;
    r.run(1000, false);
    EXPECT_EQ(r.model.calls, calls) << "no inference while the camera is off";
}

TEST(Service, ResumesWhenTheCameraComesBack)
{
    Rig r;
    r.camera(true);
    r.model.hasPose = true;
    r.model.pose = synthFist(0.5f, 0.5f);
    r.run(1000);
    r.writer.clear();
    r.run(500, false);
    r.camera(true);
    r.run(2000);
    // Back to running, announced again.
    EXPECT_GE(r.lines("\"state\":\"running\"").size(), 2u);
    EXPECT_GE(r.lines("\"type\":\"fist\"").size(), 1u);
}

TEST(Service, MissingCameraServiceIsReportedNotFatal)
{
    uint8_t* junk = static_cast<uint8_t*>(std::aligned_alloc(64, 4096));
    std::fill(junk, junk + 4096, 0);
    RingFrameReader reader(junk, 4096);
    ScriptedModel model;
    FakeResources res;
    MemorySink sink;
    GestureService svc(&model, GestureConfig(), ServiceOptions(), GuardLimits(), &res, &reader, {&sink});
    for (uint64_t t = 0; t < 2000; t += 10) svc.step(1000 * kMs + t * kMs);
    ASSERT_FALSE(sink.lines().empty());
    EXPECT_NE(sink.lines()[0].find("\"state\":\"no_camera\""), std::string::npos) << sink.lines()[0];
    EXPECT_EQ(model.calls, 0);
    std::free(junk);
}

TEST(Service, HotBoardDisablesTheServiceAndClosesThePinch)
{
    Rig r;
    r.camera(true);
    r.model.hasPose = true;
    r.model.pose = synthPinch(0.5f, 0.5f, 0.2f);
    r.run(1500);
    ASSERT_EQ(r.lines("pinch_start").size(), 1u);

    r.resources.temp = 90.0;
    r.run(2500);
    EXPECT_EQ(r.lines("pinch_end").size(), 1u);
    ASSERT_EQ(r.lines("\"state\":\"disabled\"").size(), 1u);
    EXPECT_NE(r.lines("\"state\":\"disabled\"")[0].find("temp_hard"), std::string::npos);
    const int calls = r.model.calls;
    const uint64_t processed = r.service->stats().framesProcessed;
    r.run(3000);
    EXPECT_EQ(r.model.calls, calls) << "a disabled service does no inference";
    EXPECT_EQ(r.service->stats().framesProcessed, processed);
    EXPECT_EQ(r.service->guardState(), GuardState::Disabled);
}

TEST(Service, HighCpuHalvesTheRate)
{
    Rig r(12.0);
    r.camera(true);
    r.run(6000);
    const int normalCalls = r.model.calls;
    r.cpuRate = 0.75; // above cpu_soft, below cpu_hard
    r.run(12000);     // the average needs the window to fill, then it degrades
    ASSERT_EQ(r.service->guardState(), GuardState::Degraded);
    const int before = r.model.calls;
    r.run(10000);
    const double degradedPerSecond = static_cast<double>(r.model.calls - before) / 10.0;
    EXPECT_NEAR(degradedPerSecond, 6.0, 1.0);
    EXPECT_NEAR(static_cast<double>(normalCalls) / 6.0, 12.0, 1.5);
    EXPECT_GE(r.lines("\"state\":\"degraded\"").size(), 1u);
}

TEST(Service, ModelFailureIsReportedAndRecovered)
{
    Rig r;
    r.camera(true);
    r.model.fail = true;
    r.run(1500);
    EXPECT_GE(r.service->stats().modelErrors, 5u);
    EXPECT_EQ(r.lines("\"state\":\"error\"").size(), 1u) << "one status line, not one per frame";
    r.model.fail = false;
    r.run(1500);
    EXPECT_GE(r.lines("\"state\":\"running\"").size(), 1u);
}

TEST(Service, ModelFailureDuringAPinchStillEndsThePinch)
{
    Rig r;
    r.camera(true);
    r.model.hasPose = true;
    r.model.pose = synthPinch(0.5f, 0.5f, 0.2f);
    r.run(1500);
    ASSERT_EQ(r.lines("pinch_start").size(), 1u);
    ASSERT_EQ(r.lines("pinch_end").size(), 0u);

    r.model.fail = true;
    r.run(1500);
    EXPECT_EQ(r.lines("pinch_end").size(), 1u) << "failed frames count as no hand, so the pinch ends after lost_ms";
}

TEST(Service, ShutdownClosesThePinchAndSaysStopped)
{
    Rig r;
    r.camera(true);
    r.model.hasPose = true;
    r.model.pose = synthPinch(0.5f, 0.5f, 0.2f);
    r.run(1500);
    ASSERT_EQ(r.lines("pinch_end").size(), 0u);
    r.service->shutdown(r.now);
    EXPECT_EQ(r.lines("pinch_end").size(), 1u);
    EXPECT_EQ(r.sink.lines().back().find("\"state\":\"stopped\""), std::string("{\"type\":\"status\",").size());
}

TEST(Service, StatusRepeatsEveryInterval)
{
    Rig r;
    r.camera(true);
    r.run(16000);
    EXPECT_GE(r.lines("\"type\":\"status\"").size(), 3u);
}

TEST(ShmFrameReader, FollowsTheCameraServiceAcrossRestarts)
{
    const std::string name = kicktest::uniqueShmName("gr");
    ShmFrameReader reader(name, 10, 200);
    std::vector<uint8_t> data;
    kickcam::FrameMeta meta;
    EXPECT_EQ(reader.poll(data, meta), IFrameReader::Result::Unavailable);

    auto makeRing = [&](kickcam::ShmRegion& region, kickcam::RingWriter& w) {
        kickcam::FrameFormat f;
        f.width = 32;
        f.height = 16;
        f.stride = 64;
        f.fourcc = kickcam::kFourccYuyv;
        const size_t cap = kickcam::frameBytes(f);
        std::string err;
        ASSERT_TRUE(region.create(name, kickcam::ringBytes(static_cast<uint32_t>(cap)), err)) << err;
        ASSERT_TRUE(w.init(region.data(), region.size(), static_cast<uint32_t>(cap)));
        ASSERT_TRUE(w.setFormat(f));
    };

    {
        kickcam::ShmRegion region;
        kickcam::RingWriter w;
        makeRing(region, w);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_EQ(reader.poll(data, meta), IFrameReader::Result::NotCapturing);
        w.setCapturing(true);
        std::vector<uint8_t> px(32 * 16 * 2, 7);
        ASSERT_TRUE(w.publish(px.data(), px.size(), 5000));
        EXPECT_EQ(reader.poll(data, meta), IFrameReader::Result::Frame);
        EXPECT_EQ(meta.timestampNs, 5000u);
        EXPECT_EQ(data.size(), px.size());
        EXPECT_EQ(reader.poll(data, meta), IFrameReader::Result::NoNewFrame);
    } // the service exits and unlinks the object

    // Capturing was still set when the writer went away: silence for staleMs reads as gone.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(reader.poll(data, meta), IFrameReader::Result::Unavailable);

    {
        kickcam::ShmRegion region;
        kickcam::RingWriter w;
        makeRing(region, w);
        w.setCapturing(true);
        std::vector<uint8_t> px(32 * 16 * 2, 9);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        IFrameReader::Result res = reader.poll(data, meta);
        EXPECT_NE(res, IFrameReader::Result::Unavailable);
        ASSERT_TRUE(w.publish(px.data(), px.size(), 9000));
        res = reader.poll(data, meta);
        EXPECT_EQ(res, IFrameReader::Result::Frame);
        EXPECT_EQ(data[0], 9);
    }
}
