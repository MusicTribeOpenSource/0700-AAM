#include <thread>
#include "DawEngine.h"
#include "DawSequence.h"

#if defined( AUDIO_OBJECT_AUDIO_FILE_RECORDER ) && ( AUDIO_OBJECT_AUDIO_FILE_RECORDER > 0 )
    #include "Objects/DawRecorder.h"
#endif
#if defined( AUDIO_OBJECT_AUDIO_FILE_PLAYBACK ) && ( AUDIO_OBJECT_AUDIO_FILE_PLAYBACK > 0 )
    #include "../Objects/PlayBack.h"
#endif

#include "../AudIO_airwindows.h"

#define TEST_PATCH 0

#if defined( TEST_PATCH ) && ( 0 < TEST_PATCH )
    #include "../Objects/SineWave.h"
#endif

namespace AudIO
{
#define MEM_POOL_SIZE ( 2500 )
#define BLOCK_SIZE    ( 480 )

    // NOTE::
    // This buffer will be used by AudIO to create audio graph.
    // Ensure that this has sufficient storage to accomodate worst case design requirement.
    float _audioPoolMemBuffer[( MEM_POOL_SIZE * BLOCK_SIZE )];

    const std::string              _emptyStr{ "" };
    const std::vector<std::string> _emptyVectorStr;
    const std::vector<ParamInfo>   _emptyVectorParamInfo;
    const MeterData                _emptyMeterData = { {}, {} };

    /// DawEngine...
    DawEngine::DawEngine( unsigned int ninputs, unsigned int noutputs, unsigned int nsendreturns, unsigned int nsubmix, unsigned int naux )
        : Design( ninputs, noutputs )
    {
        initAirWindowsEffects();

        // Housekeeping
        Design::setName( "Engine" );

        // Initialize memory management for AudIO...
        AudIO::MemoryPool<float>::get().initialize( &_audioPoolMemBuffer[0], MEM_POOL_SIZE, BLOCK_SIZE );

        // Create and Initialize Bus Manager
        _busMgr = std::make_unique<DawBusManager>( *this, ninputs, noutputs, nsendreturns, nsubmix );

        // Make Connections to the physical port...
        mainOut    = _busMgr->getDefaultStereoOutputDevice();
        inputChain = _busMgr->getDefaultStereoInputDevice();
        setDesignInput( inputChain );
        setDesignOutput( mainOut );

        addDefaultMeters();

#if defined( AUDIO_DAW_EXTENSIONS ) && ( AUDIO_DAW_EXTENSIONS > 0 )
        // Pre-Reserve memory for sequences, meters and preview objects...
        _sequences.reserve( 128 );
#endif

#if defined( AUDIO_OBJECT_AUDIO_FILE_PLAYBACK ) && ( AUDIO_OBJECT_AUDIO_FILE_PLAYBACK > 0 )
        _AudioPreviews.reserve( 4 );
#endif

#if defined( TEST_PATCH ) && ( 0 < TEST_PATCH )
        // Temporary object to test...
        auto sine1 = createObject( "SineWave" );
        auto sine2 = createObject( "SineWave" );
        auto sine3 = createObject( "SineWave" );
        if( auto pSine1 = dynamic_cast<AudIO::SineWave*>( getObject( sine1 ) ) )
            pSine1->setParameter( SineWave::kParamFreq, 415.0 );
        if( auto pSine2 = dynamic_cast<AudIO::SineWave*>( getObject( sine2 ) ) )
            pSine2->setParameter( SineWave::kParamFreq, 685.0 );
        if( auto pSine3 = dynamic_cast<AudIO::SineWave*>( getObject( sine3 ) ) )
            pSine3->setParameter( SineWave::kParamFreq, 1250.0 );

            //sendToBus( _busMgr->getMonoOutput( 0 ), sine1, 0.2f );
            //sendToBus( _busMgr->getMonoOutput( 1 ), sine2, 0.6f );
            //sendToBus( _busMgr->getStereoOutput( 0 ), sine3, 0.4f );
#endif

        auto availMem = AudIO::MemoryPool<float>::get().getUsed();

        _isInitialized = true;
        _devmgr.setAudioCallback( this );

        setBypass( false );
    }

    DawEngine::~DawEngine()
    {
        removeAllMeters();

#if defined( AUDIO_OBJECT_AUDIO_FILE_PLAYBACK ) && ( AUDIO_OBJECT_AUDIO_FILE_PLAYBACK > 0 )
        for( auto ids : _AudioPreviews )
            removeFromBus( mainOut, ids );
#endif

        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    }

