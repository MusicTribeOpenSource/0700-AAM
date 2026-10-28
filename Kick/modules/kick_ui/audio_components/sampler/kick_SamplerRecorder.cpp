#include "kick_SamplerRecorder.h"

namespace kick_ui
{
#define recorderMaxLengthHeight 64
#define recoderMargin           10

    SamplerRecorder::SamplerRecorder()
    {
        mt::utils::addAndMakeVisible( this, timerControl, maxLengthTimerControl, recordButton, progressBar, maxLengthLabel );
        mt::utils::addChildComponents( this, recordLabel, statusImage );

        recordLabel.setColours( ui::Theme::textColour( 2 ), juce::Colours::transparentBlack );
        recordLabel.setValueJustification( juce::Justification::centred );
        recordLabel.setValueFontHeight( 18 );
        maxLengthLabel.setColours( ui::Theme::textColour( 1 ), juce::Colours::transparentBlack );
        maxLengthLabel.setValueJustification( juce::Justification::centredBottom );
        maxLengthLabel.setValueFontHeight( 18 );

        maxLengthTimerControl.setDisplayFormat( ui::KickTimerControl::DisplayFormat::displayFormatSS );
        maxLengthTimerControl.setTime( 20.0 );
        maxLengthTimerControl.setMaxTime( 1200 );
        maxLengthTimerControl.setValueColour( ui::Theme::textColour( 2 ) );
        maxLengthTimerControl.setNumberDisplay( ui::ViewType::withTitleBottom );
        timerControl.setEditable( false );
        timerControl.setTime( 0.0f );
        timerControl.setNumberDisplay( ui::ViewType::withTitleBottom );

        formatManager.registerBasicFormats();
        recordButton.onClick = [&]() {
            if( RecordMode::recordModeNone == mode )
                setRecordMode( RecordMode::recordModeRecord );
            else if( RecordMode::recordModeRecording == mode )
                setRecordMode( RecordMode::recordModeStopped );
            else if( RecordMode::recordModeStopped == mode )
                setRecordMode( RecordMode::recordModeRecord );
        };

        recorderListner.onRecorderLengthChanged = [&]( double LengthInSeconds ) {
            _recordLength = LengthInSeconds;
        };

        recorderListner.onRecorderStarted = [&]() {
            //setRecordMode( RecordMode::recordModeRecording );
        };

        recorderListner.onRecorderStopped = [&]() {

        };

        recorderListner.onRecorderPreviewStarted = [&]() {

        };

        recorderListner.onRecorderPreviewStopped = [&]() {

        };

        ae.setRecorderListener( recorderListner );

        ui::themeChanged.connect( [&]() {
            recordButton.setImages( "sampler_Rec" );
            setRecordMode( mode );
        } );
    }

    SamplerRecorder::~SamplerRecorder()
    {
        stopTimer();
        thumbnail.clear();
        thumbnailCache.clear();
    }

    juce::Rectangle<int> SamplerRecorder::getThumbnailRectangle()
    {
        int height = 254 - loopRecorderScaleHeight;
        return juce::Rectangle<int>( 0, loopRecorderScaleHeight, getWidth() - loopRecorderMargin, height );
    }

    juce::Rectangle<int> SamplerRecorder::getScaleRectangle()
    {
        return juce::Rectangle<int>( 0, 0, getWidth(), ui::_scaledY( loopRecorderScaleHeight ) );
    }

