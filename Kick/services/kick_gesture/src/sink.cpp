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
#include "kick_gesture/sink.h"

#include "kick_camera/types.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace kickgest
{
    void StdoutSink::send(const std::string& line)
    {
        std::fputs(line.c_str(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }

    bool FileSink::open(const std::string& path, std::string& error)
    {
        out_.open(path, std::ios::app);
        if (!out_)
        {
            error = "cannot open " + path + " for writing: " + std::strerror(errno);
            return false;
        }
        return true;
    }

    void FileSink::send(const std::string& line)
    {
        out_ << line << '\n';
        out_.flush();
    }

    UnixSocketSink::~UnixSocketSink() { disconnect(); }

    void UnixSocketSink::disconnect()
    {
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
    }

    bool UnixSocketSink::tryConnect()
    {
        const uint64_t now = kickcam::monotonicNs() / 1000000ull;
        if (attempted_ && now - lastAttemptMs_ < reconnectMs_) return false;
        attempted_ = true;
        lastAttemptMs_ = now;

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (path_.size() >= sizeof(addr.sun_path)) return false;
        std::memcpy(addr.sun_path, path_.c_str(), path_.size() + 1);
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return false;
        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
        const int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
        // A Unix connect to a listening socket completes at once; to a full backlog it would block, so connect first
        // and only then switch to non-blocking mode for the writes.
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            ::close(fd);
            return false;
        }
        ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        fd_ = fd;
        return true;
    }

    void UnixSocketSink::send(const std::string& line)
    {
        if (fd_ < 0 && !tryConnect())
        {
            ++dropped_;
            return;
        }
        const std::string data = line + "\n";
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        const ssize_t n = ::send(fd_, data.data(), data.size(), flags);
        if (n == static_cast<ssize_t>(data.size())) return;
        ++dropped_;
        if (n >= 0)
        {
            // A partial line would corrupt the stream for the reader. Close so the next line starts a new connection.
            disconnect();
            return;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK) disconnect();
    }

    namespace
    {
        constexpr uint32_t kIpcMagic = 0xf2b49e2cu;
        constexpr int kIpcConnectTimeoutMs = 200;
        constexpr int kMessageTypeRegister = 1;
        constexpr int kMessageTypeEvent = 500;
        constexpr int kMessageTypeStatus = 501;
        constexpr int kTransactionTypeEvent = 30;
        constexpr int kConnectionId = 500;

        void putLe32(std::string& out, uint32_t v)
        {
            for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((v >> (8 * i)) & 0xffu));
        }
    }

    std::string ipcFrame(const std::string& payload)
    {
        std::string out;
        out.reserve(8 + payload.size());
        putLe32(out, kIpcMagic);
        putLe32(out, static_cast<uint32_t>(payload.size()));
        out += payload;
        return out;
    }

    std::string ipcEventPayload(const std::string& line, uint64_t messageId)
    {
        const bool status = line.find("\"type\":\"status\"") != std::string::npos;
        return "{\"message_id\":" + std::to_string(messageId) + ",\"originator\":" + std::to_string(kConnectionId) +
               ",\"message_type\":" + std::to_string(status ? kMessageTypeStatus : kMessageTypeEvent) +
               ",\"transaction_type\":" + std::to_string(kTransactionTypeEvent) + ",\"event\":" + line + "}";
    }

    std::string ipcRegisterPayload()
    {
        return "{\"message_type\":" + std::to_string(kMessageTypeRegister) + ",\"connection_id\":" + std::to_string(kConnectionId) +
               ",\"uuid\":\"\",\"connection_name\":\"kick-gesture\"}";
    }

    bool parseIpcEndpoint(const std::string& text, std::string& host, uint16_t& port)
    {
        const size_t colon = text.rfind(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= text.size()) return false;
        const std::string h = text.substr(0, colon);
        const std::string p = text.substr(colon + 1);
        in_addr probe{};
        if (::inet_pton(AF_INET, h.c_str(), &probe) != 1) return false;
        if (p.size() > 5 || p.find_first_not_of("0123456789") != std::string::npos) return false;
        const long n = std::strtol(p.c_str(), nullptr, 10);
        if (n < 1 || n > 65535) return false;
        host = h;
        port = static_cast<uint16_t>(n);
        return true;
    }

    LoopbackIpcSink::~LoopbackIpcSink() { disconnect(); }

    void LoopbackIpcSink::disconnect()
    {
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
    }

    bool LoopbackIpcSink::writeAll(const std::string& data)
    {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        const ssize_t n = ::send(fd_, data.data(), data.size(), flags);
        return n == static_cast<ssize_t>(data.size());
    }

    void LoopbackIpcSink::drainReplies()
    {
        // The app answers the handshake. Nothing in it is needed, but an unread socket would fill up.
        char buf[256];
        while (fd_ >= 0)
        {
            const ssize_t n = ::recv(fd_, buf, sizeof(buf), MSG_DONTWAIT);
            if (n > 0) continue;
            if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) disconnect();
            return;
        }
    }

    bool LoopbackIpcSink::tryConnect(uint64_t nowMs)
    {
        if (!backoff_.due(nowMs)) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1)
        {
            backoff_.failure(nowMs);
            return false;
        }
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            backoff_.failure(nowMs);
            return false;
        }
        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
        const int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
        const int nodelay = 1;
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
        ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

        // Loopback connects finish at once or are refused at once. The short poll covers a busy listener.
        int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (rc != 0 && errno == EINPROGRESS)
        {
            pollfd p{fd, POLLOUT, 0};
            rc = -1;
            if (::poll(&p, 1, kIpcConnectTimeoutMs) == 1)
            {
                int soError = 0;
                socklen_t len = sizeof(soError);
                if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soError, &len) == 0 && soError == 0) rc = 0;
            }
        }
        if (rc != 0)
        {
            ::close(fd);
            backoff_.failure(nowMs);
            return false;
        }
        fd_ = fd;
        if (!writeAll(ipcFrame(ipcRegisterPayload())))
        {
            disconnect();
            backoff_.failure(nowMs);
            return false;
        }
        backoff_.success();
        return true;
    }

    void LoopbackIpcSink::send(const std::string& line)
    {
        if (fd_ >= 0) drainReplies();
        if (fd_ < 0 && !tryConnect(kickcam::monotonicNs() / 1000000ull))
        {
            ++dropped_;
            return;
        }
        if (writeAll(ipcFrame(ipcEventPayload(line, nextId_++)))) return;
        // A partial frame would corrupt the stream, so any short write closes the connection. The next line starts a new one.
        ++dropped_;
        disconnect();
    }
}