    /// Effects.
    const std::map<const std::string, ObjectInfo>& DawEngine::getAudioEffects()
    {
        return ObjectFactory::Get().GetObjectsInfo();
    }

    /// Memory API's.
    float DawEngine::getFeeMemory() const // In Percentage.
    {
        auto free  = AudIO::MemoryPool<float>::get().getFree();
        auto total = AudIO::MemoryPool<float>::get().getCapacity();

        return (float)free / (float)total;
    }

    float DawEngine::getUsedMemory() const // In Percentage.
    {
        auto used  = AudIO::MemoryPool<float>::get().getUsed();
        auto total = AudIO::MemoryPool<float>::get().getCapacity();

        return (float)used / (float)total;
    }

    int DawEngine::getTotalMemory() const // In no of audio pools.
    {
        return AudIO::MemoryPool<float>::get().getCapacity();
    }

    void DawEngine::addDefaultMeters()
    {
        // Meters...
        addMeter( inputChain );
        addMeter( mainOut );
        addMeter( _busMgr->getMonoOutputID( 0 ) );
        addMeter( _busMgr->getMonoOutputID( 1 ) );
        addMeter( _busMgr->getStereoOutputID( 0 ) );
    }

    /// Audio Design
    void DawEngine::reset()
    {
        auto availMem = AudIO::MemoryPool<float>::get().getUsed();
        removeAllMeters();

#if defined( AUDIO_OBJECT_AUDIO_FILE_PLAYBACK ) && ( AUDIO_OBJECT_AUDIO_FILE_PLAYBACK > 0 )
        // Stop all previews...
        deleteAllAudioPreviews();
#endif

#if defined( AUDIO_OBJECT_AUDIO_FILE_RECORDER ) && ( AUDIO_OBJECT_AUDIO_FILE_RECORDER > 0 )
        // Stop recorder...
        if( recorder )
        {
            recorder->reset();
            recorder->reInitialize();
        }
#endif

#if defined( AUDIO_DAW_EXTENSIONS ) && ( AUDIO_DAW_EXTENSIONS > 0 )
        // Clear our playing seqeunce...
        playingSequence = nullptr;
        // Clear sequence.
        _sequences.clear();
#endif

        auto availMemAfterDestruction = AudIO::MemoryPool<float>::get().getUsed();

        /// Reset Bus Manager
        _busMgr->reset();

        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );

        // Add default meters...
        addDefaultMeters();
        auto availMemAfterReset = AudIO::MemoryPool<float>::get().getUsed();
    }

    bool DawEngine::addAudioDevice( const std::string& name, IAudioDevice* device, bool /*master*/ )
    {
        return _devmgr.addAudioDevice( name.c_str(), device, true );
    }

    void DawEngine::startAudioDevice( unsigned int SampleRate, unsigned int BlockSize ) const
    {
        unsigned int blkSize = utils::min( BlockSize, (unsigned int)BLOCK_SIZE );
        _devmgr.start( SampleRate, blkSize );
    }

    void DawEngine::pauseAudioDevice()
    {
#if defined( AUDIO_DAW_EXTENSIONS ) && ( AUDIO_DAW_EXTENSIONS > 0 )
        if( playingSequence )
            playingSequence->pause();
#endif
        _devmgr.pause();
    }

    void DawEngine::stopAudioDevice()
    {
#if defined( AUDIO_DAW_EXTENSIONS ) && ( AUDIO_DAW_EXTENSIONS > 0 )
        if( playingSequence )
            playingSequence->stop();
#endif
        _devmgr.stop();
    }