    void SamplerRecorder::paint( juce::Graphics& g )
    {
        const juce::Rectangle<int>& scaleRect = getScaleRectangle();

        g.setColour( ui::Theme::backgroundColour( 11 ) );
        g.fillRoundedRectangle( { 0.0f, 0.0f, 652.0f, 263.0f }, 5.0f );

        g.setColour( ui::Theme::backgroundColour( 2 ) );
        g.fillRoundedRectangle( { 20.0f, 293.0f, 98.0f, 40.0f }, 5.0f );

        g.setColour( ui::Theme::waveformColour( 1 ) );
        thumbnail.drawChannels( g, getThumbnailRectangle().reduced( 2 ), 0.0, thumbnail.getTotalLength(), 2.0f );

        // draw scale
        g.setColour( ui::Theme::backgroundColour( 8 ) );
        juce::Font f( 9 );
        g.setFont( f );
        int   numBeats  = 4 * 4;
        float beatWidth = static_cast<float>( getWidth() / numBeats );
        paintBeats( g, numBeats, beatWidth, scaleRect );
    }
    void SamplerRecorder::paintBeats( juce::Graphics& g, int numBeats, float beatWidth, const juce::Rectangle<int>& displayRect )
    {
        juce::String maxTick;
        int          totalBeats        = numBeats;
        int          maxTickDisplayLen = g.getCurrentFont().getStringWidth( juce::String().formatted( "%d.%d.%d", totalBeats, 4, 960 ) ) + seSamplesSpacing + seSpaceFromMarker;
        int          maxBeatDisplayLen = g.getCurrentFont().getStringWidth( juce::String().formatted( "%d.%d.%d", totalBeats, 4, 0 ) );
        float        barWidth          = 4 * beatWidth;

        // total beat markings
        int totalDigits      = 1;
        int noOfBeatMarkings = 0;
        if( std::floor( ( 4 * beatWidth ) / maxBeatDisplayLen ) > 1 )
        {
            noOfBeatMarkings = static_cast<int>( std::floor( ( 4 * beatWidth ) / maxBeatDisplayLen ) );
            if( noOfBeatMarkings >= 3 )
                noOfBeatMarkings = 4;
            else if( noOfBeatMarkings == 2 )
                noOfBeatMarkings = 1;
        }
        // total tick markings
        int noOfTickMarkings = 0;
        if( std::floor( ( beatWidth - maxBeatDisplayLen ) / maxTickDisplayLen ) > 0 )
        {
            noOfTickMarkings = static_cast<int>( std::floor( beatWidth / maxTickDisplayLen ) );
        }

        if( noOfBeatMarkings > 0 )
            totalDigits += 1;
        if( noOfTickMarkings > 0 )
            totalDigits += 1;

        // todo get ppq from settings
        int ppqResolution = 960;
        if( noOfTickMarkings > 0 )
            ppqResolution = 960 / ( noOfTickMarkings + 1 );

        float currentPosX = 0;
        float tickWidth   = beatWidth / ( noOfTickMarkings + 1 );
        float barInc      = barWidth / ( noOfBeatMarkings + 1 );
        if( noOfBeatMarkings == 4 )
            barInc = barWidth / noOfBeatMarkings;

        juce::String markingText;
        for( int i = 1; i <= totalBeats; i += 4 )
        {
            int bar = static_cast<int>( std::ceil( i / 4 ) + 1 );
            getDisplayText( markingText, bar, 1, 0, totalDigits );
            drawMarkingText( g, markingText, currentPosX, displayRect );

            auto text = juce::String( "" );
            auto xpos = currentPosX + 10;
            auto rect = displayRect;
            rect.setHeight( displayRect.getHeight() - 5 );
            for( auto ind = 0; ind < 3; ++ind )
            {
                drawMarkingText( g, text, xpos, rect );
                xpos += 10;
            }
            for( int j = 0; j < noOfBeatMarkings; j++ )
            {
                if( noOfTickMarkings )
                    currentPosX += tickWidth;
                else
                    currentPosX += barInc;

                int beat = j + 2;
                if( noOfBeatMarkings == 1 )
                    beat = 3;

                for( int k = 0; k < noOfTickMarkings; k++ )
                {
                    markingText = juce::String().formatted( "%d.%d.%d", bar, beat - 1, ( k + 1 ) * ppqResolution );
                    drawMarkingText( g, markingText, currentPosX, displayRect );
                    currentPosX += tickWidth;
                }
                if( j != 3 )
                {
                    getDisplayText( markingText, bar, beat, 0, totalDigits );
                    drawMarkingText( g, markingText, currentPosX, displayRect );
                }
                if( noOfBeatMarkings == 1 )
                    currentPosX += barInc;
                auto dispText = juce::String( "" );
                auto dispXpos = currentPosX + 10;
                auto dispRect = displayRect;
                dispRect.setHeight( displayRect.getHeight() - 5 );
                for( auto ind = 0; ind < 3; ++ind )
                {
                    drawMarkingText( g, dispText, dispXpos, dispRect );
                    dispXpos += 10;
                }
            }
            if( noOfBeatMarkings == 0 )
                currentPosX += barWidth;
        }
    }
    void SamplerRecorder::getDisplayText( juce::String& markerText, int bar, int beat, int tick, int numDigits )
    {
        if( numDigits == 1 )
            markerText = markerText.formatted( "%d", bar );
        else if( numDigits == 2 )
            markerText = markerText.formatted( "%d.%d", bar, beat );
        else if( numDigits == 3 )
            markerText = markerText.formatted( "%d.%d.%d", bar, beat, tick );
    }

