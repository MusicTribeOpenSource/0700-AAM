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
// V4L2Source against a stand-in kernel. This checks the call sequence and the bookkeeping (negotiation, buffer
// recycling, dropped-frame counting, controls). It does not prove that a real driver accepts those calls: that
// needs a /dev/videoN node, which a container does not have.
#include "kick_camera/v4l2_source.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>
#include <map>

#include <linux/videodev2.h>

using namespace kickcam;

namespace
{
    struct FakeKernel
    {
        // Device behaviour.
        uint32_t caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
        bool supportsYuyv = true;
        bool supportsNv12 = true;
        uint32_t maxWidth = 640;
        uint32_t maxHeight = 480;
        unsigned grantBuffers = 4;
        bool refuseSParm = false;
        bool stall = false; // poll() reports nothing to read
        std::map<unsigned, int> controls; // id -> value, only these ids exist
        std::vector<unsigned> acceptedExposureAuto = {V4L2_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL, V4L2_EXPOSURE_APERTURE_PRIORITY};

        // What happened.
        std::vector<unsigned long> requests;
        std::deque<unsigned> queued;
        bool streaming = false;
        bool opened = false;
        int closed = 0;
        int unmapped = 0;
        int releasedBuffers = 0;
        uint32_t requestedFourcc = 0;
        std::vector<std::vector<uint8_t>> buffers;
        uint32_t sequence = 0;
        uint32_t nextSequenceSkip = 0;
        uint32_t nextFlags = 0;
        uint32_t nextBytesUsed = 0;
        uint32_t width = 0, height = 0, fourcc = 0;
        std::vector<std::pair<unsigned, int>> controlWrites;

        V4l2Ops ops()
        {
            V4l2Ops o;
            o.open = [this](const char*, int) { opened = true; return 7; };
            o.close = [this](int) { ++closed; return 0; };
            o.ioctl = [this](int, unsigned long request, void* arg) { return handle(request, arg); };
            o.mmap = [this](size_t length, int, off_t offset) -> void*
            {
                const size_t index = static_cast<size_t>(offset) / 4096;
                if (index >= buffers.size()) return nullptr;
                buffers[index].assign(length, static_cast<uint8_t>(0x10 + index));
                return buffers[index].data();
            };
            o.munmap = [this](void*, size_t) { ++unmapped; return 0; };
            o.poll = [this](int, int) { return stall || queued.empty() ? 0 : 1; };
            return o;
        }

        int fail(int err) { errno = err; return -1; }

