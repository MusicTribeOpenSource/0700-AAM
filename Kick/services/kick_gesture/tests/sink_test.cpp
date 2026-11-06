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
#include "kick_gesture/service_config.h"
#include "kick_gesture/sink.h"

#include "test_util.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

using namespace kickgest;

namespace
{
    int listenOn(const std::string& path)
    {
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un a{};
        a.sun_family = AF_UNIX;
        std::snprintf(a.sun_path, sizeof(a.sun_path), "%s", path.c_str());
        ::unlink(path.c_str());
        EXPECT_EQ(::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)), 0);
        EXPECT_EQ(::listen(fd, 4), 0);
        return fd;
    }

    std::string readAvailable(int fd)
    {
        std::string out;
        char buf[512];
        for (int i = 0; i < 50; ++i)
        {
            const ssize_t n = ::recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
            if (n > 0)
                out.append(buf, static_cast<size_t>(n));
            else if (!out.empty())
                break;
            else
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return out;
    }
}

TEST(UnixSocketSink, DeliversNewlineDelimitedLines)
{
    kicktest::TempDir dir;
    const std::string path = dir.file("ev.sock");
    const int srv = listenOn(path);
    UnixSocketSink sink(path, 10);
    sink.send("{\"a\":1}");
    sink.send("{\"b\":2}");
    const int conn = ::accept(srv, nullptr, nullptr);
    ASSERT_GE(conn, 0);
    EXPECT_EQ(readAvailable(conn), "{\"a\":1}\n{\"b\":2}\n");
    EXPECT_TRUE(sink.connected());
    EXPECT_EQ(sink.dropped(), 0u);
    ::close(conn);
    ::close(srv);
    ::unlink(path.c_str());
}

TEST(UnixSocketSink, DropsWhileThereIsNoListenerAndConnectsLater)
{
    kicktest::TempDir dir;
    const std::string path = dir.file("later.sock");
    UnixSocketSink sink(path, 10);
    sink.send("lost1");
    sink.send("lost2");
    EXPECT_EQ(sink.dropped(), 2u);
    EXPECT_FALSE(sink.connected());

    const int srv = listenOn(path);
    std::this_thread::sleep_for(std::chrono::milliseconds(30)); // past the reconnect delay
    sink.send("kept");
    const int conn = ::accept(srv, nullptr, nullptr);
    ASSERT_GE(conn, 0);
    EXPECT_EQ(readAvailable(conn), "kept\n");
    ::close(conn);
    ::close(srv);
    ::unlink(path.c_str());
}

TEST(UnixSocketSink, ReconnectsAfterTheListenerRestarts)
{
    kicktest::TempDir dir;
    const std::string path = dir.file("restart.sock");
    int srv = listenOn(path);
    UnixSocketSink sink(path, 10);
    sink.send("one");
    int conn = ::accept(srv, nullptr, nullptr);
    ASSERT_GE(conn, 0);
    EXPECT_EQ(readAvailable(conn), "one\n");
    ::close(conn);
    ::close(srv);
    ::unlink(path.c_str());

    // The peer is gone: the write may succeed into the buffer once, then fails. Keep sending until the sink notices.
    for (int i = 0; i < 20 && sink.connected(); ++i)
    {
        sink.send("x");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_FALSE(sink.connected());

    srv = listenOn(path);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    sink.send("two");
    conn = ::accept(srv, nullptr, nullptr);
    ASSERT_GE(conn, 0);
    EXPECT_EQ(readAvailable(conn), "two\n");
    ::close(conn);
    ::close(srv);
    ::unlink(path.c_str());
}

TEST(UnixSocketSink, NeverBlocksOnAReaderThatStopsReading)
{
    kicktest::TempDir dir;
    const std::string path = dir.file("slow.sock");
    const int srv = listenOn(path);
    UnixSocketSink sink(path, 10);
    sink.send("first");
    const int conn = ::accept(srv, nullptr, nullptr);
    ASSERT_GE(conn, 0);
    // Nobody reads. Push far more than a socket buffer holds; every call must return at once.
    const std::string big(2000, 'x');
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 5000; ++i) sink.send(big);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(ms, 2000);
    EXPECT_GT(sink.dropped(), 0u);
    ::close(conn);
    ::close(srv);
    ::unlink(path.c_str());
}