    void SamplerRecorder::drawMarkingText( juce::Graphics& g, juce::String& markerText, float xpos, const juce::Rectangle<int>& displayRect )
    {
        g.drawLine( xpos + seSpaceFromMarker, displayRect.getTopLeft().getY() + 7.0f, xpos + seSpaceFromMarker, static_cast<float>( displayRect.getHeight() ) );
    }

    void SamplerRecorder::resized()
    {
        statusImage.setBoundsRelative( .07f, .502f, .095f, .07f );
        maxLengthLabel.setBoundsRelative( .03f, .70f, .1564f, .05f );
        maxLengthTimerControl.setBoundsRelative( .03f, .745f, .1564f, .2020f );
        timerControl.setBoundsRelative( .217f, .715f, .565f, .2020f );
        recordButton.setBoundsRelative( .813f, .7350f, .1564f, .2020f );
        progressBar.setBoundsRelative( .217f, .947f, .565f, .0252f );
        recordLabel.setBoundsRelative( .025f, .582f, .184f, .05f );
    }

    void SamplerRecorder::recordedTimeChanged( double totalRecordTime )
    {
        if( totalRecordTime > 0.000 )
        {
            timerControl.setTime( totalRecordTime );
        }

        if( totalRecordTime != 0 && mode == RecordMode::recordModeRecord )
            setRecordMode( RecordMode::recordModeRecording );
    }

    void SamplerRecorder::setRecordChannels( RecordChannels Chans )
    {
        recChans = Chans;
    }

    void SamplerRecorder::knobCommand( int knob, int value )
    {
        switch( knob )
        {
            case contracts::DeviceControlContracts::EN_KNOB_1:
                maxLengthTimerControl.knobRotated( value, false );
                break;

            case contracts::DeviceControlContracts::EN_KNOB_2:
                maxLengthTimerControl.knobRotated( value, true );
                break;
        }
    }

    SamplerRecorder::RecordChannels SamplerRecorder::getRecordChannels()
    {
        return (SamplerRecorder::RecordChannels)ae.getRecorderMode();
    }

    void SamplerRecorder::setRecordMode( RecordMode newMode )
    {
        mode = newMode;
        recordLabel.setVisible( false );
        statusImage.setVisible( false );
        switch( mode )
        {
            case RecordMode::recordModeNone:
                recordButton.setImages( "sampler_Rec" );
                timerControl.setTime( 0.0f );
                maxLengthTimerControl.setEditable( true );
                setThumbnail();
                break;

            case RecordMode::recordModeRecord:
                recordButton.setImages( "sampler_Rec_active" );
                mt::utils::makeVisible( statusImage, recordLabel );
                maxLengthTimerControl.setEditable( false );
                clearRecording();
                startRecording();
                break;

            case RecordMode::recordModeRecording:
                recordButton.setImages( "sampler_Rec_stop" );
                maxLengthTimerControl.setEditable( false );
                waveFormTimer = true;
                break;

            case RecordMode::recordModeStopped:
                recordButton.setImages( "sampler_Rec" );
                maxLengthTimerControl.setEditable( false );
                waveFormTimer = false;
                stopRecording();
                setThumbnail();
                break;
        }

        // signals...
        recordModeChanged( mode );
    }

