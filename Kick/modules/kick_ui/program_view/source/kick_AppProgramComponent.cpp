/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

AppProgramDisplay::AppProgramDisplay( int Index )
{
    mt::utils::addAndMakeVisible( this, name );
    mt::utils::addChildComponents( this, route, assign, edit, progCH, bankMsb, bankLsb, midiPort, note, cvPort, gatePort, modWheel, velocity, midiCh, sampler );

    mt::utils::setClickingTogglesState( false, route, assign, edit );
    mt::utils::setTitleProportion( .36f, progCH, bankMsb, bankLsb, midiCh );
    mt::utils::setTitleFontHeight( 16, progCH, bankMsb, bankLsb, midiCh, midiPort, note, cvPort, gatePort, modWheel, velocity );
    mt::utils::setValueFontHeight( 24, progCH, bankMsb, bankLsb, midiCh, midiPort, note, cvPort, gatePort, modWheel, velocity );
    sampler.setValueFontHeight( 16 );
    sampler.setValueColour( ui::Theme::textColour( 1 ) );
    progCH.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    bankMsb.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    bankLsb.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    midiCh.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    midiPort.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    note.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    cvPort.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    gatePort.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    modWheel.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
    velocity.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );

    progCH.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    bankMsb.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    bankLsb.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    midiPort.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );

    note.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    cvPort.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    gatePort.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    modWheel.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
    velocity.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );

    assign.onClick = [this]() {
        ae.setSelectedProgram( progNumber );
        kick_ui::signals::sigScreenClose( kick_ui::Screen::MenuAssignSample );
    };

    edit.onClick = [&]() {
        ae.setSelectedProgram( progNumber );
        if (progType == mt::MTProgramType::Plugin) {
            kick_ui::openClapInstrument(ae.getProgram(progNumber));
            return;
        }
        if( progType == mt::MTProgramType::Keygroup || progType == mt::MTProgramType::Midi || progType == mt::MTProgramType::Cv )
            kick_ui::signals::sigScreenClose( kick_ui::Screen::MenuPadPerform );
        else if( progType == mt::MTProgramType::Drum )
            kick_ui::signals::sigScreenClose( kick_ui::Screen::MenuSampleEdit );
        else if( progType == mt::MTProgramType::Clip || progType == mt::MTProgramType::Audio )
            kick_ui::signals::sigScreenClose( kick_ui::Screen::MenuProgramEdit );
    };

    name.onTextChange = [this]() {
        ae.setSelectedProgram( progNumber );
        ae.renameProgram( ( progNumber ), name.getText() );
    };

    // MIDI
    midiPort.onSelectionChanged = [&]( int Value, const juce::String& str ) {
        auto MTIMidi = dynamic_cast<mt::MTIMidiProgramData*>( MTIProg );
        if( MTIMidi )
        {
            selMidiPort       = str;
            MTIMidi->midiPort = str;
        }
    };

    progCH.onSelectionChanged = [&]( int Value, const juce::String& str ) {
        auto MTIMidi = dynamic_cast<mt::MTIMidiProgramData*>( MTIProg );
        if( MTIMidi )
            MTIMidi->programCh = Value;
    };

    bankLsb.onSelectionChanged = [&]( int Value, const juce::String& str ) {
        auto MTIMidi = dynamic_cast<mt::MTIMidiProgramData*>( MTIProg );
        if( MTIMidi )
            MTIMidi->bankLsb = Value;
    };

    bankMsb.onSelectionChanged = [&]( int Value, const juce::String& str ) {
        auto MTIMidi = dynamic_cast<mt::MTIMidiProgramData*>( MTIProg );
        if( MTIMidi )
            MTIMidi->bankMsb = Value;
    };

    midiCh.onSelectionChanged = [&]( int Value, const juce::String& str ) {
        auto MTIMidi = dynamic_cast<mt::MTIMidiProgramData*>( MTIProg );
        if( MTIMidi )
            MTIMidi->midiCh = Value + 1;
    };

    // CV
    note.onSelectionChanged = [&]( int, const juce::String& Value ) {
        auto MTICv = dynamic_cast<mt::MTICvProgramData*>( MTIProg );
        if( MTICv )
            MTICv->cvNote = Value;
    };
    cvPort.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
        auto MTICv = dynamic_cast<mt::MTICvProgramData*>( MTIProg );
        if( MTICv )
            MTICv->cvPort = Index;
    };

    gatePort.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
        auto MTICv = dynamic_cast<mt::MTICvProgramData*>( MTIProg );
        if( MTICv )
            MTICv->cvGate = Index;
    };

    modWheel.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
        auto MTICv = dynamic_cast<mt::MTICvProgramData*>( MTIProg );
        if( MTICv )
            MTICv->cvWheel = Index;
    };

    velocity.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
        auto MTICv = dynamic_cast<mt::MTICvProgramData*>( MTIProg );
        if( MTICv )
            MTICv->cvVelocity = Index;
    };

    sampler.onClick = [&]() {
        kick_ui::signals::sigScreenClose( kick_ui::Screen::MenuSampler );
    };

    route.onClick = [&]() {
        kick_ui::signals::sigScreenClose( kick_ui::Screen::MenuChannelMixer );
    };

    ui::themeChanged.connect( [&]() {
        route.setImages( "ROUTE", "ROUTE_active" );
        assign.setImages( "ASSIGN", "ASSIGN", "ASSIGN_active" );
        setProgram( progNumber );
    } );

    setProgram( Index );
    setInterceptsMouseClicks( true, true );
}

