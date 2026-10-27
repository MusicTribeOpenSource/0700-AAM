// SPDX-License-Identifier: AGPL-3.0-or-later
#include <AudIO_juce/AudIO_juce.h>
#include <AudIO_juce/DeviceBlocks.h>
#include "../../clap_host/tests/AllocationGuard.h"
#include <iostream>
#if defined(__linux__) || defined(__APPLE__)
 #include <csignal>
 #include <sys/resource.h>
#endif
static void check(bool ok, const char* why) { if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
template<class Condition> static bool waitFor(Condition ready) {
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 1000;
    while (!ready() && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    return ready();
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    AudIO::DawEngine engine(2, 2);
#ifndef _WIN32
    {
        const auto* previous = std::getenv("KICK_NO_AUDIO_DEVICE");
        const std::string saved = previous ? previous : "";
        const bool existed = previous != nullptr;
        setenv("KICK_NO_AUDIO_DEVICE", "1", 1);
        mt::AudioDevice device(2, 2, {});
        device.start(48000, 480); device.resume();
        check(device.getState() == AudIO::AudioDeviceState::stopped, "disabled device remains stopped even when engine resumes");
        if (existed) setenv("KICK_NO_AUDIO_DEVICE", saved.c_str(), 1); else unsetenv("KICK_NO_AUDIO_DEVICE");
    }
#endif
    mt::SampleRecorder recorder(engine);
    AudIO::Buffer<float> block(2, true, 2, true);
    const auto frames = block.getBlockSize(); check(frames == 480, "released graph pool block size");
    recorder.prepareToPlay(48000, frames);
    bool lengthCallback = false;
    recorder.onRecorderLengthChanged = [&](double seconds) {
        check(seconds == .01 && recorder.getAudioBuffer().getNumSamples() == 0,
              "length callback can read the freshly cleared recorder");
        lengthCallback = true;
    };
    recorder.setLength(.01);
    check(lengthCallback, "length callback is delivered after resizing");
    recorder.onRecorderLengthChanged = {};
    const auto fill = [&](float l, float r) { std::fill_n(block.getInputBuffer(0), frames, l); std::fill_n(block.getInputBuffer(1), frames, r); };
    const auto render = [&] { forbidAllocation = true; recorder.process(block); forbidAllocation = false; };
    int started = 0, stopped = 0;
    recorder.onRecorderStarted = [&] { ++started; }; recorder.onRecorderStopped = [&] { ++stopped; };
    fill(.25f, -.5f); recorder.startRecording(); render();
    check(!recorder.isRecording() && !recorder.isEmpty(), "recording stops exactly at buffer boundary");
    auto& captured = recorder.getAudioBuffer();
    check(captured.getNumSamples() == 480 && captured.getNumChannels() == 2 && captured.getSample(0, 0) == .25f && captured.getSample(1, 479) == -.5f, "stereo capture and stable UI snapshot");
    check(waitFor([&] { return started == 1 && stopped == 1; }), "short recording notifications are delivered on message thread");
    recorder.startPreview(); render();
    check(block.getOutputBuffer(0)[0] == .25f && block.getOutputBuffer(1)[479] == -.5f, "preview routes stereo audio");
    recorder.setOverdub(true); fill(.5f, .5f); recorder.startRecording(); render();
    check(recorder.getAudioBuffer().getSample(0, 0) == .75f, "overdub retains previous take");
    recorder.undo(); check(recorder.getAudioBuffer().getSample(0, 0) == .25f, "undo restores take");
    recorder.redo(); check(recorder.getAudioBuffer().getSample(0, 0) == .75f, "redo restores overdub");
    recorder.setOverdub(false); recorder.setDirection(mt::Recorder::REVERSE);
    for (uint32_t n = 0; n < frames; ++n) { block.getInputBuffer(0)[n] = static_cast<float>(n) / 1000; block.getInputBuffer(1)[n] = 0; }
    recorder.startRecording(); render();
    check(recorder.getAudioBuffer().getSample(0, 0) == .479f && recorder.getAudioBuffer().getSample(0, 479) == 0, "reverse recording includes both endpoints");
    recorder.setDirection(mt::Recorder::FORWARD); recorder.setRecordingMode(mt::Recorder::LR_MONO); recorder.setLength(.01);
    fill(.25f, -.5f); recorder.startRecording(); render();
    check(recorder.getAudioBuffer().getNumChannels() == 1 && recorder.getAudioBuffer().getSample(0, 0) == -.125f, "mono downmix capture");
    recorder.reset(); recorder.setThreshold(.9f); recorder.startRecording(); render();
    check(recorder.isEmpty() && recorder.isRecording(), "threshold arms without consuming record length");
    fill(1, 0); render(); check(!recorder.isEmpty() && !recorder.isRecording(), "threshold crossing starts capture");
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("kick-recording", ".wav", false);
    recorder.save(file.getFullPathName()); check(recorder.lastError().isEmpty(), "save recording transaction");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(), true));
    check(reader && reader->lengthInSamples == 480 && reader->numChannels == 1 && reader->sampleRate == 48000, "WAV recording metadata and length");
    reader.reset();
#if defined(__linux__) || defined(__APPLE__)
    juce::MemoryBlock previousRecording;
    check(file.loadFileAsData(previousRecording), "read previous WAV bytes");
    rlimit previousLimit{};
    check(getrlimit(RLIMIT_FSIZE, &previousLimit) == 0, "read recording file-size limit");
    auto limited = previousLimit; limited.rlim_cur = 1024;
    const auto previousSignal = std::signal(SIGXFSZ, SIG_IGN);
    check(setrlimit(RLIMIT_FSIZE, &limited) == 0, "force an incomplete WAV write");
    recorder.save(file.getFullPathName());
    check(setrlimit(RLIMIT_FSIZE, &previousLimit) == 0, "restore recording file-size limit");
    std::signal(SIGXFSZ, previousSignal);
    juce::MemoryBlock afterFailedSave;
    check(recorder.lastError().isNotEmpty() && file.loadFileAsData(afterFailedSave) && afterFailedSave == previousRecording,
          "buffered WAV write failure preserves the previous recording");
    recorder.save(file.getFullPathName()); check(recorder.lastError().isEmpty(), "WAV save recovers after write failure");
#endif
    file.deleteFile();
    recorder.prepareToPlay(24000, frames);
    check(recorder.getAudioBuffer().getNumSamples() == 240, "device rate change preserves recording duration");
    recorder.undo(); recorder.redo();
    check(recorder.getAudioBuffer().getNumSamples() == 240, "rate change also preserves undo/redo history");
    recorder.reset(); recorder.startPreview(); render(); check(!recorder.isPreviewing(), "empty preview is safe");
    {
        mt::Looper looper(engine); looper.prepareToPlay(48000, frames); looper.setLength(.01);
        fill(.125f, -.25f); looper.start();
        for (int i = 0; i < 2; ++i) { forbidAllocation = true; looper.process(block); forbidAllocation = false; }
        check(looper.isRecording() && looper.getAudioBuffer().getNumSamples() == 480, "looper records continuously across wrap without allocation");
        looper.stopRecording(); looper.startPreview();
        for (int i = 0; i < 2; ++i) { forbidAllocation = true; looper.process(block); forbidAllocation = false; }
        check(looper.isPreviewing() && block.getOutputBuffer(0)[0] == .125f && block.getOutputBuffer(1)[479] == -.25f, "loop preview preserves both endpoints across wrap");
        looper.setOverdub(true); looper.start();
        forbidAllocation = true; looper.process(block); forbidAllocation = false;
        looper.stopRecording(); check(looper.getAudioBuffer().getSample(0, 0) == .25f, "loop overdub retains the previous cycle");
        looper.undo(); check(looper.getAudioBuffer().getSample(0, 0) == .125f, "loop undo restores previous cycle");
        looper.redo(); check(looper.getAudioBuffer().getSample(0, 0) == .25f, "loop redo restores overdub");
        looper.setLength(.02); looper.setOverdub(false);
        const double& position = looper.getProgress(); looper.start();
        forbidAllocation = true; looper.process(block); forbidAllocation = false;
        check(waitFor([&] { return std::abs(position - .5) < 1e-9; }), "retained looper progress reference updates on the message thread");
    }
    mt::ADSR envelope; envelope.setSampleRate(48000); envelope.setParameters(48, 0, 48, 48, 64); envelope.gate(true);
    double peak = 0; for (int i = 0; i < 200; ++i) { const auto value = envelope(); check(std::isfinite(value), "finite envelope"); peak = std::max(peak, value); }
    check(peak > .5, "restored envelope generates amplitude"); envelope.gate(false);
    for (int i = 0; i < 480; ++i) envelope();
    check(!envelope.isActive(), "envelope release completes");
    mt::Filter filter; filter.setSampleRate(48000); filter.setCutoff(.8); filter.setResonance(32);
    juce::AudioBuffer<float> signal(2, 1); signal.setSample(0, 0, 1); signal.setSample(1, 0, -1);
    forbidAllocation = true; filter.process(signal); forbidAllocation = false;
    check(std::isfinite(signal.getSample(0, 0)) && std::isfinite(signal.getSample(1, 0)), "filter processes without allocation");
    struct Dummy final : AudIO::Object { Dummy() : Object(0, 2, 0) {} void process(AudIO::Buffer<float>&) override {} } another;
    check(engine.getId() != recorder.getId() && another.getId() != recorder.getId() && another.getId() != engine.getId(), "IDs remain unique across core, adapter and client translation units");
    {
        juce::AudioBuffer<float> input(2, 480), output(2, 480);
        input.clear(); output.clear(); uint32_t filled = 0;
        juce::AudioBuffer<float> stream(2, 4096), result(3, 4096);
        for (int n = 0; n < 4096; ++n) { stream.setSample(0, n, static_cast<float>(n + 1)); stream.setSample(1, n, -static_cast<float>(n + 1)); }
        int done = 0, processed = 0;
        const auto copy = [&](const auto& in, auto& out) noexcept {
            ++processed; for (int c = 0; c < 2; ++c) out.copyFrom(c, 0, in, c, 0, 480);
        };
        for (const int n : {1, 127, 512, 31, 1024, 2401}) {
            const float* ins[]{stream.getReadPointer(0, done), stream.getReadPointer(1, done)};
            float* outs[]{result.getWritePointer(0, done), result.getWritePointer(1, done), result.getWritePointer(2, done)};
            forbidAllocation = true;
            mt::detail::deviceBlocks(ins, 2, outs, 3, n, input, output, filled, copy);
            forbidAllocation = false; done += n;
        }
        check(done == 4096 && processed == 8 && filled == 256, "variable device callbacks produce fixed engine blocks");
        for (int n = 0; n < 4096; ++n) {
            const float expected = n < 480 ? 0.f : static_cast<float>(n - 479);
            check(result.getSample(0, n) == expected && result.getSample(1, n) == -expected && result.getSample(2, n) == 0, "device adapter preserves samples, channel isolation and exact pipeline delay");
        }
        input.clear(); output.clear(); filled = 0;
        float* outs[]{result.getWritePointer(0), nullptr};
        forbidAllocation = true;
        mt::detail::deviceBlocks(nullptr, 0, outs, 2, 1024, input, output, filled, copy);
        forbidAllocation = false;
        check(result.getMagnitude(0, 0, 1024) == 0, "missing device channels remain silent");
    }
    std::cout << "PASS: actual released engine, capture/preview/overdub/undo/reverse/threshold/WAV, envelope/filter, unique graph IDs and render allocation guard\n";
}
