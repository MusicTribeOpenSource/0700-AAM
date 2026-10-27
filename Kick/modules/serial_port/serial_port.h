/*
 BEGIN_JUCE_MODULE_DECLARATION
  ID: serial_port
  vendor: KICK
  version: 0.1.0
  name: KICK native serial transport
  description: POSIX serial I/O and bounded control-surface frame assembly
  license: AGPL-3.0-or-later
  dependencies: juce_core
 END_JUCE_MODULE_DECLARATION
*/
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace kick::serial {
class Port {
public:
    ~Port() { close(); }
    juce::Result open(const juce::String& path, uint32_t baud = 230400);
    void close();
    bool isOpen() const;
    int read(void*, size_t); // nonblocking; 0 means no bytes, -1 means I/O failure
    bool write(const void*, size_t, int timeoutMs = 100);
private:
    mutable std::mutex mutex;
    int descriptor = -1;
};
struct Message { uint8_t type = 0, direction = 0, command = 0; std::vector<int> data; };
uint8_t crc(const uint8_t*, size_t) noexcept;
std::vector<uint8_t> encode(const Message&);
class Decoder {
public:
    void feed(const uint8_t*, size_t, const std::function<void(const Message&)>&);
    void reset() noexcept { used = 0; }
private:
    std::array<uint8_t, 261> bytes{};
    size_t used = 0;
};
}
