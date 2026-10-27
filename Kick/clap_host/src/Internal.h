// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <kick_clap/Host.h>
#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace kick::clap::detail {
inline std::string safeString(const char* text) { return text ? text : ""; }
inline std::filesystem::path executablePath(const std::filesystem::path& path) {
#ifdef __APPLE__
    if (std::filesystem::is_directory(path)) {
        const auto directory = path / "Contents/MacOS";
        const auto conventional = directory / path.stem();
        if (std::filesystem::is_regular_file(conventional)) return conventional;
        for (const auto& entry : std::filesystem::directory_iterator(directory))
            if (entry.is_regular_file()) return entry.path();
    }
#endif
    return path;
}
struct Library {
    void* handle = nullptr;
    const clap_plugin_entry_t* entry = nullptr;
    std::string path;
    explicit Library(const std::string& source) : path(source) {
        const auto binary = executablePath(source).string();
#ifdef _WIN32
        handle = LoadLibraryW(std::filesystem::path(binary).wstring().c_str());
        if (handle) entry = reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(static_cast<HMODULE>(handle), "clap_entry"));
#else
        handle = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle) entry = static_cast<const clap_plugin_entry_t*>(dlsym(handle, "clap_entry"));
#endif
        if (!handle || !entry || !clap_version_is_compatible(entry->clap_version) ||
            !entry->init || !entry->deinit || !entry->get_factory) {
            entry = nullptr;
            close();
            throw std::runtime_error("Cannot load a compatible native CLAP binary: " + source);
        }
        if (!entry->init(path.c_str())) {
            entry = nullptr;
            close();
            throw std::runtime_error("CLAP entry initialization failed: " + source);
        }
    }
    ~Library() { close(); }
    void close() {
        if (entry) entry->deinit();
        entry = nullptr;
        if (handle) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
        }
        handle = nullptr;
    }
    const clap_plugin_factory_t* factory() const {
        const auto* f = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        return f && f->get_plugin_count && f->get_plugin_descriptor && f->create_plugin ? f : nullptr;
    }
};
inline std::shared_ptr<Library> openLibrary(const std::string& source) {
    // entry.init/deinit are per binary, not per instance. Keep a strong reference
    // in every host and coordinate init when a second program uses that binary.
    static std::mutex mutex;
    static std::map<std::string, std::weak_ptr<Library>> libraries;
    std::lock_guard<std::mutex> guard(mutex);
    auto path = std::filesystem::canonical(source).string();
    if (auto existing = libraries[path].lock()) return existing;
    auto library = std::make_shared<Library>(path);
    libraries[path] = library;
    return library;
}
inline Descriptor describe(const std::string& path, const clap_plugin_descriptor_t& d) {
    Descriptor result{path, safeString(d.id), safeString(d.name), safeString(d.vendor),
                      safeString(d.version), safeString(d.description), {}};
    if (d.features)
        for (size_t i = 0; i < 128 && d.features[i]; ++i) result.features.emplace_back(d.features[i]);
    return result;
}

// Bounded Vyukov queue. Producers claim independent slots; the audio consumer
// never spins on an unfinished producer. No allocation, locks or reclamation.
template<class T, size_t Capacity> class Queue {
    struct Slot { std::atomic<size_t> sequence{0}; T value{}; };
    std::array<Slot, Capacity> slots;
    alignas(64) std::atomic<size_t> write{0};
    alignas(64) size_t read = 0;
public:
    Queue() { for (size_t i = 0; i < Capacity; ++i) slots[i].sequence.store(i); }
    bool push(const T& value) noexcept {
        auto position = write.load(std::memory_order_relaxed);
        for (;;) {
            auto& slot = slots[position % Capacity];
            const auto seq = slot.sequence.load(std::memory_order_acquire);
            const auto difference = static_cast<intptr_t>(seq - position);
            if (difference == 0) {
                if (write.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) {
                    slot.value = value;
                    slot.sequence.store(position + 1, std::memory_order_release);
                    return true;
                }
            } else if (difference < 0) return false;
            else position = write.load(std::memory_order_relaxed);
        }
    }
    bool pop(T& value) noexcept {
        auto& slot = slots[read % Capacity];
        if (slot.sequence.load(std::memory_order_acquire) != read + 1) return false;
        value = slot.value;
        slot.sequence.store(read + Capacity, std::memory_order_release);
        ++read;
        return true;
    }
};
} // namespace kick::clap::detail
