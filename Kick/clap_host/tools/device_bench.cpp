// SPDX-License-Identifier: AGPL-3.0-or-later
#include "DeviceBenchmark.h"
#include <iostream>
#include <thread>
#include <sstream>
namespace {
using namespace kick::clap::bench;
int run(const Options& config) {
    juce::ScopedJuceInitialiser_GUI gui;
    if (!config.audio && !config.listDevices) {
        Workload workload(config); const bool loaded = workload.load();
        if (loaded) runOffline(workload);
        writeReport(config, workload.report()); return loaded && !workload.faulted() ? 0 : 1;
    }
    juce::AudioDeviceManager types;
    juce::AudioIODeviceType* selected = nullptr;
    std::ostringstream listing; listing << "{\"drivers\":["; bool firstType = true;
    for (auto* type : types.getAvailableDeviceTypes()) {
        type->scanForDevices(); const auto names = type->getDeviceNames(false);
        if (!selected && (config.driver.empty() ? !names.isEmpty() : type->getTypeName() == juce::String(config.driver))) selected = type;
        if (!firstType) listing << ','; firstType = false;
        listing << "{\"name\":" << quote(type->getTypeName().toStdString()) << ",\"outputs\":[";
        for (int i = 0; i < names.size(); ++i) { if (i) listing << ','; listing << quote(names[i].toStdString()); }
        listing << "]}";
    }
    listing << "]}\n";
    if (config.listDevices) { writeReport(config, listing.str()); return 0; }
    if (!selected) throw std::runtime_error("Requested audio driver is unavailable; use --list-devices");
    const auto names = selected->getDeviceNames(false);
    if (names.isEmpty()) throw std::runtime_error("Requested driver has no output devices");
    const auto name = config.device.empty() ? names[juce::jlimit(0, names.size() - 1, selected->getDefaultDeviceIndex(false))] : juce::String(config.device);
    if (!names.contains(name)) throw std::runtime_error("Requested output device is unavailable; use --list-devices");
    std::unique_ptr<juce::AudioIODevice> device(selected->createDevice(name, {}));
    if (!device) throw std::runtime_error("Cannot create output device");
    juce::BigInteger outputs; outputs.setRange(0, 2, true);
    const auto openError = device->open({}, outputs, config.sampleRate, static_cast<int>(config.frames));
    if (openError.isNotEmpty()) throw std::runtime_error(openError.toStdString());
    if (std::abs(device->getCurrentSampleRate() - config.sampleRate) > .1 || device->getCurrentBufferSizeSamples() != static_cast<int>(config.frames)) {
        device->close(); throw std::runtime_error("Device did not accept the requested sample rate/buffer size");
    }
    Workload workload(config);
    if (!workload.load()) { device->close(); writeReport(config, workload.report()); return 1; }
    DeviceRender callback(workload);
    struct Stop { juce::AudioIODevice& device; ~Stop() { device.stop(); device.close(); } } cleanup{*device};
    device->start(&callback);
    uint64_t previous = 0; auto lastProgress = std::chrono::steady_clock::now();
    const auto pump = [&] {
        workload.control(workload.totalFrames() / config.sampleRate);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const auto now = std::chrono::steady_clock::now(); const auto progress = workload.totalFrames();
        if (progress != previous) { previous = progress; lastProgress = now; }
        if (callback.deviceError.load() || callback.stopped.load()) workload.fail("Audio device stopped, failed or changed its format");
        if (now - lastProgress > std::chrono::seconds(3)) workload.fail("Audio device callbacks stalled");
    };
    while (workload.totalFrames() < config.warmup * config.sampleRate && !workload.faulted()) pump();
    const auto nativeStart = device->getXRunCount();
    workload.beginMeasurement();
    while (workload.measuredFrames() < std::ceil(config.seconds * config.sampleRate) && !workload.faulted()) pump();
    DeviceInfo info;
    info.driver = selected->getTypeName().toStdString(); info.name = device->getName().toStdString();
    info.inputLatency = device->getInputLatencyInSamples(); info.outputLatency = device->getOutputLatencyInSamples();
    device->stop();
    const auto nativeEnd = device->getXRunCount();
    if (nativeStart >= 0 && nativeEnd >= nativeStart) info.nativeXruns = nativeEnd - nativeStart;
    writeReport(config, workload.report(&info));
    return workload.faulted() ? 1 : 0;
}
}
int main(int argc, char** argv) {
    try {
        const auto config = kick::clap::bench::parse(argc, argv);
        if (config.help) { std::cout << kick::clap::bench::usage(); return 0; }
        try { return run(config); }
        catch (const std::exception& error) {
            kick::clap::bench::Workload failed(config);
            failed.fail(error.what());
            kick::clap::bench::writeReport(config, failed.report());
            std::cerr << error.what() << '\n';
            return 1;
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
