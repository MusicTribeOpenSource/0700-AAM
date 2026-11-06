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
#include "kick_camera/controls.h"

#include "fake_source.h"

#include <gtest/gtest.h>

using namespace kickcam;
using kicktest::FakeSource;
using kicktest::FakeState;

namespace
{
    struct Rig
    {
        std::shared_ptr<FakeState> state = std::make_shared<FakeState>();
        FakeSource source{state};
        ControlState controls;
    };
    using Calls = std::vector<std::string>;
}

TEST(Controls, PresetNamesRoundTrip)
{
    Preset p;
    EXPECT_TRUE(parsePreset("lit_room", p));
    EXPECT_EQ(p, Preset::LitRoom);
    EXPECT_TRUE(parsePreset("dim_stage", p));
    EXPECT_STREQ(presetName(p), "dim_stage");
    EXPECT_FALSE(parsePreset("bright", p));
}

TEST(Controls, LitRoomUsesAutoExposureAndAutoWhiteBalanceAtAFixedFrameRate)
{
    Rig r;
    const auto report = applyPreset(r.source, Preset::LitRoom, 15, r.controls);
    EXPECT_EQ(r.state->callLog(), (Calls{"auto_exposure=1", "keep_fps=1", "awb=1"}));
    EXPECT_TRUE(report.unsupported.empty());
}

TEST(Controls, DimStageFixesExposureAtFiveSixthsOfAFramePeriod)
{
    EXPECT_EQ(dimStageExposure(15), 555) << "period 666 units of 100 us, times 5/6";
    EXPECT_EQ(dimStageExposure(30), 277);
    Rig r;
    applyPreset(r.source, Preset::DimStage, 15, r.controls);
    EXPECT_EQ(r.state->callLog(), (Calls{"auto_exposure=0", "exposure=555", "keep_fps=1", "awb=1"}));
}

TEST(Controls, DimStageFallsBackToAutoWhenTheSourceCannotGoManual)
{
    Rig r;
    r.state->supportsAutoExposure = false;
    r.state->supportsKeepFps = false;
    const auto report = applyPreset(r.source, Preset::DimStage, 15, r.controls);
    EXPECT_EQ(report.unsupported, (Calls{"exposure_manual", "exposure_auto", "keep_frame_rate"}));
    EXPECT_EQ(r.state->callLog(), (Calls{"awb=1"}));
}

TEST(Controls, LockExposurePinsTheValueTheSensorWasUsing)
{
    Rig r;
    applyPreset(r.source, Preset::LitRoom, 15, r.controls);
    r.state->calls.clear();
    r.state->exposure = 777;
    std::string error;
    ASSERT_TRUE(lockExposure(r.source, r.controls, error)) << error;
    EXPECT_EQ(r.state->callLog(), (Calls{"auto_exposure=0", "exposure=777"}));
    EXPECT_TRUE(r.controls.exposureLocked);
}

TEST(Controls, LockExposureStillWorksWhenTheValueCannotBeRead)
{
    Rig r;
    r.state->supportsAbsolute = false;
    std::string error;
    ASSERT_TRUE(lockExposure(r.source, r.controls, error)) << error;
    EXPECT_EQ(r.state->callLog(), (Calls{"auto_exposure=0"}));
}

TEST(Controls, LockFailsCleanlyWhenManualExposureIsMissing)
{
    Rig r;
    r.state->supportsAutoExposure = false;
    std::string error;
    EXPECT_FALSE(lockExposure(r.source, r.controls, error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(r.controls.exposureLocked);
}

TEST(Controls, UnlockReturnsToWhatThePresetSays)
{
    Rig r;
    applyPreset(r.source, Preset::DimStage, 15, r.controls);
    std::string error;
    ASSERT_TRUE(lockExposure(r.source, r.controls, error));
    r.state->calls.clear();
    ASSERT_TRUE(unlockExposure(r.source, 15, r.controls, error)) << error;
    EXPECT_EQ(r.state->callLog(), (Calls{"auto_exposure=0", "exposure=555"}));
    EXPECT_FALSE(r.controls.exposureLocked);

    Rig lit;
    applyPreset(lit.source, Preset::LitRoom, 15, lit.controls);
    ASSERT_TRUE(lockExposure(lit.source, lit.controls, error));
    lit.state->calls.clear();
    ASSERT_TRUE(unlockExposure(lit.source, 15, lit.controls, error));
    EXPECT_EQ(lit.state->callLog(), (Calls{"auto_exposure=1"}));
}

TEST(Controls, WhiteBalanceLockAndUnlock)
{
    Rig r;
    std::string error;
    ASSERT_TRUE(lockWhiteBalance(r.source, r.controls, error));
    EXPECT_TRUE(r.controls.wbLocked);
    ASSERT_TRUE(unlockWhiteBalance(r.source, r.controls, error));
    EXPECT_FALSE(r.controls.wbLocked);
    EXPECT_EQ(r.state->callLog(), (Calls{"awb=0", "awb=1"}));

    r.state->supportsWhiteBalance = false;
    EXPECT_FALSE(lockWhiteBalance(r.source, r.controls, error));
    EXPECT_FALSE(r.controls.wbLocked);
}

TEST(Controls, ApplyingAPresetClearsBothLocks)
{
    Rig r;
    std::string error;
    lockExposure(r.source, r.controls, error);
    lockWhiteBalance(r.source, r.controls, error);
    applyPreset(r.source, Preset::DimStage, 15, r.controls);
    EXPECT_FALSE(r.controls.exposureLocked);
    EXPECT_FALSE(r.controls.wbLocked);
    EXPECT_EQ(r.controls.preset, Preset::DimStage);
}
