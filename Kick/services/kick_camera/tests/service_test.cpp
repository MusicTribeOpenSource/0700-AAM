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
#include "kick_camera/service.h"

#include "fake_source.h"
#include "test_util.h"

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <thread>

using namespace kickcam;
using kicktest::FakeSource;
using kicktest::FakeState;
using kicktest::TempDir;

namespace
{
    bool waitFor(const std::function<bool()>& cond, int timeoutMs = 3000)
    {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < end)
        {
            if (cond()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return cond();
    }

    // A service wired to a FakeSource, a private socket and a private shm name, plus a reader on its ring.
    struct Rig
    {
        explicit Rig(std::function<void(Config&)> tweak = nullptr, std::shared_ptr<FakeState> s = std::make_shared<FakeState>()) : state(std::move(s))
        {
            Config cfg;
            cfg.shmName = kicktest::uniqueShmName("svc");
            cfg.socketPath = dir.file("c.sock");
            cfg.source = "pattern";
            if (tweak) tweak(cfg);
            this->cfg = cfg;
            service = std::make_unique<CameraService>(cfg, [this](const Config&) { return std::make_unique<FakeSource>(state); });
            std::string error;
            started = service->start(error);
            EXPECT_TRUE(started) << error;
            EXPECT_TRUE(reader.attach(service->ringData(), service->ringSize()));
        }

        std::string ask(const std::string& command)
        {
            std::string reply, error;
            EXPECT_TRUE(ControlServer::request(cfg.socketPath, command, reply, error)) << error;
            return reply;
        }

        bool gotFrame()
        {
            std::vector<uint8_t> out;
            FrameMeta meta;
            return reader.readLatest(0, out, meta) == RingReader::Result::Ok;
        }

        TempDir dir;
        std::shared_ptr<FakeState> state;
        Config cfg;
        std::unique_ptr<CameraService> service;
        RingReader reader;
        bool started = false;
    };

    bool contains(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
}

TEST(Service, CameraIsOffAfterStartUntilSomeoneEnablesIt)
{
    Rig rig;
    EXPECT_EQ(rig.state->openCount(), 0) << "the device must not be opened at start-up";
    EXPECT_FALSE(rig.reader.capturing());
    const std::string status = rig.ask("status");
    EXPECT_TRUE(contains(status, "\"state\":\"off\"")) << status;
    EXPECT_TRUE(contains(status, "\"capturing\":false")) << status;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(rig.state->openCount(), 0);
}

TEST(Service, EnablePublishesFramesAndTheCapturingFlag)
{
    Rig rig;
    EXPECT_EQ(rig.ask("enable"), "ok");
    EXPECT_TRUE(rig.reader.capturing());
    ASSERT_TRUE(waitFor([&] { return rig.gotFrame(); }));

    std::vector<uint8_t> out;
    FrameMeta meta;
    ASSERT_EQ(rig.reader.readLatest(0, out, meta), RingReader::Result::Ok);
    EXPECT_EQ(meta.format, rig.state->format);
    EXPECT_EQ(out.size(), frameBytes(rig.state->format));
    EXPECT_GE(out[0], 0x40);

    const std::string status = rig.ask("status");
    EXPECT_TRUE(contains(status, "\"state\":\"streaming\"")) << status;
    EXPECT_TRUE(contains(status, "\"capturing\":true")) << status;
    EXPECT_TRUE(contains(status, "\"width\":32")) << status;
    EXPECT_TRUE(contains(status, "\"format\":\"YUYV\"")) << status;
    EXPECT_EQ(rig.ask("enable"), "ok") << "enable is idempotent";
    EXPECT_EQ(rig.state->openCount(), 1);
}

TEST(Service, TheIndicatorIsOnWheneverTheDeviceIsOpen)
{
    auto state = std::make_shared<FakeState>();
    Rig rig(nullptr, state);
    std::atomic<int> onWhileOpening{-1}, onWhileClosing{-1};
    state->onOpen = [&] { onWhileOpening = rig.reader.capturing() ? 1 : 0; };
    state->onClose = [&] { onWhileClosing = rig.reader.capturing() ? 1 : 0; };
    rig.ask("enable");
    rig.ask("disable");
    EXPECT_EQ(onWhileOpening.load(), 1) << "raised before the device opens";
    EXPECT_EQ(onWhileClosing.load(), 1) << "still raised while the device closes";
    EXPECT_FALSE(rig.reader.capturing());
}

TEST(Service, DisableClosesTheDeviceAndWipesThePixels)
{
    Rig rig;
    rig.ask("enable");
    ASSERT_TRUE(waitFor([&] { return rig.gotFrame(); }));
    EXPECT_EQ(rig.ask("disable"), "ok");

    EXPECT_FALSE(rig.state->open_()) << "device is closed";
    EXPECT_FALSE(rig.reader.capturing());
    std::vector<uint8_t> out;
    FrameMeta meta;
    EXPECT_EQ(rig.reader.readLatest(0, out, meta), RingReader::Result::NotCapturing);
    const auto* bytes = static_cast<const uint8_t*>(rig.service->ringData());
    const auto* header = static_cast<const RingHeader*>(rig.service->ringData());
    for (uint32_t slot = 0; slot < kRingSlots; ++slot)
    {
        const size_t start = header->slotsOffset.load() + static_cast<size_t>(slot) * header->slotStride.load() + sizeof(RingSlot);
        for (size_t i = start; i < start + header->capacity.load(); ++i) ASSERT_EQ(bytes[i], 0) << "pixel byte left at offset " << i;
    }
    EXPECT_TRUE(contains(rig.ask("status"), "\"state\":\"off\""));
}

TEST(Service, EnableThatCannotOpenTheDeviceReportsWhyAndStaysOff)
{
    auto state = std::make_shared<FakeState>();
    state->failOpen = true;
    Rig rig(nullptr, state);
    const std::string reply = rig.ask("enable");
    EXPECT_TRUE(contains(reply, "err")) << reply;
    EXPECT_TRUE(contains(reply, "open refused")) << reply;
    EXPECT_FALSE(rig.reader.capturing());
    EXPECT_FALSE(rig.state->open_());
    const std::string status = rig.ask("status");
    EXPECT_TRUE(contains(status, "\"state\":\"error\"")) << status;
    EXPECT_TRUE(contains(status, "open refused")) << status;

    state->failOpen = false;
    EXPECT_EQ(rig.ask("enable"), "ok") << "a later enable can recover";
}

TEST(Service, SourceThatProducesMoreThanMaxSizeIsRefused)
{
    auto state = std::make_shared<FakeState>();
    state->format = {1280, 720, 2560, kFourccYuyv};
    Rig rig(nullptr, state);
    const std::string reply = rig.ask("enable");
    EXPECT_TRUE(contains(reply, "max_size")) << reply;
    EXPECT_FALSE(rig.reader.capturing());
    EXPECT_FALSE(rig.state->open_()) << "the device is closed again after the refusal";
}

TEST(Service, EndOfStreamTurnsTheCameraOffAndItCanBeEnabledAgain)
{
    auto state = std::make_shared<FakeState>();
    state->framesBeforeEnd = 3;
    Rig rig(nullptr, state);
    rig.ask("enable");
    ASSERT_TRUE(waitFor([&] { return contains(rig.ask("status"), "\"state\":\"ended\""); }));
    EXPECT_FALSE(rig.reader.capturing());
    EXPECT_FALSE(rig.state->open_());
    EXPECT_EQ(rig.ask("enable"), "ok");
    EXPECT_EQ(rig.state->openCount(), 2);
}

TEST(Service, ADeviceErrorMidStreamClosesTheDeviceAndSaysWhy)
{
    auto state = std::make_shared<FakeState>();
    state->framesBeforeEnd = 2;
    state->failAfterFrames = true;
    Rig rig(nullptr, state);
    rig.ask("enable");
    ASSERT_TRUE(waitFor([&] { return contains(rig.ask("status"), "\"state\":\"error\""); }));
    EXPECT_TRUE(contains(rig.ask("status"), "unplugged"));
    EXPECT_FALSE(rig.reader.capturing());
    EXPECT_FALSE(rig.state->open_());
}

TEST(Service, PresetIsAppliedOnEnableAndCanBeChangedWhileStreaming)
{
    Rig rig([](Config& c) { c.preset = Preset::DimStage; });
    rig.ask("enable");
    const auto calls = rig.state->callLog();
    ASSERT_GE(calls.size(), 2u);
    EXPECT_EQ(calls[0], "auto_exposure=0");
    EXPECT_EQ(calls[1], "exposure=" + std::to_string(dimStageExposure(rig.state->fps)));
    EXPECT_EQ(rig.ask("preset lit_room"), "ok");
    EXPECT_TRUE(contains(rig.ask("status"), "\"preset\":\"lit_room\""));
    EXPECT_TRUE(contains(rig.ask("preset sunny"), "err"));
}

TEST(Service, ExposureAndWhiteBalanceLocksNeedTheCameraOn)
{
    Rig rig;
    EXPECT_EQ(rig.ask("lock_exposure"), "err camera is off");
    EXPECT_EQ(rig.ask("lock_wb"), "err camera is off");
    rig.ask("enable");
    EXPECT_EQ(rig.ask("lock_exposure"), "ok");
    EXPECT_EQ(rig.ask("lock_wb"), "ok");
    std::string status = rig.ask("status");
    EXPECT_TRUE(contains(status, "\"exposure_locked\":true")) << status;
    EXPECT_TRUE(contains(status, "\"wb_locked\":true")) << status;
    EXPECT_EQ(rig.ask("unlock_exposure"), "ok");
    EXPECT_EQ(rig.ask("unlock_wb"), "ok");
    status = rig.ask("status");
    EXPECT_TRUE(contains(status, "\"exposure_locked\":false")) << status;
    EXPECT_TRUE(contains(status, "\"wb_locked\":false")) << status;

    rig.ask("lock_exposure");
    rig.ask("disable");
    rig.ask("enable");
    EXPECT_TRUE(contains(rig.ask("status"), "\"exposure_locked\":false")) << "locks do not outlive a capture session";
}

TEST(Service, LockReportsAnUnsupportedControl)
{
    auto state = std::make_shared<FakeState>();
    state->supportsAutoExposure = false;
    Rig rig(nullptr, state);
    rig.ask("enable");
    const std::string reply = rig.ask("lock_exposure");
    EXPECT_TRUE(contains(reply, "err")) << reply;
}

TEST(Service, UnknownAndMalformedCommandsAreRejected)
{
    Rig rig;
    EXPECT_TRUE(contains(rig.ask("selfie"), "err"));
    EXPECT_TRUE(contains(rig.ask("enable now"), "err"));
    EXPECT_TRUE(contains(rig.ask(""), "err"));
    EXPECT_TRUE(contains(rig.ask("preset"), "err"));
}

TEST(Service, ControlSocketIsOwnerOnly)
{
    Rig rig;
    struct stat st;
    ASSERT_EQ(::stat(rig.cfg.socketPath.c_str(), &st), 0);
    EXPECT_EQ(st.st_mode & 0777, 0600u);
}

TEST(Service, TwoCommandsInOneWriteAreBothAnswered)
{
    Rig rig;
    // request() sends "status\nstatus\n" in one write and returns the first reply line. The server must not stall
    // or drop the connection over the second command, so a follow-up request still works.
    std::string reply, error;
    ASSERT_TRUE(ControlServer::request(rig.cfg.socketPath, "status\nstatus", reply, error)) << error;
    EXPECT_TRUE(contains(reply, "\"state\":\"off\""));
    EXPECT_TRUE(contains(rig.ask("status"), "\"state\":\"off\""));
}

TEST(Service, NothingIsWrittenToDiskWithoutTheDebugFlag)
{
    TempDir cwdDir;
    char before[1024];
    ASSERT_NE(::getcwd(before, sizeof(before)), nullptr);
    ASSERT_EQ(::chdir(cwdDir.path().c_str()), 0);
    {
        Rig rig;
        rig.ask("enable");
        ASSERT_TRUE(waitFor([&] { return rig.gotFrame(); }));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        rig.ask("disable");
    }
    EXPECT_EQ(::chdir(before), 0);
    EXPECT_TRUE(cwdDir.list().empty());
}

TEST(Service, DebugDumpWritesAtMostTheConfiguredNumberOfWholeFrames)
{
    TempDir dump;
    Rig rig(
        [&](Config& c)
        {
            c.debugDumpDir = dump.path();
            c.debugDumpMax = 3;
        });
    rig.ask("enable");
    ASSERT_TRUE(waitFor([&] { return dump.list().size() >= 3; }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // let a fourth frame arrive, if it were going to
    rig.ask("disable");
    const auto files = dump.list();
    ASSERT_EQ(files.size(), 3u);
    for (const auto& f : files)
    {
        struct stat st;
        ASSERT_EQ(::stat(dump.file(f).c_str(), &st), 0);
        EXPECT_EQ(static_cast<size_t>(st.st_size), frameBytes(rig.state->format));
    }
    EXPECT_TRUE(contains(rig.ask("status"), "\"debug_dump\":true"));
}

TEST(Service, RapidEnableDisableCyclesDoNotHangOrLeakTheDevice)
{
    Rig rig;
    for (int i = 0; i < 25; ++i)
    {
        ASSERT_EQ(rig.ask("enable"), "ok");
        if (i % 3 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(15));
        ASSERT_EQ(rig.ask("disable"), "ok");
    }
    EXPECT_EQ(rig.state->openCount(), rig.state->closeCount());
    EXPECT_FALSE(rig.state->open_());
}

TEST(Service, CommandsFromSeveralClientsAtOnceAreSafe)
{
    Rig rig;
    std::atomic<int> failures{0};
    std::vector<std::thread> clients;
    for (int t = 0; t < 4; ++t)
    {
        clients.emplace_back(
            [&, t]
            {
                for (int i = 0; i < 20; ++i)
                {
                    std::string reply, error;
                    const char* cmd = (t + i) % 3 == 0 ? "enable" : (t + i) % 3 == 1 ? "status" : "disable";
                    if (!ControlServer::request(rig.cfg.socketPath, cmd, reply, error)) ++failures;
                }
            });
    }
    for (auto& c : clients) c.join();
    EXPECT_EQ(failures.load(), 0);
    rig.ask("disable");
    EXPECT_EQ(rig.state->openCount(), rig.state->closeCount());
}

TEST(Service, StopRemovesTheSocketAndTheSharedMemory)
{
    Rig rig;
    rig.ask("enable");
    const std::string socketPath = rig.cfg.socketPath;
    const std::string shm = rig.cfg.shmName;
    rig.service.reset();
    EXPECT_NE(::access(socketPath.c_str(), F_OK), 0);
    ShmRegion region;
    std::string error;
    EXPECT_FALSE(region.openReadOnly(shm, error)) << "the shm object should be unlinked";
    EXPECT_FALSE(rig.state->open_());
}

TEST(Service, ARealSharedMemoryReaderInAnotherMappingSeesFrames)
{
    Rig rig;
    rig.ask("enable");
    ShmRegion region;
    std::string error;
    ASSERT_TRUE(region.openReadOnly(rig.cfg.shmName, error)) << error;
    RingReader reader;
    ASSERT_TRUE(reader.attach(region.data(), region.size()));
    ASSERT_TRUE(waitFor(
        [&]
        {
            std::vector<uint8_t> out;
            FrameMeta meta;
            return reader.readLatest(0, out, meta) == RingReader::Result::Ok;
        }));
}

TEST(Service, StartFailsOnAnInvalidConfigWithoutCreatingAnything)
{
    TempDir dir;
    Config cfg;
    cfg.shmName = kicktest::uniqueShmName("bad");
    cfg.socketPath = dir.file("c.sock");
    cfg.fps = 0;
    CameraService svc(cfg);
    std::string error;
    EXPECT_FALSE(svc.start(error));
    EXPECT_NE(error.find("fps"), std::string::npos);
    ShmRegion region;
    EXPECT_FALSE(region.openReadOnly(cfg.shmName, error));
}

TEST(Service, MakeSourceBuildsTheRequestedBackend)
{
    Config cfg;
    std::string error;
    cfg.source = "pattern";
    auto pattern = makeSource(cfg, error);
    ASSERT_NE(pattern, nullptr);
    EXPECT_STREQ(pattern->name(), "pattern");
    cfg.source = "pipe";
    EXPECT_STREQ(makeSource(cfg, error)->name(), "pipe");
    cfg.source = "file";
    cfg.file = "/nonexistent";
    EXPECT_STREQ(makeSource(cfg, error)->name(), "file");
#ifdef __linux__
    cfg.source = "v4l2";
    EXPECT_STREQ(makeSource(cfg, error)->name(), "v4l2");
#else
    cfg.source = "v4l2";
    EXPECT_EQ(makeSource(cfg, error), nullptr);
    EXPECT_NE(error.find("Linux"), std::string::npos);
#endif
}
