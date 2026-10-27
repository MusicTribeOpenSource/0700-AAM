// SPDX-License-Identifier: AGPL-3.0-or-later
#include <kick_clap/Discovery.h>
#include "Internal.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <set>
#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace kick::clap {
namespace fs = std::filesystem;
namespace {
constexpr size_t maxWireBytes = 4 * 1024 * 1024;
constexpr const char* magic = "KICKCLAP1\n";
void number(std::string& out, uint32_t n) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((n >> (8 * i)) & 255)); }
void field(std::string& out, const std::string& s) { number(out, static_cast<uint32_t>(s.size())); out += s; }
struct Reader {
    const std::string& source;
    size_t position;
    uint32_t number() {
        if (source.size() - position < 4) throw std::runtime_error("Truncated scanner output");
        uint32_t n = 0;
        for (int i = 0; i < 4; ++i) n |= static_cast<uint32_t>(static_cast<uint8_t>(source[position++])) << (8 * i);
        return n;
    }
    std::string field() {
        const auto n = number();
        if (n > maxWireBytes || n > source.size() - position) throw std::runtime_error("Invalid scanner field");
        auto s = source.substr(position, n); position += n; return s;
    }
};
}
std::vector<fs::path> defaultSearchPaths() {
    std::vector<fs::path> paths;
    if (const auto* custom = std::getenv("CLAP_PATH")) {
        std::string list(custom);
#ifdef _WIN32
        const char separator = ';';
#else
        const char separator = ':';
#endif
        for (size_t begin = 0, end; begin < list.size(); begin = end + 1) {
            end = list.find(separator, begin); if (end == std::string::npos) end = list.size();
            if (end > begin) paths.emplace_back(list.substr(begin, end - begin));
        }
    }
    if (const auto* home = std::getenv("HOME")) {
        paths.emplace_back(fs::path(home) / ".clap");
#ifdef __APPLE__
        paths.emplace_back(fs::path(home) / "Library/Audio/Plug-Ins/CLAP");
#endif
    }
#ifdef __APPLE__
    paths.emplace_back("/Library/Audio/Plug-Ins/CLAP");
#elif defined(_WIN32)
    if (const auto* common = std::getenv("COMMONPROGRAMFILES")) paths.emplace_back(fs::path(common) / "CLAP");
#else
    paths.emplace_back("/usr/lib/clap"); paths.emplace_back("/usr/local/lib/clap");
    paths.emplace_back("/opt/kick/plugins/clap");
#endif
    return paths;
}
std::vector<fs::path> findBinaries(const std::vector<fs::path>& roots) {
    std::set<fs::path> found;
    for (const auto& root : roots) {
        std::error_code ec;
        if (!fs::exists(root, ec)) continue;
        if (root.extension() == ".clap") { found.insert(fs::canonical(root, ec)); continue; }
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
        for (; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (it.depth() > 8) { it.disable_recursion_pending(); continue; }
            if (it->path().extension() == ".clap") {
                auto path = fs::canonical(it->path(), ec);
                if (!ec) found.insert(path);
                if (it->is_directory(ec)) it.disable_recursion_pending();
            }
            if (found.size() >= 8192) break;
        }
    }
    found.erase(fs::path{});
    return {found.begin(), found.end()};
}
ScanResult inspect(const fs::path& binary) {
    ScanResult result;
    try {
        auto library = detail::openLibrary(binary.string());
        const auto* factory = library->factory();
        if (!factory) throw std::runtime_error("No plugin factory");
        const auto count = factory->get_plugin_count(factory);
        if (!count || count > 4096) throw std::runtime_error("Invalid factory descriptor count");
        std::set<std::string> ids;
        for (uint32_t i = 0; i < count; ++i) {
            const auto* d = factory->get_plugin_descriptor(factory, i);
            if (!d || !d->id || !d->id[0] || !d->name || !d->name[0] || !clap_version_is_compatible(d->clap_version))
                throw std::runtime_error("Invalid or incompatible plugin descriptor");
            if (!ids.insert(d->id).second) throw std::runtime_error("Duplicate plugin ID in factory");
            result.plugins.push_back(detail::describe(library->path, *d));
        }
    } catch (const std::exception& e) { result.plugins.clear(); result.failures.push_back({binary.string(), e.what()}); }
      catch (...) { result.plugins.clear(); result.failures.push_back({binary.string(), "Plugin threw during discovery"}); }
    return result;
}
std::string encodeScan(const ScanResult& result) {
    std::string out = magic;
    number(out, static_cast<uint32_t>(result.plugins.size()));
    for (const auto& p : result.plugins) {
        field(out, p.path); field(out, p.id); field(out, p.name); field(out, p.vendor); field(out, p.version); field(out, p.description);
        number(out, static_cast<uint32_t>(p.features.size()));
        for (const auto& f : p.features) field(out, f);
    }
    number(out, static_cast<uint32_t>(result.failures.size()));
    for (const auto& f : result.failures) { field(out, f.path); field(out, f.message); }
    if (out.size() > maxWireBytes) throw std::runtime_error("Scanner result exceeds size limit");
    return out;
}
bool decodeScan(const std::string& source, ScanResult& result) {
    if (source.size() > maxWireBytes) return false;
    const auto start = source.rfind(magic);
    if (start == std::string::npos) return false;
    ScanResult decoded;
    try {
        Reader reader{source, start + std::strlen(magic)};
        auto count = reader.number(); if (count > 4096) return false;
        for (uint32_t i = 0; i < count; ++i) {
            Descriptor p;
            p.path = reader.field(); p.id = reader.field(); p.name = reader.field(); p.vendor = reader.field();
            p.version = reader.field(); p.description = reader.field();
            auto features = reader.number(); if (features > 128) return false;
            for (uint32_t j = 0; j < features; ++j) p.features.push_back(reader.field());
            if (p.path.empty() || p.id.empty() || p.name.empty()) return false;
            decoded.plugins.push_back(std::move(p));
        }
        count = reader.number(); if (count > 8192) return false;
        for (uint32_t i = 0; i < count; ++i) decoded.failures.push_back({reader.field(), reader.field()});
        if (reader.position != source.size()) return false;
        result = std::move(decoded); return true;
    } catch (...) { return false; }
}
ScanResult scan(const std::vector<fs::path>& roots, const fs::path& helper, uint32_t timeoutMs, const std::atomic<bool>* cancelled) {
    ScanResult result;
    if (timeoutMs == 0 || timeoutMs > 60000) { result.failures.push_back({helper.string(), "Invalid scanner timeout"}); return result; }
#ifdef _WIN32
    (void)roots; (void)cancelled;
    result.failures.push_back({helper.string(), "Isolated scanning currently supports Linux and macOS"});
#else
    for (const auto& path : findBinaries(roots)) {
        if (cancelled && cancelled->load()) break;
        int pipes[2];
        if (::pipe(pipes) != 0) { result.failures.push_back({path.string(), "Cannot create scanner pipe"}); continue; }
        ::fcntl(pipes[0], F_SETFD, FD_CLOEXEC); ::fcntl(pipes[1], F_SETFD, FD_CLOEXEC);
        posix_spawn_file_actions_t actions; posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, pipes[0]);
        posix_spawn_file_actions_addclose(&actions, pipes[1]);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawnattr_t attributes; posix_spawnattr_init(&attributes);
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
        posix_spawnattr_setpgroup(&attributes, 0);
        auto program = helper.string(); auto binary = path.string();
        char worker[] = "--worker";
        char* arguments[]{program.data(), worker, binary.data(), nullptr};
        pid_t pid = 0;
        const auto spawned = posix_spawn(&pid, program.c_str(), &actions, &attributes, arguments, environ);
        posix_spawnattr_destroy(&attributes); posix_spawn_file_actions_destroy(&actions);
        ::close(pipes[1]);
        if (spawned != 0) { ::close(pipes[0]); result.failures.push_back({path.string(), "Cannot launch kick-clap-scan helper"}); continue; }
        ::fcntl(pipes[0], F_SETFL, ::fcntl(pipes[0], F_GETFL) | O_NONBLOCK);
        std::string wire; int status = 0; bool done = false, timedOut = false, oversized = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!done) {
            char bytes[4096]; ssize_t n;
            while ((n = ::read(pipes[0], bytes, sizeof(bytes))) > 0) {
                if (wire.size() + static_cast<size_t>(n) > maxWireBytes) { oversized = true; break; }
                wire.append(bytes, static_cast<size_t>(n));
            }
            const auto waited = ::waitpid(pid, &status, WNOHANG);
            if (waited == pid) { done = true; break; }
            if (waited < 0 && errno != EINTR) { done = true; status = -1; break; }
            if (oversized || std::chrono::steady_clock::now() >= deadline || (cancelled && cancelled->load())) {
                timedOut = !oversized;
                ::kill(-pid, SIGKILL);
                while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
                done = true; break;
            }
            pollfd fd{pipes[0], POLLIN, 0}; ::poll(&fd, 1, 10);
        }
        char tail[4096]; ssize_t n;
        while (!oversized && (n = ::read(pipes[0], tail, sizeof(tail))) > 0) {
            if (wire.size() + static_cast<size_t>(n) > maxWireBytes) { oversized = true; break; }
            wire.append(tail, static_cast<size_t>(n));
        }
        ::close(pipes[0]);
        ScanResult single;
        if (timedOut) result.failures.push_back({binary, cancelled && cancelled->load() ? "Scan cancelled" : "Plugin scan timed out"});
        else if (oversized) result.failures.push_back({binary, "Plugin scanner output exceeded size limit"});
        else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) result.failures.push_back({binary, "Plugin crashed or failed during scan"});
        else if (!decodeScan(wire, single)) result.failures.push_back({binary, "Invalid scanner response"});
        else {
            result.plugins.insert(result.plugins.end(), single.plugins.begin(), single.plugins.end());
            result.failures.insert(result.failures.end(), single.failures.begin(), single.failures.end());
        }
    }
#endif
    return result;
}
} // namespace kick::clap
