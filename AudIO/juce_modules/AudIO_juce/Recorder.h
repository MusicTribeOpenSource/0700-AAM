// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
namespace mt {
// Replacement for the unavailable JUCE recorder adapter. Storage/control work
// runs outside processing; the audio callback only tries the control gate.
class Recorder : public AudIO::Object, public AudIO::DawRouting, public AudIO::DawInserts, private juce::Timer {
public:
    enum Direction { FORWARD, REVERSE };
    enum Mode { L_MONO, R_MONO, LR_MONO, L_STEREO, R_STEREO, LR_STEREO };
    explicit Recorder(AudIO::Design&);
    ~Recorder() override;
    std::function<void()> onRecorderStarted, onRecorderStopped, onRecorderPaused, onRecorderResumed,
        onRecorderPreviewStarted, onRecorderPreviewStopped;
    std::function<void(double)> onRecorderLengthChanged;
    void setEnable(bool v) { enabled.store(v); }
    bool isEnabled() const { return enabled.load(); }
    void setMonitorOn(bool v) { monitor.store(v); }
    bool isMonitorOn() const { return monitor.load(); }
    void setLooping(bool v) { looping.store(v); }
    bool isLooping() const { return looping.load(); }
    void setOverdub(bool v) { overdub.store(v); }
    bool isOverdubbing() const { return overdub.load(); }
    void setDirection(Direction v) { direction.store(v); }
    bool isForward() const { return direction.load() == FORWARD; }
    bool isReverse() const { return !isForward(); }
    void setRecordingMode(Mode v);
    Mode getRecordingMode() const { return mode.load(); }
    void setGain(float v) { gain.store(juce::jlimit(0.f, 1.f, v)); }
    void setGainDB(float v) { setGain(juce::Decibels::decibelsToGain(v)); }
    float getGain() const { return gain.load(); }
    void setPan(float v) { pan.store(juce::jlimit(-1.f, 1.f, v)); }
    float getPan() const { return pan.load(); }
    void setGainAndPan(float g, float p, bool) { setGain(g); setPan(p); }
    void setMonitorGain(float v) { monitorGain.store(juce::jlimit(0.f, 1.f, v)); }
    float getMonitorGain() const { return monitorGain.load(); }
    void setThreshold(float v) { threshold.store(juce::jlimit(0.f, 1.f, v)); }
    void setThresholdDB(float v) { setThreshold(juce::Decibels::decibelsToGain(v)); }
    float getThreshold() const { return threshold.load(); }
    float getThresholdDB() const { return juce::Decibels::gainToDecibels(getThreshold(), -100.f); }
    void setLength(double seconds);
    float getLength() const { return static_cast<float>(length.load()); }
    void startRecording(bool fromBeginning = true);
    void stopRecording();
    void startPreview(double seconds = 0);
    void stopPreview();
    void start(bool fromBeginning = true, bool skipThreshold = false);
    void play(double seconds = 0) { startPreview(seconds); }
    void stop() override;
    void reset();
    void clear() { reset(); }
    void setRecord(bool v) { if (v) startRecording(false); else stopRecording(); }
    void setPlay() { startPreview(); }
    bool isRecording() const { return recording.load(); }
    bool isPreviewing() const { return preview.load(); }
    bool isRunning() const { return isRecording() || isPreviewing(); }
    bool isEmpty() const { return validFrames.load() == 0; }
    float readProgress() const { return progress.load(); }
    double& getProgress() { viewProgress = progress.load(); return viewProgress; }
    juce::AudioBuffer<float>& getAudioBuffer(); // stable UI snapshot
    void undo();
    void redo() { undo(); }
    void saveRecording(const std::string& file) { save(juce::String(file)); }
    void save(const juce::String&);
    const juce::String& lastError() const { return error; }
    AudIO::MeterData readInputMeter() const;
    AudIO::MeterData readOutputMeter() const;
    void prepareToPlay(uint32_t rate, uint32_t frames) override;
    void process(AudIO::Buffer<float>&) override;
private:
    void timerCallback() override;
    void startLocked(bool, bool);
    juce::SpinLock gate;
    juce::AudioBuffer<float> audio, history, view;
    std::atomic<bool> enabled{true}, monitor{false}, looping{false}, overdub{false}, recording{false}, preview{false};
    std::atomic<float> gain{1}, pan{0}, threshold{0}, monitorGain{1}, progress{0};
    std::atomic<double> length{0};
    std::atomic<Mode> mode{LR_STEREO};
    std::atomic<Direction> direction{FORWARD};
    std::atomic<int> validFrames{0};
    std::atomic<unsigned> notices{0};
    std::array<std::atomic<float>, 2> inputRms{}, inputPeak{}, outputRms{}, outputPeak{};
    int writePosition = 0, readPosition = 0, historyFrames = 0;
    bool thresholdReached = false, hadRecording = false, hadPreview = false;
    double viewProgress = 0;
    juce::String error;
};
class SampleRecorder final : public Recorder { public: explicit SampleRecorder(AudIO::Design& d) : Recorder(d) {} };
class Looper : public Recorder { public: explicit Looper(AudIO::Design& d) : Recorder(d) { setLooping(true); } };
}