TEST(UnixSocketSink, OverlongPathIsHandled)
{
    UnixSocketSink sink(std::string(300, 'a'), 10);
    sink.send("x");
    EXPECT_FALSE(sink.connected());
    EXPECT_EQ(sink.dropped(), 1u);
}

TEST(FileSink, AppendsAndReportsOpenFailure)
{
    kicktest::TempDir dir;
    FileSink s;
    std::string err;
    ASSERT_TRUE(s.open(dir.file("e.jsonl"), err)) << err;
    s.send("{\"x\":1}");
    s.send("{\"x\":2}");
    std::ifstream in(dir.file("e.jsonl"));
    std::stringstream ss;
    ss << in.rdbuf();
    EXPECT_EQ(ss.str(), "{\"x\":1}\n{\"x\":2}\n");

    FileSink bad;
    EXPECT_FALSE(bad.open("/nonexistent-dir/e.jsonl", err));
    EXPECT_FALSE(err.empty());
}


namespace
{
    // A TCP listener on a free loopback port. `port` is filled in.
    int listenTcp(uint16_t& port)
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = 0;
        EXPECT_EQ(::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)), 0);
        EXPECT_EQ(::listen(fd, 4), 0);
        socklen_t len = sizeof(a);
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&a), &len);
        port = ntohs(a.sin_port);
        return fd;
    }

    // Takes complete IPC frames off `fd` until `count` payloads arrived or the time ran out.
    std::vector<std::string> readFrames(int fd, size_t count)
    {
        std::string buf;
        std::vector<std::string> out;
        for (int i = 0; i < 100 && out.size() < count; ++i)
        {
            char tmp[1024];
            const ssize_t n = ::recv(fd, tmp, sizeof(tmp), MSG_DONTWAIT);
            if (n > 0) buf.append(tmp, static_cast<size_t>(n));
            else std::this_thread::sleep_for(std::chrono::milliseconds(10));
            while (buf.size() >= 8)
            {
                auto le32 = [&](size_t at) {
                    return static_cast<uint32_t>(static_cast<uint8_t>(buf[at])) | static_cast<uint32_t>(static_cast<uint8_t>(buf[at + 1])) << 8 |
                           static_cast<uint32_t>(static_cast<uint8_t>(buf[at + 2])) << 16 | static_cast<uint32_t>(static_cast<uint8_t>(buf[at + 3])) << 24;
                };
                EXPECT_EQ(le32(0), 0xf2b49e2cu);
                const size_t len = le32(4);
                if (buf.size() < 8 + len) break;
                out.push_back(buf.substr(8, len));
                buf.erase(0, 8 + len);
            }
        }
        return out;
    }
}

TEST(IpcFraming, MatchesJuceInterprocessConnection)
{
    // 0xf2b49e2c is JUCE's default magic number, both numbers little endian.
    const std::string f = ipcFrame("abc");
    ASSERT_EQ(f.size(), 11u);
    EXPECT_EQ(static_cast<uint8_t>(f[0]), 0x2cu);
    EXPECT_EQ(static_cast<uint8_t>(f[1]), 0x9eu);
    EXPECT_EQ(static_cast<uint8_t>(f[2]), 0xb4u);
    EXPECT_EQ(static_cast<uint8_t>(f[3]), 0xf2u);
    EXPECT_EQ(static_cast<uint8_t>(f[4]), 3u);
    EXPECT_EQ(static_cast<uint8_t>(f[5]), 0u);
    EXPECT_EQ(f.substr(8), "abc");
}

TEST(IpcPayload, WrapsAnEventAndAStatusLine)
{
    const std::string ev = ipcEventPayload("{\"type\":\"fist\",\"hand\":\"left\",\"confidence\":0.9,\"t_ms\":5}", 7);
    EXPECT_NE(ev.find("\"message_type\":500"), std::string::npos);
    EXPECT_NE(ev.find("\"message_id\":7"), std::string::npos);
    EXPECT_NE(ev.find("\"event\":{\"type\":\"fist\""), std::string::npos);
    const std::string st = ipcEventPayload("{\"type\":\"status\",\"state\":\"running\",\"reason\":\"\",\"rate_hz\":12.0}", 8);
    EXPECT_NE(st.find("\"message_type\":501"), std::string::npos);
    EXPECT_NE(ipcRegisterPayload().find("\"message_type\":1"), std::string::npos);
}

