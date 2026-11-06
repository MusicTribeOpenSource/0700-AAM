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

#include "backoff.h"

#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace kickgest
{
    // Where event lines go. send() takes one JSON object without the newline and must never block the caller for long.
    class IEventSink
    {
    public:
        virtual ~IEventSink() = default;
        virtual void send(const std::string& line) = 0;
    };

    class StdoutSink final : public IEventSink
    {
    public:
        void send(const std::string& line) override;
    };

    // Appends lines to a file and flushes after each one.
    class FileSink final : public IEventSink
    {
    public:
        // False when the file cannot be opened.
        bool open(const std::string& path, std::string& error);
        void send(const std::string& line) override;

    private:
        std::ofstream out_;
    };

    // Keeps lines in memory. For tests.
    class MemorySink final : public IEventSink
    {
    public:
        void send(const std::string& line) override
        {
            std::lock_guard<std::mutex> g(m_);
            lines_.push_back(line);
        }
        std::vector<std::string> lines() const
        {
            std::lock_guard<std::mutex> g(m_);
            return lines_;
        }

    private:
        mutable std::mutex m_;
        std::vector<std::string> lines_;
    };

    // Newline-delimited JSON over a Unix stream socket the listener (the Kick app) owns. The service is the client:
    // it connects when the listener exists, writes without blocking, and drops a line it cannot write right away
    // instead of waiting, so a stuck app can never slow gesture recognition down. Gesture events are not worth queueing:
    // a swipe that arrives a second late is wrong. A dropped line is counted.
    class UnixSocketSink final : public IEventSink
    {
    public:
        explicit UnixSocketSink(std::string path, uint32_t reconnectMs = 1000) : path_(std::move(path)), reconnectMs_(reconnectMs) {}
        ~UnixSocketSink() override;
        void send(const std::string& line) override;

        bool connected() const { return fd_ >= 0; }
        uint64_t dropped() const { return dropped_; }

    private:
        bool tryConnect();
        void disconnect();

        std::string path_;
        uint32_t reconnectMs_;
        int fd_ = -1;
        uint64_t lastAttemptMs_ = 0;
        bool attempted_ = false;
        uint64_t dropped_ = 0;
    };

    // The frame the Kick app's IPC server expects (JUCE InterprocessConnection): the magic 0xf2b49e2c, the payload
    // length, both as 32 bit little endian, then the payload bytes. Exposed so a test can build and take frames apart.
    std::string ipcFrame(const std::string& payload);

    // The payload for one service line: the JSON envelope the app's IPC module uses, with the service's own line
    // nested under "event". A status line becomes message type 501, everything else type 500.
    std::string ipcEventPayload(const std::string& line, uint64_t messageId);

    // The REGISTER handshake the app's server wants before it accepts anything else (message_type 1).
    std::string ipcRegisterPayload();

    // Splits "host:port". False for anything else, and only dotted IPv4 hosts are accepted.
    bool parseIpcEndpoint(const std::string& text, std::string& host, uint16_t& port);

    // The same events as the Unix socket sink, sent to the Kick app's local IPC server over TCP on loopback.
    // Like the Unix sink it never blocks: a line it cannot write right away is dropped and counted. While the app
    // is not listening, connection attempts back off from 1 s up to 30 s, so a missing app costs next to nothing.
    class LoopbackIpcSink final : public IEventSink
    {
    public:
        // host must be a dotted IPv4 address. The app only listens on 127.0.0.1.
        LoopbackIpcSink(std::string host, uint16_t port, uint32_t initialBackoffMs = 1000, uint32_t maxBackoffMs = 30000)
            : host_(std::move(host)), port_(port), backoff_(initialBackoffMs, maxBackoffMs) {}
        ~LoopbackIpcSink() override;
        void send(const std::string& line) override;

        bool connected() const { return fd_ >= 0; }
        uint64_t dropped() const { return dropped_; }
        // Connection attempts that failed since the last success, for tests and the log.
        uint32_t nextRetryDelayMs() const { return backoff_.nextDelayMs(); }

    private:
        bool tryConnect(uint64_t nowMs);
        bool writeAll(const std::string& data);
        void drainReplies();
        void disconnect();

        std::string host_;
        uint16_t port_;
        kick_gesture::Backoff backoff_;
        int fd_ = -1;
        uint64_t dropped_ = 0;
        uint64_t nextId_ = 1;
    };
}
