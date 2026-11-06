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
// kick-gesture: hand tracking and gesture events from the camera service's frames.
//
//   kick-gesture --config /etc/kick-gesture.conf                      the service
//   kick-gesture --replay landmarks.jsonl --events-stdout             events from a recorded landmark stream, no camera or model
//   kick-gesture --bench --bench-image hand.ppm                       ms per frame and memory for each model set and frame size
//   kick-gesture --print-config                                       the gesture thresholds as JSON
#include "kick_gesture/config.h"
#include "kick_gesture/event_json.h"
#include "kick_gesture/replay.h"
#include "kick_gesture/service.h"
#include "kick_gesture/service_config.h"
#include "kick_gesture/sink.h"
#ifdef KICK_GESTURE_HAVE_ONNX
#include "kick_gesture/bench.h"
#include "kick_gesture/onnx_hand_model.h"
#endif

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#ifndef KICK_GESTURE_DEFAULT_MODEL_DIR
#define KICK_GESTURE_DEFAULT_MODEL_DIR "/usr/share/kick-gesture/models"
#endif

namespace
{
    std::atomic<bool> g_stop{false};
    void onSignal(int) { g_stop.store(true); }

    void usage()
    {
        std::fputs(
            "usage: kick-gesture [options]\n"
            "  --config FILE            settings file (key = value per line)\n"
            "  --gesture-config FILE    gesture thresholds (JSON), default: built-in\n"
            "  --shm NAME               camera shared memory (default /kick-camera)\n"
            "  --rate-hz N              inferences per second, 1 to 30 (default 12)\n"
            "  --model-dir DIR          directory with the .onnx models\n"
            "  --model-set int8|fp32    which models (default int8)\n"
            "  --threads N              ONNX Runtime threads, 1 to 4 (default 1)\n"
            "  --events-stdout          write events to standard output\n"
            "  --events-socket PATH     send events to a Unix stream socket the app listens on\n"
            "  --events-ipc HOST:PORT   send events to the Kick app's IPC server on loopback (127.0.0.1:50501)\n"
            "  --events-file PATH       append events to a file\n"
            "  --record FILE            record the landmark stream (hand geometry only, no pixels)\n"
            "  --replay FILE            run a recorded landmark stream through the gesture logic and exit\n"
            "  --realtime               with --replay: keep the recorded timing\n"
            "  --bench                  benchmark the models and exit\n"
            "  --bench-image FILE.ppm   a photo of a hand for the benchmark\n"
            "  --bench-frames N         frames per measurement (default 100)\n"
            "  --bench-sizes WxH,...    frame sizes (default 320x240,640x480)\n"
            "  --bench-sets a,b         model sets (default int8,fp32)\n"
            "  --print-config           print the gesture thresholds as JSON and exit\n"
            "  --version\n",
            stderr);
    }

    std::string readFile(const std::string& path, bool& ok)
    {
        std::ifstream in(path, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        ok = static_cast<bool>(in);
        return ss.str();
    }
}

int main(int argc, char** argv)
{
    using namespace kickgest;

    // First pass: find --config so the command line can override it.
    std::string configPath;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--config") == 0) configPath = argv[i + 1];

    ServiceConfig sc;
    std::string err;
    if (!configPath.empty() && !loadServiceConfigFile(configPath, sc, err))
    {
        std::fprintf(stderr, "kick-gesture: %s\n", err.c_str());
        return 2;
    }

    std::string replayPath;
    bool realtime = false;
    bool bench = false;
    bool printConfig = false;
    std::string benchImage, benchSizes, benchSets;
    int benchFrames = 100;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--help" || a == "-h")
        {
            usage();
            return 0;
        }
        if (a == "--version")
        {
            std::puts("kick-gesture 1.0.0");
            return 0;
        }
        if (a.rfind("--", 0) != 0)
        {
            std::fprintf(stderr, "kick-gesture: unexpected argument \"%s\"\n", a.c_str());
            usage();
            return 2;
        }
        a = a.substr(2);
        if (a == "realtime") { realtime = true; continue; }
        if (a == "bench") { bench = true; continue; }
        if (a == "print-config") { printConfig = true; continue; }
        std::string key = a;
        for (char& ch : key)
            if (ch == '-') ch = '_';
        if (isServiceFlag(key))
        {
            applyServiceSetting(key, "true", sc, err);
            continue;
        }
        if (i + 1 >= argc)
        {
            std::fprintf(stderr, "kick-gesture: --%s is not an option, or it is missing its value\n", a.c_str());
            return 2;
        }
        const std::string value = argv[++i];
        if (a == "config") continue;
        if (a == "replay") replayPath = value;
        else if (a == "bench-image") benchImage = value;
        else if (a == "bench-sizes") benchSizes = value;
        else if (a == "bench-sets") benchSets = value;
        else if (a == "bench-frames") benchFrames = std::atoi(value.c_str());
        else if (!applyServiceSetting(key, value, sc, err))
        {
            std::fprintf(stderr, "kick-gesture: %s\n", err.c_str());
            return 2;
        }
    }

    GestureConfig gc;
    if (!sc.gestureConfig.empty())
    {
        bool ok = false;
        const std::string text = readFile(sc.gestureConfig, ok);
        if (!ok)
        {
            std::fprintf(stderr, "kick-gesture: cannot read %s\n", sc.gestureConfig.c_str());
            return 2;
        }
        if (!parseGestureConfig(text, gc, err))
        {
            std::fprintf(stderr, "kick-gesture: %s: %s\n", sc.gestureConfig.c_str(), err.c_str());
            return 2;
        }
    }
    if (printConfig)
    {
        std::fputs(gestureConfigToJson(gc).c_str(), stdout);
        return 0;
    }