TEST(IpcEndpoint, ParsesOnlyDottedIpv4WithAPort)
{
    std::string host;
    uint16_t port = 0;
    EXPECT_TRUE(parseIpcEndpoint("127.0.0.1:50501", host, port));
    EXPECT_EQ(host, "127.0.0.1");
    EXPECT_EQ(port, 50501);
    for (const char* bad : {"", "127.0.0.1", "127.0.0.1:", ":50501", "localhost:50501", "127.0.0.1:0", "127.0.0.1:70000", "127.0.0.1:12ab", "127.0.0.1:123456"})
        EXPECT_FALSE(parseIpcEndpoint(bad, host, port)) << bad;
}

TEST(LoopbackIpcSink, RegistersFirstThenSendsEvents)
{
    uint16_t port = 0;
    const int srv = listenTcp(port);
    LoopbackIpcSink sink("127.0.0.1", port, 10, 40);
    sink.send("{\"type\":\"swipe_left\",\"hand\":\"right\",\"confidence\":0.95,\"t_ms\":1}");
    const int conn = ::accept(srv, nullptr, nullptr);
    ASSERT_GE(conn, 0);
    const auto frames = readFrames(conn, 2);
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_NE(frames[0].find("\"message_type\":1"), std::string::npos);
    EXPECT_NE(frames[0].find("\"connection_id\":500"), std::string::npos);
    EXPECT_NE(frames[1].find("\"message_type\":500"), std::string::npos);
    EXPECT_NE(frames[1].find("swipe_left"), std::string::npos);
    EXPECT_EQ(sink.dropped(), 0u);
    ::close(conn);
    ::close(srv);
}

TEST(LoopbackIpcSink, BacksOffWhileNothingListensAndConnectsLater)
{
    uint16_t port = 0;
    int srv = listenTcp(port);
    ::close(srv); // the port is now free and refuses connections
    LoopbackIpcSink sink("127.0.0.1", port, 20, 80);
    sink.send("a");
    EXPECT_EQ(sink.dropped(), 1u);
    EXPECT_EQ(sink.nextRetryDelayMs(), 40u);
    // Inside the delay no new attempt is made: the delay does not grow.
    sink.send("b");
    EXPECT_EQ(sink.dropped(), 2u);
    EXPECT_EQ(sink.nextRetryDelayMs(), 40u);
    // Past it, another failure doubles the delay, up to the cap.
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    sink.send("c");
    EXPECT_EQ(sink.nextRetryDelayMs(), 80u);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sink.send("d");
    EXPECT_EQ(sink.nextRetryDelayMs(), 80u);
    EXPECT_FALSE(sink.connected());
}

TEST(LoopbackIpcSink, ReconnectsAfterTheAppRestarts)
{
    uint16_t port = 0;
    int srv = listenTcp(port);
    LoopbackIpcSink sink("127.0.0.1", port, 10, 40);
    sink.send("one");
    int conn = ::accept(srv, nullptr, nullptr);
    ASSERT_GE(conn, 0);
    ASSERT_EQ(readFrames(conn, 2).size(), 2u);
    ::close(conn);
    ::close(srv);

    for (int i = 0; i < 40 && sink.connected(); ++i)
    {
        sink.send("x");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_FALSE(sink.connected());

    // The app comes back on the same port.
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    const int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    ASSERT_EQ(::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)), 0);
    ASSERT_EQ(::listen(fd, 4), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // past any backoff
    sink.send("two");
    conn = ::accept(fd, nullptr, nullptr);
    ASSERT_GE(conn, 0);
    const auto frames = readFrames(conn, 2);
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_NE(frames[0].find("\"message_type\":1"), std::string::npos);
    EXPECT_TRUE(sink.connected());
    ::close(conn);
    ::close(fd);
}

TEST(IpcEndpoint, TheServiceSettingTakesAnEndpointAndRejectsGarbage)
{
    ServiceConfig cfg;
    std::string err;
    EXPECT_TRUE(applyServiceSetting("events_ipc", "127.0.0.1:50501", cfg, err)) << err;
    EXPECT_EQ(cfg.eventsIpc, "127.0.0.1:50501");
    EXPECT_FALSE(applyServiceSetting("events_ipc", "localhost", cfg, err));
    EXPECT_EQ(cfg.eventsIpc, "127.0.0.1:50501"); // a rejected value leaves the setting alone
    EXPECT_TRUE(applyServiceSetting("events_ipc", "", cfg, err));
    EXPECT_TRUE(cfg.eventsIpc.empty());
}
