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
#include "kick_camera/control_server.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <map>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace kickcam
{
    namespace
    {
        constexpr size_t kMaxLine = 256;
        constexpr size_t kMaxClients = 8;

        void setNonBlocking(int fd) { ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

        // The pipe source forks a helper (ffmpeg) while this server is running. Without close-on-exec the helper
        // would inherit the listening socket and keep it open after the service dies.
        void setCloseOnExec(int fd) { ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD, 0) | FD_CLOEXEC); }

        bool makeAddress(const std::string& path, sockaddr_un& addr)
        {
            if (path.size() >= sizeof(addr.sun_path)) return false;
            std::memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
            return true;
        }
    }

    ControlServer::~ControlServer() { stop(); }

    bool ControlServer::start(const std::string& path, Handler handler, std::string& error)
    {
        stop();
        sockaddr_un addr;
        if (!makeAddress(path, addr))
        {
            error = "control socket path is too long: " + path;
            return false;
        }
        ::signal(SIGPIPE, SIG_IGN);

        listenFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listenFd_ < 0)
        {
            error = std::string("socket: ") + std::strerror(errno);
            return false;
        }
        setCloseOnExec(listenFd_);
        ::unlink(path.c_str());
        // Create the socket file with no permissions for anyone else, so there is no window where it is open.
        const mode_t oldMask = ::umask(0177);
        const int bound = ::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        ::umask(oldMask);
        if (bound != 0 || ::listen(listenFd_, 4) != 0)
        {
            error = "bind/listen " + path + ": " + std::strerror(errno);
            ::close(listenFd_);
            listenFd_ = -1;
            return false;
        }
        int pipeFds[2];
        if (::pipe(pipeFds) != 0)
        {
            error = std::string("pipe: ") + std::strerror(errno);
            ::close(listenFd_);
            listenFd_ = -1;
            ::unlink(path.c_str());
            return false;
        }
        wakeRead_ = pipeFds[0];
        wakeWrite_ = pipeFds[1];
        setCloseOnExec(wakeRead_);
        setCloseOnExec(wakeWrite_);
        setNonBlocking(listenFd_);

        path_ = path;
        handler_ = std::move(handler);
        stopping_ = false;
        thread_ = std::thread([this] { run(); });
        return true;
    }

    void ControlServer::stop()
    {
        if (thread_.joinable())
        {
            stopping_ = true;
            const char byte = 1;
            if (::write(wakeWrite_, &byte, 1) < 0) { /* the pipe is only a wake-up; the loop also polls stopping_ */ }
            thread_.join();
        }
        if (listenFd_ >= 0) ::close(listenFd_);
        if (wakeRead_ >= 0) ::close(wakeRead_);
        if (wakeWrite_ >= 0) ::close(wakeWrite_);
        if (!path_.empty()) ::unlink(path_.c_str());
        listenFd_ = wakeRead_ = wakeWrite_ = -1;
        path_.clear();
    }

    void ControlServer::run()
    {
        std::map<int, std::string> clients; // fd -> bytes received so far
        while (!stopping_)
        {
            std::vector<pollfd> fds;
            fds.push_back({wakeRead_, POLLIN, 0});
            fds.push_back({listenFd_, POLLIN, 0});
            for (const auto& c : clients) fds.push_back({c.first, POLLIN, 0});
            if (::poll(fds.data(), fds.size(), 500) < 0 && errno != EINTR) break;
            if (stopping_) break;

            if ((fds[1].revents & POLLIN) != 0)
            {
                const int fd = ::accept(listenFd_, nullptr, nullptr);
                if (fd >= 0)
                {
                    if (clients.size() >= kMaxClients) ::close(fd);
                    else
                    {
                        setNonBlocking(fd);
                        setCloseOnExec(fd);
                        clients[fd];
                    }
                }
            }
            for (size_t i = 2; i < fds.size(); ++i)
            {
                if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) == 0) continue;
                const int fd = fds[i].fd;
                char buf[128];
                const ssize_t n = ::read(fd, buf, sizeof(buf));
                bool drop = false;
                if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) drop = true;
                if (n > 0)
                {
                    std::string& pending = clients[fd];
                    pending.append(buf, static_cast<size_t>(n));
                    size_t nl;
                    while (!drop && (nl = pending.find('\n')) != std::string::npos)
                    {
                        std::string line = pending.substr(0, nl);
                        pending.erase(0, nl + 1);
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        const std::string reply = handler_(line) + "\n";
                        // A reply is a few dozen bytes, far under the socket buffer. If the client cannot take it, drop the client.
                        if (::write(fd, reply.data(), reply.size()) != static_cast<ssize_t>(reply.size())) drop = true;
                    }
                    if (pending.size() > kMaxLine) drop = true;
                }
                if (drop)
                {
                    ::close(fd);
                    clients.erase(fd);
                }
            }
        }
        for (const auto& c : clients) ::close(c.first);
    }

    bool ControlServer::request(const std::string& path, const std::string& command, std::string& reply, std::string& error, int timeoutMs)
    {
        sockaddr_un addr;
        if (!makeAddress(path, addr))
        {
            error = "control socket path is too long: " + path;
            return false;
        }
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
        {
            error = std::string("socket: ") + std::strerror(errno);
            return false;
        }
        ::signal(SIGPIPE, SIG_IGN);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            error = "cannot connect to " + path + ": " + std::strerror(errno);
            ::close(fd);
            return false;
        }
        const std::string line = command + "\n";
        if (::write(fd, line.data(), line.size()) != static_cast<ssize_t>(line.size()))
        {
            error = std::string("write: ") + std::strerror(errno);
            ::close(fd);
            return false;
        }
        reply.clear();
        while (reply.find('\n') == std::string::npos)
        {
            pollfd p = {fd, POLLIN, 0};
            if (::poll(&p, 1, timeoutMs) <= 0)
            {
                error = "no reply from " + path;
                ::close(fd);
                return false;
            }
            char buf[512];
            const ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n <= 0)
            {
                error = "connection closed before a reply";
                ::close(fd);
                return false;
            }
            reply.append(buf, static_cast<size_t>(n));
        }
        ::close(fd);
        reply.erase(reply.find('\n'));
        return true;
    }
}
