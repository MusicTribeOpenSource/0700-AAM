#pragma once
static sigslot::signal<>     sampleNeedRepaint;
static sigslot::signal<bool> stateNeedChage;

namespace kick_ui
{
    class AudioProgramGrid;
    class AudioGridComponent;

    class AudioGridData : public mt::MTAudioInterfaceListener, public juce::DeletedAtShutdown
    {
        SIGNALS : sigslot::signal<> audioGridChanged;
        SIGNALS : sigslot::signal<> audioGridPlayHeadChanged;

    public:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        const mt::MTSelected& aeSel{ ae.getSelected() };
        const mt::MTPlayhead& playHead{ ae.getPlayhead() };
        const mt::MTPlayhead& trackPlayHead{ ae.getTrackPlayhead() };

        static AudioGridData& Instance()
        {
            static auto* instance = new AudioGridData;
            return *instance;
        }

        // Refresh all data.
        void         refresh( bool Notify = true );
        inline float getWidth() const { return (float)getNumBars() * zoom.width; }
        float        getZoomWidth() const { return zoom.width; }

        void recalculate( bool Notify );

        void onTrackSelectionChange( const mt::MTSelected& /*Selected*/ ) override;
        void onPlayHeadChanged( [[maybe_unused]] mt::MTBarNBeats BnB ) override;
        void onTrackProgramChanged( int, int ) override;

        inline float getBarWidth() const { return getWidth() / (float)getNumBars(); }
        inline float getBeatWidth( const int Bar ) { return getBarWidth() / (float)getBeatsPerBar( Bar ); }
        inline float getQLWidth( const int Bar ) const { return getBarWidth() / (float)getNumQlPerBar( Bar ); }

        // Data...
        inline int getNumBars() const { return numBars; }
        inline int getNumQlPerBar( const int Bar ) const { return tempoMgr ? tempoMgr->getQuantizationPerBar( Bar ) : 16; }
        inline int getBeatsPerBar( const int Bar ) { return tempoMgr ? tempoMgr->getBeatsPerBar( Bar ) : 4; }
        inline int getTicksPerBar( const int Bar ) { return tempoMgr ? tempoMgr->getTicksPerBar( Bar ) : 0; }
        inline int getTicksPerQuantization( const int Bar ) { return tempoMgr ? tempoMgr->getTicksPerQuantization( Bar ) : 0; }
        inline int getTicksPerBeat( const int Bar ) { return tempoMgr ? tempoMgr->getTicksPerBeat( Bar ) : 0; }
        // Zoom...
        void setZoomFactor( float horozontalFactor );
        void setHorizantalZoomFactor( float horozontalFactor );
        void setDefaultZoom();

        int getXFromTime( int Time );
        int getTimeFromX( int X, bool Quantize = true );

        int getBarFromX( int X ) const;
        int getXFromBar( int Bar ) const;

        float getXPerTick( const int Bar ) { return getBarWidth() / (float)getTicksPerBar( Bar ); };
        float getTicksForX( const int Bar, const float X ) { return ( X * (float)getTicksPerBar( Bar ) ) / getBarWidth(); };

        int getQuantizedXFromX( int X ) const;

        // Grid Data API's
        unsigned int getTicksUptoBar( unsigned int Bar );
        unsigned int getTicksPerQuantizationAt( unsigned int Ticks );
        unsigned int getTicksPerTimebaseAt( unsigned int Ticks, const juce::String& Timebase );
        unsigned int getTicksPerTimebaseAt( unsigned int Ticks, mt::MTTimeBase Timebase );
        unsigned int getTicksPerBarAt( unsigned int Ticks );

        const mt::MTTempoMgr* getTempoMgr() const { return tempoMgr; }

        int            getTimeBase( mt::MTTimeBase Timebase );
        mt::MTTimeBase getTimeBase( const juce::String& Timebase );
        int            getTimeBaseIndex( int TimeDiv );

    private:
        AudioGridData();
        ~AudioGridData();

    private:
        struct zoomFactor
        {
            float width{ 250.5f }; // Minimum pixels per BAR.
        } zoom;

        static const float gridWidth;
        static const float gridHeight;

        mt::MTTempoMgr* tempoMgr{ nullptr };

        unsigned int numBars{ 1 };
        unsigned int totalTicks{ 0 };

        double seqLengthMs{ 0.0 };
    };

    class AudioPlayhead : public juce::Component
        , juce::AsyncUpdater
    {
    public:
        AudioPlayhead();
        ~AudioPlayhead() = default;

        void  drawAt( float X, int MaxWidth );
        float getPlayHeadPosition() const { return normX; }

    private:
        void handleAsyncUpdate() override;
        void paint( juce::Graphics& g ) override;

    private:
        float normX{ 0 };
    };