        int handle(unsigned long request, void* arg)
        {
            requests.push_back(request);
            switch (request)
            {
                case VIDIOC_QUERYCAP:
                {
                    auto* c = static_cast<v4l2_capability*>(arg);
                    c->capabilities = caps | V4L2_CAP_DEVICE_CAPS;
                    c->device_caps = caps;
                    return 0;
                }
                case VIDIOC_S_FMT:
                {
                    auto* f = static_cast<v4l2_format*>(arg);
                    requestedFourcc = f->fmt.pix.pixelformat;
                    const bool yuyv = f->fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV;
                    const bool ok = yuyv ? supportsYuyv : supportsNv12;
                    if (!ok) f->fmt.pix.pixelformat = supportsYuyv ? V4L2_PIX_FMT_YUYV : V4L2_PIX_FMT_NV12; // drivers substitute
                    f->fmt.pix.width = std::min(f->fmt.pix.width, maxWidth);
                    f->fmt.pix.height = std::min(f->fmt.pix.height, maxHeight);
                    const bool isYuyv = f->fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV;
                    f->fmt.pix.bytesperline = isYuyv ? f->fmt.pix.width * 2 : f->fmt.pix.width;
                    f->fmt.pix.sizeimage = isYuyv ? f->fmt.pix.bytesperline * f->fmt.pix.height : f->fmt.pix.bytesperline * f->fmt.pix.height * 3 / 2;
                    width = f->fmt.pix.width;
                    height = f->fmt.pix.height;
                    fourcc = f->fmt.pix.pixelformat;
                    return 0;
                }
                case VIDIOC_S_PARM:
                    return refuseSParm ? fail(EINVAL) : 0; // leaves the requested interval in place, as many drivers do
                case VIDIOC_REQBUFS:
                {
                    auto* r = static_cast<v4l2_requestbuffers*>(arg);
                    if (r->count == 0)
                    {
                        ++releasedBuffers;
                        return 0;
                    }
                    r->count = std::min(r->count, grantBuffers);
                    buffers.assign(r->count, {});
                    return 0;
                }
                case VIDIOC_QUERYBUF:
                {
                    auto* b = static_cast<v4l2_buffer*>(arg);
                    b->length = fourcc == V4L2_PIX_FMT_YUYV ? width * height * 2 : width * height * 3 / 2;
                    b->m.offset = b->index * 4096;
                    return 0;
                }
                case VIDIOC_QBUF:
                    queued.push_back(static_cast<v4l2_buffer*>(arg)->index);
                    return 0;
                case VIDIOC_DQBUF:
                {
                    if (queued.empty()) return fail(EAGAIN);
                    auto* b = static_cast<v4l2_buffer*>(arg);
                    b->index = queued.front();
                    queued.pop_front();
                    sequence += nextSequenceSkip;
                    nextSequenceSkip = 0;
                    b->sequence = sequence++;
                    b->flags = nextFlags | V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
                    nextFlags = 0;
                    b->bytesused = nextBytesUsed != 0 ? nextBytesUsed : (fourcc == V4L2_PIX_FMT_YUYV ? width * height * 2 : width * height * 3 / 2);
                    nextBytesUsed = 0;
                    b->timestamp.tv_sec = 5;
                    b->timestamp.tv_usec = 250000;
                    std::memset(buffers[b->index].data(), static_cast<int>(0x80 + b->sequence), 4);
                    return 0;
                }
                case VIDIOC_STREAMON: streaming = true; return 0;
                case VIDIOC_STREAMOFF: streaming = false; return 0;
                case VIDIOC_S_CTRL:
                {
                    auto* c = static_cast<v4l2_control*>(arg);
                    if (!controls.count(c->id)) return fail(EINVAL);
                    if (c->id == V4L2_CID_EXPOSURE_AUTO &&
                        std::find(acceptedExposureAuto.begin(), acceptedExposureAuto.end(), static_cast<unsigned>(c->value)) == acceptedExposureAuto.end())
                        return fail(EINVAL);
                    controls[c->id] = c->value;
                    controlWrites.emplace_back(c->id, c->value);
                    return 0;
                }
                case VIDIOC_G_CTRL:
                {
                    auto* c = static_cast<v4l2_control*>(arg);
                    if (!controls.count(c->id)) return fail(EINVAL);
                    c->value = controls[c->id];
                    return 0;
                }
            }
            return fail(ENOTTY);
        }
    };

    FrameFormat want(uint32_t w, uint32_t h, uint32_t fourcc = 0) { return {w, h, 0, fourcc}; }
}

