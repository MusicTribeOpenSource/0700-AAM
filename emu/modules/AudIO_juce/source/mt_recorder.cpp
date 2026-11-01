/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

namespace mt
{
    //==============================================================================
    // Recorder

    Recorder::Recorder()
    {
        recBuffer.setSize( 2, 0 );
    }

    void Recorder::prepareRecorder( double SampleRate, int /*BlockSize*/ )
    {
        sampleRateHz = SampleRate;
        if( capacity.load() == 0 )
            setLength( kDefaultLengthSeconds );
    }

    void Recorder::setLength( double TimeInSeconds )
    {
        const int samples = juce::jmax( 1, (int)std::ceil( juce::jmax( 0.1, TimeInSeconds ) * sampleRateHz.load() ) );
        if( samples == capacity.load() )
            return;

        // Not safe while the audio thread records into the buffer. Kick only
        // changes the length while stopped, so stop to be sure.
        const bool wasRec = recording.exchange( false );
        armed             = false;
        previewing        = false;

        recBuffer.setSize( 2, samples, true, true, false );
        capacity        = samples;
        recordedSamples = juce::jmin( recordedSamples.load(), samples );
        head            = 0;
        juce::ignoreUnused( wasRec );
    }

    void Recorder::clearBuffer()
    {
        recBuffer.clear();
        recordedSamples = 0;
        head            = 0;
        progress        = 0.0;
    }

    void Recorder::beginRecording( bool FromBeginning, bool SkipThreshold )
    {
        previewing = false;
        if( FromBeginning || !overdub )
            head = 0;
        if( !overdub )
            recordedSamples = 0;

        if( SkipThreshold || threshold.load() <= 0.f )
        {
            armed     = false;
            recording = true;
            recordingStarted();
        }
        else
        {
            armed = true; // Waits in renderBlock().
        }
    }

    void Recorder::endRecording()
    {
        const bool was = recording.exchange( false ) || armed.exchange( false );
        if( was )
            recordingStopped();
    }

    void Recorder::beginPlayback( double FromSeconds )
    {
        if( recordedSamples.load() == 0 )
            return;

        recording = false;
        armed     = false;
        head      = juce::jlimit( 0, juce::jmax( 0, recordedSamples.load() - 1 ), (int)( FromSeconds * sampleRateHz.load() ) );
        if( isReverse() )
            head = recordedSamples.load() - 1 - head;
        previewing = true;
        playbackStarted();
    }

    void Recorder::endPlayback()
    {
        if( previewing.exchange( false ) )
            playbackStopped();
    }

    void Recorder::measure( Levels& L, const float* A, const float* B, int N ) noexcept
    {
        const float* ch[2] = { A, B };
        for( int c = 0; c < 2; ++c )
        {
            float pk = 0.f, sum = 0.f;
            if( ch[c] != nullptr )
                for( int i = 0; i < N; ++i )
                {
                    const auto v = std::abs( ch[c][i] );
                    pk           = juce::jmax( pk, v );
                    sum += v * v;
                }
            const auto rms = N > 0 ? std::sqrt( sum / (float)N ) : 0.f;
            // Simple peak hold with release so the UI meters fall back smoothly.
            const auto pkDb  = juce::Decibels::gainToDecibels( pk, -100.f );
            const auto rmsDb = juce::Decibels::gainToDecibels( rms, -100.f );
            L.peak[c]        = juce::jmax( pkDb, L.peak[c].load() - 1.5f );
            L.rms[c]         = juce::jmax( rmsDb, L.rms[c].load() - 1.5f );
        }
    }

    AudIO::MeterData Recorder::toMeterData( const Levels& L )
    {
        AudIO::MeterData d( 2 );
        for( int c = 0; c < 2; ++c )
        {
            d[(size_t)c].peak = L.peak[c].load();
            d[(size_t)c].rms  = L.rms[c].load();
        }
        return d;
    }

    AudIO::MeterData Recorder::readInputMeter() { return toMeterData( inLevels ); }
    AudIO::MeterData Recorder::readOutputMeter() { return toMeterData( outLevels ); }