    const std::string modelDir = sc.modelDir.empty() ? KICK_GESTURE_DEFAULT_MODEL_DIR : sc.modelDir;

    if (bench)
    {
#ifdef KICK_GESTURE_HAVE_ONNX
        BenchOptions bo;
        bo.modelDir = modelDir;
        bo.imagePath = benchImage;
        bo.threads = sc.threads;
        bo.frames = benchFrames > 0 ? benchFrames : 100;
        if (!benchSizes.empty() && !parseSizes(benchSizes, bo.sizes))
        {
            std::fputs("kick-gesture: --bench-sizes needs WxH[,WxH...]\n", stderr);
            return 2;
        }
        if (!benchSets.empty())
        {
            bo.modelSets.clear();
            std::stringstream ss(benchSets);
            std::string item;
            while (std::getline(ss, item, ','))
            {
                if (item != "int8" && item != "fp32")
                {
                    std::fputs("kick-gesture: --bench-sets takes int8 and fp32\n", stderr);
                    return 2;
                }
                bo.modelSets.push_back(item);
            }
        }
        if (runBench(bo, std::cout, err) != 0)
        {
            std::fprintf(stderr, "kick-gesture: %s\n", err.c_str());
            return 1;
        }
        return 0;
#else
        std::fputs("kick-gesture: this build has no ONNX Runtime backend, so there is nothing to benchmark\n", stderr);
        return 3;
#endif
    }

    std::signal(SIGPIPE, SIG_IGN);

    StdoutSink stdoutSink;
    FileSink fileSink;
    std::unique_ptr<UnixSocketSink> socketSink;
    std::unique_ptr<LoopbackIpcSink> ipcSink;
    std::vector<IEventSink*> sinks;
    if (sc.eventsStdout || (!replayPath.empty() && sc.eventsFile.empty() && sc.eventsSocket.empty() && sc.eventsIpc.empty())) sinks.push_back(&stdoutSink);
    if (!sc.eventsFile.empty())
    {
        if (!fileSink.open(sc.eventsFile, err))
        {
            std::fprintf(stderr, "kick-gesture: %s\n", err.c_str());
            return 2;
        }
        sinks.push_back(&fileSink);
    }
    if (!sc.eventsSocket.empty())
    {
        socketSink = std::make_unique<UnixSocketSink>(sc.eventsSocket);
        sinks.push_back(socketSink.get());
    }
    if (!sc.eventsIpc.empty())
    {
        std::string host;
        uint16_t port = 0;
        if (!parseIpcEndpoint(sc.eventsIpc, host, port))
        {
            std::fprintf(stderr, "kick-gesture: events_ipc must be host:port, got %s\n", sc.eventsIpc.c_str());
            return 2;
        }
        ipcSink = std::make_unique<LoopbackIpcSink>(host, port);
        sinks.push_back(ipcSink.get());
    }

    if (!replayPath.empty())
    {
        std::ifstream in(replayPath);
        if (!in)
        {
            std::fprintf(stderr, "kick-gesture: cannot read %s\n", replayPath.c_str());
            return 2;
        }
        GestureEngine engine(gc);
        const ReplayResult r = replayStream(in, engine, [&](const GestureEvent& e) {
            const std::string line = eventToJson(e);
            for (IEventSink* s : sinks) s->send(line);
        }, realtime);
        if (!r.ok)
        {
            std::fprintf(stderr, "kick-gesture: %s: %s\n", replayPath.c_str(), r.error.c_str());
            return 1;
        }
        std::fprintf(stderr, "kick-gesture: replayed %zu frames, %zu events\n", r.frames, r.events);
        return 0;
    }

    if (sinks.empty())
    {
        std::fputs("kick-gesture: no event sink; give --events-ipc, --events-socket, --events-file or --events-stdout\n", stderr);
        return 2;
    }
#ifdef KICK_GESTURE_HAVE_ONNX
    OnnxModelOptions mo;
    const std::string suffix = sc.modelSet == "int8" ? "_int8.onnx" : ".onnx";
    mo.palmModelPath = modelDir + "/palm_detection_mediapipe_2023feb" + suffix;
    mo.landmarkModelPath = modelDir + "/handpose_estimation_mediapipe_2023feb" + suffix;
    mo.intraOpThreads = sc.threads;
    auto model = OnnxHandModel::create(mo, err);
    if (!model)
    {
        std::fprintf(stderr, "kick-gesture: cannot load the models from %s: %s\n", modelDir.c_str(), err.c_str());
        return 3;
    }

    std::ofstream recordFile;
    std::unique_ptr<LandmarkWriter> recorder;
    if (!sc.record.empty())
    {
        recordFile.open(sc.record, std::ios::trunc);
        if (!recordFile)
        {
            std::fprintf(stderr, "kick-gesture: cannot write %s\n", sc.record.c_str());
            return 2;
        }
        recorder = std::make_unique<LandmarkWriter>(recordFile);
        recorder->writeHeader();
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    ServiceOptions so;
    so.rateHz = sc.rateHz;
    so.statusIntervalMs = sc.statusIntervalMs;
    ShmFrameReader reader(sc.shm);
    SystemResourceReader resources;
    GestureService service(model.get(), gc, so, sc.guard, &resources, &reader, sinks, recorder.get());
    service.run(g_stop);
    return 0;
#else
    std::fputs("kick-gesture: this build has no ONNX Runtime backend (it was configured without it, or the download was blocked); only --replay and --print-config work\n", stderr);
    return 3;
#endif
}