void AppProgramDisplay::setProgram( int Prog )
{
    if( juce::isPositiveAndBelow( Prog, mt::getAudioInterface().getNumPrograms() ) )
    {
        progNumber = Prog;
        MTIProg    = ae.getProgram( progNumber );
        if( MTIProg )
        {
            progType = MTIProg->type.get();
            refreshDisplay();
            triggerAsyncUpdate();

            if( isShowing() )
                if( mt::MTProgramType::Midi == progType )
                    startTimer( 2500 );

            if( onProgChange )
                onProgChange( Prog );
        }
    }
}

void AppProgramDisplay::timerCallback()
{
    if( isShowing() )
        updateMidiPorts();
    else
        stopTimer();
}

void AppProgramDisplay::handleAsyncUpdate()
{
    MTIProg = ae.getProgram( progNumber );
    if( MTIProg )
    {
        name.setText( MTIProg->name.get(), juce::dontSendNotification );

        switch( progType )
        {
            case mt::MTProgramType::Midi:
            {
                auto mProg = dynamic_cast<mt::MTIMidiProgramData*>( MTIProg );
                if( mProg )
                {
                    updateMidiPorts();
                    bankLsb.setSelected( mProg->bankLsb.get(), juce::dontSendNotification );
                    bankMsb.setSelected( mProg->bankMsb.get(), juce::dontSendNotification );
                    progCH.setSelected( mProg->programCh.get(), juce::dontSendNotification );
                    midiCh.setSelected( mProg->midiCh.get() - 1, juce::dontSendNotification );
                }
                break;
            }
            case mt::MTProgramType::Cv:
            {
                auto cvProg = dynamic_cast<mt::MTICvProgramData*>( MTIProg );
                if( cvProg )
                {
                    note.setSelected( cvProg->cvNote.get(), juce::dontSendNotification );
                    cvPort.setSelected( cvProg->cvPort.get(), juce::dontSendNotification );
                    gatePort.setSelected( cvProg->cvGate.get(), juce::dontSendNotification );
                    modWheel.setSelected( cvProg->cvWheel.get(), juce::dontSendNotification );
                    velocity.setSelected( cvProg->cvVelocity.get(), juce::dontSendNotification );
                }
                break;
            }
            default:
                break;
        }
    }
}

void AppProgramDisplay::updateMidiPorts()
{
    auto mProg  = dynamic_cast<mt::MTIMidiProgramData*>( MTIProg );
    selMidiPort = mProg ? mProg->midiPort.get() : "";
    auto ports  = ae.getMidiDeviceNames( false ); //Get Out Devices
    if( ( ( std::find( ports.begin(), ports.end(), selMidiPort ) ) != ports.end() ) && ports.size() >= 1 )
    {
        auto it            = std::find( ports.begin(), ports.end(), selMidiPort );
        auto selectedIndex = 0;
        if( it != ports.end() )
            selectedIndex = std::distance( ports.begin(), it );
        midiPort.setListOptions( ports, selectedIndex );
    }
    else if( selMidiPort.isEmpty() && ports.size() )
        midiPort.setListOptions( ports );
}

void AppProgramDisplay::mouseDown( const juce::MouseEvent& e )
{
    ui::clearFocusComponent();
}

