namespace kick_ui
{
    namespace
    {
        // Pad state the MCU sends for a pressure change on a held pad (Firmware/kick-mcu contract.h,
        // kPadStateAftertouch). It is not a note-on or a note-off.
        constexpr int padStatePressureUpdate = 2;
    } // namespace


    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControlHardware-right1 ///////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlHardwareButtons::MidiControlHardwareButtons()

    {
        mt::utils::addAndMakeVisible( this, lightLed, lightLedOptions, typeOptions, type, ccNumberValue, ccNumberLabel, midiChannelValue, midiChannelLabel );
        lightLedOptions.setListOptions( { "Never", "Midi Input", "Local" } );
        typeOptions.setToggleButtonText( "Momentary", "Toggle" );

        midiChannelValue.showTitle( false );
        ccNumberValue.showTitle( false );
        midiChannelLabel.setValueJustification( juce::Justification::centredLeft );
        ccNumberLabel.setValueJustification( juce::Justification::centredLeft );
        type.setValueJustification( juce::Justification::centredLeft );
        lightLed.setValueJustification( juce::Justification::centredLeft );

        mt::utils::setFontHeight( 18, typeOptions, ccNumberLabel, type, lightLed, midiChannelLabel );
        lightLed.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        type.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        midiChannelLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        ccNumberLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        typeOptions.setValueColour( ui::Theme::textColour( 1 ) );
        midiChannelValue.showTitle( false );
        ccNumberValue.showTitle( false );
    }

    void MidiControlHardwareButtons::resized()
    {
        auto b = getLocalBounds();
        lightLed.setBoundsRelative( 0.0306f, 0.0593f, 0.2453f, 0.111f );
        lightLedOptions.setBoundsRelative( 0.6932f, 0.0505f, 0.3067f, 0.1111f );
        midiChannelLabel.setBoundsRelative( 0.0306f, 0.1957f, 0.2453f, 0.111f );
        midiChannelValue.setBoundsRelative( 0.6932f, 0.1868f, 0.3067f, 0.111f );
        ccNumberLabel.setBoundsRelative( 0.0360f, 0.3320f, 0.2453f, 0.111f );
        ccNumberValue.setBoundsRelative( 0.6932f, 0.3232f, 0.3067f, 0.111f );
        type.setBoundsRelative( 0.0360f, 0.4684f, 0.2453f, 0.111f );
        typeOptions.setBoundsRelative( 0.6932f, 0.4595f, 0.3067f, 0.111f );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControlHardware-right2
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlHardwareQlink::MidiControlHardwareQlink()

    {
        mt::utils::addAndMakeVisible( &listComp, lowRangeValue, lowRangeLabel, highRangeLabel, highRangeValue, touchSenceLabel, touchSenceLabel, noteLabel, note1, note2, type, touchSenceOptions, typeOptions, lightLed, lightLedOptions, midiChannelLabel, midiChannelValue, ccNumberLabel, ccNumberValue );

        lowRangeLabel.setValueJustification( juce::Justification::centredLeft );
        highRangeLabel.setValueJustification( juce::Justification::centredLeft );
        touchSenceLabel.setValueJustification( juce::Justification::centredLeft );
        noteLabel.setValueJustification( juce::Justification::centredLeft );
        type.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        mt::utils::setFontHeight( 18, type, lowRangeLabel, highRangeLabel, touchSenceLabel, noteLabel, typeOptions );
        lowRangeLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        highRangeLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        touchSenceLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        noteLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        touchSenceOptions.setValueColour( ui::Theme::textColour( 1 ) );
        typeOptions.setValueColour( ui::Theme::textColour( 1 ) );
        type.setText( "Mode", juce::dontSendNotification );
        typeOptions.setToggleButtonText( "Absolute", "Relative" );

        setScrollOnDragMode( juce::Viewport::ScrollOnDragMode::all );
        setViewedComponent( &listComp, false );
        setScrollBarThickness( 6 );
        lowRangeValue.showTitle( false );
        highRangeValue.showTitle( false );
        note1.showTitle( false );
    }
    void MidiControlHardwareQlink::resized()
    {
        int yAxis{ 10 };
        lightLed.setBounds( 20, yAxis, 160, 44 );
        lightLedOptions.setBounds( 450, yAxis, 195, 44 );
        yAxis += ( 44 + 12 );
        midiChannelLabel.setBounds( 20, yAxis, 160, 44 );
        midiChannelValue.setBounds( 450, yAxis, 195, 44 );
        yAxis += ( 44 + 12 );
        ccNumberLabel.setBounds( 20, yAxis, 160, 44 );
        ccNumberValue.setBounds( 450, yAxis, 195, 44 );
        yAxis += ( 44 + 12 );
        type.setBounds( 20, yAxis, 160, 44 );
        typeOptions.setBounds( 450, yAxis, 195, 44 );
        yAxis += ( 44 + 12 );
        lowRangeLabel.setBounds( 20, yAxis, 160, 44 );
        lowRangeValue.setBounds( 450, yAxis, 195, 44 );
        yAxis += ( 44 + 12 );
        highRangeLabel.setBounds( 20, yAxis, 160, 44 );
        highRangeValue.setBounds( 450, yAxis, 195, 44 );
        yAxis += ( 44 + 12 );
        touchSenceLabel.setBounds( 20, yAxis, 160, 44 );
        touchSenceOptions.setBounds( 450, yAxis, 195, 44 );
        yAxis += ( 44 + 12 );
        noteLabel.setBounds( 20, yAxis, 160, 44 );
        note1.setBounds( 450, yAxis, 95, 44 );
        note2.setBounds( 548, yAxis, 95, 44 );
        yAxis += ( 44 + 12 );
        listComp.setBounds( getX(), getY(), getWidth(), yAxis );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControlHardware-pad
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlPadOptions::MidiControlPadOptions()
    {
        mt::utils::addAndMakeVisible( this, on, velocityLabel, led, ledBox, midiBox, midiLabel, noteLabel, noteBox1, noteBox2, afterTouchLabel, afterTouchBox );
        on.setValueColour( ui::Theme::textColour( 1 ) );
        mt::utils::setFontHeight( 18, on, velocityLabel, midiLabel, noteLabel, afterTouchLabel, led );
        velocityLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        midiLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        noteLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        afterTouchLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        led.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        ledBox.setListOptions( { "Never", "Midi Input", "Local" } );
        afterTouchBox.setListOptions( { "Off", "Channel", "Poly" } );
        midiBox.showTitle( false );
        noteBox1.showTitle( false );
    }
    void MidiControlPadOptions::resized()
    {
        on.setBoundsRelative( 0.6932f, 0.4595f, 0.3067f, 0.111f );
        velocityLabel.setBoundsRelative( 0.0306f, 0.4684f, 0.2453f, 0.111f );
        led.setBoundsRelative( 0.0306f, 0.0593f, 0.2453f, 0.111f );
        ledBox.setBoundsRelative( 0.6932f, 0.0505f, 0.3067f, 0.111f );
        midiLabel.setBoundsRelative( 0.0306f, 0.1957f, 0.2453f, 0.111f );
        midiBox.setBoundsRelative( 0.6932f, 0.1868f, 0.3067f, 0.111f );
        noteLabel.setBoundsRelative( 0.0306f, 0.3327f, 0.2453f, 0.111f );
        noteBox1.setBoundsRelative( 0.6932f, 0.3232f, 0.1457f, 0.111f );
        noteBox2.setBoundsRelative( 0.8527f, 0.3232f, 0.1457f, 0.111f );
        afterTouchLabel.setBoundsRelative( 0.0306f, 0.6047f, 0.2453f, 0.111f );
        afterTouchBox.setBoundsRelative( 0.6932f, 0.5959f, 0.3067f, 0.111f );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControlXY
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlXYFXOptions::MidiControlXYFXOptions()

    {
        mt::utils::addAndMakeVisible( this, xLabel, yLabel, mute1, mute2, ccNum1, ccNum2, lowRang1, lowRang2, highRang1, highRang2, midiCh, ccNumber, lowRang, highRang, midCh1, midCh2 );
        mt::utils::setFontHeight( 18, xLabel, yLabel, midiCh, ccNumber, lowRang, highRang );
        mt::utils::setFontHeight( 20, mute1, mute2 );
        xLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        yLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        midiCh.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        ccNumber.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        lowRang.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        highRang.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        mute1.setValueColour( ui::Theme::textColour( 1 ) );
        mute2.setValueColour( ui::Theme::textColour( 1 ) );
        xLabel.setValueJustification( juce::Justification::centred );
        yLabel.setValueJustification( juce::Justification::centred );
        midCh1.showTitle( false );
        midCh2.showTitle( false );
        ccNum1.showTitle( false );
        ccNum2.showTitle( false );
        lowRang1.showTitle( false );
        lowRang2.showTitle( false );
        highRang1.showTitle( false );
        highRang2.showTitle( false );
    }

    MidiControlXYFXOptions::~MidiControlXYFXOptions()
    {
    }

    void MidiControlXYFXOptions::resized()
    {
        xLabel.setBoundsRelative( 0.3859f, 0.022f, 0.265f, 0.1f );
        yLabel.setBoundsRelative( 0.7076f, 0.022f, 0.265f, 0.1f );
        mute1.setBoundsRelative( 0.3859f, 0.14545f, 0.265f, 0.1f );
        mute2.setBoundsRelative( 0.7076f, 0.14545f, 0.265f, 0.1f );
        midiCh.setBoundsRelative( 0.0584f, 0.2681f, 0.2953f, 0.1f );
        ccNumber.setBoundsRelative( 0.0584f, 0.3909f, 0.2953f, 0.1f );
        lowRang.setBoundsRelative( 0.0584f, 0.5136f, 0.2953f, 0.1f );
        highRang.setBoundsRelative( 0.0584f, 0.6363f, 0.2953f, 0.1f );
        midCh1.setBoundsRelative( 0.3859f, 0.2681f, 0.2631f, 0.1f );
        midCh2.setBoundsRelative( 0.7076f, 0.2681f, 0.2631f, 0.1f );
        ccNum1.setBoundsRelative( 0.3859f, 0.3909f, 0.2631f, 0.1f );
        ccNum2.setBoundsRelative( 0.7076f, 0.3909f, 0.2631f, 0.1f );
        lowRang1.setBoundsRelative( 0.3859f, 0.5136f, 0.2631f, 0.1f );
        lowRang2.setBoundsRelative( 0.7076f, 0.5136f, 0.2631f, 0.1f );
        highRang1.setBoundsRelative( 0.3859f, 0.6363f, 0.2631f, 0.1f );
        highRang2.setBoundsRelative( 0.7076f, 0.6363f, 0.2631f, 0.1f );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControlHardware-Left
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlHardwareLeft::MidiControlHardwareLeft()
    {
        mt::utils::addAndMakeVisible( this, Control, option1List, bank, setAll, para, bankLabel, setAllLabel );
        mt::utils::setFontHeight( 18, para, bankLabel, setAllLabel, Control );

        Control.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        Control.setValueJustification( juce::Justification::centred );
        para.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        para.setValueColour( ui::Theme::textColour( 1 ) );
        bankLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        bankLabel.setValueColour( ui::Theme::textColour( 1 ) );
        setAllLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        setAllLabel.setValueColour( ui::Theme::textColour( 1 ) );
        option1List.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );
        bank.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );

        std::vector<juce::String> options = {
            "Step Seq", "Next Seq", "Track Mute", "Sample Edit", "Program Edit", "Browser", "Tap", "Enter", "-", "+ ", "Erase", "Track Mix", "TC", "PAD Mute", "Copy", "Pad Mixer", "sampler", "XYFX", "Q-Link 1", "Q-Link 2", "Q-Link 3", "Q-Link 4",
            "Pad 1", "Pad 2", "Pad 3", "Pad 4", "Pad 5", "Pad 6", "Pad 7", "Pad 8", "Pad 9", "Pad 10", "Pad 11", "Pad 12", "Pad 13", "Pad 14", "Pad 15", "Pad 16", "XYFXX-Axis", "XYFXY-Axis"
        };

        option1List.setListOptions( options );

        setPadOptionsVisible( false );
    }

    MidiControlHardwareLeft::~MidiControlHardwareLeft()
    {
    }

    void MidiControlHardwareLeft::resized()
    {
        option1List.setBoundsRelative( 0.3859f, 0.1616f, 0.5847f, 0.111f );
        para.setBoundsRelative( 0.0292f, 0.19949f, 0.3859f, 0.03535f );
        bank.setBounds( 172, option1List.getBottom() + 10, 160, 44 );
        bankLabel.setBounds( 10, para.getBottom() + 40, 40, 14 );
        setAllLabel.setBoundsRelative( 0.02923f, 0.4722f, 0.3859f, 0.0355f );
        setAll.setBoundsRelative( 0.8421f, 0.4343f, 0.12865f, 0.111f );
        Control.setBoundsRelative( 0, 0, 1.0f, 0.12626f );
    }
    void MidiControlHardwareLeft::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, leftTopBounds, ui::Theme::backgroundColour( 1 ), 6.0f, true, true, false, false );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControlHardware
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlHardwareRight::MidiControlHardwareRight()
    {
        addStackedComponent( &button );
        addStackedComponent( &qlink );
        addStackedComponent( &xy );
        addStackedComponent( &pad );
    }

    MidiControlHardwareRight::~MidiControlHardwareRight()
    {
    }

    MidiControlHardware::MidiControlHardware()
    {
        mt::utils::addAndMakeVisible( this, leftComp, rightComp );

        vTree       = mt::getSettings().midiControl.vTree;
        midiControl = vTree;

        //Hardware
        midiControl.removeAllChildren( nullptr );
        juce::ValueTree stepControl( "CONTROL" );
        stepControl.setProperty( "PadParameters", "Step Seq", nullptr );
        stepControl.setProperty( "ButtonLed", "Never", nullptr );
        stepControl.setProperty( "MIDIChannel", "1", nullptr );
        stepControl.setProperty( "CCNumber", "2", nullptr );
        stepControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( stepControl, nullptr );

        juce::ValueTree nextControl( "CONTROL" );
        nextControl.setProperty( "PadParameters", "Next Seq", nullptr );
        nextControl.setProperty( "ButtonLed", "Never", nullptr );
        nextControl.setProperty( "MIDIChannel", "1", nullptr );
        nextControl.setProperty( "CCNumber", "3", nullptr );
        nextControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( nextControl, nullptr );

        juce::ValueTree trackControl( "CONTROL" );
        trackControl.setProperty( "PadParameters", "Track Mute", nullptr );
        trackControl.setProperty( "ButtonLed", "Never", nullptr );
        trackControl.setProperty( "MIDIChannel", "1", nullptr );
        trackControl.setProperty( "CCNumber", "4", nullptr );
        trackControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( trackControl, nullptr );

        juce::ValueTree sampleEditControl( "CONTROL" );
        sampleEditControl.setProperty( "PadParameters", "Sample Edit", nullptr );
        sampleEditControl.setProperty( "ButtonLed", "Never", nullptr );
        sampleEditControl.setProperty( "MIDIChannel", "1", nullptr );
        sampleEditControl.setProperty( "CCNumber", "5", nullptr );
        sampleEditControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( sampleEditControl, nullptr );

        juce::ValueTree pgmEditControl( "CONTROL" );
        pgmEditControl.setProperty( "PadParameters", "Program Edit", nullptr );
        pgmEditControl.setProperty( "ButtonLed", "Never", nullptr );
        pgmEditControl.setProperty( "MIDIChannel", "1", nullptr );
        pgmEditControl.setProperty( "CCNumber", "6", nullptr );
        pgmEditControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( pgmEditControl, nullptr );

        juce::ValueTree browseControl( "CONTROL" );
        browseControl.setProperty( "PadParameters", "Browser", nullptr );
        browseControl.setProperty( "ButtonLed", "Never", nullptr );
        browseControl.setProperty( "MIDIChannel", "1", nullptr );
        browseControl.setProperty( "CCNumber", "7", nullptr );
        browseControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( browseControl, nullptr );

        juce::ValueTree undoControl( "CONTROL" );
        undoControl.setProperty( "PadParameters", "Undo", nullptr );
        undoControl.setProperty( "ButtonLed", "Never", nullptr );
        undoControl.setProperty( "MIDIChannel", "1", nullptr );
        undoControl.setProperty( "CCNumber", "8", nullptr );
        undoControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( undoControl, nullptr );

        juce::ValueTree tapControl( "CONTROL" );
        tapControl.setProperty( "PadParameters", "Tap", nullptr );
        tapControl.setProperty( "ButtonLed", "Never", nullptr );
        tapControl.setProperty( "MIDIChannel", "1", nullptr );
        tapControl.setProperty( "CCNumber", "9", nullptr );
        tapControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( tapControl, nullptr );

        juce::ValueTree enterControl( "CONTROL" );
        enterControl.setProperty( "PadParameters", "Enter", nullptr );
        enterControl.setProperty( "ButtonLed", "Never", nullptr );
        enterControl.setProperty( "MIDIChannel", "1", nullptr );
        enterControl.setProperty( "CCNumber", "10", nullptr );
        enterControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( enterControl, nullptr );

        juce::ValueTree signControl( "CONTROL" );
        signControl.setProperty( "PadParameters", "-/+", nullptr );
        signControl.setProperty( "ButtonLed", "Never", nullptr );
        signControl.setProperty( "MIDIChannel", "1", nullptr );
        signControl.setProperty( "CCNumber", "11", nullptr );
        signControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( signControl, nullptr );

        juce::ValueTree eraseControl( "CONTROL" );
        eraseControl.setProperty( "PadParameters", "Erase", nullptr );
        eraseControl.setProperty( "ButtonLed", "Never", nullptr );
        eraseControl.setProperty( "MIDIChannel", "1", nullptr );
        eraseControl.setProperty( "CCNumber", "12", nullptr );
        eraseControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( eraseControl, nullptr );

        juce::ValueTree chmixerControl( "CONTROL" );
        chmixerControl.setProperty( "PadParameters", "Track Mix", nullptr );
        chmixerControl.setProperty( "ButtonLed", "Never", nullptr );
        chmixerControl.setProperty( "MIDIChannel", "1", nullptr );
        chmixerControl.setProperty( "CCNumber", "13", nullptr );
        chmixerControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( chmixerControl, nullptr );

        juce::ValueTree tcControl( "CONTROL" );
        tcControl.setProperty( "PadParameters", "TC", nullptr );
        tcControl.setProperty( "ButtonLed", "Never", nullptr );
        tcControl.setProperty( "MIDIChannel", "1", nullptr );
        tcControl.setProperty( "CCNumber", "14", nullptr );
        tcControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( tcControl, nullptr );

        juce::ValueTree padmuteControl( "CONTROL" );
        padmuteControl.setProperty( "PadParameters", "PAD Mute", nullptr );
        padmuteControl.setProperty( "ButtonLed", "Never", nullptr );
        padmuteControl.setProperty( "MIDIChannel", "1", nullptr );
        padmuteControl.setProperty( "CCNumber", "14", nullptr );
        padmuteControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( padmuteControl, nullptr );

        juce::ValueTree copyControl( "CONTROL" );
        copyControl.setProperty( "PadParameters", "Copy", nullptr );
        copyControl.setProperty( "ButtonLed", "Never", nullptr );
        copyControl.setProperty( "MIDIChannel", "1", nullptr );
        copyControl.setProperty( "CCNumber", "16", nullptr );
        copyControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( copyControl, nullptr );

        juce::ValueTree padmixControl( "CONTROL" );
        padmixControl.setProperty( "PadParameters", "Pad Mixer", nullptr );
        padmixControl.setProperty( "ButtonLed", "Never", nullptr );
        padmixControl.setProperty( "MIDIChannel", "1", nullptr );
        padmixControl.setProperty( "CCNumber", "17", nullptr );
        padmixControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( padmixControl, nullptr );

        juce::ValueTree samplerControl( "CONTROL" );
        samplerControl.setProperty( "PadParameters", "sampler", nullptr );
        samplerControl.setProperty( "ButtonLed", "Never", nullptr );
        samplerControl.setProperty( "MIDIChannel", "1", nullptr );
        samplerControl.setProperty( "CCNumber", "18", nullptr );
        samplerControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( samplerControl, nullptr );

        juce::ValueTree XYFXControl( "CONTROL" );
        XYFXControl.setProperty( "PadParameters", "XYFX", nullptr );
        XYFXControl.setProperty( "ButtonLed", "Never", nullptr );
        XYFXControl.setProperty( "MIDIChannel", "1", nullptr );
        XYFXControl.setProperty( "CCNumber", "19", nullptr );
        XYFXControl.setProperty( "Type", "Toggle", nullptr );
        midiControl.appendChild( XYFXControl, nullptr );

        //  "Q-Link
        juce::ValueTree qLink1Control( "CONTROL" );
        qLink1Control.setProperty( "PadParameters", "Q-Link 1", nullptr );
        qLink1Control.setProperty( "QlinkLed", "Never", nullptr );
        qLink1Control.setProperty( "QlinkMIDIChannel", "1", nullptr );
        qLink1Control.setProperty( "QlinkCCNumber", "20", nullptr );
        qLink1Control.setProperty( "Mode", "Relative", nullptr );
        qLink1Control.setProperty( "LowRange", "0", nullptr );
        qLink1Control.setProperty( "HighRange", "127", nullptr );
        qLink1Control.setProperty( "TouchSense", "Off", nullptr );
        qLink1Control.setProperty( "Note", "0", nullptr );
        qLink1Control.setProperty( "NoteContent", "C-2", nullptr );
        midiControl.appendChild( qLink1Control, nullptr );

        juce::ValueTree qLink2Control( "CONTROL" );
        qLink2Control.setProperty( "PadParameters", "Q-Link 2", nullptr );
        qLink2Control.setProperty( "QlinkLed", "Never", nullptr );
        qLink2Control.setProperty( "QlinkMIDIChannel", "1", nullptr );
        qLink2Control.setProperty( "QlinkCCNumber", "21", nullptr );
        qLink2Control.setProperty( "Mode", "Relative", nullptr );
        qLink2Control.setProperty( "LowRange", "0", nullptr );
        qLink2Control.setProperty( "HighRange", "127", nullptr );
        qLink2Control.setProperty( "TouchSense", "Off", nullptr );
        qLink2Control.setProperty( "Note", "1", nullptr );
        qLink2Control.setProperty( "NoteContent", "C#-2", nullptr );
        midiControl.appendChild( qLink2Control, nullptr );

        juce::ValueTree qLink3Control( "CONTROL" );
        qLink3Control.setProperty( "PadParameters", "Q-Link 3", nullptr );
        qLink3Control.setProperty( "QlinkLed", "Never", nullptr );
        qLink3Control.setProperty( "QlinkMIDIChannel", "1", nullptr );
        qLink3Control.setProperty( "QlinkCCNumber", "22", nullptr );
        qLink3Control.setProperty( "Mode", "Relative", nullptr );
        qLink3Control.setProperty( "LowRange", "0", nullptr );
        qLink3Control.setProperty( "HighRange", "127", nullptr );
        qLink3Control.setProperty( "TouchSense", "Off", nullptr );
        qLink3Control.setProperty( "Note", "2", nullptr );
        qLink3Control.setProperty( "NoteContent", "D-2", nullptr );
        midiControl.appendChild( qLink3Control, nullptr );

        juce::ValueTree qLink4Control( "CONTROL" );
        qLink4Control.setProperty( "PadParameters", "Q-Link 4", nullptr );
        qLink4Control.setProperty( "QlinkLed", "Never", nullptr );
        qLink4Control.setProperty( "QlinkMIDIChannel", "1", nullptr );
        qLink4Control.setProperty( "QlinkCCNumber", "23", nullptr );
        qLink4Control.setProperty( "Mode", "Relative", nullptr );
        qLink4Control.setProperty( "LowRange", "0", nullptr );
        qLink4Control.setProperty( "HighRange", "127", nullptr );
        qLink4Control.setProperty( "TouchSense", "Off", nullptr );
        qLink4Control.setProperty( "Note", "3", nullptr );
        qLink4Control.setProperty( "NoteContent", "D#-2", nullptr );
        midiControl.appendChild( qLink4Control, nullptr );

        juce::ValueTree pad1Control( "CONTROL" );
        pad1Control.setProperty( "PadParameters", "Pad 1", nullptr );
        pad1Control.setProperty( "Bank", "A", nullptr );
        pad1Control.setProperty( "SetAll", "0", nullptr );
        pad1Control.setProperty( "PadLed", "Local", nullptr );
        pad1Control.setProperty( "PadNo", "1", nullptr );
        pad1Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad1Control.setProperty( "NoteNumber", "0", nullptr );
        pad1Control.setProperty( "NoteValue", "C-2", nullptr );
        pad1Control.setProperty( "Velocity", "Off", nullptr );
        pad1Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad1Control, nullptr );

        juce::ValueTree pad2Control( "CONTROL" );
        pad2Control.setProperty( "PadParameters", "Pad 2", nullptr );
        pad2Control.setProperty( "Bank", "A", nullptr );
        pad2Control.setProperty( "SetAll", "0", nullptr );
        pad2Control.setProperty( "PadLed", "Local", nullptr );
        pad2Control.setProperty( "PadNo", "1", nullptr );
        pad2Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad2Control.setProperty( "NoteNumber", "1", nullptr );
        pad2Control.setProperty( "NoteValue", "C#-2", nullptr );
        pad2Control.setProperty( "Velocity", "Off", nullptr );
        pad2Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad2Control, nullptr );