TEST(V4L2, OpensNegotiatesYuyvAndStartsStreaming)
{
    FakeKernel k;
    V4L2Source s("/dev/video0", want(320, 240), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    EXPECT_EQ(s.format(), (FrameFormat{320, 240, 640, kFourccYuyv}));
    EXPECT_EQ(s.fps(), 15u);
    EXPECT_TRUE(k.streaming);
    EXPECT_EQ(k.queued.size(), 4u) << "every buffer is queued before STREAMON";
    EXPECT_EQ(k.requestedFourcc, V4L2_PIX_FMT_YUYV) << "YUYV is tried first";
}

TEST(V4L2, FallsBackToNv12WhenTheDeviceHasNoYuyv)
{
    FakeKernel k;
    k.supportsYuyv = false;
    V4L2Source s("/dev/video0", want(320, 240), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    EXPECT_EQ(s.format(), (FrameFormat{320, 240, 320, kFourccNv12}));
}

TEST(V4L2, TakesTheSizeTheDriverAdjustsTo)
{
    FakeKernel k;
    k.maxWidth = 160;
    k.maxHeight = 120;
    V4L2Source s("/dev/video0", want(320, 240), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;
    EXPECT_EQ(s.format().width, 160u);
    EXPECT_EQ(s.format().height, 120u);
}

TEST(V4L2, ARequestedFormatTheDeviceWillNotGiveIsAnError)
{
    FakeKernel k;
    k.supportsNv12 = false;
    V4L2Source s("/dev/video0", want(320, 240, kFourccNv12), 15, k.ops());
    std::string error;
    EXPECT_FALSE(s.open(error));
    EXPECT_NE(error.find("NV12"), std::string::npos) << error;
    EXPECT_GE(k.closed, 1) << "a failed open releases the device";
}

TEST(V4L2, DevicesThatCannotCaptureOrStreamAreRefusedWithAReason)
{
    FakeKernel mplane;
    mplane.caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;
    V4L2Source a("/dev/video0", want(320, 240), 15, mplane.ops());
    std::string error;
    EXPECT_FALSE(a.open(error));
    EXPECT_NE(error.find("multi-planar"), std::string::npos) << error;

    FakeKernel readOnly;
    readOnly.caps = V4L2_CAP_VIDEO_CAPTURE;
    V4L2Source b("/dev/video0", want(320, 240), 15, readOnly.ops());
    EXPECT_FALSE(b.open(error));
    EXPECT_NE(error.find("streaming"), std::string::npos) << error;

    FakeKernel noDevice;
    V4l2Ops ops = noDevice.ops();
    ops.open = [](const char*, int) { errno = ENOENT; return -1; };
    V4L2Source c("/dev/video9", want(320, 240), 15, ops);
    EXPECT_FALSE(c.open(error));
    EXPECT_NE(error.find("/dev/video9"), std::string::npos) << error;
}

TEST(V4L2, TooFewBuffersFailsTheOpen)
{
    FakeKernel k;
    k.grantBuffers = 1;
    V4L2Source s("/dev/video0", want(320, 240), 15, k.ops());
    std::string error;
    EXPECT_FALSE(s.open(error));
}

TEST(V4L2, FramesAreHandedOutAndBuffersGoBackToTheDriverOnTheNextCall)
{
    FakeKernel k;
    V4L2Source s("/dev/video0", want(64, 32), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error)) << error;

    FrameView v;
    ASSERT_EQ(s.next(v, 100, error), ReadResult::Frame) << error;
    EXPECT_EQ(v.size, 64u * 32 * 2);
    EXPECT_EQ(v.data[0], 0x80) << "points into the mapped buffer";
    EXPECT_EQ(v.timestampNs, 5ull * 1000000000 + 250000ull * 1000) << "kernel monotonic timestamp is used";
    EXPECT_EQ(k.queued.size(), 3u) << "the delivered buffer is still held";
    ASSERT_EQ(s.next(v, 100, error), ReadResult::Frame);
    EXPECT_EQ(k.queued.size(), 3u) << "the first buffer went back, the second is held";
    EXPECT_EQ(s.droppedFrames(), 0u);
}

TEST(V4L2, TimesOutWhenNothingIsQueuedForCapture)
{
    FakeKernel k;
    k.grantBuffers = 2;
    V4L2Source s("/dev/video0", want(64, 32), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error));
    FrameView v;
    ASSERT_EQ(s.next(v, 10, error), ReadResult::Frame);
    k.stall = true; // the sensor stopped producing
    EXPECT_EQ(s.next(v, 10, error), ReadResult::Timeout);
}

TEST(V4L2, SequenceGapsAndBadBuffersCountAsDropped)
{
    FakeKernel k;
    V4L2Source s("/dev/video0", want(64, 32), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error));
    FrameView v;
    ASSERT_EQ(s.next(v, 10, error), ReadResult::Frame);
    k.nextSequenceSkip = 3; // three frames never reached userspace
    ASSERT_EQ(s.next(v, 10, error), ReadResult::Frame);
    EXPECT_EQ(s.droppedFrames(), 3u);

    k.nextFlags = V4L2_BUF_FLAG_ERROR;
    EXPECT_EQ(s.next(v, 10, error), ReadResult::Timeout);
    EXPECT_EQ(s.droppedFrames(), 4u);

    k.nextBytesUsed = 10; // truncated frame
    EXPECT_EQ(s.next(v, 10, error), ReadResult::Timeout);
    EXPECT_EQ(s.droppedFrames(), 5u);
    EXPECT_EQ(k.queued.size(), 4u) << "all four buffers are back with the driver: rejected ones were requeued, none leak";
}

