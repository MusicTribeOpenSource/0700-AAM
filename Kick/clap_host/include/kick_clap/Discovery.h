// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "Host.h"
#include <atomic>
#include <filesystem>

namespace kick::clap {
struct ScanFailure { std::string path, message; };
struct ScanResult { std::vector<Descriptor> plugins; std::vector<ScanFailure> failures; };
std::vector<std::filesystem::path> defaultSearchPaths();
std::vector<std::filesystem::path> findBinaries(const std::vector<std::filesystem::path>& roots);
// Loads native code. Only call in the scanner worker or with a trusted fixture.
ScanResult inspect(const std::filesystem::path& binary);
// Each binary is inspected by a fresh helper process with timeout/crash isolation.
ScanResult scan(const std::vector<std::filesystem::path>& roots,
                const std::filesystem::path& scannerExecutable,
                uint32_t timeoutMilliseconds = 5000,
                const std::atomic<bool>* cancelled = nullptr);
std::string encodeScan(const ScanResult&);
bool decodeScan(const std::string&, ScanResult&);
} // namespace kick::clap