    class IAudioTools : public juce::MouseListener
    {
    public:
        explicit IAudioTools( AudioGridComponent& GridComp, AudioGridData& GridData, juce::Viewport& ViewPort, AudioGridToolType Type )
            : type( Type ), gridData( GridData ), viewPort( ViewPort ), gridComp( GridComp )
        {
        }
        virtual void             paint( juce::Graphics& g ) = 0;
        inline AudioGridToolType getType() { return type; }

        virtual void mouseDown( const juce::MouseEvent& event )
        {
            mDownTime = gridData.getTimeFromX( event.getMouseDownX(), false );
            x1        = event.position.x;
            if( type != AudioGridToolType::GridMagnifyTool )
            {
                if( auto pProg = dynamic_cast<mt::MTAudioProgram*>( ae.getProgram( ae.getSelectedProgram() ) ) )
                {
                    pProg->selectAllSamples( false );
                    pProg->selectAudioSample( mDownTime );
                    sampleNeedRepaint();
                }
            }
        }

    protected:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        const mt::MTSelected& aeSelected{ mt::getAudioInterface().getSelected() };
        AudioGridComponent&   gridComp;
        AudioGridData&        gridData;
        juce::Viewport&       viewPort;

        int mDownTime{ 0 };
        int x1{ 0 }, x2{ 0 };

    private:
        const AudioGridToolType type;
    };

    class BrowseSamplePopup : public juce::Component
        , public mt::MTAudioInterfaceListener
    {
    public:
        std::function<void( const juce::String& )> onSampleSelected;

        struct BBT
        {
            int bar{ 1 };
            int beat{ 1 };
            int tick{ 1 };
        };

        BrowseSamplePopup();
        ui::KickListBox           availableSamples{ ui::KickListBox::withoutTitleBar, "SAMPLES", {} };
        ui::KickNumber            barNumber{ "BAR", BAR::MIN, BAR::MAX, BAR::MIN, BAR::STEP, true };
        ui::KickNumber            beatNumber{ "BEAT", BAR::MIN, BAR::MAX, BAR::MIN, BAR::STEP, true };
        ui::KickNumber            tickNumber{ "Tick", BAR::MIN, BAR::MAX, BAR::MIN, BAR::STEP, true };
        std::vector<juce::String> samples;

        juce::String& getSelectedSample() { return selectedSample; }

        BBT getBBTValue() { return bbtValue; }

    private:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        juce::String          selectedSample{ "" };
        void                  resized() override;
        void                  visibilityChanged() override;
        mt::MTBarNBeats       BnBValue;
        void                  onSequenceListChanged() override{};
        void                  onPlayHeadChanged( mt::MTBarNBeats BnB ) override;
        void                  refresh();
        void                  paint( juce::Graphics& g ) override
        {
            g.fillAll( ui::Theme::backgroundColour( 1 ) );
        }

        BBT bbtValue;
    };

    class AudioScaleComponent : public juce::Component
    {
    public:
        std::function<void( float Value )> onMouseClick;

        AudioScaleComponent()
        {
            gridData.audioGridChanged.connect( [&] { repaint(); } );
        }

    private:
        void mouseUp( const juce::MouseEvent& ) override;
        void paint( juce::Graphics& g ) override;

        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        AudioGridData&        gridData{ AudioGridData::Instance() };
    };

    class AudioGridComponent : public juce::Component
        , public juce::AsyncUpdater
    {
    public:
        AudioGridComponent( juce::Viewport& );
        ~AudioGridComponent();
        void mouseUp( const juce::MouseEvent& event ) override;
        void mouseMove( const juce::MouseEvent& event ) override;
        void mouseDrag( const juce::MouseEvent& event ) override;
        void mouseDown( const juce::MouseEvent& event ) override;
        void toolTypeChanged( AudioGridToolType toolType );
        void refresh();

    private:
        IAudioTools*                                   activeTool{ nullptr };
        std::map<AudioGridToolType, IAudioTools*>      tools;
        mt::MTAudioInterface&                          ae{ mt::getAudioInterface() };
        mt::MTAudioProgram*                            pAudio;
        AudioGridData&                                 gridData{ AudioGridData::Instance() };
        std::vector<std::unique_ptr<ui::WaveRenderer>> wavComps;
        juce::Viewport&                                gridViewPort;

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void handleAsyncUpdate() override;
        void visibilityChanged() override;
    };

    class AudioGridParameterComponent : public juce::Component
    {
    public:
        AudioGridParameterComponent( mt::MTAudioProgram*& AudioPgm );

        void setSelectedParam( const juce::String& Param );
        void clearDataForSelectedParam();
        void setSelectedTool( AudioGridToolType ToolType );

