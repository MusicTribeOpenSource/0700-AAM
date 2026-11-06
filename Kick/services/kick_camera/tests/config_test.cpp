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
#include "kick_camera/config.h"

#include "test_util.h"

#include <gtest/gtest.h>

using namespace kickcam;

TEST(Config, DefaultsMatchThePlan)
{
    Config c;
    EXPECT_EQ(c.source, "v4l2");
    EXPECT_EQ(c.width, 320u);
    EXPECT_EQ(c.height, 240u);
    EXPECT_EQ(c.fps, 15u);
    EXPECT_FALSE(c.enableOnStart) << "the camera is off by default";
    EXPECT_TRUE(c.debugDumpDir.empty()) << "nothing is written to disk by default";
    EXPECT_EQ(c.shmName, "/kick-camera");
    EXPECT_EQ(c.socketPath, "/run/kick-camera.sock");
    std::string error;
    EXPECT_TRUE(validate(c, error)) << error;
}

TEST(Config, SettingsParseAndBadValuesAreNamed)
{
    Config c;
    std::string error;
    EXPECT_TRUE(applySetting(c, "size", "640x480", error));
    EXPECT_EQ(c.width, 640u);
    EXPECT_EQ(c.height, 480u);
    EXPECT_TRUE(applySetting(c, "format", "NV12", error));
    EXPECT_EQ(c.fourcc, kFourccNv12);
    EXPECT_TRUE(applySetting(c, "format", "auto", error));
    EXPECT_EQ(c.fourcc, 0u);
    EXPECT_TRUE(applySetting(c, "preset", "dim_stage", error));
    EXPECT_EQ(c.preset, Preset::DimStage);
    EXPECT_TRUE(applySetting(c, "enable", "yes", error));
    EXPECT_TRUE(c.enableOnStart);

    EXPECT_FALSE(applySetting(c, "size", "big", error));
    EXPECT_NE(error.find("size"), std::string::npos);
    EXPECT_FALSE(applySetting(c, "fps", "-3", error));
    EXPECT_FALSE(applySetting(c, "source", "usb", error));
    EXPECT_FALSE(applySetting(c, "colour", "red", error));
    EXPECT_NE(error.find("colour"), std::string::npos);
}

TEST(Config, FileHasCommentsBlankLinesAndReportsTheBadLine)
{
    kicktest::TempDir dir;
    kicktest::writeFile(dir.file("good.conf"), std::string("# camera\n\nsource = pattern   # no hardware\nfps=10\n"));
    Config c;
    std::string error;
    ASSERT_TRUE(loadConfigFile(dir.file("good.conf"), c, error)) << error;
    EXPECT_EQ(c.source, "pattern");
    EXPECT_EQ(c.fps, 10u);

    kicktest::writeFile(dir.file("bad.conf"), std::string("fps=10\nthis line has no equals\n"));
    EXPECT_FALSE(loadConfigFile(dir.file("bad.conf"), c, error));
    EXPECT_NE(error.find("bad.conf:2"), std::string::npos);

    EXPECT_FALSE(loadConfigFile(dir.file("missing.conf"), c, error));
}

TEST(Config, ValidateRejectsImpossibleCombinations)
{
    std::string error;
    Config c;
    c.width = 1280;
    c.height = 720;
    EXPECT_FALSE(validate(c, error)) << "larger than max_size";
    c = Config();
    c.fps = 0;
    EXPECT_FALSE(validate(c, error));
    c = Config();
    c.width = 321;
    EXPECT_FALSE(validate(c, error)) << "odd width";
    c = Config();
    c.source = "file";
    EXPECT_FALSE(validate(c, error)) << "file source without a file";
    c = Config();
    c.shmName = "kick-camera";
    EXPECT_FALSE(validate(c, error));
    c = Config();
    c.shmName = "/a/b";
    EXPECT_FALSE(validate(c, error));
    c = Config();
    c.socketPath = std::string(120, 'a');
    EXPECT_FALSE(validate(c, error));
}

TEST(Config, RingCapacityHoldsAMaxSizeYuyvFrame)
{
    Config c;
    EXPECT_EQ(ringCapacity(c), 640u * 480 * 2);
}
