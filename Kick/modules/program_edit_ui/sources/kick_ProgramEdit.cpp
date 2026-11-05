
namespace kick_ui
{
    ProgramEditHeaderComponent::ProgramEditHeaderComponent()
    {
        mt::utils::addChildComponents( this, programs, icon );
        mt::utils::addAndMakeVisible( this, icon );
        icon.setInterceptsMouseClicks( false, false );
    }

    void ProgramEditHeaderComponent::setHeaderSubComponent( juce::Component* Comp )
    {
        if( progHeader )
            removeChildComponent( progHeader );

        progHeader = Comp;
        if( progHeader )
            addAndMakeVisible( progHeader );

        resized();
    }

    void ProgramEditHeaderComponent::resized()
    {
        auto b = getLocalBounds();
        //programs.setBounds( b.removeFromLeft( .66 * b.getWidth() ).reduced( 0, 16 ) );
        programs.setBounds( b.removeFromLeft( .4 * b.getWidth() ).reduced( 0, 16 ) );
        icon.setBounds( b.removeFromLeft( 64 ).reduced( 10, 16 ) );
        if( progHeader )
            progHeader->setBounds( b );
    }

    void ProgramEditHeaderComponent::setIcon( const juce::String& Icon )
    {
        icon.setImages( Icon );
        repaint();
    }

    void ProgramEditHeaderComponent::visibilityChanged()
    {
        for( auto it : getChildren() )
            it->setVisible( isVisible() );

        if( isVisible() )
        {
            auto pProg = mt::getAudioInterface().getSelectedProgramData();
            programs.setValues( pProg ? pProg->name.get() : "", pProg ? pProg->type.get() : mt::MTProgramType::Unknown, juce::sendNotification );
        }
    }

