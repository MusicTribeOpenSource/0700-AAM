// SPDX-License-Identifier: AGPL-3.0-or-later
#include <serial_port/serial_port.h>
#include <PktMgr/PktMgr.hpp>
#include <iostream>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
static void check(bool ok, const char* why) { if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
int main() {
    using namespace kick::serial;
    Message message{4, 0, 0, {1, 1, 127}};
    const auto packet = encode(message);
    check(packet.size() == 10 && packet[5] == 4, "wire length includes CRC once");
    std::array<uint8_t, 255> rx{}, tx{}; CPktMgr legacy(CPktMgr::EN_COM_CHANNEL_USB, rx.data(), rx.size(), tx.data(), tx.size(), 60);
    legacy.Init();
    CPktMgr::HWMsg_t hw; hw.pktType = CPktMgr::EN_PKT_TYPE_KNOBS; hw.data = message.data;
    std::vector<char> original; legacy.BuildAndTransmitPacket(hw, original);
    check(std::vector<uint8_t>(original.begin(), original.end()) == packet, "new encoder matches released protocol and CRC");
    Decoder decoder; int calls = 0;
    auto receive = [&](const Message& decoded) { ++calls; check(decoded.type == 4 && decoded.direction == 0 && decoded.data == message.data, "decoded fields"); };
    for (const auto byte : packet) decoder.feed(&byte, 1, receive);
    check(calls == 1, "packet survives arbitrary fragmentation");
    auto damaged = packet; damaged.back() ^= 1;
    decoder.feed(damaged.data(), damaged.size(), receive);
    check(calls == 1, "bad CRC rejected");
    const uint8_t noise[]{0xff, 'M', 'M', 'T', 255, 0, 'M', 'T', 1, 0, 0, 0};
    decoder.feed(noise, sizeof(noise), receive);
    decoder.feed(packet.data(), packet.size(), receive);
    decoder.feed(packet.data(), packet.size(), receive);
    check(calls == 3, "noise, malformed lengths and coalesced frames recover");
    Message maximum{0, 1, 0, std::vector<int>(254, 255)};
    const auto large = encode(maximum); check(large.size() == 261, "maximum frame is bounded");
    decoder.feed(large.data(), large.size(), [&](const Message& decoded) { check(decoded.data == maximum.data, "maximum frame recall"); });
    maximum.data.push_back(0); check(encode(maximum).empty(), "reject oversized outgoing frame");
#ifndef _WIN32
    const auto master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    check(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0, "create pseudo terminal");
    const auto slave = juce::String(ptsname(master));
    Port port; check(port.open(slave, 115200).wasOk(), "open/configure real terminal backend");
    check(::write(master, packet.data(), 3) == 3, "fragmented incoming bytes");
    std::array<uint8_t, 64> bytes{};
    int count = 0;
    for (int i = 0; i < 100 && !count; ++i) { count = port.read(bytes.data(), bytes.size()); if (!count) juce::Thread::sleep(1); }
    check(count == 3 && std::equal(bytes.begin(), bytes.begin() + 3, packet.begin()), "terminal raw mode preserves bytes");
    check(port.write(packet.data(), packet.size()), "write real terminal");
    count = 0;
    for (int i = 0; i < 100 && count <= 0; ++i) { count = static_cast<int>(::read(master, bytes.data(), bytes.size())); if (count <= 0) juce::Thread::sleep(1); }
    check(count == static_cast<int>(packet.size()) && std::equal(packet.begin(), packet.end(), bytes.begin()), "terminal output matches wire packet");
    port.close(); check(!port.isOpen() && port.read(bytes.data(), bytes.size()) == -1 && !port.write(packet.data(), packet.size()), "closed port fails explicitly");
    check(port.open(slave, 230400).wasOk(), "port reconnect at board default baud");
    ::close(master);
    check(port.read(bytes.data(), bytes.size()) == -1, "disconnected terminal is detected");
    port.close();
#endif
    std::cout << "PASS: wire compatibility, split/coalesced frames, CRC/noise/length checks, native terminal I/O and reconnect\n";
}