void AppProgramDisplay::refreshDisplay()
{
    mt::utils::makeVisible( name );
    mt::utils::makeHidden( route, assign, edit, progCH, bankMsb, bankLsb, midiPort, note, cvPort, gatePort, modWheel, velocity, midiCh, sampler );

    switch( progType )
    {
        case mt::MTProgramType::Drum:
            edit.setImages( "editSample", "editSample_active" );
            mt::utils::makeVisible( route, assign, edit );
            break;
        case mt::MTProgramType::Keygroup:
            edit.setImages( "padPerform", "padperform_active" );
            mt::utils::makeVisible( route, assign, edit );
            break;
        case mt::MTProgramType::Midi:
            edit.setImages( "padPerform", "padperform_active" );
            mt::utils::makeVisible( progCH, bankMsb, bankLsb, midiPort, edit, midiCh );
            break;
        case mt::MTProgramType::Audio:
            edit.setImages( "editAudio", "editAudio_active" );
            mt::utils::makeVisible( route, assign, edit, sampler );
            break;
        case mt::MTProgramType::Clip:
            edit.setImages( "editClip", "editClip_active" );
            mt::utils::makeVisible( route, assign, edit );
            break;
        case mt::MTProgramType::Plugin:
            edit.setImages("EDIT");
            mt::utils::makeVisible( route, edit );
            break;
        case mt::MTProgramType::Cv:
            edit.setImages( "padPerform", "padperform_active" );
            mt::utils::makeVisible( note, cvPort, gatePort, modWheel, velocity, edit );
            break;
        case mt::MTProgramType::Unknown:
        default:
            break;
    }
}

void AppProgramDisplay::resized()
{
    name.setBoundsRelative( .065f, .3035f, .16f, .3928f );
    route.setBoundsRelative( .245f, .3035f, .08f, .3928f );
    assign.setBoundsRelative( .345f, .3035f, .08f, .3928f );
    edit.setBoundsRelative( .856f, .3035f, .08f, .3928f );
    sampler.setBoundsRelative( .445f, .3035f, .08f, .3928f );
    progCH.setBoundsRelative( .245f, .0892f, .1f, .6071f );
    bankMsb.setBoundsRelative( .367f, .0892f, .1f, .6071f );
    bankLsb.setBoundsRelative( .489f, .0892f, .1f, .6071f );
    midiPort.setBoundsRelative( .611f, .0892f, .1f, .6071f );
    midiCh.setBoundsRelative( .733f, .0892f, .1f, .6071f );

    note.setBoundsRelative( .246f, .0892f, .1f, .6071f );
    cvPort.setBoundsRelative( .368f, .0892f, .1f, .6071f );
    gatePort.setBoundsRelative( .490f, .0892f, .1f, .6071f );
    modWheel.setBoundsRelative( .612f, .0892f, .1f, .6071f );
    velocity.setBoundsRelative( .734f, .0892f, .1f, .6071f );
}

//==============================================================================
//  AppProgramComponent
//==============================================================================
AppProgramComponent::AppProgramComponent( int Index )

{
    //addMouseListener( this, true );
    mt::utils::addChildComponents( this, srNo, prg, options );
    mt::utils::disableMouseInterceptsAndFocus( srNo );

    options.setBackgroundColour( ui::Theme::backgroundColour( 1 ) );

    options.onClick = [&]() {
        mt::getAudioInterface().setSelectedProgram( prg.getProgNumber() );
        if( onPrgEdit )
            onPrgEdit();
    };

    prg.onProgChange = [&]( int No ) {
        srNo.setText( juce::String( No + 1 ) );
        juce::String icon = kick_ui::getEditProgramIcon( prg.getProgramType() );
        srNo.setImage( icon );
    };

    srNo.setColours( ui::Theme::textColour( 0 ), ui::Theme::backgroundColour( 6 ), ui::Theme::backgroundColour( 12 ) );
    prg.setProgram( Index );

    ui::themeChanged.connect( [&]() {
        srNo.setImage( "ID_Program" );
        options.setImages( "editOptions" );
    } );
}

AppProgramComponent::~AppProgramComponent()
{
    removeMouseListener( this );
}

void AppProgramComponent::setProgram( int Prog )
{
    auto& ae  = mt::getAudioInterface();
    auto& sel = ae.getSelected();

    if( juce::isPositiveAndBelow( Prog, ae.getNumPrograms() ) )
    {
        prg.setProgram( Prog );
        mt::utils::makeVisible( srNo, prg, options );
    }
    else
        mt::utils::makeHidden( srNo, prg, options );

    resized();
}

void AppProgramComponent::resized()
{
    srNo.setBoundsRelative( .001f, .01f, .044f, .97f );
    prg.setBoundsRelative( .0f, 0.f, 1.f, 1.f );
    options.setBoundsRelative( .957f, .0136f, .04f, .965f );
}

void AppProgramComponent::visibilityChanged()
{
    if( isVisible() )
        updateData();
}
