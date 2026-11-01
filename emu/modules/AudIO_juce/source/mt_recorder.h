/*
 * Sample recorder and looper audio objects.
 *
 * Kick creates both in MTAudioRecorderManager::initRecorder()
 * (Kick/modules/engine/audio_engine/engine/mt_audio_recorder.cpp) and drives
 * them through MTAudioRecorderManager and MTLooperInterface. The originals
 * were in the private AudIO_juce module. These are clean-room versions written
 * against those call sites: same names and signatures, my own DSP.
 *
 * Threading: setters are called from the message thread and only touch
 * atomics. Buffer (re)allocation happens in setLength(), which Kick calls
 * before recording starts.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

namespace mt
{
    /// Shared record/playback engine for SampleRecorder and Looper.
    class Recorder
    {
    public:
        enum Direction
        {
            FORWARD = 0,
            REVERSE,
        };

        /// Input channel selection. Matches ui::SamplerRecorder::RecordChannels,
        /// which kick_SamplerRecorder.cpp passes straight through as the mode.
        enum Mode
        {
            L_MONO = 0,
            R_MONO,
            LR_MONO,
            L_STEREO,
            R_STEREO,
            LR_STEREO,
        };

        static constexpr double kDefaultLengthSeconds = 30.0;

        Recorder();
        virtual ~Recorder() = default;

        // Setup...
        void setEnable( bool Enable ) noexcept { enabled = Enable; }
        void setLength( double TimeInSeconds );
        void setGain( float Gain ) noexcept { gain = juce::jlimit( 0.f, 2.f, Gain ); }
        void setGainDB( float GainDB ) noexcept { setGain( juce::Decibels::decibelsToGain( GainDB ) ); }
        void setPan( float Pan ) noexcept { pan = juce::jlimit( -1.f, 1.f, Pan ); }
        void setGainAndPan( float Gain, float Pan, bool /*Force*/ = false ) noexcept
        {
            setGain( Gain );
            setPan( Pan );
        }
        void setThreshold( float Gain ) noexcept { threshold = juce::jmax( 0.f, Gain ); }
        void setThresholdDB( float DB ) noexcept { setThreshold( juce::Decibels::decibelsToGain( DB, -100.f ) ); }
        void setDirection( Direction D ) noexcept { direction = D; }
        void setRecordingMode( Mode M ) noexcept { mode = M; }
        void setMonitorOn( bool On ) noexcept { monitorOn = On; }
        void setMonitorGain( float Gain ) noexcept { monitorGain = juce::jlimit( 0.f, 2.f, Gain ); }
        void setLooping( bool Loop ) noexcept { looping = Loop; }
        void setOverdub( bool Overdub ) noexcept { overdub = Overdub; }

        // State...
        bool  isEnabled() const noexcept { return enabled; }
        float getGain() const noexcept { return gain; }
        float getPan() const noexcept { return pan; }
        float getThreshold() const noexcept { return threshold; }
        float getThresholdDB() const noexcept { return juce::Decibels::gainToDecibels( threshold.load(), -100.f ); }
        float getMonitorGain() const noexcept { return monitorGain; }
        Mode  getRecordingMode() const noexcept { return mode; }
        bool  isForward() const noexcept { return direction == FORWARD; }
        bool  isReverse() const noexcept { return direction == REVERSE; }
        bool  isMonitorOn() const noexcept { return monitorOn; }
        bool  isLooping() const noexcept { return looping; }
        bool  isOverdubbing() const noexcept { return overdub; }
        bool  isRecording() const noexcept { return recording || armed; }
        bool  isPreviewing() const noexcept { return previewing; }
        bool  isRunning() const noexcept { return recording || armed || previewing; }
        bool  isEmpty() const noexcept { return recordedSamples.load() == 0; }

        /// Length of the record buffer in seconds.
        float getLength() const noexcept { return (float)( (double)capacity.load() / juce::jmax( 1.0, sampleRateHz.load() ) ); }
        /// Seconds actually recorded.
        double getRecordedLength() const noexcept { return (double)recordedSamples.load() / juce::jmax( 1.0, sampleRateHz.load() ); }

        /// 0..1 position of the record or play head.
        float   readProgress() const noexcept { return (float)progress; }
        double& getProgress() noexcept { return progress; }

        juce::AudioBuffer<float>& getAudioBuffer() noexcept { return recBuffer; }

        AudIO::MeterData readInputMeter();
        AudIO::MeterData readOutputMeter();

    protected:
        void prepareRecorder( double SampleRate, int BlockSize );

        /// Arms or starts recording. With SkipThreshold false, recording waits
        /// until the input crosses the threshold.
        void beginRecording( bool FromBeginning, bool SkipThreshold );
        void endRecording();
        void beginPlayback( double FromSeconds );
        void endPlayback();
        void clearBuffer();

        /// Runs one block. In and Out may alias (AudIO buffers are separate, but
        /// the code does not rely on it). Returns true if anything was written.
        void renderBlock( const float* InL, const float* InR, float* OutL, float* OutR, int NumSamples );

        bool saveToWav( const juce::File& File ) const;

        // Hooks for subclasses (called on the audio thread).
        virtual void recordingStarted() {}
        virtual void recordingStopped() {}
        virtual void playbackStarted() {}
        virtual void playbackStopped() {}

        juce::AudioBuffer<float> recBuffer;

        std::atomic<bool>      enabled{ false }, monitorOn{ false }, looping{ false }, overdub{ false };
        std::atomic<bool>      recording{ false }, armed{ false }, previewing{ false };
        std::atomic<float>     gain{ 1.f }, pan{ 0.f }, threshold{ 0.f }, monitorGain{ 1.f };
        std::atomic<Direction> direction{ FORWARD };
        std::atomic<Mode>      mode{ LR_STEREO };
        std::atomic<int>       capacity{ 0 }, recordedSamples{ 0 };
        /// Length of the take that beginRecording() is about to replace. Looper::recordingStarted()
        /// snapshots this many samples, since recordedSamples is already reset by then.
        std::atomic<int>       lengthBeforeRecording{ 0 };
        std::atomic<double>    sampleRateHz{ 48000.0 };

        int    head{ 0 };
        double progress{ 0.0 };

        /// Per-channel levels in dB, written on the audio thread.
        struct Levels
        {
            std::atomic<float> peak[2]{ { -100.f }, { -100.f } };
            std::atomic<float> rms[2]{ { -100.f }, { -100.f } };
        };
        static void            measure( Levels& L, const float* A, const float* B, int N ) noexcept;
        static AudIO::MeterData toMeterData( const Levels& L );

        Levels inLevels, outLevels;
    };

    /// One-shot sampler recorder (the SAMPLER screen).
    class SampleRecorder
        : public AudIO::Object
        , public AudIO::DawRouting
        , public Recorder
        , private juce::AsyncUpdater
    {
    public:
        explicit SampleRecorder( AudIO::Design& D, const juce::String& Name = "SampleRecorder" );
        ~SampleRecorder() override;

        void reset();
        void startRecording( bool FromBeginning );
        void stopRecording();
        void saveRecording( const std::string& FilePath );
        void startPreview( double FromSeconds );
        void stopPreview();

        std::function<void()>         onRecorderStarted        = []() {};
        std::function<void()>         onRecorderStopped        = []() {};
        std::function<void()>         onRecorderPaused         = []() {};
        std::function<void()>         onRecorderResumed        = []() {};
        std::function<void()>         onRecorderPreviewStarted = []() {};
        std::function<void()>         onRecorderPreviewStopped = []() {};
        std::function<void( double )> onRecorderLengthChanged  = []( double ) {};

    private:
        void prepareToPlay( uint32_t samplerate, uint32_t blocksize ) override;
        void process( AudIO::Buffer<float>& buffer ) override;
        void teardown() override { AudIO::Object::teardown(); }

        void recordingStarted() override { post( kStarted ); }
        void recordingStopped() override { post( kStopped ); }
        void playbackStarted() override { post( kPreviewStarted ); }
        void playbackStopped() override { post( kPreviewStopped ); }

        enum Event
        {
            kStarted        = 1 << 0,
            kStopped        = 1 << 1,
            kPreviewStarted = 1 << 2,
            kPreviewStopped = 1 << 3,
        };
        void post( int E )
        {
            pending.fetch_or( E );
            triggerAsyncUpdate();
        }
        void handleAsyncUpdate() override;

        std::atomic<int> pending{ 0 };
    };

    /// Phrase looper (the LOOPER screen), with up to four insert effects.
    class Looper
        : public AudIO::Object
        , public AudIO::DawRouting
        , public AudIO::DawInserts
        , public Recorder
    {
    public:
        explicit Looper( AudIO::Design& D, const juce::String& Name = "Looper" );
        ~Looper() override;

        void setRecord( bool Record );
        void setPlay();

        void start( bool FromBeginning, bool SkipThresholdDetection );
        void play( double FromSeconds = 0.0 );
        void stop();
        void clear();
        void undo();
        void redo();
        void save( const juce::String& FilePath );

    private:
        void prepareToPlay( uint32_t samplerate, uint32_t blocksize ) override;
        void process( AudIO::Buffer<float>& buffer ) override;
        void teardown() override { AudIO::Object::teardown(); }

        void recordingStarted() override;

        juce::AudioBuffer<float> undoBuffer, redoBuffer;
        int                      undoLength{ 0 }, redoLength{ 0 };
        std::atomic<bool>        snapshotPending{ false };
    };
}

namespace EMPTY
{
    static const juce::AudioBuffer<float> SampleBuffer{};
    static const juce::String             String{};
}
