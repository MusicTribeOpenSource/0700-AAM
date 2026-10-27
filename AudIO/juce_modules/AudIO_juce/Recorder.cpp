// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudIO_juce.h"
namespace mt {
namespace {
// The format writer owns its stream and writes the final header in its
// destructor. Capture failures through that final write and the disk flush.
class RecordingOutputStream final : public juce::FileOutputStream {
public:
    RecordingOutputStream(const juce::File& file, bool& success)
        : FileOutputStream(file, 0), written(success) {}
    ~RecordingOutputStream() override {
        flush();
        written = written && getStatus().wasOk();
    }
    bool write(const void* data, size_t bytes) override {
        const bool ok = FileOutputStream::write(data, bytes);
        written = written && ok;
        return ok;
    }
    bool setPosition(juce::int64 position) override {
        const bool ok = FileOutputStream::setPosition(position);
        written = written && ok;
        return ok;
    }
private:
    bool& written;
};
}
Recorder::Recorder(AudIO::Design& design) : AudIO::Object(2, 2, 0), AudIO::DawRouting(design, AudIO::BusType::Object) {
    AudIO::DawRouting::setID(getId()); setName("Recorder"); startTimerHz(30);
}
Recorder::~Recorder() { stopTimer(); recording.store(false); preview.store(false); }
void Recorder::prepareToPlay(uint32_t rate, uint32_t frames) {
    if (!rate || !frames) return;
    const juce::SpinLock::ScopedLockType lock(gate);
    stop();
    const auto previousRate = getSampleRate();
    if (previousRate > 0 && previousRate != rate) {
        // Device setup runs off the render thread. Preserve take duration and
        // undo history when the driver negotiates a different sample rate.
        const auto resample = [&](juce::AudioBuffer<float>& source, int& valid) {
            if (!source.getNumSamples()) return;
            const double scale = rate / previousRate;
            const auto count = static_cast<int>(std::llround(source.getNumSamples() * scale));
            juce::AudioBuffer<float> converted(source.getNumChannels(), count);
            for (int c = 0; c < source.getNumChannels(); ++c) for (int n = 0; n < count; ++n) {
                const double position = n / scale;
                const auto first = juce::jmin(static_cast<int>(position), source.getNumSamples() - 1);
                const auto next = juce::jmin(first + 1, source.getNumSamples() - 1);
                converted.setSample(c, n, source.getSample(c, first) + static_cast<float>(position - first) * (source.getSample(c, next) - source.getSample(c, first)));
            }
            valid = juce::jlimit(0, count, static_cast<int>(std::llround(valid * scale)));
            source = std::move(converted);
        };
        auto valid = validFrames.load(); resample(audio, valid); validFrames.store(valid);
        resample(history, historyFrames); writePosition = readPosition = 0;
    }
    AudIO::Object::prepareToPlay(rate, frames); AudIO::DawInserts::prepareToPlay(rate, frames);
}
void Recorder::setRecordingMode(Mode value) {
    if (value < L_MONO || value > LR_STEREO) return;
    mode.store(value);
}
void Recorder::setLength(double seconds) {
    if (!std::isfinite(seconds)) return;
    seconds = juce::jlimit(0., 600., seconds);
    {
        const juce::SpinLock::ScopedLockType lock(gate);
        recording.store(false); preview.store(false);
        audio.setSize(mode.load() <= LR_MONO ? 1 : 2, static_cast<int>(seconds * getSampleRate()), false, true);
        audio.clear(); history.setSize(0, 0); validFrames.store(0); historyFrames = 0;
        length.store(seconds); writePosition = readPosition = 0; progress.store(0);
    }
    // UI callbacks may read snapshots or change recorder controls themselves.
    if (onRecorderLengthChanged) onRecorderLengthChanged(seconds);
}
void Recorder::startLocked(bool fromBeginning, bool skipThreshold) {
    history.makeCopyOf(audio); historyFrames = validFrames.load();
    if (!overdub.load() && fromBeginning) { audio.clear(); validFrames.store(0); }
    if (fromBeginning || writePosition >= audio.getNumSamples() || writePosition < 0)
        writePosition = isForward() ? 0 : audio.getNumSamples() - 1;
    thresholdReached = skipThreshold || threshold.load() <= 0;
    preview.store(false); recording.store(audio.getNumSamples() > 0);
    if (recording.load()) notices.fetch_or(1);
}
void Recorder::start(bool fromBeginning, bool skipThreshold) {
    if (audio.getNumSamples() == 0 || audio.getNumChannels() != (mode.load() <= LR_MONO ? 1 : 2))
        setLength(length.load() > 0 ? length.load() : 30);
    const juce::SpinLock::ScopedLockType lock(gate); startLocked(fromBeginning, skipThreshold);
}
void Recorder::startRecording(bool fromBeginning) { start(fromBeginning, false); }
void Recorder::stopRecording() { if (recording.exchange(false)) notices.fetch_or(2); }
void Recorder::startPreview(double seconds) {
    const juce::SpinLock::ScopedLockType lock(gate);
    recording.store(false);
    if (!std::isfinite(seconds) || validFrames.load() <= 0) return;
    readPosition = juce::jlimit(0, validFrames.load() - 1, static_cast<int>(seconds * getSampleRate()));
    if (seconds <= 0 && isReverse()) readPosition = validFrames.load() - 1;
    preview.store(true);
}
void Recorder::stopPreview() { preview.store(false); }
void Recorder::stop() { stopRecording(); stopPreview(); }
void Recorder::reset() {
    const juce::SpinLock::ScopedLockType lock(gate); stop(); audio.clear();
    validFrames.store(0); writePosition = readPosition = 0; progress.store(0);
}
juce::AudioBuffer<float>& Recorder::getAudioBuffer() {
    const juce::SpinLock::ScopedLockType lock(gate);
    view.setSize(audio.getNumChannels(), validFrames.load());
    for (int c = 0; c < view.getNumChannels(); ++c) view.copyFrom(c, 0, audio, c, 0, view.getNumSamples());
    return view;
}
void Recorder::undo() {
    const juce::SpinLock::ScopedLockType lock(gate);
    stop(); if (!history.getNumSamples()) return;
    std::swap(audio, history); const auto current = validFrames.exchange(historyFrames); historyFrames = current;
    length.store(audio.getNumSamples() / getSampleRate()); writePosition = readPosition = 0; progress.store(0);
}
void Recorder::save(const juce::String& path) {
    stop(); auto& snapshot = getAudioBuffer();
    if (snapshot.getNumSamples() == 0 || snapshot.getNumChannels() == 0) { error = "There is no recording to save"; return; }
    juce::TemporaryFile temporary{juce::File(path)};
    // Direct writes let the format writer detect partial disk writes before
    // replacing the previous take; buffered flushes can hide short writes.
    bool written = true;
    auto output = std::make_unique<RecordingOutputStream>(temporary.getFile(), written);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(output->openedOk() ? wav.createWriterFor(output.get(), getSampleRate(), static_cast<unsigned>(snapshot.getNumChannels()), 24, {}, 0) : nullptr);
    if (!writer) { error = "Cannot create recording file"; return; }
    output.release(); // The writer now owns the stream.
    const bool audioWritten = writer->writeFromAudioSampleBuffer(snapshot, 0, snapshot.getNumSamples());
    writer.reset();
    if (!audioWritten || !written || !temporary.overwriteTargetFileWithTemporary()) error = "Cannot save recording"; else error.clear();
}
AudIO::MeterData Recorder::readInputMeter() const {
    return {{juce::Decibels::gainToDecibels(inputRms[0].load(), -100.f), juce::Decibels::gainToDecibels(inputPeak[0].load(), -100.f)},
            {juce::Decibels::gainToDecibels(inputRms[1].load(), -100.f), juce::Decibels::gainToDecibels(inputPeak[1].load(), -100.f)}};
}
AudIO::MeterData Recorder::readOutputMeter() const {
    return {{juce::Decibels::gainToDecibels(outputRms[0].load(), -100.f), juce::Decibels::gainToDecibels(outputPeak[0].load(), -100.f)},
            {juce::Decibels::gainToDecibels(outputRms[1].load(), -100.f), juce::Decibels::gainToDecibels(outputPeak[1].load(), -100.f)}};
}
void Recorder::timerCallback() {
    // Legacy ProgressBar/LooperPlayHead consumers retain this reference. Only
    // the message thread writes the view; DSP publishes the atomic progress.
    viewProgress = progress.load();
    const auto rec = recording.load(), playing = preview.load();
    const auto events = notices.exchange(0);
    const auto notifyStart = [&] { if (onRecorderStarted) onRecorderStarted(); };
    const auto notifyStop = [&] { if (onRecorderStopped) onRecorderStopped(); if (onRecorderLengthChanged) onRecorderLengthChanged(validFrames.load() / getSampleRate()); };
    if ((events & 3) == 3) { if (rec) { notifyStop(); notifyStart(); } else { notifyStart(); notifyStop(); } }
    else if (events & 1) notifyStart();
    else if (events & 2) notifyStop();
    else if (rec != hadRecording) { if (rec) notifyStart(); else notifyStop(); }
    hadRecording = rec;
    if (playing != hadPreview) { if (playing) { if (onRecorderPreviewStarted) onRecorderPreviewStarted(); } else if (onRecorderPreviewStopped) onRecorderPreviewStopped(); hadPreview = playing; }
}
void Recorder::process(AudIO::Buffer<float>& buffer) {
    const juce::SpinLock::ScopedTryLockType lock(gate);
    if (!lock.isLocked() || !enabled.load()) { buffer.clearOutputs(); return; }
    const auto frames = buffer.getBlockSize();
    const auto activeMode = mode.load(); const auto g = gain.load(); const auto p = pan.load();
    const std::array<float, 2> gains{g * (p > 0 ? 1 - p : 1), g * (p < 0 ? 1 + p : 1)};
    std::array<double, 2> inSum{}, outSum{}; std::array<float, 2> inMax{}, outMax{};
    for (uint32_t n = 0; n < frames; ++n) {
        std::array<float, 2> input{}, recorded{}, output{};
        for (int c = 0; c < 2; ++c) { const auto* source = buffer.getInputBuffer(c); input[c] = source && std::isfinite(source[n]) ? source[n] : 0; inSum[c] += input[c] * input[c]; inMax[c] = std::max(inMax[c], std::abs(input[c])); }
        switch (activeMode) {
            case L_MONO: case L_STEREO: recorded = {input[0] * gains[0], input[0] * gains[0]}; break;
            case R_MONO: case R_STEREO: recorded = {input[1] * gains[1], input[1] * gains[1]}; break;
            case LR_MONO: recorded[0] = recorded[1] = (input[0] * gains[0] + input[1] * gains[1]) * .5f; break;
            default: recorded = {input[0] * gains[0], input[1] * gains[1]}; break;
        }
        if (monitor.load()) for (int c = 0; c < 2; ++c) output[c] = recorded[c] * monitorGain.load();
        if (recording.load() && audio.getNumSamples() > 0) {
            if (!thresholdReached && std::max(std::abs(input[0]), std::abs(input[1])) >= threshold.load()) thresholdReached = true;
            if (thresholdReached) {
                for (int c = 0; c < audio.getNumChannels(); ++c) audio.setSample(c, writePosition, recorded[c] + (overdub.load() ? audio.getSample(c, writePosition) : 0));
                validFrames.store(std::max(validFrames.load(), writePosition + 1));
                writePosition += isForward() ? 1 : -1;
                if (writePosition >= audio.getNumSamples() || writePosition < 0) { if (looping.load()) writePosition = isForward() ? 0 : audio.getNumSamples() - 1; else { recording.store(false); notices.fetch_or(2); } }
            }
        } else if (preview.load() && validFrames.load() > 0) {
            for (int c = 0; c < 2; ++c) output[c] += audio.getSample(std::min(c, audio.getNumChannels() - 1), readPosition) * gains[c];
            readPosition += isForward() ? 1 : -1;
            if (readPosition >= validFrames.load() || readPosition < 0) { if (looping.load()) readPosition = isForward() ? 0 : validFrames.load() - 1; else preview.store(false); }
        }
        for (int c = 0; c < 2; ++c) { if (auto* target = buffer.getOutputBuffer(c)) target[n] = output[c]; outSum[c] += output[c] * output[c]; outMax[c] = std::max(outMax[c], std::abs(output[c])); }
    }
    AudIO::DawInserts::process(buffer);
    for (int c = 0; c < 2; ++c) { inputRms[c].store(frames ? static_cast<float>(std::sqrt(inSum[c] / frames)) : 0); inputPeak[c].store(inMax[c]); outputRms[c].store(frames ? static_cast<float>(std::sqrt(outSum[c] / frames)) : 0); outputPeak[c].store(outMax[c]); }
    const auto at = preview.load() ? readPosition : writePosition;
    progress.store(audio.getNumSamples() ? juce::jlimit(0.f, 1.f, static_cast<float>(at) / audio.getNumSamples()) : 0);
}
}