#if defined( AUDIO_DAW_EXTENSIONS ) && ( AUDIO_DAW_EXTENSIONS > 0 )
    DawSequence* DawEngine::createSequence( const std::string& Name )
    {
        if( !isSeqExists( Name ) )
        {
            auto seq  = std::make_unique<DawSequence>( Name );
            auto pSeq = seq.get();
            _sequences.push_back( std::move( seq ) );
            return pSeq;
        }
        return nullptr;
    }

    int DawEngine::deleteSequence( const std::string& Name )
    {
        return deleteSequence( getSequenceIndex( Name ) );
    }

    int DawEngine::deleteSequence( uint32_t Index )
    {
        if( utils::isPositiveAndBelow( Index, (uint32_t)_sequences.size() ) )
        {
            { // Remove meter if added...
                auto seq = getSequence( Index );
                seq->stop();
                // Stop playback, if this is the playing sequence...
                if( playingSequence == seq )
                {
                    stopSequence();
                    playingSequence = nullptr;
                }
            }
            // Delete sequence now.
            auto it = _sequences.begin() + Index;
            _sequences.erase( it );
            return 0;
        }
        return -1;
    };

    bool DawEngine::isSeqExists( const std::string& Name )
    {
        return getSequence( Name );
    }

    int DawEngine::getSequenceIndex( const std::string& Name ) const
    {
        int index = 0;
        for( const auto& it : _sequences )
        {
            if( it->getName() == Name )
                return index;
            ++index;
        }
        return -1;
    }

    DawSequence* DawEngine::getSequence( const std::string& Name ) const
    {
        for( const auto& it : _sequences )
            if( it->getName() == Name )
                return it.get();
        return nullptr;
    }

    DawSequence* DawEngine::getSequence( uint32_t Index ) const
    {
        if( utils::isLessOrEuqal( Index, (uint32_t)_sequences.size() ) )
        {
            return _sequences[Index].get();
        }
        return nullptr;
    }

    const std::vector<std::unique_ptr<DawSequence>>& DawEngine::getSequences() const
    {
        return _sequences;
    }

    std::vector<std::string> DawEngine::getSequenceNames() const
    {
        std::vector<std::string> seq;
        for( const auto& it : _sequences )
        {
            seq.push_back( it->getName() );
        }
        return seq;
    }

    /// Transport API's
    void DawEngine::playSequence( const std::string& Name )
    {
        playSequence( getSequence( Name ) );
    }

    void DawEngine::playSequence( uint32_t Index )
    {
        playSequence( getSequence( Index ) );
    }

    void DawEngine::playSequence( DawSequence* Seq )
    {
        if( Seq && playingSequence != Seq )
        {
            stopSequence();
            playingSequence = Seq;
            playingSequence->play();
        }
        else
        {
            int i = 0;
        }
    }

    void DawEngine::stopSequence()
    {
        if( playingSequence )
        {
            playingSequence->stop();
            playingSequence = nullptr;
        }
    }

    void DawEngine::pauseSequence()
    {
        if( playingSequence )
            playingSequence->pause();
    }

    void DawEngine::resumeSequence()
    {
        if( playingSequence )
            playingSequence->resume();
    }

    /// Track api's
    bool DawEngine::isTrackExists( uint32_t Sequence, const std::string& Name )
    {
        if( auto seq = getSequence( Sequence ) )
            return seq->getTrack( Name ) ? true : false;
        return false;
    }

    int DawEngine::getTrackIndex( uint32_t Sequence, const std::string& Name ) const
    {
        if( auto seq = getSequence( Sequence ) )
            return seq->getTrackIndex( Name );
        return -1;
    }

    DawTrack* DawEngine::getTrack( uint32_t Sequence, const std::string& Name ) const
    {
        if( auto seq = getSequence( Sequence ) )
            return seq->getTrack( Name );
        return nullptr;
    }

    DawTrack* DawEngine::getTrack( uint32_t Sequence, uint32_t Index ) const
    {
        if( auto seq = getSequence( Sequence ) )
            return seq->getTrack( Index );
        return nullptr;
    }

    const std::vector<std::unique_ptr<DawTrack>>& DawEngine::getTracks( uint32_t Sequence ) const
    {
        static std::vector<std::unique_ptr<DawTrack>> _empty;
        if( auto seq = getSequence( Sequence ) )
            return seq->getTracks();
        return _empty;
    }

    std::vector<std::string> DawEngine::getTrackNames( uint32_t Sequence ) const
    {
        if( auto seq = getSequence( Sequence ) )
            return seq->getTrackNames();
        return _emptyVectorStr;
    }

    // Iterators...
    void DawEngine::forAllSequences( std::function<void( DawSequence* S )>& func )
    {
        if( func )
        {
            for( const auto& it : _sequences )
            {
                func( it.get() );
            }
        }
    }

    void DawEngine::forAllTracksInSequence( const uint32_t Seq, std::function<void( DawTrack* S )>& func )
    {
        if( auto seq = getSequence( Seq ) )
        {
            seq->forAllTracks( func );
        }
    }
#endif