    void SamplerRecorder::setMonitorOn( bool on )
    {
        ae.setMonitorOn( on );
    }

    float SamplerRecorder::getRecordThresholdDB()
    {
        return ae.getRecorderThresholdDB();
    }

    void SamplerRecorder::setRecordThresholdDB( float threshDb )
    {
        ae.setRecorderThresholdDB( threshDb );
    }

    double SamplerRecorder::getCurrentRecordTime()
    {
        return timerControl.getTime();
    }

    void SamplerRecorder::setNumChannels( int channels )
    {
        numChannels = channels;
    }

    void SamplerRecorder::clearRecording()
    {
        _progress = 0.0;
        thumbnail.clear();
        thumbnailCache.clear();
        stopRecording( true );
        triggerAsyncUpdate();
    }

    void SamplerRecorder::setGain( float Gain ) { ae.setRecorderGain( Gain ); }
    void SamplerRecorder::setGainDB( float GainDB ) { ae.setRecorderGainDB( GainDB ); }

    bool SamplerRecorder::isRecording() { return ae.isRecorderRunning(); }
    bool SamplerRecorder::isMonitorOn() { return ae.isRecorderMonitorOn(); }

    void SamplerRecorder::startRecording()
    {
        _progress     = 0.0;
        _recordLength = maxLengthTimerControl.getTime();
        //TODO
        ae.setupRecorder( _recordLength, 1.f, 0, false, false );
        //ae.setupRecorder( 2, 1.f, 0, false, false );
        ae.startRecorder( recChans );
        // signal...
        recordingStarted( ae.isRecorderRunning() );
    }

    void SamplerRecorder::stopRecording( bool cancel )
    {
        if( cancel )
            ae.clearRecording();
        else
            ae.stopRecorder();
        // signal...
        recordingStarted( ae.isRecorderRunning() );
    }

    juce::Rectangle<float> SamplerRecorder::getRecorderRect() const
    {
        float width  = (float)getWidth();
        float height = (float)( getHeight() - recoderMargin - recorderMaxLengthHeight );
        return juce::Rectangle<float>( 0.0f, 0.0f, width, height );
    }

    juce::Rectangle<float> SamplerRecorder::getMaxLengthRect() const
    {
        float width = (float)getWidth();
        float y     = (float)( getHeight() - recorderMaxLengthHeight );
        return juce::Rectangle<float>( 0.0f, y, width, (float)( recorderMaxLengthHeight ) );
    }

    void SamplerRecorder::visibilityChanged()
    {
        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );

        if( visible )
        {
            recordFile    = ae.getTempDir().getSiblingFile( "sampler.wav" );
            _recordLength = maxLengthTimerControl.getTime();
            startTimerHz( 30 );
            setRecordMode( mode );
        }
        else
        {
            ae.setMonitorOn( false );
            stopTimer();
        }
    }
    void SamplerRecorder::setThumbnail()
    {
        auto& source = ae.getRecorderAudioBuffer();
        // The hash never changes, so without this the cache keeps serving the first
        // (still silent) thumbnail taken right after REC and the waveform stays flat.
        thumbnailCache.removeThumb( 6783672364 );
        thumbnail.setSource( &source, source.getNumSamples(), 6783672364 );
        triggerAsyncUpdate();
    }
    void SamplerRecorder::timerCallback()
    {
        if( waveFormTimer )
            setThumbnail();

        signalStrengthChanged( ae.getRecorderInputMeter() );

        _progress = ae.getRecorderProgress();
        timerControl.setTime( _progress * _recordLength );
        progressBar.updateProgress( static_cast<int>( _progress * 100 ) );
        if( _progress == 1.0 && mode == RecordMode::recordModeRecording )
            setRecordMode( RecordMode::recordModeStopped );
        else if( 0.0 < _progress && mode == RecordMode::recordModeRecord )
            setRecordMode( RecordMode::recordModeRecording );
    }
    void SamplerRecorder::handleAsyncUpdate()
    {
        repaint();
    }
}
