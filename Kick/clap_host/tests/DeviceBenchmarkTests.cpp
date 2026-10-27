// SPDX-License-Identifier: AGPL-3.0-or-later
#include "DeviceBenchmark.h"
#include "AllocationGuard.h"
#include <iostream>
using namespace kick::clap::bench;
static void check(bool ok, const char* why) { if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
int main(int argc, char** argv) {
    check(argc == 2, "fixture");
    juce::ScopedJuceInitialiser_GUI gui;
    Options config; config.path = argv[1]; config.id = "org.kick.test.synth"; config.audio = true;
    Workload workload(config); check(workload.load(), "load audio callback workload");
    DeviceRender callback(workload); workload.control(0); workload.beginMeasurement();
    std::array<float, 128> left, right, extra; float* outputs[]{left.data(), right.data(), nullptr, extra.data()};
    left.fill(1); right.fill(1); extra.fill(1);
    forbidAllocation = true;
    callback.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 4, 128, {});
    forbidAllocation = false;
    check(left[0] == 0 && right[0] == 0 && extra[0] == 0 && workload.measuredFrames() == 128, "muted callback computes DSP and clears all device outputs including extras");
    callback.audioDeviceError("disconnected");
    left.fill(1); extra.fill(1);
    callback.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 4, 128, {});
    check(left[0] == 0 && extra[0] == 0 && workload.measuredFrames() == 128, "failed device remains silent without doing more DSP");
    DeviceInfo info; auto report = workload.report(&info);
    check(report.find("\"native_xruns\":null") != std::string::npos && report.find("\"mode\":\"audio-device\"") != std::string::npos, "unknown xrun support is explicit");
    config.audible = true;
    Workload audible(config); check(audible.load(), "audible workload"); audible.control(0);
    DeviceRender audibleCallback(audible);
    forbidAllocation = true; audibleCallback.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 4, 128, {}); forbidAllocation = false;
    check(left[0] > 0 && left[0] <= .1f && extra[0] == 0, "explicit audible mode applies headroom and clears extra channels");
    std::cout << "PASS: actual JUCE device callback, allocation guard, mute/audible routing, null/extra channels, device errors and unsupported xrun reporting\n";
}
