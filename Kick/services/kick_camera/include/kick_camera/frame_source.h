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

#include "kick_camera/types.h"

#include <memory>
#include <string>

namespace kickcam
{
    enum class ReadResult
    {
        Frame,       // out holds a frame
        Timeout,     // nothing arrived within the timeout; call again
        EndOfStream, // a finite source ran out, or a pipe closed
        Error        // the source failed; error says why. The source is not usable afterwards.
    };

    // Where frames come from. Every backend hands out frames in YUYV or NV12.
    //
    // Threading: next() is called from one capture thread. The control setters may be called from another
    // thread while next() is blocked, so a backend must make them safe against that.
    class FrameSource
    {
    public:
        virtual ~FrameSource() = default;

        // Opens the device or file and settles on a format. Nothing is captured before this succeeds.
        virtual bool open(std::string& error) = 0;

        // Releases the device. Safe to call twice, and safe on a source that never opened.
        virtual void close() = 0;

        // The negotiated format. Only meaningful after open() succeeded.
        virtual FrameFormat format() const = 0;

        // Frames per second the backend settled on, 0 when unknown.
        virtual uint32_t fps() const = 0;

        // Waits up to timeoutMs for the next frame. The view stays valid until the next call.
        virtual ReadResult next(FrameView& out, int timeoutMs, std::string& error) = 0;

        // Short name for status output: "v4l2", "file", "pattern", "pipe".
        virtual const char* name() const = 0;

        // Frames the source knows it lost (V4L2 sequence gaps and buffers flagged in error). 0 for sources
        // that cannot lose frames.
        virtual uint64_t droppedFrames() const { return 0; }

        // Sensor controls. A backend that cannot do one returns false and changes nothing.

        // True: the sensor picks exposure. False: exposure stays where it is (manual).
        virtual bool setAutoExposure(bool) { return false; }
        // Exposure time in units of 100 microseconds, the unit V4L2_CID_EXPOSURE_ABSOLUTE uses.
        virtual bool setExposureAbsolute(int) { return false; }
        virtual bool getExposureAbsolute(int&) { return false; }
        // True: the sensor keeps the frame rate fixed in low light. False lets it stretch exposure and drop fps.
        virtual bool setKeepFrameRate(bool) { return false; }
        virtual bool setAutoWhiteBalance(bool) { return false; }
    };
}