    private:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        AudioGridData&        gridData{ AudioGridData::Instance() };
        mt::MTAudioProgram*&  pAudio;
        AudioGridToolType     toolType{ AudioGridToolType ::GridPencilTool };
        float                 ticksWidth;
        juce::String          paramName{ "None" };

        void paint( juce::Graphics& g ) override;
        void mouseUp( const juce::MouseEvent& event ) override;
        void mouseDrag( const juce::MouseEvent& event ) override;
        void handleMouseEvent( const juce::MouseEvent& );
    };

    class AudioProgramGrid : public juce::Component
        , public ui::KickViewport::Listener
        , public juce::Timer
    {
    public:
        AudioProgramGrid( mt::MTAudioProgram*& Audio );
        ~AudioProgramGrid() = default;

        void                                                                refresh();
        void                                                                handleCommand( CPktMgr::HWMsg_t& );
        void                                                                timerCallback() override;
        juce::Component&                                                    getHeaderComponent() { return headerTools; }
        std::function<void( int sampleIndex, int bar, int beat, int tick )> onSampleChanges;
        void                                                                updateAudioPlayhead();
        void                                                                nudgeSelectedSample( juce::Array<juce::ValueTree>& SelSample, int Value, double qlWidth );
        ui::KickListBox                                                     automation{ ui::KickListBox::withoutTitleBar, "", {} };
        ui::KickImageButton                                                 ecBtn{ "E", "UP", "Down" };
        ui::KickButton                                                      clearAuto{ "CLEAR AUTO" };
        int                                                                 getStartTick( int bar, int beat, int tick );
        void                                                                setZoomInZoomOut( int xValue, bool isZoomIn );

    private:
        void resized() override;
        void visibilityChanged() override;
        void paint( juce::Graphics& ) override;

        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        const mt::MTSelected& aeSel{ mt::getAudioInterface().getSelected() };
        const mt::MTPlayhead& audioPlayHead{ ae.getTrackPlayhead() };
        void                  updateOnViewPositionChange( ui::KickViewport* pViewport );

        mt::MTAudioProgram*& pAudio;
        ui::KickViewport     scaleViewport;
        ui::KickViewport     audiogridViewport;
        ui::KickViewport     paramViewport;
        ui::KickViewport     phViewport;

        AudioGridData&              gridData{ AudioGridData::Instance() };
        AudioGridComponent          gridComponent;
        AudioScaleComponent         scaleComp;
        AudioHeaderTools            headerTools;
        AudioGridParameterComponent paramComp{ pAudio };

        AudioPlayhead     playhead;
        ui::GridSettings& gridSettings{ ui::GridSettings::Instance() };
        int               gridPage{ 0 };

        double getSampleLengthInTicks( const juce::String& Source, int Loostart, int LoopEnd );

    private:
        std::vector<juce::Component*> components;
        ui::KickButton                sampleList{ "BROWSE SAMPLES " };
        ui::KickNumber                fadeIn{ "FADE IN", 0, 100, 0 };
        ui::KickNumber                fadeOut{ "FADE OUT", 0, 100, 0 };
        ui::KickNumber                level{ "LEVEL" };
        ui::KickNumber                semi{ "SEMI", SEMI::MIN, SEMI::MAX, SEMI::DEFAULT };
        ui::KickNumber                fine{ "FINE", FINE::MIN, FINE::MAX, FINE::DEFAULT };
        ui::KickNumberf               bpm{ "BPM", BPM::MIN, BPM::MAX, BPM::DEFAULT };
        ui::KickToggleButton          warp{ "WARP", "WARP", "WARP" };
        ui::KickToggleButton          reverse{ "REVERSE", "REVERSE", "REVERSE" };
        ui::KickToggleButton          mute{ "MUTE", "MUTE", "MUTE" };
        ui::KickToggleButton          tc{ "TC", "TC", "TC" };
        ui::KickToggleButton          dontSnap{ "DONT'T SNAP", "DONT'T SNAP", "DONT'T SNAP" };
        ui::KickToggleButton          nudge{ "NUDGE", "NUDGE", "NUDGE" };
        ui::KickToggleButton          editStart{ "EDIT START", "EDIT START", "EDIT START" };
        ui::KickToggleButton          editEnd{ "EDIT END", "EDIT END", "EDIT END" };
        ui::KickButton                split{ "SPLIT" };
        ui::KickButton                duplicate{ "DUPLICATE" };
        ui::KickImageButton           plusButton{ "PLUS", "Plus", "Plus", "Plus_Active" };
        ui::KickImageButton           minusButton{ "MINUS", "Minus", "Minus", "Minus_Active" };
        ui::KickLabel                 zoomLabel{ "ZOOM" };
    };

}
