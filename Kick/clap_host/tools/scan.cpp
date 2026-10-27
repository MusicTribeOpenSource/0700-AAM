// SPDX-License-Identifier: AGPL-3.0-or-later
#include <kick_clap/Discovery.h>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "--worker") {
            const auto result = kick::clap::inspect(argv[2]);
            const auto wire = kick::clap::encodeScan(result);
            std::cout.write(wire.data(), static_cast<std::streamsize>(wire.size()));
            return 0;
        }
        std::vector<std::filesystem::path> paths;
        for (int i = 1; i < argc; ++i) paths.emplace_back(argv[i]);
        if (paths.empty()) paths = kick::clap::defaultSearchPaths();
        std::filesystem::path executable(argv[0]);
        if (!executable.has_parent_path()) {
            if (const auto* searchPath = std::getenv("PATH")) {
                const std::string list(searchPath);
#ifdef _WIN32
                constexpr char separator = ';';
#else
                constexpr char separator = ':';
#endif
                for (size_t start = 0, end; start <= list.size(); start = end + 1) {
                    end = list.find(separator, start); if (end == std::string::npos) end = list.size();
                    auto candidate = std::filesystem::path(list.substr(start, end - start)) / executable;
                    if (std::filesystem::is_regular_file(candidate)) { executable = candidate; break; }
                    if (end == list.size()) break;
                }
            }
        }
        executable = std::filesystem::canonical(executable);
        const auto result = kick::clap::scan(paths, executable);
        for (const auto& p : result.plugins)
            std::cout << p.name << " | " << p.id << " | " << p.vendor << " | " << p.path << '\n';
        for (const auto& failure : result.failures) std::cerr << failure.path << ": " << failure.message << '\n';
        return result.failures.empty() ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
