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
#include "kick_camera/control_server.h"
#include "kick_camera/service.h"

#include <csignal>
#include <cstdio>
#include <cstring>

namespace
{
    void usage()
    {
        std::fputs(
            "kick-camera: camera capture service\n"
            "\n"
            "  kick-camera [--config FILE] [--KEY VALUE ...] [--enable]\n"
            "  kick-camera [--socket PATH] --ctl COMMAND [ARG]   (--ctl takes the rest of the command line)\n"
            "\n"
            "Settings (also valid as key=value lines in the config file; command-line values win):\n"
            "  --source v4l2|file|pattern|pipe   --device /dev/video0   --file PATH   --pipe-command CMD\n"
            "  --size WxH (320x240)   --fps N (15)   --format auto|yuyv|nv12   --max-size WxH (640x480)   --loop true|false\n"
            "  --shm /kick-camera   --socket /run/kick-camera.sock   --preset lit_room|dim_stage\n"
            "  --debug-dump DIR   --debug-dump-max N (100)   --enable (open the camera at start-up)\n"
            "\n"
            "The camera is closed until a client sends 'enable' on the control socket.\n"
            "Commands: enable, disable, status, lock_exposure, unlock_exposure, lock_wb, unlock_wb, preset NAME\n",
            stderr);
    }

    std::string keyFromFlag(const char* flag)
    {
        std::string k(flag + 2);
        for (char& c : k)
            if (c == '-') c = '_';
        return k;
    }
}

int main(int argc, char** argv)
{
    kickcam::Config cfg;
    std::string error;
    std::string ctlCommand;
    bool ctl = false;

    // The config file is applied first so command-line flags override it, wherever --config appears.
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::strcmp(argv[i], "--config") == 0 && !kickcam::loadConfigFile(argv[i + 1], cfg, error))
        {
            std::fprintf(stderr, "kick-camera: %s\n", error.c_str());
            return 2;
        }
    }
    for (int i = 1; i < argc; ++i)
    {
        const std::string flag = argv[i];
        if (flag == "--help" || flag == "-h")
        {
            usage();
            return 0;
        }
        if (flag == "--config") { ++i; continue; }
        if (flag == "--enable")
        {
            cfg.enableOnStart = true;
            continue;
        }
        if (flag.rfind("--", 0) != 0 || i + 1 >= argc)
        {
            usage();
            return 2;
        }
        if (flag == "--ctl")
        {
            // Everything after --ctl is the command, so "--ctl preset dim_stage" works.
            ctl = true;
            for (int j = i + 1; j < argc; ++j) ctlCommand += (j > i + 1 ? " " : "") + std::string(argv[j]);
            break;
        }
        if (!kickcam::applySetting(cfg, keyFromFlag(argv[i]), argv[i + 1], error))
        {
            std::fprintf(stderr, "kick-camera: %s\n", error.c_str());
            return 2;
        }
        ++i;
    }

    if (ctl)
    {
        std::string reply;
        if (!kickcam::ControlServer::request(cfg.socketPath, ctlCommand, reply, error))
        {
            std::fprintf(stderr, "kick-camera: %s\n", error.c_str());
            return 1;
        }
        std::puts(reply.c_str());
        return reply.rfind("err", 0) == 0 ? 1 : 0;
    }

    // Block the termination signals before any thread exists so every thread inherits the mask and sigwait gets them.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);

    kickcam::CameraService service(cfg);
    if (!service.start(error))
    {
        std::fprintf(stderr, "kick-camera: %s\n", error.c_str());
        return 1;
    }
    std::fprintf(stderr, "kick-camera: ready, camera is off (socket %s, shm %s)\n", cfg.socketPath.c_str(), cfg.shmName.c_str());
    if (cfg.enableOnStart && !service.enable(error)) std::fprintf(stderr, "kick-camera: enable failed: %s\n", error.c_str());

    int sig = 0;
    sigwait(&signals, &sig);
    std::fprintf(stderr, "kick-camera: stopping\n");
    service.stop();
    return 0;
}
