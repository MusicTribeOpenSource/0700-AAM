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

// Integration test: kick-gesture's LoopbackIpcSink -> the app's GestureIpcServer, over real loopback TCP with the real
// JUCE InterprocessConnection framing. A frame the sink builds wrongly, a handshake the server rejects, or an envelope
// the decoder cannot read shows up here as a missing signal.

#include "contracts/gesture/gesture_signals.h"
#include "gesture/ipc/gesture_ipc_server.h"
#include "kick_gesture/sink.h"

#include <juce_events/juce_events.h>

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
    int failures = 0;

    void check(bool ok, const std::string& what)
    {
        std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
        if (!ok) ++failures;
    }

    // Pumps the JUCE message loop until `done` or the time ran out.
    bool pumpUntil(const std::function<bool()>& done, int timeoutMs = 3000)
    {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!done() && std::chrono::steady_clock::now() < end)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        return done();
    }

    void pumpFor(int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }

    // A free loopback port: bind to 0, read it, close it.
    uint16_t freePort()
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a));
        socklen_t len = sizeof(a);
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&a), &len);
        const uint16_t port = ntohs(a.sin_port);
        ::close(fd);
        return port;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::vector<contracts::GestureEvent> events;
    std::vector<contracts::GestureServiceStatus> statuses;
    contracts::GestureSignals::gestureReceived.connect([&](const contracts::GestureEvent& e) { events.push_back(e); });
    contracts::GestureSignals::serviceStatusChanged.connect([&](const contracts::GestureServiceStatus& s) { statuses.push_back(s); });

    const uint16_t port = freePort();
    kick_gesture::GestureIpcServer server(port);
    server.start();
    pumpFor(100); // let the listener thread start

    {
        kickgest::LoopbackIpcSink sink("127.0.0.1", port, 20, 80);
        sink.send(R"({"type":"swipe_left","hand":"right","confidence":0.97,"t_ms":1000})");
        check(sink.connected(), "the sink connects to the app's server");

        check(pumpUntil([&] { return !events.empty(); }), "a swipe event arrives as a gestureReceived signal");
        if (!events.empty())
        {
            check(events[0].kind == contracts::GestureKind::SwipeLeft, "the gesture kind is swipe_left");
            check(events[0].hand == contracts::GestureHand::Right, "the hand is right");
            check(events[0].confidence > 0.96f && events[0].confidence < 0.98f, "the confidence survives");
            check(events[0].tMs == 1000, "t_ms survives");
            check(!events[0].hasValue, "a swipe carries no value");
        }
        check(!statuses.empty() && statuses[0].connected, "registering fires serviceStatusChanged with connected true");

        sink.send(R"({"type":"pinch_change","value":0.412,"hand":"left","confidence":0.931,"t_ms":2000})");
        check(pumpUntil([&] { return events.size() >= 2; }), "a pinch_change arrives");
        if (events.size() >= 2)
        {
            check(events[1].kind == contracts::GestureKind::PinchChange && events[1].hasValue, "it carries a value");
            check(events[1].value > 0.41f && events[1].value < 0.42f, "the value survives");
        }

        const size_t statusesBefore = statuses.size();
        sink.send(R"({"type":"status","state":"degraded","reason":"cpu_soft","rate_hz":6.0,"t_ms":3000})");
        check(pumpUntil([&] { return statuses.size() > statusesBefore; }), "a status line fires serviceStatusChanged");
        if (statuses.size() > statusesBefore)
        {
            check(statuses.back().connected && statuses.back().state == "degraded" && statuses.back().reason == "cpu_soft", "state and reason survive");
            check(statuses.back().rateHz > 5.9 && statuses.back().rateHz < 6.1, "rate_hz survives");
        }

        // Things that must be ignored without upsetting the server: an unknown gesture and a line that is not an object.
        const size_t eventsBefore = events.size();
        sink.send(R"({"type":"wave","hand":"left","confidence":0.9,"t_ms":4000})");
        sink.send("[1,2,3]");
        sink.send(R"({"type":"fist","hand":"left","confidence":0.99,"t_ms":5000})");
        check(pumpUntil([&] { return events.size() > eventsBefore; }), "events after junk still arrive");
        check(events.size() == eventsBefore + 1 && events.back().kind == contracts::GestureKind::Fist, "the unknown gesture and the array were dropped");
    }

    // The sink went out of scope, so the connection closed.
    check(pumpUntil([&] { return !statuses.empty() && !statuses.back().connected; }), "closing the connection fires serviceStatusChanged with connected false");

    // A second service connects after the first left.
    {
        kickgest::LoopbackIpcSink again("127.0.0.1", port, 20, 80);
        const size_t before = events.size();
        again.send(R"({"type":"palm_hold","hand":"right","confidence":0.9,"t_ms":6000})");
        check(pumpUntil([&] { return events.size() > before; }), "a reconnecting service is accepted again");
    }

    server.stop();
    std::printf("%s\n", failures == 0 ? "ALL PASSED" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
