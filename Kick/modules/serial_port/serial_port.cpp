// SPDX-License-Identifier: AGPL-3.0-or-later
#include "serial_port.h"
#include <cerrno>
#include <chrono>
#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif
namespace kick::serial {
juce::Result Port::open(const juce::String& path, uint32_t baud) {
    close();
#ifdef _WIN32
    juce::ignoreUnused(path, baud); return juce::Result::fail("Native serial transport requires Linux or macOS");
#else
    const std::lock_guard<std::mutex> guard(mutex);
    speed_t speed;
    switch (baud) {
        case 9600: speed = B9600; break;
        case 115200: speed = B115200; break;
#ifdef B230400
        case 230400: speed = B230400; break;
#endif
        default: return juce::Result::fail("Unsupported serial baud rate");
    }
    const int fd = ::open(path.toRawUTF8(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return juce::Result::fail("Cannot open serial device: " + path + ": " + juce::String(std::strerror(errno)));
    termios settings{};
    if (tcgetattr(fd, &settings) != 0) { ::close(fd); return juce::Result::fail("Cannot read serial settings"); }
    cfmakeraw(&settings);
    settings.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
#ifdef CRTSCTS
    settings.c_cflag &= ~CRTSCTS;
#endif
    settings.c_cflag |= CS8 | CLOCAL | CREAD;
    settings.c_cc[VMIN] = 0; settings.c_cc[VTIME] = 0;
    cfsetispeed(&settings, speed); cfsetospeed(&settings, speed);
    if (tcsetattr(fd, TCSANOW, &settings) != 0) { ::close(fd); return juce::Result::fail("Cannot configure serial device"); }
    descriptor = fd; return juce::Result::ok();
#endif
}
void Port::close() {
    const std::lock_guard<std::mutex> guard(mutex);
#ifndef _WIN32
    if (descriptor >= 0) ::close(descriptor);
#endif
    descriptor = -1;
}
bool Port::isOpen() const { const std::lock_guard<std::mutex> guard(mutex); return descriptor >= 0; }
int Port::read(void* target, size_t capacity) {
    const std::lock_guard<std::mutex> guard(mutex);
#ifndef _WIN32
    if (descriptor < 0) return -1;
    pollfd status{descriptor, POLLIN, 0};
    if (::poll(&status, 1, 0) > 0 && (status.revents & (POLLERR | POLLHUP | POLLNVAL))) return -1;
    ssize_t count;
    do { count = ::read(descriptor, target, capacity); } while (count < 0 && errno == EINTR);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    return static_cast<int>(count);
#else
    juce::ignoreUnused(target, capacity); return -1;
#endif
}
bool Port::write(const void* source, size_t size, int timeoutMs) {
    const std::lock_guard<std::mutex> guard(mutex);
#ifndef _WIN32
    if (descriptor < 0 || timeoutMs < 0) return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    const auto* bytes = static_cast<const uint8_t*>(source);
    for (size_t sent = 0; sent < size;) {
        const auto count = ::write(descriptor, bytes + sent, size - sent);
        if (count > 0) sent += static_cast<size_t>(count);
        else if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) return false;
        if (sent == size) return true;
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) return false;
        pollfd fd{descriptor, POLLOUT, 0}; const auto ready = ::poll(&fd, 1, static_cast<int>(remaining));
        if (ready == 0 || (ready < 0 && errno != EINTR) || (fd.revents & (POLLERR | POLLHUP | POLLNVAL))) return false;
    }
    return true;
#else
    juce::ignoreUnused(source, size, timeoutMs); return false;
#endif
}
uint8_t crc(const uint8_t* bytes, size_t size) noexcept {
    uint8_t result = 0xff;
    for (size_t i = 0; i < size; ++i) {
        result ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) result = static_cast<uint8_t>((result & 0x80) ? (result << 1) ^ 0x31 : result << 1);
    }
    return result;
}
std::vector<uint8_t> encode(const Message& message) {
    if (message.data.size() > 254 || message.type > 7 || message.direction > 1) return {};
    std::vector<uint8_t> bytes{'M', 'T', message.type, message.direction, message.command, static_cast<uint8_t>(message.data.size() + 1)};
    for (const auto value : message.data) { if (value < 0 || value > 255) return {}; bytes.push_back(static_cast<uint8_t>(value)); }
    bytes.push_back(crc(bytes.data(), bytes.size())); return bytes;
}
void Decoder::feed(const uint8_t* input, size_t size, const std::function<void(const Message&)>& receive) {
    for (size_t i = 0; i < size; ++i) {
        const auto byte = input[i];
        if (used == 0) { if (byte == 'M') bytes[used++] = byte; continue; }
        if (used == 1) {
            if (byte == 'T') bytes[used++] = byte;
            else if (byte != 'M') used = 0;
            continue;
        }
        bytes[used++] = byte;
        if ((used == 3 && byte > 7) || (used == 4 && byte > 1) || (used == 6 && byte == 0)) {
            used = byte == 'M' ? 1 : 0; continue;
        }
        if (used < 7 || used != size_t(6) + bytes[5]) continue;
        if (crc(bytes.data(), used - 1) == byte) {
            Message message{bytes[2], bytes[3], bytes[4], {}};
            for (size_t j = 6; j < used - 1; ++j) message.data.push_back(bytes[j]);
            used = 0; receive(message);
        } else used = byte == 'M' ? 1 : 0;
    }
}
}