#if defined( AUDIO_OBJECT_AUDIO_FILE_PLAYBACK ) && ( AUDIO_OBJECT_AUDIO_FILE_PLAYBACK > 0 )
    unsigned int DawEngine::createAudioPreview( const std::string& File )
    {
        unsigned int ID = createObject( "AudioPlayback" );
        addMeter( ID );
        sendToBus( mainOut, ID, 1.f );

        _AudioPreviews.push_back( ID );

        if( auto pObj = dynamic_cast<AudioPlayback*>( getObject( ID ) ) )
        {
            pObj->load( File );
        }
        return ID;
    }

    void DawEngine::deleteAudioPreview( unsigned int ID )
    {
        if( auto pObj = dynamic_cast<AudioPlayback*>( getObject( ID ) ) )
        {
            pObj->stop();
            removeFromBus( mainOut, ID );
            removeObject( ID );
            removeMeter( ID );
            _AudioPreviews.erase( std::find( _AudioPreviews.begin(), _AudioPreviews.end(), ID ) );
            std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
        }
    }

    void DawEngine::deleteAllAudioPreviews()
    {
        for( auto it : _AudioPreviews )
            deleteAudioPreview( it );
        _AudioPreviews.clear();
    }

    void DawEngine::playAudioPreview( unsigned int ID, bool FromBeginning )
    {
        if( auto pObj = dynamic_cast<AudioPlayback*>( getObject( ID ) ) )
            pObj->play( FromBeginning );
    }

    void DawEngine::stopAudioPreview( unsigned int ID )
    {
        if( auto pObj = dynamic_cast<AudioPlayback*>( getObject( ID ) ) )
            pObj->stop();
    }

    void DawEngine::setAudioPreviewGainAndPan( unsigned int ID, float Gain, float Pan )
    {
        if( auto pObj = dynamic_cast<AudioPlayback*>( getObject( ID ) ) )
            pObj->setVolumeAndPan( Gain, Pan );
    }

    void DawEngine::setAudioPreviewMute( unsigned int ID, bool Mute )
    {
        if( auto pObj = dynamic_cast<AudioPlayback*>( getObject( ID ) ) )
            pObj->setMute( Mute );
    }

    void DawEngine::setAudioPreviewBypass( unsigned int ID, bool Bypass )
    {
        if( auto pObj = dynamic_cast<AudioPlayback*>( getObject( ID ) ) )
            pObj->setBypass( Bypass );
    }

    void DawEngine::setAudioPreviewLoop( unsigned int ID, bool Loop )
    {
        if( auto pObj = dynamic_cast<AudioPlayback*>( getObject( ID ) ) )
            pObj->setLooping( Loop );
    }

    const MeterData& DawEngine::getAudioPreviewMeter( unsigned int ID ) const
    {
        return readMeter( ID );
    }
#endif

