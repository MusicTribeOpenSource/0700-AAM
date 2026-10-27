// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Benchmark.h"
#include <iostream>
int main(int argc, char** argv) {
    using namespace kick::clap::bench;
    try {
        const auto config = parse(argc, argv);
        if (config.help) { std::cout << usage(); return 0; }
        if (config.audio || config.listDevices) throw std::runtime_error("Use kick-clap-device-bench for audio-device measurements");
        Workload workload(config);
        const bool loaded = workload.load();
        if (loaded) runOffline(workload);
        writeReport(config, workload.report());
        return loaded && !workload.faulted() ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