    void Recorder::renderBlock( const float* InL, const float* InR, float* OutL, float* OutR, int N )
    {
        measure( inLevels, InL, InR, N );

        const int   cap   = capacity.load();
        const float g     = gain.load();
        const float p     = pan.load();
        const float gL    = g * juce::jmin( 1.f, 1.f - p );
        const float gR    = g * juce::jmin( 1.f, 1.f + p );
        const float mon   = monitorOn.load() ? monitorGain.load() : 0.f;
        const auto  m     = mode.load();
        auto*       bufL  = cap > 0 ? recBuffer.getWritePointer( 0 ) : nullptr;
        auto*       bufR  = cap > 0 ? recBuffer.getWritePointer( 1 ) : nullptr;
        const int   step  = isReverse() ? -1 : 1;

        // Threshold detection: start once any selected input crosses the threshold.
        if( armed.load() && InL != nullptr )
        {
            const auto thr = threshold.load();
            for( int i = 0; i < N; ++i )
            {
                const auto l = InL[i], r = InR != nullptr ? InR[i] : l;
                if( std::abs( l ) >= thr || std::abs( r ) >= thr )
                {
                    armed     = false;
                    recording = true;
                    recordingStarted();
                    break;
                }
            }
        }

        for( int i = 0; i < N; ++i )
        {
            const float l = InL != nullptr ? InL[i] : 0.f;
            const float r = InR != nullptr ? InR[i] : l;

            // Input channel selection.
            float srcL = l, srcR = r;
            switch( m )
            {
                case L_MONO: srcR = l; break;
                case R_MONO: srcL = r; break;
                case LR_MONO: srcL = srcR = 0.5f * ( l + r ); break;
                case L_STEREO: srcR = 0.f; break;
                case R_STEREO: srcL = 0.f; break;
                case LR_STEREO:
                default: break;
            }

            float outL = mon * srcL, outR = mon * srcR;

            if( recording.load() && bufL != nullptr )
            {
                if( head < 0 || head >= cap )
                {
                    if( looping.load() )
                        head = isReverse() ? cap - 1 : 0;
                    else
                    {
                        recording = false;
                        recordingStopped();
                    }
                }
                if( recording.load() )
                {
                    const bool layered = overdub.load() && head < recordedSamples.load();
                    // While overdubbing, play back what is already there so the
                    // performer hears the loop.
                    if( layered )
                    {
                        outL += gL * bufL[head];
                        outR += gR * bufR[head];
                    }
                    bufL[head] = layered ? bufL[head] + srcL : srcL;
                    bufR[head] = layered ? bufR[head] + srcR : srcR;
                    recordedSamples = juce::jmax( recordedSamples.load(), isReverse() ? cap - head : head + 1 );
                    head += step;
                    progress = (double)head / (double)juce::jmax( 1, cap );
                }
            }
            else if( previewing.load() && bufL != nullptr )
            {
                const int len = recordedSamples.load();
                if( head < 0 || head >= len )
                {
                    if( looping.load() && len > 0 )
                        head = isReverse() ? len - 1 : 0;
                    else
                    {
                        previewing = false;
                        playbackStopped();
                    }
                }
                if( previewing.load() )
                {
                    outL += gL * bufL[head];
                    outR += gR * bufR[head];
                    head += step;
                    progress = (double)head / (double)juce::jmax( 1, len );
                }
            }

            if( OutL != nullptr )
                OutL[i] = outL;
            if( OutR != nullptr )
                OutR[i] = outR;
        }

        measure( outLevels, OutL, OutR, N );
    }

    bool Recorder::saveToWav( const juce::File& File ) const
    {
        const int len = recordedSamples.load();
        if( len <= 0 )
            return false;

        File.getParentDirectory().createDirectory();
        File.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream( File.createOutputStream() );
        if( stream == nullptr )
            return false;

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer( wav.createWriterFor( stream.get(), sampleRateHz.load(), 2, 24, {}, 0 ) );
        if( writer == nullptr )
            return false;
        stream.release(); // The writer owns the stream now.

        // A reverse recording is stored back to front; write it in time order.
        if( isReverse() )
        {
            juce::AudioBuffer<float> tmp( 2, len );
            const int                cap = capacity.load();
            for( int c = 0; c < 2; ++c )
                for( int i = 0; i < len; ++i )
                    tmp.setSample( c, i, recBuffer.getSample( c, cap - len + i ) );
            tmp.reverse( 0, len );
            return writer->writeFromAudioSampleBuffer( tmp, 0, len );
        }
        return writer->writeFromAudioSampleBuffer( recBuffer, 0, len );
    }

    //==============================================================================
    // SampleRecorder

    SampleRecorder::SampleRecorder( AudIO::Design& D, const juce::String& Name )
        : AudIO::Object( 2, 2, 0 )
        , AudIO::DawRouting( D, AudIO::BusType::Object, AudIO::BusWidth::Stereo )
    {
        setName( Name.toStdString() );
        AudIO::DawRouting::setID( getId() );
    }

    SampleRecorder::~SampleRecorder()
    {
        cancelPendingUpdate();
    }

    void SampleRecorder::reset()
    {
        endRecording();
        endPlayback();
        clearBuffer();
    }

    void SampleRecorder::startRecording( bool FromBeginning )
    {
        beginRecording( FromBeginning, false );
    }

    void SampleRecorder::stopRecording()
    {
        endRecording();
    }

    void SampleRecorder::saveRecording( const std::string& FilePath )
    {
        if( !saveToWav( juce::File( FilePath ) ) )
            juce::Logger::writeToLog( "[emu] SampleRecorder: could not save " + juce::String( FilePath ) );
    }