#if defined( AUDIO_OBJECT_AUDIO_FILE_RECORDER ) && ( AUDIO_OBJECT_AUDIO_FILE_RECORDER > 0 )
    void DawEngine::initializeRecorder()
    {
        if( recorder )
            return;

        // Recorder Object...
        recorder = dynamic_cast<DRecorder*>( getObject( createObject( std::make_unique<DRecorder>( *this ) ) ) );
        if( recorder )
        {
            recorder->initAudioRouting();
            recorder->connectInputTo( _busMgr->getDefaultStereoInputDevice() );
            //recorder->setMonitor( true );

            //auto& eff = getAudioEffects();
            //recorder->addEffect( "Melt", 0 );
            //recorder->addEffect( "NCSeventeen", 1 );
            //auto& effPrams = recorder->getEffectParameters( 0 );
        }
    }

    void DawEngine::setEnableRecorder( bool Enable )
    {
        if( recorder )
            recorder->setBypass( !Enable );
    }

    void DawEngine::setupRecorder( double timeInSeconds, float Gain, float Pan, bool Looping, bool Overdub )
    {
        if( recorder )
        {
            recorder->reset();
            recorder->setVolumeAndPan( Gain, Pan );
            recorder->setLength( timeInSeconds );
            recorder->setLooping( Looping );
            recorder->setOverdub( Overdub );
        }
    }

    void DawEngine::setMonitorOn( bool Monitor )
    {
        if( recorder )
            recorder->setMonitor( Monitor );
    }

    void DawEngine::setRecorderLoop( bool Looping )
    {
        if( recorder )
            recorder->setLooping( Looping );
    }

    void DawEngine::setRecorderOverdub( bool Overdub )
    {
        if( recorder )
            recorder->setOverdub( Overdub );
    }

    void DawEngine::setRecorderDirection( bool ForwardDirection )
    {
        if( recorder )
            recorder->setDirection( ForwardDirection );
    }

    void DawEngine::setRecorderPan( float Pan )
    {
        if( recorder )
            recorder->setVolume( Pan );
    }

    void DawEngine::setRecorderGain( float Gain )
    {
        if( recorder )
            recorder->setVolume( Gain );
    }

    void DawEngine::setRecorderGainDB( float GainDB )
    {
        if( recorder )
            recorder->setVolumeDB( GainDB );
    }

    void DawEngine::setRecroderThreshold( float Gain )
    {
        if( recorder )
            recorder->setThreshold( Gain );
    }
    void DawEngine::setRecroderThresholdDB( float DB )
    {
        if( recorder )
            recorder->setThresholdDB( DB );
    }

    void DawEngine::startRecorder( int Mode, bool ForwardDirection, bool FromBeginning )
    {
        if( recorder )
        {
            recorder->start( (AudioRecorder::Mode)Mode, ForwardDirection ? AudioRecorder::FORWARD : AudioRecorder::REVERSE, FromBeginning );
        }
    }

    void DawEngine::stopRecorder()
    {
        if( recorder )
            recorder->stop();
    }

    void DawEngine::saveRecording( const std::string& FilePath )
    {
        if( recorder )
            recorder->save( FilePath );
    }

    float DawEngine::getRecorderGain() const { return recorder ? recorder->getVolume() : 0.f; }
    float DawEngine::getRecorderProgress() const { return recorder ? recorder->readProgress() : 0.f; }
    float DawEngine::getRecorderThreshold() const { return recorder ? recorder->getThreshold() : 0.f; }
    float DawEngine::getRecorderThresholdDB() const { return recorder ? recorder->getThresholdDB() : -100.f; }

    bool DawEngine::isRecorderEnabled() const { return recorder ? !recorder->isBypassed() : false; }
    bool DawEngine::isRecorderMonitorOn() const { return recorder ? recorder->isMonitorOn() : false; }
    bool DawEngine::isRecorderLooping() const { return recorder ? recorder->isLooping() : false; }
    bool DawEngine::isRecorderOverdubbing() const { return recorder ? recorder->isOverdubbing() : false; }
    bool DawEngine::isRecorderInForwardDirection() const { return recorder ? recorder->isForwardDirection() : false; }
    bool DawEngine::isRecorderRunning() const { return recorder ? recorder->isRecording() : false; }
    bool DawEngine::isRecorderPreviewing() const { return recorder ? recorder->isPreviewing() : false; }

    const MeterData& DawEngine::getRecorderInputMeter() { return recorder ? recorder->readInputMeter() : _emptyMeterData; }
    const MeterData& DawEngine::getRecorderOutputMeter() { return recorder ? recorder->readOutputMeter() : _emptyMeterData; }

    const std::vector<std::vector<float>>& DawEngine::getRecorderChannelsData()
    {
        static std::vector<std::vector<float>> empty;
        if( recorder )
            return recorder->getRecorderData();
        return empty;
    }

    void DawEngine::startRecorderPreview( double fromStartTimeInSeconds )
    {
        if( recorder )
            recorder->startPreview( fromStartTimeInSeconds );
    }
    void DawEngine::stopRecorderPreview()
    {
        if( recorder )
            recorder->stopPreview();
    }

    void DawEngine::setRecorderListeners( DawRecorderListener& Listener )
    {
        if( recorder )
        {
            recorder->onRecorderStarted        = Listener.onRecorderStarted;
            recorder->onRecorderStopped        = Listener.onRecorderStopped;
            recorder->onRecorderPaused         = Listener.onRecorderPaused;
            recorder->onRecorderResumed        = Listener.onRecorderResumed;
            recorder->onRecorderLengthChanged  = Listener.onRecorderLengthChanged;
            recorder->onRecorderPreviewStarted = Listener.onRecorderPreviewStarted;
            recorder->onRecorderPreviewStopped = Listener.onRecorderPreviewStopped;
        }
    }
#endif

    void DawEngine::dumpAllMeters()
    {
        //readAllMeters( [&]( const std::string& name, AudIO::MeterData* data ) {
        //    std::cout << "\nM: " << name << " L: " << data->rms[0] << " R: " << data->rms[1];
        //} );

#if defined( AUDIO_OBJECT_AUDIO_FILE_RECORDER ) && ( AUDIO_OBJECT_AUDIO_FILE_RECORDER > 0 )
        if( recorder )
        {
            auto& in  = recorder->readInputMeter();
            auto& out = recorder->readOutputMeter();
            //std::cout << "\nM: " << recorder->getName() << " [  IN] L: " << in.rms[0] << " R: " << in.rms[1];
            //std::cout << "\nM: " << recorder->getName() << " [ OUT] L: " << out.rms[0] << " R: " << out.rms[1];
        }
#endif
    }

    void DawEngine::readMeters( const std::function<void( const std::string& meterName, MeterData& data )>& MeterCallback )
    {
        Design::readAllMeters( MeterCallback );
    }

};