TEST(V4L2, SequenceNumberWrapIsNotADrop)
{
    FakeKernel k;
    V4L2Source s("/dev/video0", want(64, 32), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error));
    k.sequence = 0xFFFFFFFFu;
    FrameView v;
    ASSERT_EQ(s.next(v, 10, error), ReadResult::Frame);
    ASSERT_EQ(s.next(v, 10, error), ReadResult::Frame); // sequence 0 after 0xFFFFFFFF
    EXPECT_EQ(s.droppedFrames(), 0u);
}

TEST(V4L2, CloseStopsStreamingUnmapsEverythingAndReleasesTheDevice)
{
    FakeKernel k;
    V4L2Source s("/dev/video0", want(64, 32), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error));
    s.close();
    EXPECT_FALSE(k.streaming);
    EXPECT_EQ(k.unmapped, 4);
    EXPECT_EQ(k.releasedBuffers, 1);
    EXPECT_EQ(k.closed, 1);
    s.close();
    EXPECT_EQ(k.closed, 1) << "closing twice does nothing";
    FrameView v;
    EXPECT_EQ(s.next(v, 10, error), ReadResult::Error);
}

TEST(V4L2, UvcStyleDeviceUsesAperturePriorityForAutoExposure)
{
    FakeKernel k;
    k.controls = {{V4L2_CID_EXPOSURE_AUTO, 0}, {V4L2_CID_EXPOSURE_ABSOLUTE, 333}, {V4L2_CID_EXPOSURE_AUTO_PRIORITY, 1}, {V4L2_CID_AUTO_WHITE_BALANCE, 1}};
    V4L2Source s("/dev/video0", want(64, 32), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error));

    ASSERT_TRUE(s.setAutoExposure(true));
    EXPECT_EQ(k.controls[V4L2_CID_EXPOSURE_AUTO], V4L2_EXPOSURE_APERTURE_PRIORITY);
    int value = 0;
    ASSERT_TRUE(s.getExposureAbsolute(value));
    EXPECT_EQ(value, 333);
    ASSERT_TRUE(s.setAutoExposure(false));
    EXPECT_EQ(k.controls[V4L2_CID_EXPOSURE_AUTO], V4L2_EXPOSURE_MANUAL);
    ASSERT_TRUE(s.setExposureAbsolute(555));
    EXPECT_EQ(k.controls[V4L2_CID_EXPOSURE_ABSOLUTE], 555);
    ASSERT_TRUE(s.setKeepFrameRate(true));
    EXPECT_EQ(k.controls[V4L2_CID_EXPOSURE_AUTO_PRIORITY], 0) << "0 means constant frame rate";
    ASSERT_TRUE(s.setAutoWhiteBalance(false));
    EXPECT_EQ(k.controls[V4L2_CID_AUTO_WHITE_BALANCE], 0);
}

TEST(V4L2, SensorStyleDeviceFallsBackToPlainAutoExposure)
{
    FakeKernel k;
    k.controls = {{V4L2_CID_EXPOSURE_AUTO, 1}};
    k.acceptedExposureAuto = {V4L2_EXPOSURE_AUTO, V4L2_EXPOSURE_MANUAL};
    V4L2Source s("/dev/video0", want(64, 32), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error));
    ASSERT_TRUE(s.setAutoExposure(true));
    EXPECT_EQ(k.controls[V4L2_CID_EXPOSURE_AUTO], V4L2_EXPOSURE_AUTO);
}

TEST(V4L2, MissingControlsReturnFalseWithoutSideEffects)
{
    FakeKernel k; // no controls at all
    V4L2Source s("/dev/video0", want(64, 32), 15, k.ops());
    std::string error;
    ASSERT_TRUE(s.open(error));
    EXPECT_FALSE(s.setAutoExposure(true));
    EXPECT_FALSE(s.setExposureAbsolute(100));
    int value = 0;
    EXPECT_FALSE(s.getExposureAbsolute(value));
    EXPECT_FALSE(s.setKeepFrameRate(true));
    EXPECT_FALSE(s.setAutoWhiteBalance(true));
    EXPECT_TRUE(k.controlWrites.empty());
}