    AudioHeaderTools::AudioHeaderTools()
    {
        mt::utils::addAndMakeVisible( this, bars, selectAll, selectSample, pencil, erase, paste, cut, copy, settings, magnify );
        mt::utils::setRadioGroupID( (int)RadioGroupId::AudioHeader, selectAll, selectSample, pencil, erase, magnify, cut );
        paste.setVisible( false );
        pencil.triggerClick();

        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), bars );
        mt::utils::setTitleFontHeight( 16, bars );
        bars.setTitleProportion( .4f );
    }

    void AudioHeaderTools::resized()
    {
        bars.setBoundsRelative( .0f, .21f, .1574f, .5789f );
        magnify.setBoundsRelative( 0.1829f, .21f, 0.0936f, 0.5789f );
        selectAll.setBoundsRelative( 0.2851f, .21f, 0.0936f, 0.5789f );
        selectSample.setBoundsRelative( 0.3872f, .21f, 0.0936f, 0.5789f );
        pencil.setBoundsRelative( 0.4893f, .21f, 0.0936f, 0.5789f );
        erase.setBoundsRelative( 0.5914f, .21f, 0.0936f, 0.5789f );
        cut.setBoundsRelative( 0.6936f, .21f, 0.0936f, 0.5789f );
        copy.setBoundsRelative( 0.7957f, .21f, 0.0936f, 0.5789f );
        paste.setBoundsRelative( 0.7957f, .21f, 0.0936f, 0.5789f );
        settings.setBoundsRelative( 0.8978f, .21f, 0.0936f, 0.5789f );
    }

    void AudioHeaderTools::paint( juce::Graphics& g )
    {
    }

    ProgramEdit::ProgramEdit()
        : kick_ui::IScreen( kick_ui::Screen::MenuProgramEdit )
    {
        header.addComponent( &hdrComp );

        programs.emplace_back( std::make_unique<CvProgramEdit>() );
        programs.emplace_back( std::make_unique<MidiProgramEdit>() );
        programs.emplace_back( std::make_unique<ClipProgramEdit>() );
        programs.emplace_back( std::make_unique<DrumProgramEdit>() );
        programs.emplace_back( std::make_unique<KeyGroupProgramEdit>() );
        programs.emplace_back( std::make_unique<AudioProgramEdit>() );

        for( const auto& it : programs )
            it->setVisible( false );

        // Add child components...
        mt::utils::addChildComponents( this, selectedPadLabel, header, infoLabel, audioHdrTools, tracks, audioheader );
        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };
        audioHdrTools.setVisible( false );
        tracks.setVisible( false );
        header.showOptions( false, false );
        infoLabel.setValueColour( ui::Theme::textColour( 1 ) );
        infoLabel.setValueFontHeight( 24 );
        infoLabel.setValueJustification( juce::Justification::centred );
        selectedPadLabel.setValueColour( ui::Theme::textColour( 1 ) );
        selectedPadLabel.setValueFontHeight( 24 );
        selectedPadLabel.setVisible( false );
        selectedPadLabel.toFront( true );

        hdrComp.programs.onSelectionChanged = [&]( const juce::String& Program, const mt::MTProgramType Type ) {
            auto& ae       = mt::getAudioInterface();
            auto  progType = ae.getProgramType( Program );
            auto  sel      = ae.getProgramIndexByName( Program );

            ae.setSelectedProgram( sel );
            loadProgram( progType, sel );
        };

        tracks.onSelectionChanged = [&]( int Index, const juce::String& Option ) {
            if( Index >= 0 )
            {
                auto& ae    = mt::getAudioInterface();
                auto  pProg = ae.getProgramDataForTrack( ae.getSelectedSequence(), Index );
                if( pProg )
                {
                    if( pProg->type.get() != mt::MTProgramType::Audio )
                    {
                        ae.setSelectedTrack( Index );
                        kick_ui::signals::sigScreenClose( kick_ui::Screen::HomeGridView );
                    }
                    else
                    {
                        ae.setSelectedProgram( ae.getProgramIndexByName( pProg->name.get() ) );
                        loadProgram( pProg->type.get(), ae.getProgramIndexByName( pProg->name.get() ) );
                    }
                }
                else
                {
                    kick_ui::signals::sigScreenClose( kick_ui::Screen::HomeGridView );
                }
            }
        };

        ui::themeChanged.connect( [&]() {
            selectedProgram->setupHeader( header );
        } );
    }

    void ProgramEdit::loadProgram( mt::MTProgramType Type, int Index )
    {
        header.removeAllButtons();
        audioHdrTools.setVisible( false );
        if( selectedProgram )
        {
            if( selectedProgram->getType() == mt::MTProgramType::Audio && Type != mt::MTProgramType::Audio )
            {
                auto& headerComp = dynamic_cast<AudioProgramEdit*>( selectedProgram )->getHeaderComponent();
                headerComp.setVisible( false );
            }
            selectedProgram->setVisible( false );
            removeChildComponent( selectedProgram );
            selectedProgram = nullptr;
        }

        switch( Type )
        {
            case mt::MTProgramType::Cv: selectedProgram = programs[0].get(); break;
            case mt::MTProgramType::Midi: selectedProgram = programs[1].get(); break;
            case mt::MTProgramType::Clip: selectedProgram = programs[2].get(); break;
            case mt::MTProgramType::Drum: selectedProgram = programs[3].get(); break;
            case mt::MTProgramType::Keygroup: selectedProgram = programs[4].get(); break;
            case mt::MTProgramType::Audio: selectedProgram = programs[5].get(); break;
        }

        if( selectedProgram )
        {
            audioheader.setVisible( false );
            header.setVisible( true );
            hdrComp.setIcon( getProgramIcon( Type ) );
            selectedProgram->setupHeader( header );
            selectedProgram->setProgram( Index );
            infoLabel.setVisible( false );
            mt::utils::addAndMakeVisible( this, selectedProgram );
            if( Type == mt::MTProgramType::Drum || Type == mt::MTProgramType::Clip )
                selectedPadLabel.setVisible( true );
            else
                selectedPadLabel.setVisible( false );
            if( Type == mt::MTProgramType::Audio )
            {
                header.setVisible( false );
                audioheader.setVisible( true );
                selectedProgram->setupHeader( audioheader );
                audioheader.onBackButtonPressed = [this]( bool ) { closeScreen(); };
                audioheader.addComponent( &tracks );

                audioHdrTools.setVisible( true );
                header.showOptions( false, false );
                auto& headerComp = dynamic_cast<AudioProgramEdit*>( selectedProgram )->getHeaderComponent();
                mt::utils::addAndMakeVisible( this, headerComp, tracks );

                headerComp.setVisible( true );
                headerComp.setBoundsRelative( .391f, .0f, .4598f, .1423f );
                tracks.setBoundsRelative( .225f, .03f, .1565f, 0.09f );

                auto& ae  = mt::getAudioInterface();
                auto& sel = ae.getSelected();
                tracks.setListOptions( ae.getTrackList( sel.sequence ), -1 );
                tracks.setSelected( mt::getAudioInterface().getSelectedTrack(), juce::dontSendNotification );
            }
            else
            {
                header.toFront( true );
                selectedPadLabel.toFront( true );
            }
        }
        else
        {
            for( const auto& it : programs )
                it->setVisible( false );
            if( Type == mt::MTProgramType::Audio )
                infoLabel.setText( "Program Edit is not available for Audio Program", juce::dontSendNotification );
            else
                infoLabel.setText( "No Programs Available", juce::dontSendNotification );
            infoLabel.setVisible( true );
        }
    }

    void ProgramEdit::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( selectedProgram )
        {
            if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) || contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
            {
                auto&            ae         = mt::getAudioInterface();
                int              bankOffset = ae.getBankOffset();
                juce::String     selBank    = ae.getBank();
                auto             padNo      = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
                auto             padState   = contracts::DclContractsHelper::getPadState( f_HwMsg );
                auto             velocity   = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );
                std::vector<int> pads;
                selectedPadLabel.setText( selBank + juce::String::formatted( "%02d", padNo + 1 ), juce::dontSendNotification );

                padNo += bankOffset;
                pads.push_back( padNo );

                for( auto& it : pads )
                {
                    juce::MidiMessage Msg;

                    if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
                        Msg = juce::MidiMessage::noteOn( 1, it, (juce::uint8)velocity );
                    else if( padState == contracts::DeviceControlContracts::BTN_RELEASED )
                        Msg = juce::MidiMessage::noteOff( 1, it );

                    ae.addLiveMidiMsg( ae.getSelected(), Msg );
                }
                pads.clear();
            }
            else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
            {
                auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
                auto& ae         = mt::getAudioInterface();
                auto& aeSel      = ae.getSelected();
                switch( buttonCode )
                {
                    case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); return;
                    case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); return;
                    case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); return;
                }
            }
            selectedProgram->handleCommand( f_HwMsg );
        }
    }

    void ProgramEdit::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 4 ) );
    }

    void ProgramEdit::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        audioheader.setBounds( header.getBounds() );
        //audioHdrTools.setBoundsRelative( .4567f, .0f, .489f, .1423f );
        infoLabel.setBoundsRelative( .1f, .45f, .8f, .2f );
        selectedPadLabel.setBounds( 600, 30, 140, 20 );
        for( const auto& it : programs )
            it->setBounds( b );
    }

    void ProgramEdit::visibilityChanged()
    {
        auto children = getChildren();
        for( auto* it : children )
        {
            if( it )
                it->setVisible( isVisible() );
        }

        if( isVisible() )
        {
            if( selectedProgram )
            {
                auto& ae    = mt::getAudioInterface();
                auto  pProg = ae.getProgramDataForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() );
                hdrComp.programs.setValues( pProg ? pProg->name.get() : ae.getProgramNameByIndex( ae.getSelectedProgram() ), selectedProgram->getType(), juce::sendNotification );
            }
        }
    }

}