        juce::ValueTree pad3Control( "CONTROL" );
        pad3Control.setProperty( "PadParameters", "Pad 3", nullptr );
        pad3Control.setProperty( "Bank", "A", nullptr );
        pad3Control.setProperty( "SetAll", "0", nullptr );
        pad3Control.setProperty( "PadLed", "Local", nullptr );
        pad3Control.setProperty( "PadNo", "1", nullptr );
        pad3Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad3Control.setProperty( "NoteNumber", "2", nullptr );
        pad3Control.setProperty( "NoteValue", "D-2", nullptr );
        pad3Control.setProperty( "Velocity", "Off", nullptr );
        pad3Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad3Control, nullptr );

        juce::ValueTree pad4Control( "CONTROL" );
        pad4Control.setProperty( "PadParameters", "Pad 4", nullptr );
        pad4Control.setProperty( "Bank", "A", nullptr );
        pad4Control.setProperty( "SetAll", "0", nullptr );
        pad4Control.setProperty( "PadLed", "Local", nullptr );
        pad4Control.setProperty( "PadNo", "1", nullptr );
        pad4Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad4Control.setProperty( "NoteNumber", "3", nullptr );
        pad4Control.setProperty( "NoteValue", "D#-2", nullptr );
        pad4Control.setProperty( "Velocity", "Off", nullptr );
        pad4Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad4Control, nullptr );

        juce::ValueTree pad5Control( "CONTROL" );
        pad5Control.setProperty( "PadParameters", "Pad 5", nullptr );
        pad5Control.setProperty( "Bank", "A", nullptr );
        pad5Control.setProperty( "SetAll", "0", nullptr );
        pad5Control.setProperty( "PadLed", "Local", nullptr );
        pad5Control.setProperty( "PadNo", "1", nullptr );
        pad5Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad5Control.setProperty( "NoteNumber", "4", nullptr );
        pad5Control.setProperty( "NoteValue", "E-2", nullptr );
        pad5Control.setProperty( "Velocity", "Off", nullptr );
        pad5Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad5Control, nullptr );

        juce::ValueTree pad6Control( "CONTROL" );
        pad6Control.setProperty( "PadParameters", "Pad 6", nullptr );
        pad6Control.setProperty( "Bank", "A", nullptr );
        pad6Control.setProperty( "SetAll", "0", nullptr );
        pad6Control.setProperty( "PadLed", "Local", nullptr );
        pad6Control.setProperty( "PadNo", "1", nullptr );
        pad6Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad6Control.setProperty( "NoteNumber", "5", nullptr );
        pad6Control.setProperty( "NoteValue", "F-2", nullptr );
        pad6Control.setProperty( "Velocity", "Off", nullptr );
        pad6Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad6Control, nullptr );

        juce::ValueTree pad7Control( "CONTROL" );
        pad7Control.setProperty( "PadParameters", "Pad 7", nullptr );
        pad7Control.setProperty( "Bank", "A", nullptr );
        pad7Control.setProperty( "SetAll", "0", nullptr );
        pad7Control.setProperty( "PadLed", "Local", nullptr );
        pad1Control.setProperty( "PadNo", "1", nullptr );
        pad7Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad7Control.setProperty( "NoteNumber", "6", nullptr );
        pad7Control.setProperty( "NoteValue", "F#-2", nullptr );
        pad7Control.setProperty( "Velocity", "Off", nullptr );
        pad7Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad7Control, nullptr );

        juce::ValueTree pad8Control( "CONTROL" );
        pad8Control.setProperty( "PadParameters", "Pad 8", nullptr );
        pad8Control.setProperty( "Bank", "A", nullptr );
        pad8Control.setProperty( "SetAll", "0", nullptr );
        pad8Control.setProperty( "PadLed", "Local", nullptr );
        pad1Control.setProperty( "PadNo", "1", nullptr );
        pad1Control.setProperty( "PadNo", "1", nullptr );
        pad8Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad8Control.setProperty( "NoteNumber", "7", nullptr );
        pad8Control.setProperty( "NoteValue", "G-2", nullptr );
        pad8Control.setProperty( "Velocity", "Off", nullptr );
        pad8Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad8Control, nullptr );

        juce::ValueTree pad9Control( "CONTROL" );
        pad9Control.setProperty( "PadParameters", "Pad 9", nullptr );
        pad9Control.setProperty( "Bank", "A", nullptr );
        pad9Control.setProperty( "SetAll", "0", nullptr );
        pad9Control.setProperty( "PadLed", "Local", nullptr );
        pad9Control.setProperty( "PadNo", "1", nullptr );
        pad9Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad9Control.setProperty( "NoteNumber", "8", nullptr );
        pad9Control.setProperty( "NoteValue", "G#-2", nullptr );
        pad9Control.setProperty( "Velocity", "Off", nullptr );
        pad9Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad9Control, nullptr );

        juce::ValueTree pad10Control( "CONTROL" );
        pad10Control.setProperty( "PadParameters", "Pad 10", nullptr );
        pad10Control.setProperty( "Bank", "A", nullptr );
        pad10Control.setProperty( "SetAll", "0", nullptr );
        pad10Control.setProperty( "PadLed", "Local", nullptr );
        pad10Control.setProperty( "PadNo", "1", nullptr );
        pad10Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad10Control.setProperty( "NoteNumber", "9", nullptr );
        pad10Control.setProperty( "NoteValue", "A-2", nullptr );
        pad10Control.setProperty( "Velocity", "Off", nullptr );
        pad10Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad10Control, nullptr );

        juce::ValueTree pad11Control( "CONTROL" );
        pad11Control.setProperty( "PadParameters", "Pad 11", nullptr );
        pad11Control.setProperty( "Bank", "A", nullptr );
        pad11Control.setProperty( "SetAll", "0", nullptr );
        pad11Control.setProperty( "PadLed", "Local", nullptr );
        pad1Control.setProperty( "PadNo", "1", nullptr );
        pad11Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad1Control.setProperty( "NoteNumber", "0", nullptr );
        pad1Control.setProperty( "NoteValue", "C-2", nullptr );
        pad11Control.setProperty( "Velocity", "Off", nullptr );
        pad11Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad11Control, nullptr );

        juce::ValueTree pad12Control( "CONTROL" );
        pad12Control.setProperty( "PadParameters", "Pad 12", nullptr );
        pad12Control.setProperty( "Bank", "A", nullptr );
        pad12Control.setProperty( "SetAll", "0", nullptr );
        pad12Control.setProperty( "PadLed", "Local", nullptr );
        pad12Control.setProperty( "PadNo", "1", nullptr );
        pad12Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad12Control.setProperty( "NoteNumber", "11", nullptr );
        pad12Control.setProperty( "NoteValue", "B-2", nullptr );
        pad12Control.setProperty( "Velocity", "Off", nullptr );
        pad12Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad12Control, nullptr );

        juce::ValueTree pad13Control( "CONTROL" );
        pad13Control.setProperty( "PadParameters", "Pad 13", nullptr );
        pad13Control.setProperty( "Bank", "A", nullptr );
        pad13Control.setProperty( "SetAll", "0", nullptr );
        pad13Control.setProperty( "PadLed", "Local", nullptr );
        pad13Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad13Control.setProperty( "NoteNumber", "12", nullptr );
        pad13Control.setProperty( "NoteValue", "C-1", nullptr );
        pad13Control.setProperty( "Velocity", "Off", nullptr );
        pad13Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad13Control, nullptr );

        juce::ValueTree pad14Control( "CONTROL" );
        pad14Control.setProperty( "PadParameters", "Pad 14", nullptr );
        pad14Control.setProperty( "Bank", "A", nullptr );
        pad14Control.setProperty( "SetAll", "0", nullptr );
        pad14Control.setProperty( "PadLed", "Local", nullptr );
        pad14Control.setProperty( "PadNo", "1", nullptr );
        pad14Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad14Control.setProperty( "NoteNumber", "13", nullptr );
        pad14Control.setProperty( "NoteValue", "C#-1", nullptr );
        pad14Control.setProperty( "Velocity", "Off", nullptr );
        pad14Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad14Control, nullptr );

        juce::ValueTree pad15Control( "CONTROL" );
        pad15Control.setProperty( "PadParameters", "Pad 15", nullptr );
        pad15Control.setProperty( "Bank", "A", nullptr );
        pad15Control.setProperty( "SetAll", "0", nullptr );
        pad15Control.setProperty( "PadLed", "Local", nullptr );
        pad15Control.setProperty( "PadNo", "1", nullptr );
        pad15Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad15Control.setProperty( "NoteNumber", "14", nullptr );
        pad15Control.setProperty( "NoteValue", "D-1", nullptr );
        pad15Control.setProperty( "Velocity", "Off", nullptr );
        pad15Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad15Control, nullptr );

        juce::ValueTree pad16Control( "CONTROL" );
        pad16Control.setProperty( "PadParameters", "Pad 16", nullptr );
        pad16Control.setProperty( "Bank", "A", nullptr );
        pad16Control.setProperty( "SetAll", "0", nullptr );
        pad16Control.setProperty( "PadLed", "Local", nullptr );
        pad16Control.setProperty( "PadNo", "1", nullptr );
        pad16Control.setProperty( "PadMIDIChannel", "1", nullptr );
        pad16Control.setProperty( "NoteNumber", "15", nullptr );
        pad16Control.setProperty( "NoteValue", "D#-1", nullptr );
        pad16Control.setProperty( "Velocity", "Off", nullptr );
        pad16Control.setProperty( "Aftertouch", "Off", nullptr );
        midiControl.appendChild( pad16Control, nullptr );

        //xy
        juce::ValueTree xyfxControl( "CONTROL" );
        xyfxControl.setProperty( "PadParameters", "XYFXX-Axis", nullptr );
        xyfxControl.setProperty( "MIDIChannelX", "1", nullptr );
        xyfxControl.setProperty( "MIDIChannelY", "1", nullptr );
        xyfxControl.setProperty( "Mute1", "Default", nullptr );
        xyfxControl.setProperty( "Mute2", "Default", nullptr );
        xyfxControl.setProperty( "CCNumberX", "24", nullptr );
        xyfxControl.setProperty( "CCNumberY", "25", nullptr );
        xyfxControl.setProperty( "LowRangeX", "0", nullptr );
        xyfxControl.setProperty( "HighRangeX", "127", nullptr );
        xyfxControl.setProperty( "LowRangeY", "0", nullptr );
        xyfxControl.setProperty( "HighRangeY", "127", nullptr );
        midiControl.appendChild( xyfxControl, nullptr );

        leftComp.option1List.onSelectionChanged = [&]( int index, const juce::String& ) {
            leftComp.setPadOptionsVisible( false );
            bool bankSel{ false };
            if( index <= 17 )
            {
                rightComp.setCurentComponent( 0 ); //buttons
            }
            else if( index >= 18 && index <= 21 )
            {
                rightComp.setCurentComponent( 1 );
            }
            else if( index >= 22 && index <= 37 ) // pad screens
            {
                leftComp.setPadOptionsVisible( true );
                rightComp.setCurentComponent( 3 );
                auto b = leftComp.bank.getSelectedIndex();

                leftComp.bank.setSelected( b < 0 ? 0 : b );
                if( leftComp.setAll.getToggleState() == false )
                {
                    bankSel = true;
                    loadBankSettings( leftComp.bank.getSelected() );
                }
            }
            else if( index == 37 || index == 38 ) // xyfx
            {
                rightComp.setCurentComponent( 2 );
            }
            if( !bankSel )
                refresh();
        };

        rightComp.button.typeOptions.onClick                = [&]() { saveData(); };
        rightComp.button.midiChannelValue.onValueChanged    = [&]( int value ) { saveData(); };
        rightComp.button.ccNumberValue.onValueChanged       = [&]( int value ) { saveData(); };
        rightComp.button.lightLedOptions.onSelectionChanged = [&]( int index, const juce::String ) { saveData(); };
        rightComp.qlink.lightLedOptions.onSelectionChanged  = [&]( int index, const juce::String ) { saveData(); };
        rightComp.qlink.midiChannelValue.onValueChanged     = [&]( int value ) { saveData(); };

        rightComp.qlink.ccNumberValue.onValueChanged  = [&]( int value ) { saveData(); };
        rightComp.qlink.typeOptions.onClick           = [&]() { saveData(); };
        rightComp.qlink.touchSenceOptions.onClick     = [&]() { saveData(); };
        rightComp.qlink.lowRangeValue.onValueChanged  = [&]( int value ) { saveData(); };
        rightComp.qlink.highRangeValue.onValueChanged = [&]( int value ) { saveData(); };
        rightComp.qlink.note2.onSelectionChanged      = [this]( int index, const juce::String& ) {
            rightComp.qlink.note1.setValue( index, juce::dontSendNotification );
            saveData();
        };
        rightComp.qlink.note1.onValueChanged = [this]( int intex ) {
            rightComp.qlink.note2.setSelected( intex, juce::dontSendNotification );
            saveData();
        };
        rightComp.pad.ledBox.onSelectionChanged = [&]( int index, const juce::String value ) {
            if( !leftComp.setAll.getToggleState() )
                saveBankData();
            else
                saveData();
        };
        leftComp.bank.onSelectionChanged = [&]( int index, const juce::String bank ) {
            bool toggleState = leftComp.setAll.getToggleState();
            bool isBankLoad{ false };
            if( toggleState == true )
                padSelect( true );
            else
            {
                for( auto id = 0; id < midiControl.getNumChildren(); ++id )
                {
                    auto  bankTree              = midiControl.getChild( id );
                    auto& bankProperty          = bankTree.getProperty( "Bank" );
                    auto  padParametersProperty = bankTree.getChildWithName( "CONTROL" ).getProperty( "PadParameters" ).toString();
                    if( bankTree.hasType( "BANK" ) && ( bankProperty == bank ) && ( padParametersProperty == leftComp.option1List.getSelected() ) )
                    {
                        isBankLoad = true;
                        loadBankSettings( bank );
                        break;
                    }
                }
                if( !isBankLoad && !toggleState )
                    createNewBankSettings( bank );
            }
        };
        rightComp.pad.afterTouchBox.onSelectionChanged = [&]( int index, const juce::String value ) {
            if( !leftComp.setAll.getToggleState() )
                saveBankData();
            else
                saveData();
        };
        rightComp.pad.midiBox.onValueChanged = [&]( int value ) {
            if( !leftComp.setAll.getToggleState() )
                saveBankData();
            else
                saveData();
        };
        rightComp.pad.noteBox2.onSelectionChanged = [this]( int index, const juce::String& value ) {
            rightComp.pad.noteBox1.setValue( index, juce::dontSendNotification );
            if( !leftComp.setAll.getToggleState() )
                saveBankData();
            else
                saveData();
        };
        rightComp.pad.noteBox1.onValueChanged = [this]( int intex ) {
            rightComp.pad.noteBox2.setSelected( intex, juce::dontSendNotification );
            if( !leftComp.setAll.getToggleState() )
                saveBankData();
            else
                saveData();
        };
        rightComp.pad.on.onClick = [&]() {
            if( !leftComp.setAll.getToggleState() )
                saveBankData();
            else
                saveData();
        };
        leftComp.setAll.onClick               = [&]() { saveData(); };
        rightComp.xy.midCh1.onValueChanged    = [&]( int value ) { saveData(); };
        rightComp.xy.midCh2.onValueChanged    = [&]( int value ) { saveData(); };
        rightComp.xy.ccNum1.onValueChanged    = [&]( int value ) { saveData(); };
        rightComp.xy.ccNum2.onValueChanged    = [&]( int value ) { saveData(); };
        rightComp.xy.lowRang1.onValueChanged  = [&]( int value ) { saveData(); };
        rightComp.xy.highRang1.onValueChanged = [&]( int value ) { saveData(); };
        rightComp.xy.lowRang2.onValueChanged  = [&]( int value ) { saveData(); };
        rightComp.xy.highRang2.onValueChanged = [&]( int value ) { saveData(); };
        rightComp.xy.mute1.onClick            = [&]() { saveData(); };
        rightComp.xy.mute2.onClick            = [&]() { saveData(); };
    }

    void MidiControlHardware::knobCommand( int knob, int value )
    {
        knobRotated = true;
        switch( knob )
        {
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1:
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_2:
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_3:
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_4:
                juce::String name = "Q-Link " + juce::String( knob );
                leftComp.option1List.setSelected( name );
                break;
        }
        updateValue( knob, value );
    }

    void MidiControlHardware::saveData()
    {
        for( auto id = 0; id < midiControl.getNumChildren(); ++id )
        {
            if( midiControl.getChild( id ).hasProperty( "Type" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                bool isMomentary = rightComp.button.typeOptions.getToggleState();
                midiControl.getChild( id ).setProperty( "Type", isMomentary ? "Momentary" : "Toggle", nullptr );
            }

            if( midiControl.getChild( id ).hasProperty( "ButtonLed" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                juce::String value = rightComp.button.lightLedOptions.getSelectedValue();
                midiControl.getChild( id ).setProperty( "ButtonLed", value, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "CCNumber" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                int ccIndex = rightComp.button.ccNumberValue.getValue();
                midiControl.getChild( id ).setProperty( "CCNumber", ccIndex, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "MIDIChannel" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )

            {
                int selectedIndex = rightComp.button.midiChannelValue.getValue();
                midiControl.getChild( id ).setProperty( "MIDIChannel", selectedIndex, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "QlinkLed" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                juce::String value = rightComp.qlink.lightLedOptions.getSelectedValue();
                midiControl.getChild( id ).setProperty( "QlinkLed", value, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "QlinkMIDIChannel" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                int selectedIndex = rightComp.qlink.midiChannelValue.getValue();
                midiControl.getChild( id ).setProperty( "QlinkMIDIChannel", selectedIndex, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "QlinkCCNumber" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                int ccIndex = rightComp.qlink.ccNumberValue.getValue();

                midiControl.getChild( id ).setProperty( "QlinkCCNumber", ccIndex, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "Mode" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                bool isAbsolute = rightComp.qlink.typeOptions.getToggleState();

                midiControl.getChild( id ).setProperty( "Mode", isAbsolute ? "Absolute" : "Relative", nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "TouchSense" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                bool ison = rightComp.qlink.touchSenceOptions.getToggleState();
                midiControl.getChild( id ).setProperty( "TouchSense", ison ? "On" : "Off", nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "LowRange" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                int lowIndex = rightComp.qlink.lowRangeValue.getValue();

                midiControl.getChild( id ).setProperty( "LowRange", lowIndex, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "HighRange" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                int highIndex = rightComp.qlink.highRangeValue.getValue();

                midiControl.getChild( id ).setProperty( "HighRange", highIndex, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "Note" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                int highIndex = rightComp.qlink.note1.getValue();

                midiControl.getChild( id ).setProperty( "Note", highIndex, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "NoteContent" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                juce::String note1 = rightComp.qlink.note2.getSelectedValue();

                midiControl.getChild( id ).setProperty( "NoteContent", note1, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "SetAll" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                bool sel = leftComp.setAll.getToggleState();

                midiControl.getChild( id ).setProperty( "SetAll", sel ? "1" : "0", nullptr );
                padSelect( sel );
            }
            if( midiControl.getChild( id ).hasProperty( "MIDIChannelX" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                int value = rightComp.xy.midCh1.getValue();

                midiControl.getChild( id ).setProperty( "MIDIChannelX", value, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "MIDIChannelY" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )

            {
                int value = rightComp.xy.midCh2.getValue();
                midiControl.getChild( id ).setProperty( "MIDIChannelY", value, nullptr );
            }

            if( midiControl.getChild( id ).hasProperty( "CCNumberX" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                int value = rightComp.xy.ccNum1.getValue();

                midiControl.getChild( id ).setProperty( "CCNumberX", value, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "CCNumberY" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )

            {
                int value = rightComp.xy.ccNum2.getValue();

                midiControl.getChild( id ).setProperty( "CCNumberY", value, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "LowRangeX" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                int value = rightComp.xy.lowRang1.getValue();

                midiControl.getChild( id ).setProperty( "LowRangeX", value, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "HighRangeX" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                int valueX = rightComp.xy.highRang1.getValue();

                midiControl.getChild( id ).setProperty( "HighRangeX", valueX, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "LowRangeY" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                int valuey = rightComp.xy.lowRang2.getValue();

                midiControl.getChild( id ).setProperty( "LowRangeY", valuey, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "HighRangeY" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                int valY = rightComp.xy.highRang2.getValue();

                midiControl.getChild( id ).setProperty( "HighRangeY", valY, nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "Mute1" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                bool ison = rightComp.xy.mute1.getToggleState();

                midiControl.getChild( id ).setProperty( "Mute1", ison ? "Active" : "Default", nullptr );
            }
            if( midiControl.getChild( id ).hasProperty( "Mute2" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                bool ison = rightComp.xy.mute2.getToggleState();

                midiControl.getChild( id ).setProperty( "Mute2", ison ? "Active" : "Default", nullptr );
            }
        }
    }
    void MidiControlHardware::refresh()
    {
        for( auto id = 0; id < midiControl.getNumChildren(); ++id )
        {
            if( midiControl.getChild( id ).hasProperty( "Type" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                auto& option      = midiControl.getChild( id ).getProperty( "Type" );
                bool  togglestate = option == "Momentary" ? true : false;
                rightComp.button.typeOptions.setToggleState( togglestate, juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "ButtonLed" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.button.lightLedOptions.setSelected( midiControl.getChild( id ).getProperty( "ButtonLed" ).toString(), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "CCNumber" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.button.ccNumberValue.setValue( midiControl.getChild( id ).getProperty( "CCNumber" ), juce::dontSendNotification );
            }

            if( midiControl.getChild( id ).hasProperty( "MIDIChannel" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.button.midiChannelValue.setValue( midiControl.getChild( id ).getProperty( "MIDIChannel" ), juce::dontSendNotification );
            }

            if( midiControl.getChild( id ).hasProperty( "QlinkLed" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.qlink.lightLedOptions.setSelected( midiControl.getChild( id ).getProperty( "QlinkLed" ).toString(), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "QlinkMIDIChannel" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.qlink.midiChannelValue.setValue( midiControl.getChild( id ).getProperty( "QlinkMIDIChannel" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "QlinkCCNumber" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.qlink.ccNumberValue.setValue( midiControl.getChild( id ).getProperty( "QlinkCCNumber" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "LowRange" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.qlink.lowRangeValue.setValue( midiControl.getChild( id ).getProperty( "LowRange" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "HighRange" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.qlink.highRangeValue.setValue( midiControl.getChild( id ).getProperty( "HighRange" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "Mode" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                auto& option      = midiControl.getChild( id ).getProperty( "Mode" );
                bool  togglestate = option == "Absolute" ? true : false;
                rightComp.qlink.typeOptions.setToggleState( togglestate, juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "TouchSense" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                auto& option      = midiControl.getChild( id ).getProperty( "TouchSense" );
                bool  togglestate = option == "On" ? true : false;
                rightComp.qlink.touchSenceOptions.setToggleState( togglestate, juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "Note" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.qlink.note1.setValue( midiControl.getChild( id ).getProperty( "Note" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "NoteContent" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.qlink.note2.setSelected( midiControl.getChild( id ).getProperty( "NoteContent" ).toString(), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "Bank" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                leftComp.bank.setSelected( midiControl.getChild( id ).getProperty( "Bank" ).toString(), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "SetAll" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                auto& sel       = midiControl.getChild( id ).getProperty( "SetAll" );
                bool  seltoggle = sel == "1" ? true : false;
                leftComp.setAll.setToggleState( seltoggle, juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "PadLed" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.pad.ledBox.setSelected( midiControl.getChild( id ).getProperty( "PadLed" ).toString(), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "Velocity" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                auto& vel = midiControl.getChild( id ).getProperty( "Velocity" );
                bool  op  = vel == "On" ? true : false;
                rightComp.pad.on.setToggleState( op, juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "PadMIDIChannel" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.pad.midiBox.setValue( midiControl.getChild( id ).getProperty( "PadMIDIChannel" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "Aftertouch" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.pad.afterTouchBox.setSelected( midiControl.getChild( id ).getProperty( "Aftertouch" ).toString(), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "NoteNumber" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.pad.noteBox1.setValue( midiControl.getChild( id ).getProperty( "NoteNumber" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "NoteValue" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.pad.noteBox2.setSelected( midiControl.getChild( id ).getProperty( "NoteValue" ).toString(), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "MIDIChannelX" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.xy.midCh1.setValue( midiControl.getChild( id ).getProperty( "MIDIChannelX" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "MIDIChannelY" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.xy.midCh2.setValue( midiControl.getChild( id ).getProperty( "MIDIChannelY" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "CCNumberX" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.xy.ccNum1.setValue( midiControl.getChild( id ).getProperty( "CCNumberX" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "CCNumberY" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.xy.ccNum2.setValue( midiControl.getChild( id ).getProperty( "CCNumberY" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "LowRangeX" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.xy.lowRang1.setValue( midiControl.getChild( id ).getProperty( "LowRangeX" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "HighRangeX" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.xy.highRang1.setValue( midiControl.getChild( id ).getProperty( "HighRangeX" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "LowRangeY" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.xy.lowRang2.setValue( midiControl.getChild( id ).getProperty( "LowRangeY" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "HighRangeY" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                rightComp.xy.highRang2.setValue( midiControl.getChild( id ).getProperty( "HighRangeY" ), juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "Mute1" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                auto& mute        = midiControl.getChild( id ).getProperty( "Mute1" );
                bool  togglestate = mute == "Active" ? true : false;
                rightComp.xy.mute1.setToggleState( togglestate, juce::dontSendNotification );
            }
            if( midiControl.getChild( id ).hasProperty( "Mute2" ) && ( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() ) )
            {
                auto& mute        = midiControl.getChild( id ).getProperty( "Mute2" );
                bool  togglestate = mute == "Active" ? true : false;
                rightComp.xy.mute2.setToggleState( togglestate, juce::dontSendNotification );
            }
        }
    }
    void MidiControlHardware::padSelect( bool sel )
    {
        if( sel )
        {
            for( auto id = 0; id < midiControl.getNumChildren(); ++id )
            {
                if( midiControl.getChild( id ).hasProperty( "Bank" ) )
                {
                    if( midiControl.getChild( id ).hasProperty( "PadLed" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
                    {
                        juce::String value = rightComp.pad.ledBox.getSelectedValue();

                        midiControl.getChild( id ).setProperty( "PadLed", value, nullptr );
                    }
                    if( midiControl.getChild( id ).hasProperty( "Bank" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
                    {
                        juce::String value = leftComp.bank.getSelectedValue();

                        midiControl.getChild( id ).setProperty( "Bank", value, nullptr );
                    }
                    if( midiControl.getChild( id ).hasProperty( "Aftertouch" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
                    {
                        juce::String value1 = rightComp.pad.afterTouchBox.getSelectedValue();

                        midiControl.getChild( id ).setProperty( "Aftertouch", value1, nullptr );
                    }
                    if( midiControl.getChild( id ).hasProperty( "PadMIDIChannel" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
                    {
                        int value = rightComp.pad.midiBox.getValue();

                        midiControl.getChild( id ).setProperty( "PadMIDIChannel", value, nullptr );
                    }
                    if( midiControl.getChild( id ).hasProperty( "NoteNumber" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
                    {
                        int value = rightComp.pad.noteBox1.getValue();

                        midiControl.getChild( id ).setProperty( "NoteNumber", value, nullptr );
                    }
                    if( midiControl.getChild( id ).hasProperty( "NoteValue" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
                    {
                        juce::String note = rightComp.pad.noteBox2.getSelectedValue();

                        midiControl.getChild( id ).setProperty( "NoteValue", note, nullptr );
                    }
                    if( midiControl.getChild( id ).hasProperty( "Velocity" ) && midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
                    {
                        bool ison = rightComp.pad.on.getToggleState();

                        midiControl.getChild( id ).setProperty( "Velocity", ison ? "On" : "Off", nullptr );
                    }
                }
            }
        }
        else if( !sel )
            loadDefaultSettings();
    }
    void MidiControlHardware::createNewBankSettings( juce::String bankName )
    {
        bool            bankFound = false;
        juce::ValueTree bankTree;
        juce::ValueTree newBankTree( "BANK" );
        for( auto id = 0; id < midiControl.getNumChildren(); ++id )
        {
            bankTree = midiControl.getChild( id );
            if( bankTree.hasType( "BANK" ) && bankTree.hasProperty( "Bank" ) && bankTree.getProperty( "Bank" ) == bankName )
            {
                bankFound = true;
                break;
            }
        }
        juce::ValueTree newPadControl( "CONTROL" );
        newPadControl.setProperty( "PadParameters", leftComp.option1List.getSelected(), nullptr );
        newPadControl.setProperty( "PadLed", "Local", nullptr );
        newPadControl.setProperty( "Aftertouch", "Off", nullptr );
        newPadControl.setProperty( "PadMIDIChannel", "1", nullptr );
        newPadControl.setProperty( "NoteNumber", "0", nullptr );
        newPadControl.setProperty( "NoteValue", "C-2", nullptr );
        newPadControl.setProperty( "Velocity", "Off", nullptr );
        newPadControl.setProperty( "Bank", bankName, nullptr );

        if( bankFound )
        {
            bool padUpdated = false;
            for( auto padId = 0; padId < bankTree.getNumChildren(); ++padId )
            {
                auto padControl = bankTree.getChild( padId );
                if( padControl.hasProperty( "PadParameters" ) && padControl.getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
                {
                    bankTree.removeChild( padControl, nullptr );
                    bankTree.appendChild( newPadControl, nullptr );
                    padUpdated = true;
                    break;
                }
            }
            if( !padUpdated )
            {
                bankTree.appendChild( newPadControl, nullptr );
            }
        }
        else
        {
            newBankTree.setProperty( "Bank", bankName, nullptr );
            newBankTree.appendChild( newPadControl, nullptr );
            midiControl.appendChild( newBankTree, nullptr );
        }
    }
    void MidiControlHardware::saveBankData()
    {
        for( auto i = 0; i < midiControl.getNumChildren(); ++i )
        {
            auto bankTree = midiControl.getChild( i );
            for( auto j = 0; j < bankTree.getNumChildren(); ++j )
            {
                auto childVTree = bankTree.getChild( j );
                if( childVTree.hasProperty( "PadLed" ) && childVTree.hasType( "CONTROL" ) && childVTree.getProperty( "PadParameters" ) == leftComp.option1List.getSelected() && childVTree.getProperty( "Bank" ) == leftComp.bank.getSelected() )
                {
                    juce::String value = rightComp.pad.ledBox.getSelectedValue();
                    childVTree.setProperty( "PadLed", value, nullptr );
                }
                if( childVTree.hasProperty( "Aftertouch" ) && childVTree.hasType( "CONTROL" ) && childVTree.getProperty( "PadParameters" ).toString() == leftComp.option1List.getSelected() && childVTree.getProperty( "Bank" ).toString() == leftComp.bank.getSelected() )

                {
                    juce::String value1 = rightComp.pad.afterTouchBox.getSelectedValue();
                    childVTree.setProperty( "Aftertouch", value1, nullptr );
                }
                if( childVTree.hasProperty( "PadMIDIChannel" ) && childVTree.hasType( "CONTROL" ) && childVTree.getProperty( "PadParameters" ).toString() == leftComp.option1List.getSelected() && childVTree.getProperty( "Bank" ).toString() == leftComp.bank.getSelected() )

                {
                    int value = rightComp.pad.midiBox.getValue();
                    childVTree.setProperty( "PadMIDIChannel", value, nullptr );
                }
                if( childVTree.hasProperty( "NoteNumber" ) && childVTree.hasType( "CONTROL" ) && childVTree.getProperty( "PadParameters" ).toString() == leftComp.option1List.getSelected() && childVTree.getProperty( "Bank" ).toString() == leftComp.bank.getSelected() )
                {
                    int value = rightComp.pad.noteBox1.getValue();
                    childVTree.setProperty( "NoteNumber", value, nullptr );
                }
                if( childVTree.hasProperty( "NoteValue" ) && childVTree.hasType( "CONTROL" ) && childVTree.getProperty( "PadParameters" ).toString() == leftComp.option1List.getSelected() && childVTree.getProperty( "Bank" ).toString() == leftComp.bank.getSelected() )
                {
                    juce::String note = rightComp.pad.noteBox2.getSelectedValue();
                    childVTree.setProperty( "NoteValue", note, nullptr );
                }
                if( childVTree.hasProperty( "Velocity" ) && childVTree.hasType( "CONTROL" ) && childVTree.getProperty( "PadParameters" ).toString() == leftComp.option1List.getSelected() && childVTree.getProperty( "Bank" ).toString() == leftComp.bank.getSelected() )

                {
                    bool ison = rightComp.pad.on.getToggleState();
                    childVTree.setProperty( "Velocity", ison ? "On" : "Off", nullptr );
                }
            }
        }
    }
    void MidiControlHardware::loadBankSettings( juce::String bankName )
    {
        bool hasBankType = false;
        for( auto id = 0; id < midiControl.getNumChildren(); ++id )
        {
            if( midiControl.getChild( id ).hasType( "BANK" ) )
            {
                auto bankTree = midiControl.getChild( id );

                for( auto j = 0; j < bankTree.getNumChildren(); ++j )
                {
                    auto childVTree = bankTree.getChild( j );
                    if( childVTree.hasProperty( "PadParameters" ) && childVTree.getProperty( "PadParameters" ) == leftComp.option1List.getSelected() && childVTree.getProperty( "Bank" ).toString() == leftComp.bank.getSelected() )
                    {
                        hasBankType = true;
                        if( childVTree.hasProperty( "PadLed" ) )
                            rightComp.pad.ledBox.setSelected( childVTree.getProperty( "PadLed" ).toString(), juce::dontSendNotification );

                        if( childVTree.hasProperty( "Aftertouch" ) )
                            rightComp.pad.afterTouchBox.setSelected( childVTree.getProperty( "Aftertouch" ).toString(), juce::dontSendNotification );

                        if( childVTree.hasProperty( "PadMIDIChannel" ) )
                            rightComp.pad.midiBox.setValue( childVTree.getProperty( "PadMIDIChannel" ).toString().getIntValue(), juce::dontSendNotification );

                        if( childVTree.hasProperty( "NoteNumber" ) )
                            rightComp.pad.noteBox1.setValue( childVTree.getProperty( "NoteNumber" ).toString().getIntValue(), juce::dontSendNotification );

                        if( childVTree.hasProperty( "NoteValue" ) )
                            rightComp.pad.noteBox2.setSelected( childVTree.getProperty( "NoteValue" ).toString(), juce::dontSendNotification );

                        if( childVTree.hasProperty( "Velocity" ) )
                        {
                            auto& vel = childVTree.getProperty( "Velocity" );
                            bool  op  = vel == "On" ? true : false;
                            rightComp.pad.on.setToggleState( op, juce::dontSendNotification );
                        }
                    }
                }
            }
        }
        if( !hasBankType )
        {
            loadDefaultSettings();
        }
    }
    void MidiControlHardware::loadDefaultSettings()
    {
        for( auto id = 0; id < midiControl.getNumChildren(); ++id )
        {
            if( midiControl.getChild( id ).getProperty( "PadParameters" ) == leftComp.option1List.getSelected() )
            {
                rightComp.pad.ledBox.setSelected( midiControl.getChild( id ).getProperty( "PadLed" ).toString(), juce::dontSendNotification );
                rightComp.pad.afterTouchBox.setSelected( midiControl.getChild( id ).getProperty( "Aftertouch" ).toString(), juce::dontSendNotification );
                rightComp.pad.midiBox.setValue( midiControl.getChild( id ).getProperty( "PadMIDIChannel" ).toString().getIntValue(), juce::dontSendNotification );
                rightComp.pad.noteBox1.setValue( midiControl.getChild( id ).getProperty( "NoteNumber" ).toString().getIntValue(), juce::dontSendNotification );
                rightComp.pad.noteBox2.setSelected( midiControl.getChild( id ).getProperty( "NoteValue" ).toString().getIntValue(), juce::dontSendNotification );
                rightComp.pad.on.setToggleState( midiControl.getChild( id ).getProperty( "Velocity" ), juce::dontSendNotification );
            }
        }
    }

    void MidiControlHardware::handleCommand( CPktMgr::HWMsg_t& f_HWMsg )
    {
        float padVelocity   = contracts::DclContractsHelper::getPadVelocity( f_HWMsg );
        int   padAfterTouch = contracts::DclContractsHelper::getPadAftertouch( f_HWMsg );
        int   padNumber     = contracts::DclContractsHelper::getPadNumber( f_HWMsg );
        int   buttonState   = contracts::DclContractsHelper::getButtonState( f_HWMsg );

        midimessages( Knob, knobValue, padVelocity, padAfterTouch, buttonState );

        if( contracts::DclContractsHelper::isKnobMessage( f_HWMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HWMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HWMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
            {
                knobCommand( knob, value );
                setKnobValue( value );
            }
        }
    }

    void MidiControlHardware::midimessages( int knob, int knobvalue, float padVelocity, int padAfterTouch, int buttonState )
    {
        if( knobRotated )
        {
            knobRotated = false;
            return;
        }
        bool controlProcessed = false;
        Knob                  = knob;
        juce::Random random;
        int          randomValue = random.nextInt( 128 );

        for( auto id = 0; id < midiControl.getNumChildren(); ++id )
        {
            auto              child = midiControl.getChild( id );
            juce::MidiMessage ccMessage;
            juce::MidiMessage noteOnMessage;
            juce::MidiMessage noteOffMessage;
            juce::MidiMessage aftertouchMessage;
            juce::MidiMessage channelPressureMessage;

            if( child.hasProperty( "CCNumber" ) && child.hasProperty( "MIDIChannel" ) && child.getPropertyAsValue( "PadParameters", nullptr ) == leftComp.option1List.getSelected() )
            {
                auto midiChannel = static_cast<int>( child.getProperty( "MIDIChannel" ) );
                auto ccNumber    = static_cast<int>( child.getProperty( "CCNumber" ) );
                auto buttonType  = child.getProperty( "Type" ).toString();

                if( buttonType == "Momentary" )
                {
                    ccMessage = juce::MidiMessage::controllerEvent( midiChannel, ccNumber, ( buttonState == 1 ) ? 127 : 0 );
                    sendMidiMassages( ccMessage );
                }
                else if( buttonType == "Toggle" )
                {
                    ccMessage = juce::MidiMessage::controllerEvent( midiChannel, ccNumber, ( buttonState == 1 ) ? 127 : 0 );
                    sendMidiMassages( ccMessage );
                }
            }
            else if( child.hasProperty( "QlinkCCNumber" ) && child.hasProperty( "QlinkMIDIChannel" ) && child.getPropertyAsValue( "PadParameters", nullptr ) == leftComp.option1List.getSelected() )
            {
                int  midiChannel = static_cast<int>( child.getProperty( "QlinkMIDIChannel" ) );
                int  ccNumber    = static_cast<int>( child.getProperty( "QlinkCCNumber" ) );
                int  note        = static_cast<int>( child.getProperty( "Note" ) );
                auto mode        = child.getProperty( "Mode" ).toString();
                auto touchSense  = child.getProperty( "TouchSense" ).toString();

                if( mode == "Absolute" && touchSense == "Off" )
                {
                    ccMessage = juce::MidiMessage::controllerEvent( midiChannel, ccNumber, knobvalue );
                    sendMidiMassages( ccMessage );
                }
                else if( mode == "Relative" && touchSense == "Off" )
                {
                    ccMessage = juce::MidiMessage::controllerEvent( midiChannel, ccNumber, 127 );
                    sendMidiMassages( ccMessage );
                }
                else if( mode == "Absolute" && touchSense == "On" )
                {
                    ccMessage = juce::MidiMessage::controllerEvent( midiChannel, ccNumber, knobvalue );
                    sendMidiMassages( ccMessage );

                    noteOnMessage = juce::MidiMessage::noteOn( midiChannel, note, ( padVelocity < 0.0f ? 0.0f : ( padVelocity > 1.0f ? 1.0f : padVelocity ) ) );
                    sendMidiMassages( noteOnMessage );

                    noteOffMessage = juce::MidiMessage::noteOff( midiChannel, note, 0.0f );
                    sendMidiMassages( noteOffMessage );
                }
                else if( mode == "Relative" && touchSense == "On" )
                {
                    ccMessage = juce::MidiMessage::controllerEvent( midiChannel, ccNumber, 127 );
                    sendMidiMassages( ccMessage );

                    noteOnMessage = juce::MidiMessage::noteOn( midiChannel, note, ( padVelocity < 0.0f ? 0.0f : ( padVelocity > 1.0f ? 1.0f : padVelocity ) ) );
                    sendMidiMassages( noteOnMessage );

                    noteOffMessage = juce::MidiMessage::noteOff( midiChannel, note, 0.0f );
                    sendMidiMassages( noteOffMessage );
                }
            }
            else if( child.hasType( "CONTROL" ) || child.hasType( "BANK" ) )
            {
                if( child.hasProperty( "PadMIDIChannel" ) && ( (int)child.getProperty( "SetAll" ) == 1 ) && child.getPropertyAsValue( "PadParameters", nullptr ) == leftComp.option1List.getSelected() )
                {
                    controlProcessed  = true;
                    auto& midiChannel = child.getProperty( "PadMIDIChannel" );
                    auto& padNote     = child.getProperty( "NoteNumber" );
                    auto  velocity    = child.getProperty( "Velocity" ).toString();
                    auto  aftertouch  = child.getProperty( "Aftertouch" ).toString();

                    float velocityValue = ( velocity == "On" ) ? static_cast<float>( padVelocity ) / 127.0f : ( padVelocity < 0.0f ? 0.0f : ( padVelocity > 1.0f ? 1.0f : padVelocity ) );
                    if( buttonState == 1 )
                    {
                        noteOnMessage = juce::MidiMessage::noteOn( midiChannel, padNote, velocityValue );
                        sendMidiMassages( noteOnMessage );
                    }
                    else if( buttonState != padStatePressureUpdate )
                    {
                        noteOffMessage = juce::MidiMessage::noteOff( midiChannel, padNote, 0.0f );
                        sendMidiMassages( noteOffMessage );
                    }

                    if( aftertouch == "Poly" )
                    {
                        aftertouchMessage = juce::MidiMessage::aftertouchChange( midiChannel, padNote, padAfterTouch );
                        sendMidiMassages( aftertouchMessage );
                    }
                    else if( aftertouch == "Channel" )
                    {
                        channelPressureMessage = juce::MidiMessage::channelPressureChange( midiChannel, randomValue );
                        sendMidiMassages( channelPressureMessage );
                    }
                    break;
                }
                else
                {
                    if( child.hasType( "BANK" ) )
                    {
                        for( auto j = 0; j < child.getNumChildren(); ++j )
                        {
                            auto childVTree = child.getChild( j );
                            if( childVTree.hasProperty( "PadParameters" ) && childVTree.getProperty( "PadParameters" ) == leftComp.option1List.getSelected() && childVTree.getProperty( "Bank" ).toString() == leftComp.bank.getSelected() )
                            {
                                auto& midiChannel = childVTree.getProperty( "PadMIDIChannel" );
                                auto& padNote     = childVTree.getProperty( "NoteNumber" );
                                auto  velocity    = childVTree.getProperty( "Velocity" ).toString();
                                auto  aftertouch  = childVTree.getProperty( "Aftertouch" ).toString();

                                float velocityValue = ( velocity == "On" ) ? static_cast<float>( padVelocity ) / 127.0f : ( padVelocity < 0.0f ? 0.0f : ( padVelocity > 1.0f ? 1.0f : padVelocity ) );
                                if( buttonState == 1 )
                                {
                                    noteOnMessage = juce::MidiMessage::noteOn( midiChannel, padNote, velocityValue );
                                    sendMidiMassages( noteOnMessage );
                                }
                                else
                                {
                                    noteOffMessage = juce::MidiMessage::noteOff( midiChannel, padNote, 0.0f );
                                    sendMidiMassages( noteOffMessage );
                                }

                                if( aftertouch == "Poly" )
                                {
                                    aftertouchMessage = juce::MidiMessage::aftertouchChange( midiChannel, padNote, padAfterTouch );
                                    sendMidiMassages( aftertouchMessage );
                                }
                                else if( aftertouch == "Channel" )
                                {
                                    channelPressureMessage = juce::MidiMessage::channelPressureChange( midiChannel, randomValue );
                                    sendMidiMassages( channelPressureMessage );
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void MidiControlHardware::sendMidiMassages( juce::MidiMessage& data )
    {
        if( outputPort != "None" )
        {
            mt::getAudioInterface().sendMidiMessage( outputPort, data );
            if( onMessageSend )
                onMessageSend( data, outputPort );
        }
    }
    void MidiControlHardware::resized()
    {
        auto b = getLocalBounds();
        leftComp.setBoundsRelative( 0.0097f, 0.0186f, 0.333f, 0.7388f );
        rightComp.setBoundsRelative( 0.3435f, 0.0186f, 0.6367f, 0.7388f );
    }
    void MidiControlHardware::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, leftComp.getBounds(), ui::Theme::backgroundColour( 3 ) );
        mt::utils::fillRoundedRect( g, { 362, 10, 652, 396 }, ui::Theme::backgroundColour( 1 ) );
    }

    void MidiControlHardware::visibilityChanged()
    {
        if( isVisible() )
        {
            leftComp.option1List.setSelected( leftComp.option1List.getSelectedIndex() );
            refresh();
        }
    }

    void MidiControlHardware::updateValue( int knob, int value )
    {
        auto name = juce::String( "Q-Link " ) + juce::String( knob );
        for( auto id = 0; id < midiControl.getNumChildren(); ++id )
        {
            auto child = midiControl.getChild( id );

            juce::MidiMessage ccMessage;
            juce::String      mOn;
            if( child.hasProperty( "QlinkCCNumber" ) && child.hasProperty( "QlinkMIDIChannel" ) && child.getPropertyAsValue( "PadParameters", nullptr ).toString() == name )
            {
                auto& midiChannel  = child.getProperty( "QlinkMIDIChannel" );
                auto& ccNumber     = child.getProperty( "QlinkCCNumber" );
                int   currentValue = child.hasProperty( "ValueByte" ) ? (int)child.getProperty( "ValueByte" ) : (int)child.getProperty( "LowRange" );
                int   newValue     = currentValue + value;
                newValue           = juce::jlimit( (int)child.getProperty( "LowRange" ), (int)child.getProperty( "HighRange" ), newValue );
                child.setProperty( "ValueByte", newValue, nullptr );
                ccMessage = juce::MidiMessage::controllerEvent( midiChannel, ccNumber, newValue );
                mOn       = ccMessage.getDescription();
                sendMidiMassages( ccMessage );
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControlXY
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlXY::MidiControlXY()
    {
        mt::utils::addAndMakeVisible( this, leftComp, rightComp );

        rightComp.midCh1.onValueChanged = [this]( int value ) {
            vTree.setProperty( "MIDIChannelX", value, nullptr );
        };

        rightComp.midCh2.onValueChanged = [this]( int Value ) {
            vTree.setProperty( "MIDIChannelY", Value, nullptr );
        };
        rightComp.ccNum1.onValueChanged = [this]( int value ) {
            vTree.setProperty( "CCNumberX", value, nullptr );
        };
        rightComp.ccNum2.onValueChanged = [this]( int value ) {
            vTree.setProperty( "CCNumberY", value, nullptr );
        };
        rightComp.highRang1.onValueChanged = [this]( int value ) {
            vTree.setProperty( "HighRangeX", value, nullptr );
        };
        rightComp.highRang2.onValueChanged = [this]( int value ) {
            vTree.setProperty( "HighRangeY", value, nullptr );
        };
        rightComp.lowRang1.onValueChanged = [this]( int value ) {
            vTree.setProperty( "LowRangeX", value, nullptr );
        };
        rightComp.lowRang2.onValueChanged = [this]( int value ) {
            vTree.setProperty( "LowRangeY", value, nullptr );
        };
        rightComp.mute1.onClick = [this]() {
            bool ison = rightComp.mute1.getToggleState();
            vTree.setProperty( "Mute1", ison ? "Active" : "Default", nullptr );
        };
        rightComp.mute2.onClick = [this]() {
            bool ison = rightComp.mute2.getToggleState();
            vTree.setProperty( "Mute2", ison ? "Active" : "Default", nullptr );
        };
    }
    void MidiControlXY::refresh()
    {
        rightComp.midCh1.setValue( vTree.getProperty( "MIDIChannelX" ), juce::dontSendNotification );
        rightComp.midCh2.setValue( vTree.getProperty( "MIDIChannelY" ), juce::dontSendNotification );
        rightComp.ccNum1.setValue( vTree.getProperty( "CCNumberX" ), juce::dontSendNotification );
        rightComp.ccNum2.setValue( vTree.getProperty( "CCNumberY" ), juce::dontSendNotification );
        rightComp.highRang1.setValue( vTree.getProperty( "HighRangeX" ), juce::dontSendNotification );
        rightComp.highRang2.setValue( vTree.getProperty( "HighRangeY" ), juce::dontSendNotification );
        rightComp.lowRang1.setValue( vTree.getProperty( "LowRangeX" ), juce::dontSendNotification );
        rightComp.lowRang2.setValue( vTree.getProperty( "LowRangeY" ), juce::dontSendNotification );

        bool isMute1Active = vTree.getProperty( "Mute1" ).toString() == "Active";
        rightComp.mute1.setToggleState( isMute1Active, juce::dontSendNotification );

        bool isMute2Active = vTree.getProperty( "Mute2" ).toString() == "Active";
        rightComp.mute2.setToggleState( isMute2Active, juce::dontSendNotification );

        leftComp.onValueChanged = [&]( float xaxis, float yaxis ) {
            midiMessages( xaxis, yaxis );
        };
    }
    void MidiControlXY::visibilityChanged()
    {
        bool found = false;
        for( auto id = 0; id < mt::getSettings().midiControl.vTree.getNumChildren(); ++id )
        {
            auto child = mt::getSettings().midiControl.vTree.getChild( id );
            if( child.hasProperty( "MIDIChannelX" ) )
            {
                vTree = child;
                found = true;
                break;
            }
        }
        if( found )
        {
            refresh();
        }
    }
    void MidiControlXY::midiMessages( float xaxis, float yaxis )
    {
        juce::MidiMessage ccMessagex;
        juce::MidiMessage ccMessagey;

        for( auto id = 0; id < mt::getSettings().midiControl.vTree.getNumChildren(); ++id )
        {
            auto child = mt::getSettings().midiControl.vTree.getChild( id );

            bool  isMute1Active = vTree.getProperty( "Mute1" ).toString() == "Default";
            bool  isMute2Active = vTree.getProperty( "Mute2" ).toString() == "Default";
            auto& midiChannelx  = child.getProperty( "MIDIChannelX" );
            auto& ccNumberx     = child.getProperty( "CCNumberX" );
            int   highrangex    = child.getProperty( "HighRangeX" );
            int   lowRangex     = child.getProperty( "LowRangeX" );
            auto& midiChannely  = child.getProperty( "MIDIChannelY" );
            auto& ccNumbery     = child.getProperty( "CCNumberY" );
            int   highrangey    = child.getProperty( "HighRangeY" );
            int   lowRangey     = child.getProperty( "LowRangeY" );

            int mappedValuex = xaxis * ( highrangex - lowRangex ) + lowRangex;
            int mappedValuey = yaxis * ( highrangey - lowRangey ) + lowRangey;

            if( isMute1Active && isMute2Active )
            {
                if( child.hasProperty( "MIDIChannelX" ) && child.hasProperty( "CCNumberX" ) )
                {
                    ccMessagex = juce::MidiMessage::controllerEvent( midiChannelx, ccNumberx, mappedValuex );
                    ccMessagey = juce::MidiMessage::controllerEvent( midiChannely, ccNumbery, mappedValuey );
                }
            }
            else if( !isMute1Active && isMute2Active )
            {
                if( child.hasProperty( "MIDIChannelX" ) && child.hasProperty( "CCNumberX" ) )
                    ccMessagex = juce::MidiMessage::controllerEvent( midiChannely, ccNumbery, mappedValuey );
            }
            else if( isMute1Active && !isMute2Active )
            {
                if( child.hasProperty( "MIDIChannelX" ) && child.hasProperty( "CCNumberX" ) )
                    ccMessagey = juce::MidiMessage::controllerEvent( midiChannelx, ccNumberx, mappedValuex );
            }
        }
        if( onGridCoordinateChanged )
            onGridCoordinateChanged( ccMessagex, ccMessagey );
    }

    void MidiControlXY::resized()
    {
        leftComp.setBounds( 10, 10, 643, 393 );
        rightComp.setBounds( 662, 10, 348, 440 );
    }
    void MidiControlXY::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, leftComp.getBounds(), ui::Theme::backgroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, rightComp.getBounds(), ui::Theme::backgroundColour( 1 ) );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControlmidilearn
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlMidiLearnLeft::MidiControlMidiLearnLeft()
    {
        mt::utils::addAndMakeVisible( this, learn, enableMapping, sourceList, tracklabel, track, pgm, pgmlabel, ret, retlabel, sub, sublabel, clear, del, source, main, mainlabel, setAll, padEnable, bank, banklist, padno, padnolist );
        mt::utils::makeHidden( bank, banklist, padno, padnolist );
        mt::utils::setFontHeight( 18, clear, del, source, tracklabel, pgmlabel, retlabel, sublabel, mainlabel, padEnable, bank, padno );

        clear.setValueColour( ui::Theme::textColour( 1 ) );
        del.setValueColour( ui::Theme::textColour( 1 ) );
        source.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        sourceList.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );
        source.setValueColour( ui::Theme::textColour( 1 ) );
        tracklabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        tracklabel.setValueColour( ui::Theme::textColour( 1 ) );
        track.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );
        pgmlabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        pgmlabel.setValueColour( ui::Theme::textColour( 1 ) );
        pgm.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );
        retlabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        retlabel.setValueColour( ui::Theme::textColour( 1 ) );
        ret.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );
        sublabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        sublabel.setValueColour( ui::Theme::textColour( 1 ) );
        sub.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );
        mainlabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        mainlabel.setValueColour( ui::Theme::textColour( 1 ) );
        main.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );
        padEnable.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        padEnable.setValueColour( ui::Theme::textColour( 1 ) );
        bank.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        bank.setValueColour( ui::Theme::textColour( 1 ) );
        banklist.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );
        padno.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        padno.setValueColour( ui::Theme::textColour( 1 ) );
        padnolist.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );

        setleftOptionsVisible( false );
        setTrackviewVisible( false );
        setPgmVisible( false );
        setReturnVisible( false );
        setSubmixVisible( false );
        setMainVisible( false );
        setpadVisible( false );
    }

    void MidiControlMidiLearnLeft::paint( juce::Graphics& g )
    {
    }

    void MidiControlMidiLearnLeft::resized()
    {
        source.setBoundsRelative( 0.0292f, 0.06313f, 0.3274f, 0.0353f );
        sourceList.setBoundsRelative( 0.3859f, 0.02525f, 0.5847f, 0.111f );
        track.setBoundsRelative( 0.3859f, 0.1616f, 0.5847f, 0.1111f );
        tracklabel.setBoundsRelative( 0.0292f, 0.1994f, 0.3274f, 0.0504f );
        pgm.setBoundsRelative( 0.3859f, 0.1616f, 0.5847f, 0.1111f );
        pgmlabel.setBoundsRelative( 0.0292f, 0.1994f, 0.3274f, 0.0504f );
        ret.setBoundsRelative( 0.3859f, 0.1616f, 0.5847f, 0.1111f );
        retlabel.setBoundsRelative( 0.0292f, 0.1994f, 0.3274f, 0.0504f );
        sub.setBoundsRelative( 0.3859f, 0.1616f, 0.5847f, 0.1111f );
        sublabel.setBoundsRelative( 0.0292f, 0.1994f, 0.3274f, 0.0504f );
        main.setBoundsRelative( 0.3859f, 0.1616f, 0.5847f, 0.1111f );
        mainlabel.setBoundsRelative( 0.0292f, 0.1994f, 0.3274f, 0.0504f );
        padEnable.setBoundsRelative( 0.02923f, 0.3005f, 0.3274f, 0.111f );
        setAll.setBoundsRelative( 0.8121f, 0.3143f, 0.13895f, 0.111f );
        bank.setBoundsRelative( 0.0369f, 0.5267f, 0.1476f, 0.0409f );
        banklist.setBoundsRelative( 0.2859f, 0.492f, 0.2090f, 0.111f );
        padno.setBoundsRelative( 0.608f, 0.488f, 0.1414f, 0.128f );
        padnolist.setBoundsRelative( 0.7584f, 0.492f, 0.2090f, 0.128f );
        clear.setBoundsRelative( 0.046f, 0.8636f, 0.456f, 0.111f );
        del.setBoundsRelative( 0.533f, 0.8636f, 0.456f, 0.111f );
    }

    MidiControlMidiLearnLeft::~MidiControlMidiLearnLeft()
    {
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControlMidilearn -right
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlMidiLearnTableView ::MidiControlMidiLearnTableView()
    {
        table.reset( new ui::KickTableView{ "List", provider } );
        mt::utils::addAndMakeVisible( this, table.get(), name, type, data, ch, flip );
        mt::utils::setFontHeight( 18, name, type, data, ch, flip );
        name.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        type.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        data.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        ch.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        flip.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
    }

    MidiControlMidiLearnTableView ::~MidiControlMidiLearnTableView()
    {
    }

    void MidiControlMidiLearnTableView ::addStructure( MidiRightData tableData, int row )
    {
        juce::MessageManager::callAsync( [&, tableData, row]() {
            if( row < midiRightDataStruct.size() && row != -1 )
            {
                midiRightDataStruct[row]         = tableData;
                midiRightDataStruct[row].channel = tableData.channel;
                midiRightDataStruct[row].type    = tableData.type;
                midiRightDataStruct[row].data    = tableData.data;
            }
            else
                midiRightDataStruct.push_back( tableData );
            table->refresh();
        } );
    }

    void MidiControlMidiLearnTableView ::resized()
    {
        table->setBoundsRelative( 0.012f, 0.16f, 0.997f, 0.842f );
        table->setRowHeight( 52 );
        table->setRowBackgroundColour( ui::Theme::backgroundColour( 1 ) );
        table->setSelectedRowBackgroundColour( ui::Theme::backgroundColour( 7 ) );
        name.setBoundsRelative( 0.07227f, 0.0146f, 0.2622f, 0.1286f );
        type.setBoundsRelative( 0.2857f, 0.0146f, 0.1840f, 0.1286f );
        data.setBoundsRelative( 0.4691f, 0.0146f, 0.1840f, 0.1286f );
        ch.setBoundsRelative( 0.66989f, 0.0146f, 0.1840f, 0.1286f );
        flip.setBoundsRelative( 0.84780f, 0.0146f, 0.1840f, 0.1286f );
    }
    void MidiControlMidiLearnTableView ::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, rightTopBounds, ui::Theme::backgroundColour( 1 ), 6.0f, true, true, false, false );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControlMidilearn
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlMidiLearn ::MidiControlMidiLearn()

    {
        mt::utils::addAndMakeVisible( this, leftComp, rightComp, add, clear1, midComp );
        clear1.setValueColour( ui::Theme::textColour( 1 ) );
        clear1.setValueFontHeight( 18 );
        populateMaps();
        rightComp.provider.onRowSelected = [&]( int ) { refresh(); };
        rightComp.provider.numRows       = [&]() { return numRows; };
        rightComp.provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
            auto comp = static_cast<MidiLearnTableEntry*>( Existing );
            if( juce::isPositiveAndBelow( Row, numRows ) )
            {
                if( comp == nullptr )
                {
                    comp = new MidiLearnTableEntry;
                }
                if( juce::isPositiveAndBelow( Row, rightComp.midiRightDataStruct.size() ) )
                {
                    comp->setData( rightComp.midiRightDataStruct[Row] );
                }
                return comp;
            }
            return nullptr;
        };

        add.onClick = [&]() {
            if( numRows == 0 )
                midilearn.removeAllChildren( &ae.getUndoManager() );
            leftComp.setleftOptionsVisible( true );
            midComp.setTargetVisible( true );
            numRows += 1;

            juce::ValueTree childTree{ "MIDIDATA" };
            childTree.setProperty( "Source", "None", nullptr );
            childTree.setProperty( "Bank", "None", nullptr );
            childTree.setProperty( "Pad", "None", nullptr );
            childTree.setProperty( "SetAll", "0", nullptr );
            childTree.setProperty( "Target", "Off", nullptr );
            childTree.setProperty( "Type", "None", nullptr );
            childTree.setProperty( "CH", "None", nullptr );
            childTree.setProperty( "Data", "None", nullptr );
            childTree.setProperty( "Flip", "Disabled", nullptr );
            childTree.setProperty( "Learn", "Disabled", nullptr );
            childTree.setProperty( "CC", "None", nullptr );
            childTree.setProperty( "Control", "None", nullptr );
            childTree.setProperty( "Controller", "None", nullptr );
            midilearn.appendChild( childTree, nullptr );

            midComp.setTypeVisible( false );
            rightComp.table->refresh();
            rightComp.table->selectRow( numRows - 1 );
            leftComp.sourceList.setSelected( 0 );
            saveData( numRows - 1 );
        };
        clear1.onClick = [&]() {
            auto p = new ui::FilePopup{ "Are You Sure You Want To Clear All The Assignments ?", false, false };
            mt::utils::showOptions(
                "WARNING!", p, [&, p]( int R ) {
                    if( R == UI::APPLY )
                    {
                           numRows = 0;
                          midilearn.removeAllChildren( &ae.getUndoManager() );
                         rightComp.table->refresh();
                    }
                   delete p; }, ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };
        leftComp.clear.onClick = [&]() {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );
            if( childTree.isValid() )
            {
                childTree.setProperty( "Type", "None", nullptr );
                childTree.setProperty( "CH", "0", nullptr );
                childTree.setProperty( "Data", "0", nullptr );
                childTree.setProperty( "CC", "0", nullptr );
            }
            saveData( rightComp.table->getSelectedRow() );
        };
        leftComp.del.onClick = [&]() {
            midilearn.removeChild( rightComp.table->getSelectedRow(), &ae.getUndoManager() );
            numRows = numRows - 1;
            rightComp.table->selectRow( rightComp.table->getSelectedRow() - 1 );
            rightComp.table->refresh();
        };

        leftComp.pgm.onSelectionChanged = [&]( int index, const juce::String& value ) {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );
            if( childTree.isValid() )
            {
                auto* program = ae.getProgram( value );
                if( program != nullptr )
                {
                    auto type = program->getType();
                    if( type == mt::MTProgramType::Drum || type == mt::MTProgramType::Clip )
                    {
                        leftComp.setpadVisible( true );
                        juce::String programName = leftComp.pgm.getSelectedValue();
                        childTree.setProperty( "Source", programName, nullptr );
                        saveData( rightComp.table->getSelectedRow() );
                    }
                    else
                    {
                        leftComp.setpadVisible( false );
                        juce::String programName = leftComp.pgm.getSelectedValue();
                        childTree.setProperty( "Source", programName, nullptr );
                        saveData( rightComp.table->getSelectedRow() );
                    }
                }
                leftComp.setAll.setToggleState( false, juce::dontSendNotification );
            }
        };

        leftComp.banklist.onSelectionChanged = [&]( int index, const juce::String& ) {
            childTree         = midilearn.getChild( rightComp.table->getSelectedRow() );
            juce::String bank = leftComp.banklist.getSelectedValue();
            childTree.setProperty( "Bank", bank, nullptr );
        };

        leftComp.padnolist.onSelectionChanged = [&]( int index, const juce::String& ) {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );
            int num   = leftComp.padnolist.getSelectedIndex();
            childTree.setProperty( "Pad", num + 1, nullptr );
        };

        leftComp.setAll.onClick = [&]() {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );
            bool sel  = leftComp.setAll.getToggleState();
            childTree.setProperty( "SetAll", sel ? "1" : "0", nullptr );
            leftComp.padno.setVisible( leftComp.setAll.getToggleState() );
            leftComp.padnolist.setVisible( leftComp.setAll.getToggleState() );

            leftComp.bank.setVisible( leftComp.setAll.getToggleState() );
            leftComp.banklist.setVisible( leftComp.setAll.getToggleState() );
        };

        leftComp.sourceList.onSelectionChanged = [&]( int index, const juce::String& ) {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );
            if( childTree.isValid() )
            {
                juce::String con = leftComp.sourceList.getSelectedValue();
                childTree.setProperty( "Control", con, nullptr );
                if( index == 0 )
                {
                    midComp.targetList.setListOptions( { "Off" } );

                    leftComp.setTrackviewVisible( false );
                    leftComp.setPgmVisible( false );
                    leftComp.setReturnVisible( false );
                    leftComp.setSubmixVisible( false );
                    leftComp.setMainVisible( false );
                    leftComp.setpadVisible( false );

                    juce::String  value  = leftComp.sourceList.getSelectedValue();
                    juce ::String target = midComp.targetList.getSelectedValue();
                    childTree.setProperty( "Source", value, nullptr );
                    childTree.setProperty( "Target", target, nullptr );
                    saveData( rightComp.table->getSelectedRow() );
                }
                else if( index == 1 )
                {
                    auto targetValue = childTree.getProperty( "Controller" ).toString();
                    midComp.targetList.setListOptions( { "Off", "Mixer" } );
                    targetValue = targetValue == "None" ? "Off" : targetValue;
                    midComp.targetList.setSelected( targetValue );
                    //midComp.setTypeVisible( true );

                    leftComp.setTrackviewVisible( true );
                    leftComp.setPgmVisible( false );
                    leftComp.setReturnVisible( false );
                    leftComp.setSubmixVisible( false );
                    leftComp.setMainVisible( false );
                    leftComp.setpadVisible( false );

                    auto&                     sel            = ae.getSelected();
                    auto&                     availabletrack = ae.getTrackList( sel.sequence );
                    auto                      id             = 0;
                    std::vector<juce::String> lists;
                    for( auto& it : availabletrack )
                    {
                        auto pProg = ae.getProgramDataForTrack( sel.sequence, id );
                        auto type  = pProg ? pProg->type.get() : mt::MTProgramType::Unknown;
                        if( type != mt::MTProgramType::Audio && type != mt::MTProgramType::Unknown )
                            lists.push_back( it );
                        ++id;
                    }

                    auto trackValue = childTree.getProperty( "Source" ).toString();
                    leftComp.track.setListOptions( lists );
                    if( lists.empty() )
                        trackValue = "None";
                    else
                        trackValue = trackValue == "None" ? lists[0] : trackValue;
                    leftComp.track.setSelected( trackValue );
                }
                else if( index == 2 )
                {
                    auto targetValue = childTree.getProperty( "Controller" ).toString();
                    midComp.targetList.setListOptions( { "Off", "Mixer" } );
                    targetValue = targetValue == "None" ? "Off" : targetValue;
                    midComp.targetList.setSelected( targetValue );

                    leftComp.setTrackviewVisible( true );
                    leftComp.setPgmVisible( false );
                    leftComp.setReturnVisible( false );
                    leftComp.setSubmixVisible( false );
                    leftComp.setMainVisible( false );
                    leftComp.setpadVisible( false );
                    auto& sel            = ae.getSelected();
                    auto& availabletrack = ae.getTrackList( sel.sequence );

                    auto                      id = 0;
                    std::vector<juce::String> lists;
                    for( auto& it : availabletrack )
                    {
                        auto pProg = ae.getProgramDataForTrack( sel.sequence, id );
                        auto type  = pProg ? pProg->type.get() : mt::MTProgramType::Unknown;
                        if( type == mt::MTProgramType::Audio )
                        {
                            lists.push_back( it );
                        }
                        ++id;
                    }
                    auto trackValue = childTree.getProperty( "Source" ).toString();
                    leftComp.track.setListOptions( lists );

                    if( lists.empty() )
                        trackValue = "None";
                    else
                        trackValue = trackValue == "None" ? lists[0] : trackValue;
                    leftComp.track.setSelected( trackValue );
                }
                else if( index == 3 )
                {
                    auto targetValue = childTree.getProperty( "Controller" ).toString();
                    midComp.targetList.setListOptions( { "Off", "Mixer", "Program" } );
                    targetValue = targetValue == "None" ? "Off" : targetValue;
                    midComp.targetList.setSelected( targetValue );
                    leftComp.setPgmVisible( true );
                    leftComp.setTrackviewVisible( false );
                    leftComp.setReturnVisible( false );
                    leftComp.setSubmixVisible( false );
                    leftComp.setMainVisible( false );
                    leftComp.setpadVisible( false );

                    auto& p        = ae.getProgramList();
                    auto  pgmValue = childTree.getProperty( "Source" ).toString();
                    leftComp.pgm.setListOptions( p );
                    if( p.size() == 0 )
                        pgmValue = "None";
                    else
                        pgmValue = pgmValue == "None" ? p[0] : pgmValue;
                    leftComp.pgm.setSelected( pgmValue );
                }
                else if( index == 4 )
                {
                    auto targetValue = childTree.getProperty( "Controller" ).toString();
                    midComp.targetList.setListOptions( { "Off", "Mixer" } );
                    targetValue = targetValue == "None" ? "Off" : targetValue;
                    midComp.targetList.setSelected( targetValue );

                    leftComp.setPgmVisible( false );
                    leftComp.setTrackviewVisible( false );
                    leftComp.setReturnVisible( true );
                    leftComp.setSubmixVisible( false );
                    leftComp.setMainVisible( false );
                    leftComp.setpadVisible( false );

                    auto retValue = childTree.getProperty( "Source" ).toString();
                    leftComp.ret.setListOptions( { "Return 1", "Return 2", "Return 3", "Return 4" } );

                    retValue = retValue == "None" ? "Return 1" : retValue;
                    leftComp.ret.setSelected( retValue );
                }
                else if( index == 5 )
                {
                    auto targetValue = childTree.getProperty( "Controller" ).toString();
                    midComp.targetList.setListOptions( { "Off", "Mixer" } );
                    targetValue = targetValue == "None" ? "Off" : targetValue;
                    midComp.targetList.setSelected( targetValue );

                    leftComp.setSubmixVisible( true );
                    leftComp.setPgmVisible( false );
                    leftComp.setTrackviewVisible( false );
                    leftComp.setReturnVisible( false );
                    leftComp.setMainVisible( false );
                    leftComp.setpadVisible( false );

                    auto subValue = childTree.getProperty( "Source" ).toString();
                    leftComp.sub.setListOptions( { "Submix 1", "Submix 2", "Submix 3", "Submix 4", "Submix 5", "Submix 6", "Submix 7", "Submix 8" } );

                    subValue = subValue == "None" ? "Submix 1" : subValue;
                    leftComp.sub.setSelected( subValue );
                }
                else if( index == 6 )
                {
                    auto targetValue = childTree.getProperty( "Controller" ).toString();
                    midComp.targetList.setListOptions( { "Off", "Mixer" } );
                    targetValue = targetValue == "None" ? "Off" : targetValue;
                    midComp.targetList.setSelected( targetValue );
                    leftComp.setMainVisible( true );
                    leftComp.setSubmixVisible( false );
                    leftComp.setPgmVisible( false );
                    leftComp.setTrackviewVisible( false );
                    leftComp.setReturnVisible( false );
                    leftComp.setpadVisible( false );
                    auto outValue = childTree.getProperty( "Source" ).toString();
                    leftComp.main.setListOptions( { "Out-1,2", "Out-1", "Out-2" } );

                    outValue = outValue == "None" ? "Out-1,2" : outValue;
                    leftComp.main.setSelected( outValue );
                }
            }
        };

        midComp.targetList.onSelectionChanged = [&]( int index, const juce::String& value ) {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );
            if( childTree.isValid() )
            {
                juce::String tar = midComp.targetList.getSelectedValue();
                childTree.setProperty( "Controller", tar, nullptr );
                saveData( rightComp.table->getSelectedRow() );
                if( index <= 0 )
                {
                    midComp.targetList.setSelected( 0, juce::dontSendNotification );
                    midComp.setTypeVisible( false );
                }
                else if( index == 1 )
                {
                    if( leftComp.sourceList.getSelected() == "MIDI Track" )
                    {
                        midComp.setTypeVisible( true );
                        auto targetValue = childTree.getProperty( "Target" ).toString();
                        midComp.typeList.setListOptions( { "Volume", "Pan", "Mute", "solo" } );
                        targetValue = targetValue == "Off" ? "Volume" : targetValue;
                        midComp.typeList.setSelected( targetValue );
                    }
                    else if( leftComp.sourceList.getSelected() == "Return" )
                    {
                        midComp.setTypeVisible( true );
                        auto targetValue = childTree.getProperty( "Target" ).toString();
                        midComp.typeList.setListOptions( { "Volume", "Pan", "Mute" } );
                        targetValue = targetValue == "Off" ? "Volume" : targetValue;
                        midComp.typeList.setSelected( targetValue );
                    }
                    else if( leftComp.sourceList.getSelected() == "Main" )
                    {
                        midComp.setTypeVisible( true );
                        auto targetValue = childTree.getProperty( "Target" ).toString();
                        midComp.typeList.setListOptions( { "Volume", "Pan", "Mute" } );
                        targetValue = targetValue == "Off" ? "Volume" : targetValue;
                        midComp.typeList.setSelected( targetValue );
                    }
                    else if( leftComp.sourceList.getSelected() == ( "Audio Track" ) || ( "Submix" ) )
                    {
                        midComp.setTypeVisible( true );
                        auto targetValue = childTree.getProperty( "Target" ).toString();
                        midComp.typeList.setListOptions( { "Volume", "Pan", "Mute", "solo", "Send 1", "Send 2", "Send 3", "Send 4" } );
                        targetValue = targetValue == "Off" ? "Volume" : targetValue;
                        midComp.typeList.setSelected( targetValue );
                    }
                    else if( leftComp.sourceList.getSelected() == "Program" )
                    {
                        midComp.setTypeVisible( true );
                        auto targetValue = childTree.getProperty( "Target" ).toString();
                        midComp.typeList.setListOptions( { "Volume", "Pan", "Mute", "solo", "Send 1", "Send 2", "Send 3", "Send 4" } );
                        targetValue = targetValue == "Off" ? "Volume" : targetValue;
                        midComp.typeList.setSelected( targetValue );
                    }
                }
                else if( index == 2 )
                {
                    auto selected = leftComp.pgm.getSelectedValue();

                    auto* program = ae.getProgram( selected );
                    auto  type    = ( program != nullptr ) ? program->getType() : mt::MTProgramType::Unknown;
                    bool  isPad   = childTree.getProperty( "SetAll" );
                    midComp.setTypeVisible( true );
                    if( type == mt::MTProgramType::Drum )
                    {
                        if( isPad )
                        {
                            auto targetValue = childTree.getProperty( "Target" ).toString();
                            midComp.typeList.setListOptions( { "Polyphony", "Glide All", "Bend Range", "Doubling", "Doubling Detune", "Glide Time", "Osc 1 Octave", "Osc 1 Fine", "Osc 1 Shape", "Osc 1 F_Envtoshape", "Osc 1 Quard", "Quard_Detune", "Sub Osc Shape", "Osc 2 Octave", "Osc 2 Micro Detune", "Osc 2 Shape", "Osc 2 F_EnvtoShape", "Osc 2 Sync", "Osc 2 EQ Gain", " Osc 2 EQ Freq", "Osc 2 EQ Keytrack", "Osc 2 EQDrive", "Osc 1 Level", "Osc 2 Level", "Sub Osc Level", "Ring  ModLevel", "Mixer Drive", "LPF Cutoff", "LPF Reso", " LPF Slop", "LPF Saturation", "LPF Env", "LPF Keytrack", "Filter Attack", "Filter Decay", "Filter Sustain", "Filter Release", "Filter Release", "Filter Hold", "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release", "Amp Hold", "Env 3 StartLevel", "Env 3 StartTime", "Env 3 Slop Hold", "Env 3 Slop Release", " Env 3 Destination", "Lfo To Pitch", " Lfo To Filter", " Lfo To Amp ", "Lfo To PAN", "Lfo Wave", "Lfo Rate", "Layer 1 Semi", "Layer 1 PAN", "Layer 1 Fine", "Layer 1 Level", "Layer 2 Semi", "Layer 2 PAN", "Layer 2 Fine", "Layer 2 Level", "Layer 3 Semi", "Layer 3 PAN", "Layer 3 Fine", "Layer 3 Level", "Layer 4 Semi", "Layer 1 PAN", "Layer 4 Fine", "Layer 4 Level" } );
                            targetValue = targetValue == "Off" ? "Polyphony" : targetValue;
                            midComp.typeList.setSelected( targetValue );
                        }
                        else
                        {
                            auto targetValue = childTree.getProperty( "Target" ).toString();
                            midComp.typeList.setListOptions( { " Global SemiTune", " Global FineTune" } );
                            targetValue = targetValue == "Off" ? "Global SemiTune" : targetValue;
                            midComp.typeList.setSelected( targetValue );
                        }
                    }
                    else if( type == mt::MTProgramType::Midi )
                    {
                        auto targetValue = childTree.getProperty( "Target" ).toString();
                        midComp.typeList.setListOptions( { "Bank Select", "Modulation", " Breath", " CC3", " Foot", " Portamento ", " DataEntry ", " Volume ", " Balance ", " CC9 ", " Pan ", " Expression ", " Effect1 ", " Effect2 ", " CC14 ", " CC15 ", " GenPurpose1 ", " GenPurpose2 ", " genpurpose3 ", " genpurpose4 ", " CC20 ", " CC21 ", "CC21", " CC23 ", " CC24 ", " CC25 ", " CC26 ", " CC27 ", " CC28 ", " CC29 ", " CC30 ", " CC31 ", " BankSelLSB ", " ModulationLSB ", " BreathLSB ", " CC35 ", " FootLSB ", " PortamentoLSB ", " DataEntryLSB ", " VolumeLSB ", " BalanceLSB ", " CC41 ", " PanLSB ", " ExpressionLSB ", " Effect1LSB ", " Effect2LSB ", " CC46 ", " CC47 ", " GenPur1LSB ", " GenPur2LSB ", " GenPur3LSB ", " GenPur4LSB ", " CC52 ", " CC53 ", " CC54 ", " CC55 ", " CC56 ", " CC57 ", " CC58 ", " CC59 ", " CC60 ", " CC61 ", " CC62 ", " CC63 ", " SustainOnOff ", " PmentoOnOff ", " SostOnOff ", " SoftPedOnOff ", " LegatoAwitch ", " Hold2 ", " SndControl1 ", " SndControl2 ", " SndControl3 ", " SndControl4 ", " SndControl5 ", " SndControl6 ", " SndControl7 ", " SndControl8 ", " SndControl9 ", " SndControl10 ", " GenPurpose5 ", " GenPurpose6 ", " GenPurpose7 ", " GenPurpose8 ", " PmentoControl ", " CC85 ", " CC86 ", " CC87 ", " HiResVelocity ", " CC89 ", " CC90 ", " Send1 ", " Send2 ", " Send3 ", " Send4 ", " Send4 ", " DataIncrement ", " DataDecrement ", " NRPNLSB ", " NRPNMSB ", " RPNLSB ", " RPNMSB ", " CC102 ", " CC103 ", " CC104 ", " CC105 ", " CC106 ", " CC107 ", " CC108 ", " CC109 ", " CC110 ", " CC111 ", " CC112 ", " CC113 ", " CC114 ", " CC115 ", " CC116 ", " CC117 ", " CC118 ", " CC119 ", " AllSoundOff ", " ResetAll ", " Localonoff ", " AllnoteOff ", " OmniModeOff ", " OmniModeOn ", " MonoMode ", " PolyMode " } );
                        targetValue = targetValue == "Off" ? "Bank Select" : targetValue;
                        midComp.typeList.setSelected( targetValue );
                    }
                    else if( type == mt::MTProgramType::Keygroup )
                    {
                        auto targetValueKey = childTree.getProperty( "Target" ).toString();
                        midComp.typeList.setListOptions( { "Program Polyphony", "Global SemiTune", "Global FineTune", "Portamento Time", "PortLegato", "PortQuantize" } );
                        targetValueKey = targetValueKey == "Off" ? "Program Polyphony" : targetValueKey;
                        midComp.typeList.setSelected( targetValueKey );
                    }
                    else if( type == mt::MTProgramType::Audio )
                    {
                    }
                    else if( type == mt::MTProgramType::Clip )
                    {
                        if( isPad )
                        {
                            auto targetValueClip = childTree.getProperty( "Target" ).toString();
                            midComp.typeList.setListOptions( { "SemiTune", "FineTune", "Reverse", "Wrap", "Fade" } );
                            targetValueClip = targetValueClip == "Off" ? "SemiTune" : targetValueClip;
                            midComp.typeList.setSelected( targetValueClip );
                        }
                        else
                        {
                            auto targetValue = childTree.getProperty( "Target" ).toString();
                            midComp.typeList.setListOptions( { " Global SemiTune", " Global FineTune" } );
                            targetValue = targetValue == "Off" ? "Global SemiTune" : targetValue;
                            midComp.typeList.setSelected( targetValue );
                        }
                    }
                    else if( type == mt::MTProgramType::Cv )
                    {
                        auto targetValueCv = childTree.getProperty( "Target" ).toString();
                        midComp.typeList.setListOptions( { "CV Out1", "CV Out2", "CV Out3", "CV Out4", "CV Out5", "CV Out6", "CV Out7", "CV Out8" } );
                        targetValueCv = targetValueCv == "Off" ? "CV Out1" : targetValueCv;
                        midComp.typeList.setSelected( targetValueCv );
                    }
                    else if( type == mt::MTProgramType::Plugin )
                    {
                    }
                }
            }
        };

        midComp.typeList.onSelectionChanged = [&]( int index, const juce::String& ) {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );
            if( childTree.isValid() )
            {
                if( ( leftComp.sourceList.getSelected() == "MIDI Track" ) && ( midComp.targetList.getSelected() == "Mixer" ) )
                {
                    juce::String type = midComp.typeList.getSelectedValue();
                    childTree.setProperty( "Target", type, nullptr );
                    saveData( rightComp.table->getSelectedRow() );
                }
                else if( ( leftComp.sourceList.getSelected() == "Audio Track" ) && ( midComp.targetList.getSelected() == "Mixer" ) )
                {
                    juce::String type = midComp.typeList.getSelectedValue();
                    childTree.setProperty( "Target", type, nullptr );
                    saveData( rightComp.table->getSelectedRow() );
                }
                else if( ( leftComp.sourceList.getSelected() == "Program" ) && ( midComp.targetList.getSelected() == "Mixer" ) )
                {
                    juce::String type = midComp.typeList.getSelectedValue();
                    childTree.setProperty( "Target", type, nullptr );
                    saveData( rightComp.table->getSelectedRow() );
                }
                else if( ( leftComp.sourceList.getSelected() == "Program" ) && ( midComp.targetList.getSelected() == "Program" ) )
                {
                    juce::String type = midComp.typeList.getSelectedValue();
                    childTree.setProperty( "Target", type, nullptr );
                    saveData( rightComp.table->getSelectedRow() );
                }
                else if( ( leftComp.sourceList.getSelected() == "Return" ) && ( midComp.targetList.getSelected() == "Mixer" ) )
                {
                    juce::String type = midComp.typeList.getSelectedValue();
                    childTree.setProperty( "Target", type, nullptr );
                    saveData( rightComp.table->getSelectedRow() );
                }
                else if( ( leftComp.sourceList.getSelected() == "Submix" ) && ( midComp.targetList.getSelected() == "Mixer" ) )
                {
                    juce::String type = midComp.typeList.getSelectedValue();
                    childTree.setProperty( "Target", type, nullptr );
                    saveData( rightComp.table->getSelectedRow() );
                }
                else if( ( leftComp.sourceList.getSelected() == "Main" ) && ( midComp.targetList.getSelected() == "Mixer" ) )
                {
                    juce::String type = midComp.typeList.getSelectedValue();
                    childTree.setProperty( "Target", type, nullptr );
                    saveData( rightComp.table->getSelectedRow() );
                }
            }
        };

        leftComp.track.onSelectionChanged = [&]( int index, const juce::String& ) {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );
            if( childTree.isValid() )
            {
                if( leftComp.sourceList.getSelected() == "MIDI Track" )
                {
                    juce::String value = leftComp.track.getSelectedValue();
                    childTree.setProperty( "Source", value, nullptr );
                    saveData( rightComp.table->getSelectedRow() );
                }
                else if( leftComp.sourceList.getSelected() == "Audio Track" )
                {
                    juce::String value = leftComp.track.getSelectedValue();
                    childTree.setProperty( "Source", value, nullptr );
                    saveData( rightComp.table->getSelectedRow() );
                }
            }
        };

        leftComp.ret.onSelectionChanged = [&]( int index, const juce::String& ) {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );

            juce::String ret = leftComp.ret.getSelectedValue();
            childTree.setProperty( "Source", ret, nullptr );
            saveData( rightComp.table->getSelectedRow() );
        };

        leftComp.sub.onSelectionChanged = [&]( int index, const juce::String& ) {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );

            juce::String sub = leftComp.sub.getSelectedValue();
            childTree.setProperty( "Source", sub, nullptr );
            saveData( rightComp.table->getSelectedRow() );
        };

        leftComp.main.onSelectionChanged = [&]( int intex, const juce::String& ) {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );

            juce::String main = leftComp.main.getSelectedValue();
            childTree.setProperty( "Source", main, nullptr );
            saveData( rightComp.table->getSelectedRow() );
        };

        midComp.flip.onClick = [&]() {
            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );

            bool Enable = midComp.flip.getToggleState();
            if( childTree.isValid() )
                childTree.setProperty( "Flip", Enable ? "Enable" : "Disabled", nullptr );
            saveData( rightComp.table->getSelectedRow() );
        };

        ui::themeChanged.connect( [&]() {
            add.setImages( "Add_normal", "Add_normal", "Add_active" );
        } );
    }

    void MidiControlMidiLearn::saveData( int index )
    {
        MidiRightData data;
        childTree         = midilearn.getChild( index );
        int availableRows = midilearn.getNumChildren();
        numRows           = availableRows;

        juce::String sourcedata = childTree.getProperty( "Source" );
        juce::String targetData = childTree.getProperty( "Target" );
        juce::String flipdata   = childTree.getProperty( "Flip" );
        auto&        value      = childTree.getProperty( "Data" );
        auto&        channels   = childTree.getProperty( "CH" );
        juce::String types      = childTree.getProperty( "Type" );
        data.source             = sourcedata;
        data.target             = targetData;
        data.flip               = flipdata;
        data.data               = value;
        data.channel            = channels;
        data.type               = types;

        rightComp.addStructure( data, index );
    }
    void MidiControlMidiLearn::startLearning()
    {
        auto midiInputs = juce::MidiInput::getAvailableDevices();
        for( const auto& input : midiInputs )
        {
            DBG( "Available MIDI input: " << input.name );
        }

        if( !midiInputs.isEmpty() )
        {
            midiInput = juce::MidiInput::openDevice( midiInputs[0].identifier, this );

            if( midiInput )
            {
                midiInput->start();
                activeMidiInput  = std::move( midiInput );
                isLearning       = true;
                isMappingEnabled = true;
            }
        }
    }
    void MidiControlMidiLearn::stopMidiInput()
    {
        if( activeMidiInput )
        {
            DBG( "Stopping MIDI input..." );
            activeMidiInput->stop();
            isLearning = false;
            activeMidiInput->start();
        }
    }

    MidiControlMidiLearn ::~MidiControlMidiLearn()
    {
    }
    void MidiControlMidiLearn::handleIncomingMidiMessage( juce::MidiInput* source, const juce::MidiMessage& message )
    {
        messageBuffer.push_back( { message, juce::Time::getCurrentTime() } );
        if( isLearning )
        {
            processBufferedMessages();
        }
        else if( isMappingEnabled )
        {
            parameterMapping( message );
        }
    }
    void MidiControlMidiLearn::processBufferedMessages()
    {
        while( !messageBuffer.empty() )
        {
            const auto&              messageData = messageBuffer.front();
            const juce::MidiMessage& message     = messageData.message;

            childTree = midilearn.getChild( rightComp.table->getSelectedRow() );
            MidiRightData data;

            if( message.isNoteOn() )
            {
                int channel    = message.getChannel();
                int noteNumber = message.getNoteNumber();
                int velocity   = message.getVelocity();
                data.channel   = channel;
                data.data      = velocity;
                data.type      = "Note";
                childTree.setProperty( "CH", channel, nullptr );
                childTree.setProperty( "Data", velocity, nullptr );
                childTree.setProperty( "Type", "Note", nullptr );

                saveData( rightComp.table->getSelectedRow() );
                //parameterMapping( message );
            }
            else if( message.isNoteOff() )
            {
                int channel    = message.getChannel();
                int noteNumber = message.getNoteNumber();
                int velocity   = message.getVelocity();
                data.channel   = channel;
                data.data      = velocity;
                data.type      = "Note";
                childTree.setProperty( "CH", channel, nullptr );
                childTree.setProperty( "Data", velocity, nullptr );
                childTree.setProperty( "Type", "Note", nullptr );
                saveData( rightComp.table->getSelectedRow() );
                //parameterMapping( message );
            }
            else if( message.isController() )
            {
                int channel          = message.getChannel();
                int controllerNumber = message.getControllerNumber();
                int controllerValue  = message.getControllerValue();
                data.channel         = channel;
                data.data            = controllerValue;
                data.type            = "cc";
                childTree.setProperty( "CH", channel, nullptr );
                childTree.setProperty( "Data", controllerValue, nullptr );
                childTree.setProperty( "Type", "cc", nullptr );
                childTree.setProperty( "CC", controllerNumber, nullptr );
                parameterMapping( message );
                saveData( rightComp.table->getSelectedRow() );
                ledUpdate( controllerNumber, channel );
            }

            messageBuffer.erase( messageBuffer.begin() );
        }
    }
    void MidiControlMidiLearn::parameterMapping( const juce::MidiMessage& message )
    {
        int   ccValue   = message.getControllerValue();
        int   seqNum    = ae.getSelectedSequence();
        auto  trackData = ae.getTrackData( seqNum, ae.getSelectedTrack() );
        auto& trackName = leftComp.track.getSelected();

        auto* program = ae.getProgram( leftComp.pgm.getSelectedValue() );

        int          trackNum  = leftComp.track.getSelectedIndex();
        int          pgmNumber = leftComp.pgm.getSelectedIndex();
        juce::String pgmName   = leftComp.pgm.getSelectedValue();
        juce::String value     = midComp.typeList.getSelectedValue();
        int          ccIndex   = midComp.typeList.getSelectedIndex();
        juce::String trck      = leftComp.track.getSelectedValue();
        busNo                  = leftComp.ret.getSelectedIndex();
        busNos                 = leftComp.sub.getSelectedIndex();

        busNameReturn = busMgr.getReturnsBusName( busNo );
        busNameSubmix = busMgr.getSubmixBusName( busNos );

        auto trackMute  = ae.getTrackMute( seqNum, trackNum );
        auto trackSolo  = ae.getTrackSolo( seqNum, trackNum );
        auto trackLevel = ae.getTrackLevel( seqNum, trackNum );

        int selectedRow = rightComp.table->getSelectedRow();

        bool isSetAll  = midilearn.getChild( selectedRow ).getProperty( "SetAll" );
        auto selPadNum = midilearn.getChild( selectedRow ).getProperty( "Pad" );
        int  actualPad = static_cast<int>( selPadNum ) - 1;
        int  selBank   = midilearn.getChild( selectedRow ).getProperty( "Bank" );
        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && ( leftComp.sourceList.getSelected() == "MIDI Track" || leftComp.sourceList.getSelected() == "Audio Track" ) )
        {
            auto trkName = trackData ? trackData->name.get() : "";
            if( trackName == trkName )
            {
                juce::MessageManager::callAsync( [&]() {
                    if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Mute" )
                        ae.setTrackMute( seqNum, trackNum, ( trackMute ) ? false : true );
                    if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "solo" )
                        ae.setTrackSolo( seqNum, trackNum, ( trackSolo ) ? false : true );
                    if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Volume" )
                        ae.setTrackLevel( seqNum, trackNum, ccValue );
                } );
            }
        }
        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && leftComp.sourceList.getSelected() == "Program" && midilearn.getChild( selectedRow ).getProperty( "Controller" ) == "Mixer" )
        {
            juce::MessageManager::callAsync( [&]() {
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Volume" )
                    program->setLevel( ccValue );
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Mute" )
                {
                    auto state       = ae.getProgram( pgmName );
                    bool toggleState = state ? state->mute.get() : false;
                    program->setMute( toggleState ? false : true );
                }
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "solo" )
                {
                    auto state       = ae.getProgram( pgmName );
                    bool toggleState = state ? state->solo.get() : false;
                    program->setSolo( toggleState ? false : true );
                }
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Pan" )
                    program->setPan( ccValue );
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Send 1" )
                    program->setSendGain( 0, ccValue );
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Send 2" )
                    program->setSendGain( 1, ccValue );
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Send 3" )
                    program->setSendGain( 2, ccValue );
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Send 4" )
                    program->setSendGain( 3, ccValue );
            } );
        }
        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && leftComp.sourceList.getSelected() == "Program" && midilearn.getChild( selectedRow ).getProperty( "Controller" ) == "Program" )
        {
            auto  selected        = leftComp.pgm.getSelectedValue();
            auto* selectedProgram = ae.getProgram( selected );
            auto  type            = ( selectedProgram != nullptr ) ? selectedProgram->getType() : mt::MTProgramType::Unknown;
            if( type == mt::MTProgramType::Drum )
            {
                pDrum = dynamic_cast<mt::MTDrumProgram*>( ae.getProgram( pgmNumber ) );
                juce::MessageManager::callAsync( [&, actualPad, ccValue]() {
                    if( isSetAll && midilearn.getChild( selectedRow ).hasProperty( "CC" ) )
                    {
                        if( value == "Filter Attack" )
                            pDrum->setEnvelopeAttack( actualPad, toEnum<mt::EnvelopeType>( 1 ), ccValue );
                        else if( value == "Filter Decay" )
                            pDrum->setEnvelopeDecay( actualPad, toEnum<mt::EnvelopeType>( 1 ), ccValue );
                        else if( value == "Filter Sustain" )
                            pDrum->getEnvelopeSustain( actualPad, toEnum<mt::EnvelopeType>( 1 ) );
                        else if( value == "Filter Hold" )
                            pDrum->setEnvelopeHold( actualPad, toEnum<mt::EnvelopeType>( 1 ), ccValue );
                        else if( value == "Amp Attack" )
                            pDrum->setEnvelopeAttack( actualPad, toEnum<mt::EnvelopeType>( 0 ), ccValue );
                        else if( value == "Amp Decay" )
                            pDrum->setEnvelopeDecay( actualPad, toEnum<mt::EnvelopeType>( 0 ), ccValue );
                        else if( value == "Amp Sustain" )
                            pDrum->getEnvelopeSustain( actualPad, toEnum<mt::EnvelopeType>( 0 ) );
                        else if( value == "Amp Hold" )
                            pDrum->setEnvelopeHold( actualPad, toEnum<mt::EnvelopeType>( 0 ), ccValue );
                        else if( value == "Lfo To Pitch" )
                            pDrum->setLFOPitch( actualPad, ccValue );
                        else if( value == " Lfo To Filter" )
                            pDrum->setLFOFilter( actualPad, ccValue );
                        else if( value == " Lfo To Amp " )
                            pDrum->setLFOLevel( actualPad, ccValue );
                        else if( value == "Lfo To PAN" )
                            pDrum->setLFOPan( actualPad, ccValue );
                        else if( value.contains( "Layer" ) )
                        {
                            for( int i = 1; i <= 4; ++i )
                            {
                                juce::String layerPrefix = "Layer " + juce::String( i ) + " ";
                                if( value == layerPrefix + "Level" )
                                {
                                    pDrum->setLayerGain( actualPad, i - 1, ccValue );
                                    break;
                                }
                                else if( value == layerPrefix + "PAN" )
                                {
                                    pDrum->setLayerPan( actualPad, i - 1, ccValue );
                                    break;
                                }
                                else if( value == layerPrefix + "Semi" )
                                {
                                    pDrum->setLayerSemi( actualPad, i - 1, ccValue );
                                    break;
                                }
                                else if( value == layerPrefix + "Fine" )
                                {
                                    pDrum->setLayerFine( actualPad, i - 1, ccValue );
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == " Global SemiTune" )
                            pDrum->setSemi( ccValue );
                        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == " Global FineTune" )
                            pDrum->setFine( ccValue );
                    }
                } );
            }
            if( type == mt::MTProgramType::Midi )
            {
                auto vt = ae.getProgramVT( pgmNumber );

                juce::Identifier property( "CC" + std::to_string( ccIndex ) );
                if( vt.hasProperty( property ) )
                {
                    vt.setProperty( property, ccValue, nullptr );
                }
            }
            if( type == mt::MTProgramType::Cv )
            {
                auto             vt = ae.getProgramVT( pgmNumber );
                juce::Identifier property( "CV" + std::to_string( ccIndex ) );
                if( vt.hasProperty( property ) )
                {
                    vt.setProperty( property, ccValue, nullptr );
                }
            }
            if( type == mt::MTProgramType::Clip )
            {
                juce::MessageManager::callAsync( [&, selectedRow, value, ccValue, isSetAll, selBank, selPadNum]() {
                    if( isSetAll )
                    {
                        int actualPad = static_cast<int>( selPadNum ) - 1;
                        pClip         = dynamic_cast<mt::MTClipProgram*>( ae.getProgram( pgmNumber ) );
                        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "SemiTune" )
                            pClip->setNoteSemi( actualPad, ccValue );
                        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "FineTune" )
                            pClip->setNoteFine( actualPad, ccValue );
                        //if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Reverse" )
                        //    pClip->setNoteReverse( actualPad, pClip->getClipNoteData( actualPad ).reverse ? false : true );
                        //if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Fade" )
                        //    pClip->setNoteFade( actualPad, pClip->getClipNoteData( actualPad ).fade ? false : true );
                        //if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Wrap" )
                        //    pClip->setNoteFade( actualPad, pClip->getClipNoteData( actualPad ).warp ? false : true );
                    }
                    else
                    {
                        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == " Global SemiTune" )
                            selectedProgram->setSemi( ccValue );
                        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == " Global FineTune" )
                            selectedProgram->setFine( ccValue );
                    }
                } );
            }
        }
        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && leftComp.sourceList.getSelected() == "Return" )
        {
            juce::MessageManager::callAsync( [&]() {
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Pan" )
                    busMgr.setBusPan( busNameReturn, ccValue );
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Volume" )
                    busMgr.setBusGain( busNameReturn, ccValue );
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Mute" )
                {
                    auto state = busMgr.isBusMuted( busNameReturn );
                    busMgr.setBusMute( busNameReturn, state ? false : true ); //mute state
                }
            } );
        }
        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && leftComp.sourceList.getSelected() == "Submix" )
        {
            juce::MessageManager::callAsync( [&]() {
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Pan" )
                    busMgr.setBusPan( busNameSubmix, ccValue );
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Mute" )
                {
                    auto state = busMgr.isBusMuted( busNameSubmix );
                    busMgr.setBusMute( busNameSubmix, ccValue ); //mute state
                }
                if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Solo" )
                {
                    auto state = busMgr.isBusSolo( busNameSubmix );
                    busMgr.setBusSolo( busNameSubmix, ccValue ); //mute state
                }
            } );
        }
        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && leftComp.sourceList.getSelected() == "Main" )
        {
            juce::String              list = leftComp.main.getSelectedValue();
            std::vector<juce::String> busNames;
            auto&                     outBusNames = busMgr.getOutputBusNames();

            for( auto& n : outBusNames )
                busNames.push_back( n );

            for( const auto& busName : busNames )

            {
                if( busName == list )
                {
                    juce::MessageManager::callAsync( [&]() {
                        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Volume" )
                            busMgr.setBusGain( busName, ccValue );
                        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Pan" )
                            busMgr.setBusPan( busName, ccValue );
                        if( midilearn.getChild( selectedRow ).hasProperty( "CC" ) && value == "Mute" )
                        {
                            auto state = busMgr.isBusSolo( busName );
                            busMgr.setBusMute( busName, ccValue ); // mute state
                        }
                    } );
                }
            }
        }
    }

    void MidiControlMidiLearn::populateMaps()
    {
        buttonLedToButtonId.set( "Step Seq", contracts::DeviceControlContracts::BtnLedSequence::EN_STEP_SEQ );
        buttonLedToButtonId.set( "Next Seq", contracts::DeviceControlContracts::BtnLedSequence::EN_NEXT_SEQ );
        buttonLedToButtonId.set( "Track Mute", contracts::DeviceControlContracts::BtnLedSequence::EN_TRACK_MUTE );
        buttonLedToButtonId.set( "Sample Edit", contracts::DeviceControlContracts::BtnLedSequence::EN_SAMPLE_EDIT );
        buttonLedToButtonId.set( "Program Edit", contracts::DeviceControlContracts::BtnLedSequence::EN_PROG_EDIT );
        buttonLedToButtonId.set( "Browser", contracts::DeviceControlContracts::BtnLedSequence::EN_BROWSE );
        buttonLedToButtonId.set( "Tap", contracts::DeviceControlContracts::BtnLedSequence::EN_TAP_TEMPO );
        buttonLedToButtonId.set( "Enter", contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER );
        buttonLedToButtonId.set( "-/+", contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS );
        buttonLedToButtonId.set( "Erase", contracts::DeviceControlContracts::BtnLedSequence::EN_ERASE );
        buttonLedToButtonId.set( "Track Mix", contracts::DeviceControlContracts::BtnLedSequence::EN_TRACK_MIX );
        buttonLedToButtonId.set( "TC", contracts::DeviceControlContracts::BtnLedSequence::EN_TC );
        buttonLedToButtonId.set( "PAD Mute", contracts::DeviceControlContracts::BtnLedSequence::EN_PAD_MUTE );
        buttonLedToButtonId.set( "Copy", contracts::DeviceControlContracts::BtnLedSequence::EN_COPY );
        buttonLedToButtonId.set( "Pad Mixer", contracts::DeviceControlContracts::BtnLedSequence::EN_PAD_MIX );
        buttonLedToButtonId.set( "sampler", contracts::DeviceControlContracts::BtnLedSequence::EN_SAMPLER );
        buttonLedToButtonId.set( "XYFX", contracts::DeviceControlContracts::BtnLedSequence::EN_FX );
        //buttonLedToButtonId.set( "Undo",contracts::DeviceControlContracts::BtnLedSequence:: );

        padLedToButtonId.set( "Pad 1", contracts::DeviceControlContracts::PadSequence::EN_PAD1 );
        padLedToButtonId.set( "Pad 2", contracts::DeviceControlContracts::PadSequence::EN_PAD2 );
        padLedToButtonId.set( "Pad 3", contracts::DeviceControlContracts::PadSequence::EN_PAD3 );
        padLedToButtonId.set( "Pad 4", contracts::DeviceControlContracts::PadSequence::EN_PAD4 );
        padLedToButtonId.set( "Pad 5", contracts::DeviceControlContracts::PadSequence::EN_PAD5 );
        padLedToButtonId.set( "Pad 6", contracts::DeviceControlContracts::PadSequence::EN_PAD6 );
        padLedToButtonId.set( "Pad 7", contracts::DeviceControlContracts::PadSequence::EN_PAD7 );
        padLedToButtonId.set( "Pad 8", contracts::DeviceControlContracts::PadSequence::EN_PAD8 );
        padLedToButtonId.set( "Pad 9", contracts::DeviceControlContracts::PadSequence::EN_PAD9 );
        padLedToButtonId.set( "Pad 10", contracts::DeviceControlContracts::PadSequence::EN_PAD10 );
        padLedToButtonId.set( "Pad 11", contracts::DeviceControlContracts::PadSequence::EN_PAD11 );
        padLedToButtonId.set( "Pad 12", contracts::DeviceControlContracts::PadSequence::EN_PAD12 );
        padLedToButtonId.set( "Pad 13", contracts::DeviceControlContracts::PadSequence::EN_PAD13 );
        padLedToButtonId.set( "Pad 14", contracts::DeviceControlContracts::PadSequence::EN_PAD14 );
        padLedToButtonId.set( "Pad 15", contracts::DeviceControlContracts::PadSequence::EN_PAD15 );
        padLedToButtonId.set( "Pad 16", contracts::DeviceControlContracts::PadSequence::EN_PAD16 );
    }

    void MidiControlMidiLearn::ledUpdate( int ccNumber, int channel )
    {
        for( auto id = 0; id < mt::getSettings().midiControl.vTree.getNumChildren(); ++id )
        {
            auto child = mt::getSettings().midiControl.vTree.getChild( id );

            if( child.getProperty( "ButtonLed" ) == "Midi Input" )
            {
                int ccButton      = child.getProperty( "CCNumber" );
                int channelButton = child.getProperty( "MIDIChannel" );
                if( ( ccNumber == ccButton ) && ( channelButton == channel ) )
                {
                    auto param = child.getProperty( "PadParameters" );
                    if( buttonLedToButtonId.contains( param ) )
                    {
                        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setLEDONMessage( buttonLedToButtonId[param] ) );
                    }
                }
            }
            if( ( child.getProperty( "PadLed" ) == "Midi Input" ) )
            {
                int channelPad = child.getProperty( "PadMIDIChannel" );
                if( channelPad == channel )
                {
                    auto param = child.getProperty( "PadParameters" );
                    if( padLedToButtonId.contains( param ) )
                    {
                        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setLEDONMessage( padLedToButtonId[param] ) );
                    }
                }
            }
        }
    }
    void MidiControlMidiLearn::refresh()
    {
        int selectedRow = rightComp.table->getSelectedRow();
        childTree       = midilearn.getChild( selectedRow );
        if( childTree.isValid() )
        {
            juce::String leftList  = childTree.getProperty( "Source" );
            juce::String rightList = childTree.getProperty( "Target" );

            if( midilearn.getChild( selectedRow ).hasProperty( "Source" ) && midilearn.getChild( selectedRow ).hasProperty( "Target" ) )
            {
                if( midilearn.getChild( selectedRow ).getProperty( "Control" ) == "None" && ( midilearn.getChild( selectedRow ).getProperty( "Controller" ) == "Off" || midilearn.getChild( selectedRow ).getProperty( "Controller" ) == "None" ) )
                {
                    leftComp.sourceList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Source" ).toString(), juce::sendNotification );
                    midComp.targetList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Target" ).toString(), juce::sendNotification );
                    leftComp.setpadVisible( false );
                    leftComp.bank.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.banklist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padno.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padnolist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                }

                if( midilearn.getChild( selectedRow ).getProperty( "Control" ) == "MIDI Track" )
                {
                    leftComp.sourceList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Control" ).toString(), juce::sendNotification );
                    midComp.targetList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Controller" ).toString(), juce::sendNotification );
                    leftComp.track.setSelected( midilearn.getChild( selectedRow ).getProperty( "Source" ).toString(), juce::sendNotification );
                    midComp.typeList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Target" ).toString(), juce::sendNotification );
                    leftComp.setpadVisible( false );
                    leftComp.bank.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.banklist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padno.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padnolist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                }
                if( midilearn.getChild( selectedRow ).getProperty( "Control" ) == "Audio Track" )
                {
                    leftComp.sourceList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Control" ).toString(), juce::sendNotification );
                    midComp.targetList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Controller" ).toString(), juce::sendNotification );
                    leftComp.track.setSelected( midilearn.getChild( selectedRow ).getProperty( "Source" ).toString(), juce::sendNotification );
                    midComp.typeList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Target" ).toString(), juce::sendNotification );
                    leftComp.setpadVisible( false );
                    leftComp.bank.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.banklist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padno.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padnolist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                }
                if( midilearn.getChild( selectedRow ).getProperty( "Control" ) == "Program" && midilearn.getChild( selectedRow ).getProperty( "Controller" ) == "Mixer" )
                {
                    leftComp.sourceList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Control" ).toString(), juce::sendNotification );
                    midComp.targetList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Controller" ).toString(), juce::sendNotification );
                    leftComp.pgm.setSelected( midilearn.getChild( selectedRow ).getProperty( "Source" ).toString(), juce::sendNotification );
                    midComp.typeList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Target" ).toString(), juce::sendNotification );
                    leftComp.setAll.setToggleState( midilearn.getChild( selectedRow ).getProperty( "SetAll" ), juce::sendNotification );
                    leftComp.padnolist.setSelected( midilearn.getChild( selectedRow ).getProperty( "Pad" ).toString(), juce::sendNotification );
                    leftComp.banklist.setSelected( midilearn.getChild( selectedRow ).getProperty( "Bank" ).toString(), juce::sendNotification );
                    leftComp.bank.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.banklist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padno.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padnolist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                }
                if( midilearn.getChild( selectedRow ).getProperty( "Control" ) == "Program" && midilearn.getChild( selectedRow ).getProperty( "Controller" ) == "Program" )
                {
                    leftComp.sourceList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Control" ).toString(), juce::sendNotification );
                    midComp.targetList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Controller" ).toString(), juce::sendNotification );
                    leftComp.pgm.setSelected( midilearn.getChild( selectedRow ).getProperty( "Source" ).toString(), juce::sendNotification );
                    midComp.typeList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Target" ).toString(), juce::sendNotification );
                    leftComp.setAll.setToggleState( midilearn.getChild( selectedRow ).getProperty( "SetAll" ), juce::dontSendNotification );
                    leftComp.padnolist.setSelected( midilearn.getChild( selectedRow ).getProperty( "Pad" ).toString(), juce::dontSendNotification );
                    leftComp.banklist.setSelected( midilearn.getChild( selectedRow ).getProperty( "Bank" ).toString(), juce::dontSendNotification );
                    leftComp.bank.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.banklist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padno.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padnolist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                }

                if( midilearn.getChild( selectedRow ).getProperty( "Control" ) == "Return" )
                {
                    leftComp.sourceList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Control" ).toString(), juce::sendNotification );
                    midComp.targetList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Controller" ).toString(), juce::sendNotification );
                    leftComp.ret.setSelected( midilearn.getChild( selectedRow ).getProperty( "Source" ).toString(), juce::sendNotification );
                    midComp.typeList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Target" ).toString(), juce::sendNotification );
                    leftComp.setpadVisible( false );
                    leftComp.bank.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.banklist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padno.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padnolist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                }
                if( midilearn.getChild( selectedRow ).getProperty( "Control" ) == "Submix" )
                {
                    leftComp.sourceList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Control" ).toString(), juce::sendNotification );
                    midComp.targetList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Controller" ).toString(), juce::sendNotification );
                    leftComp.sub.setSelected( midilearn.getChild( selectedRow ).getProperty( "Source" ).toString(), juce::sendNotification );
                    midComp.typeList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Target" ).toString(), juce::sendNotification );
                    leftComp.setpadVisible( false );
                    leftComp.bank.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.banklist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padno.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padnolist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                }
                if( midilearn.getChild( selectedRow ).getProperty( "Control" ) == "Main" )
                {
                    leftComp.sourceList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Control" ).toString(), juce::sendNotification );
                    midComp.targetList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Controller" ).toString(), juce::sendNotification );
                    leftComp.main.setSelected( midilearn.getChild( selectedRow ).getProperty( "Source" ).toString(), juce::sendNotification );
                    midComp.typeList.setSelected( midilearn.getChild( selectedRow ).getProperty( "Target" ).toString(), juce::sendNotification );
                    leftComp.setpadVisible( false );
                    leftComp.bank.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.banklist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padno.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                    leftComp.padnolist.setVisible( midilearn.getChild( selectedRow ).getProperty( "SetAll" ) );
                }
            }
        }
        else
        {
            leftComp.setleftOptionsVisible( false );
            midComp.setTargetVisible( false );
            midComp.setTypeVisible( false );
            leftComp.setTrackviewVisible( false );
            leftComp.setPgmVisible( false );
            leftComp.setReturnVisible( false );
            leftComp.setSubmixVisible( false );
            leftComp.setMainVisible( false );
            leftComp.setpadVisible( false );
        }
    }

    void MidiControlMidiLearn::refreshTable()
    {
        auto& comps = rightComp.table->getRowComponents();
        auto  row   = rightComp.table->getSelectedRow();
        auto  comp  = dynamic_cast<MidiLearnTableEntry*>( comps[row] );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////// MidiControMidileran
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    MidiControlMidiLearnRight::MidiControlMidiLearnRight()
    {
        mt::utils::addAndMakeVisible( this, targetList, typeList, flip, type, fliplabel, target );
        mt::utils::setFontHeight( 18, target, type, fliplabel );

        target.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        target.setValueColour( ui::Theme::textColour( 1 ) );
        targetList.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );
        type.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        type.setValueColour( ui::Theme::textColour( 1 ) );
        typeList.setBackGroundColours( ui::Theme::textColour( 4 ), ui::Theme::backgroundColour() );
        fliplabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        fliplabel.setValueColour( ui::Theme::textColour( 1 ) );
        flip.setValueColour( ui::Theme::textColour( 1 ) );
        flip.setValueFontHeight( 18 );
        setTargetVisible( false );
        setTypeVisible( false );
    }
    void MidiControlMidiLearnRight::resized()
    {
        auto b = getLocalBounds();
        fliplabel.setBoundsRelative( 0.03800f, 0.807f, 0.456f, 0.111f );
        flip.setBoundsRelative( 0.273f, 0.800f, 0.7011f, 0.128f );
        target.setBoundsRelative( 0.02923f, 0.03313f, 0.3274f, 0.111f );
        targetList.setBoundsRelative( 0.3859f, 0.02525f, 0.5847f, 0.111f );
        type.setBoundsRelative( 0.02923f, 0.2105f, 0.3274f, 0.111f );
        typeList.setBoundsRelative( 0.3859f, 0.22525f, 0.5847f, 0.111f );
    }
    void MidiControlMidiLearn::resized()
    {
        auto b = getLocalBounds();
        leftComp.setBounds( 10, 64, 271, 342 );
        rightComp.setBounds( 290, 64, 440, 342 );
        midComp.setBounds( 740, 64, 271, 342 );
        add.setBoundsRelative( 0.01f, 0.0100f, 0.0391f, 0.0961f );
        clear1.setBoundsRelative( 0.904f, 0.02003f, 0.088f, 0.075f );
    }
    void MidiControlMidiLearn::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, leftComp.getBounds(), ui::Theme::backgroundColour( 3 ) );
        mt::utils::fillRoundedRect( g, rightComp.getBounds(), ui::Theme::backgroundColour( 3 ) );
        mt::utils::fillRoundedRect( g, midComp.getBounds(), ui::Theme::backgroundColour( 3 ) );
        mt::utils::fillRoundedRect( g, topBounds, ui::Theme::backgroundColour( 3 ), 6.0f, true, true, true, true );
    }

    void MidiControlMidiLearn::visibilityChanged()
    {
        projectVTree = ae.getValueTree();
        midilearn    = projectVTree.getOrCreateChildWithName( mt::IDs::MIDILEARN, &ae.getUndoManager() );
    }

    MidiLearnTableEntry::MidiLearnTableEntry()
    {
        mt::utils::addAndMakeVisible( this, target, type, data, ch, flip );
        mt::utils::setFontHeight( 14, target, type, data, ch, flip );

        target.setTitleJustification( juce::Justification::centredLeft );
        target.setValueJustification( juce::Justification::topLeft );
        target.setTitleFontHeight( 16 );
        target.setValueFontHeight( 14 );
        target.setTextColour( ui::Theme::textColour( 1 ) );
        target.setPadding( 10 );
        target.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        type.setValueJustification( juce::Justification::centred );
        type.setTextColour( ui::Theme::textColour( 1 ) );
        type.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        data.setValueJustification( juce::Justification::centred );
        data.setTextColour( ui::Theme::textColour( 1 ) );
        data.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        ch.setValueJustification( juce::Justification::centred );
        ch.setTextColour( ui::Theme::textColour( 1 ) );
        ch.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        flip.setValueJustification( juce::Justification::centred );
        flip.setTextColour( ui::Theme::textColour( 1 ) );
        flip.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
    }
    void MidiLearnTableEntry::resized()
    {
        target.setBounds( 20, 10, 100, 24 );
        type.setBounds( 90, 10, 100, 24 );
        data.setBounds( 170, 10, 100, 31 );
        flip.setBounds( 320, 10, 100, 31 );
        ch.setBounds( 250, 10, 100, 31 );
    }
    void MidiLearnTableEntry::paint( juce::Graphics& g )
    {}

    void MidiLearnTableEntry::setData( MidiRightData& Data )
    {
        target.setText( Data.source, juce::dontSendNotification );
        target.setLine2Text( Data.target );
        type.setText( Data.type, juce::dontSendNotification );
        flip.setText( Data.flip, juce::dontSendNotification );
        ch.setText( juce::String( Data.channel ), juce::dontSendNotification );
        data.setText( juce::String( Data.data ), juce::dontSendNotification );
    }
}