    void SampleRecorder::startPreview( double FromSeconds ) { beginPlayback( FromSeconds ); }
    void SampleRecorder::stopPreview() { endPlayback(); }

    void SampleRecorder::prepareToPlay( uint32_t samplerate, uint32_t blocksize )
    {
        AudIO::Object::prepareToPlay( samplerate, blocksize );
        prepareRecorder( samplerate, (int)blocksize );
    }

    void SampleRecorder::process( AudIO::Buffer<float>& buffer )
    {
        const int n = (int)buffer.getBlockSize();
        if( !isEnabled() )
        {
            buffer = 0;
            return;
        }
        renderBlock( buffer.getInputBuffer( 0 ), buffer.getInputBuffer( 1 ), buffer.getOutputBuffer( 0 ), buffer.getOutputBuffer( 1 ), n );
    }

    void SampleRecorder::handleAsyncUpdate()
    {
        const int e = pending.exchange( 0 );
        if( e & kStarted )
            onRecorderStarted();
        if( e & kStopped )
        {
            onRecorderStopped();
            onRecorderLengthChanged( getRecordedLength() );
        }
        if( e & kPreviewStarted )
            onRecorderPreviewStarted();
        if( e & kPreviewStopped )
            onRecorderPreviewStopped();
    }

    //==============================================================================
    // Looper

    Looper::Looper( AudIO::Design& D, const juce::String& Name )
        : AudIO::Object( 2, 2, 0 )
        , AudIO::DawRouting( D, AudIO::BusType::Object, AudIO::BusWidth::Stereo )
    {
        setName( Name.toStdString() );
        AudIO::DawRouting::setID( getId() );
        looping = true;
    }

    Looper::~Looper() = default;

    void Looper::setRecord( bool Record )
    {
        if( Record )
            start( false, false );
        else
            endRecording();
    }

    void Looper::setPlay()
    {
        if( isRecording() )
            endRecording();
        play( 0.0 );
    }

    void Looper::start( bool FromBeginning, bool SkipThresholdDetection )
    {
        if( undoBuffer.getNumSamples() < capacity.load() )
            undoBuffer.setSize( 2, capacity.load() );
        snapshotPending = true;
        beginRecording( FromBeginning, SkipThresholdDetection );
    }

    void Looper::play( double FromSeconds )
    {
        beginPlayback( FromSeconds );
    }

    void Looper::stop()
    {
        endRecording();
        endPlayback();
    }

    void Looper::clear()
    {
        stop();
        clearBuffer();
        undoLength = redoLength = 0;
    }

    void Looper::recordingStarted()
    {
        // Snapshot happens on the audio thread right as recording starts, so
        // undo returns to the loop as it was before this pass.
        if( snapshotPending.exchange( false ) )
        {
            const int len = recordedSamples.load();
            if( undoBuffer.getNumSamples() < len )
                return; // Not preallocated; skip rather than allocate here.
            for( int c = 0; c < 2; ++c )
                undoBuffer.copyFrom( c, 0, recBuffer, c, 0, len );
            undoLength = len;
        }
    }

    void Looper::undo()
    {
        stop();
        if( undoBuffer.getNumSamples() == 0 )
            return;
        redoBuffer.makeCopyOf( recBuffer );
        redoLength = recordedSamples.load();
        for( int c = 0; c < 2; ++c )
            recBuffer.copyFrom( c, 0, undoBuffer, c, 0, juce::jmin( undoBuffer.getNumSamples(), recBuffer.getNumSamples() ) );
        recordedSamples = undoLength;
    }

    void Looper::redo()
    {
        stop();
        if( redoBuffer.getNumSamples() == 0 )
            return;
        recBuffer.makeCopyOf( redoBuffer );
        recordedSamples = redoLength;
    }

    void Looper::save( const juce::String& FilePath )
    {
        if( !saveToWav( juce::File( FilePath ) ) )
            juce::Logger::writeToLog( "[emu] Looper: could not save " + FilePath );
    }

    void Looper::prepareToPlay( uint32_t samplerate, uint32_t blocksize )
    {
        AudIO::Object::prepareToPlay( samplerate, blocksize );
        AudIO::DawInserts::prepareToPlay( samplerate, blocksize );
        prepareRecorder( samplerate, (int)blocksize );
        undoBuffer.setSize( 2, capacity.load() );
        undoBuffer.clear();
    }

    void Looper::process( AudIO::Buffer<float>& buffer )
    {
        const int n = (int)buffer.getBlockSize();
        if( !isEnabled() )
        {
            buffer = 0;
            return;
        }
        renderBlock( buffer.getInputBuffer( 0 ), buffer.getInputBuffer( 1 ), buffer.getOutputBuffer( 0 ), buffer.getOutputBuffer( 1 ), n );
        AudIO::DawInserts::process( buffer );
    }
}
