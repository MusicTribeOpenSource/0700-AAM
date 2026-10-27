namespace ui
{
    /////////////////////////////////////////////////////////////////////////

    IPv4Editor::IPv4Editor()
    {
        intermidentValues.fill(0);
        editorsValue.fill(0);
        setLookAndFeel( ui::KickLooknFeel::getLnf() );

        mt::utils::addAndMakeVisible( this, num1, num2, num3, num4, num5, num6, num7, num8, num9, num0, clr, apply, cancel );
        mt::utils::setPlacementFlag( juce::Justification::centred, num1, num2, num3, num4, num5, num6, num7, num8, num9, num0, clr );

        for( auto id = 0; id < editors.size(); ++id )
        {
            mt::utils::addAndMakeVisible( this, editors[id] );
            editors[id].setID( id );
            juce::Font fnt( 36.f );
            editors[id].clear();
            editors[id].setFont( fnt );
            editors[id].setColour( juce::TextEditor::backgroundColourId, ui::Theme::backgroundColour( 13 ) );
            editors[id].setColour( juce::TextEditor::textColourId, ui::Theme::textColour( 2 ) );
            editors[id].setColour( juce::TextEditor::outlineColourId, ui::Theme::outlineColour( 0 ) );
            editors[id].setColour( juce::TextEditor::focusedOutlineColourId, ui::Theme::outlineColour( 0 ) );
            editors[id].setInputRestrictions( 3 );
            editors[id].setText( "000", juce::dontSendNotification );
            editors[id].setFocusContainerType( juce::Component::FocusContainerType::keyboardFocusContainer );
            editors[id].setJustification( juce::Justification::centred );
            editors[id].onMouseDown  = [&]( int ID ) { selectedEditor = ID; };
            editors[id].onTextChange = [&, id]() {
                editorsValue[id] = (int)editors[id].getTextValue().getValue();
            };
            editorsValue[id] = 0;
        }

        for( auto id = 0; id < editorSep.size(); ++id )
        {
            mt::utils::addAndMakeVisible( this, editorSep[id] );
            editorSep[id].setText( ".", juce::dontSendNotification );
            editorSep[id].setValueColour( ui::Theme::textColour( 1 ) );
            editorSep[id].setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
            editorSep[id].setValueFontHeight( 40 );
            editorSep[id].setValueJustification( juce::Justification::centred );
        }

        num0.onClick  = [&]() { addInput( 0 ); };
        num1.onClick  = [&]() { addInput( 1 ); };
        num2.onClick  = [&]() { addInput( 2 ); };
        num3.onClick  = [&]() { addInput( 3 ); };
        num4.onClick  = [&]() { addInput( 4 ); };
        num5.onClick  = [&]() { addInput( 5 ); };
        num6.onClick  = [&]() { addInput( 6 ); };
        num7.onClick  = [&]() { addInput( 7 ); };
        num8.onClick  = [&]() { addInput( 8 ); };
        num9.onClick  = [&]() { addInput( 9 ); };
        clr.onClick   = [&]() { intermidentValues[selectedEditor] =0; refreshDisplay( ); };
        apply.onClick = [&]() {
            refreshDisplay();
            ipAddress = juce::String( editorsValue[0] ) + "." + juce::String( editorsValue[1] ) + "." + juce::String( editorsValue[2] ) + "." + juce::String( editorsValue[3] );
            mt::utils::exitDialog( this, UI::APPLY );
        };
        cancel.onClick = [&]() {
            refreshDisplay();
            mt::utils::exitDialog( this, UI::CANCEL );
        };

        for( auto it : getChildren() )
        {
            it->setWantsKeyboardFocus( false );
        }

        ui::themeChanged.connect( [&]() {
            num1.setImages( "numPad_1", "numPad_1", "numPad_1_Active" );
            num2.setImages( "numPad_2", "numPad_2", "numPad_2_Active" );
            num3.setImages( "numPad_3", "numPad_3", "numPad_3_Active" );
            num4.setImages( "numPad_4", "numPad_4", "numPad_4_Active" );
            num5.setImages( "numPad_5", "numPad_5", "numPad_5_Active" );
            num6.setImages( "numPad_6", "numPad_6", "numPad_6_Active" );
            num7.setImages( "numPad_7", "numPad_7", "numPad_7_Active" );
            num8.setImages( "numPad_8", "numPad_8", "numPad_8_Active" );
            num9.setImages( "numPad_9", "numPad_9", "numPad_9_Active" );
            num0.setImages( "numPad_0", "numPad_0", "numPad_0_Active" );
            clr.setImages( "numPad_C", "numPad_C", "numPad_C_Active" );
            apply.setImages( "numPad_Apply" );
            cancel.setImages( "numPad_Clear" );
        } );
    }

    void IPv4Editor::setIPAddress( std::array<int, 4> arr )
    {
        for( auto id = 0; id < 4; ++id )
        {
            auto val              = juce::jlimit( 0, 255, arr[id] );
            editorsValue[id]      = val;
            intermidentValues[id] = val;
            editors[id].setText( juce::String( val ).paddedLeft( '0', 3 ) );
        }
    }

    void IPv4Editor::addInput( int i )
    {
        auto now = std::chrono::steady_clock::now();
        if( now - lastInputTime > timeoutDuration )
        {
            intermidentValues[selectedEditor] = 0; // Reset current value if timeout
        }
        lastInputTime = now;

        intermidentValues[selectedEditor] = intermidentValues[selectedEditor] * 10 + i;
        if( intermidentValues[selectedEditor] > 255 )
        {
            intermidentValues[selectedEditor] = 255; // Ensure value does not exceed max
        }
        refreshDisplay();
    }

    void IPv4Editor::refreshDisplay()
    {
        auto val = juce::jlimit( 0, 255, intermidentValues[selectedEditor] );
        editors[selectedEditor].setText( juce::String( val ).paddedLeft( '0', 3 ) );
    }

    void IPv4Editor::show( juce::String title, std::function<void( int )> CallbackOnExit )
    {
        mt::utils::showDialog( title, this, [this, CallbackOnExit]( int R ) {
            if( CallbackOnExit )
                CallbackOnExit( R );
        },
                               { 0, 0, 500, 480 } );
    }

    void IPv4Editor::resized()
    {
        auto b = getLocalBounds().reduced( 20 );

        auto tEBounds = b.removeFromTop( 60 ).reduced( 30, 0 );
        auto eW       = ( tEBounds.getWidth() - 60 ) / editors.size();
        //Editors
        for( auto id = 0; id < editors.size(); ++id )
        {
            editors[id].setBounds( tEBounds.removeFromLeft( eW ).reduced( 0, 5 ) );
            if( id < editorSep.size() )
                editorSep[id].setBounds( tEBounds.removeFromLeft( 20 ) );
        }
        // Control Options
        b.removeFromTop( 20 );
        auto ctrlOptions = b.removeFromBottom( 54 ).removeFromBottom( 34 );
        cancel.setBounds( ctrlOptions.removeFromLeft( ( ctrlOptions.getWidth() - 10 ) / 2 ) );
        ctrlOptions.removeFromLeft( 10 );
        apply.setBounds( ctrlOptions );

        //Num Pad
        auto HRow = ( b.getHeight() - 30 ) / 4;
        auto row1 = b.removeFromTop( HRow ).reduced( 34, 0 );
        b.removeFromTop( 10 );
        auto row2 = b.removeFromTop( HRow ).reduced( 34, 0 );
        b.removeFromTop( 10 );
        auto row3 = b.removeFromTop( HRow ).reduced( 34, 0 );
        b.removeFromTop( 10 );
        auto row4 = b.removeFromTop( HRow ).reduced( 34, 0 );

        auto W = row1.getWidth() / 3;

        //Row 1
        num7.setBounds( row1.removeFromLeft( W ).reduced( 5, 0 ) );
        num8.setBounds( row1.removeFromLeft( W ).reduced( 5, 0 ) );
        num9.setBounds( row1.removeFromLeft( W ).reduced( 5, 0 ) );

        //Row 2
        num4.setBounds( row2.removeFromLeft( W ).reduced( 5, 0 ) );
        num5.setBounds( row2.removeFromLeft( W ).reduced( 5, 0 ) );
        num6.setBounds( row2.removeFromLeft( W ).reduced( 5, 0 ) );

        //Row 3
        num1.setBounds( row3.removeFromLeft( W ).reduced( 5, 0 ) );
        num2.setBounds( row3.removeFromLeft( W ).reduced( 5, 0 ) );
        num3.setBounds( row3.removeFromLeft( W ).reduced( 5, 0 ) );

        //Row 4
        row4.removeFromLeft( 50 );
        num0.setBounds( row4.removeFromLeft( W ).reduced( 5, 0 ) );
        clr.setBounds( row4.removeFromLeft( 160 ).reduced( 5, 0 ) );
    }

    KickIPv4::KickIPv4( const juce::String& Title, const juce::String& Value )
    {
        for( auto id = 0; id < editorValues.size(); ++id )
            editorValues[id] = 0;

        setName( Title );
        setTitle( Title );
        showDropdown( true );
        setText( Value, juce::dontSendNotification );
        setValueFontHeight( 18 );
        setTitleFontHeight( 16 );
        setTitleProportion( .35f );
    }

    void KickIPv4::setIPAddress( int ( &arr )[4] )
    {
        for( auto id = 0; id < editorValues.size(); ++id )
            editorValues[0] = arr[0];
        auto text = juce::String( editorValues[0] ) + "." + juce::String( editorValues[1] ) + "." + juce::String( editorValues[2] ) + "." + juce::String( editorValues[3] );
        setText( text, juce::sendNotification );
    }

    void KickIPv4::mouseDoubleClick( const juce::MouseEvent& e )
    {
        if( !isEditing && isVisible() )
        {
            isEditing   = true;
            auto& IPv4E = IPv4Editor::Instance();
            IPv4E.setIPAddress( editorValues );
            IPv4E.show( "IPV4 EDITOR", [&]( int R ) {
                isEditing = false;
                if( R == UI::APPLY )
                {
                    editorValues = IPv4E.getIPAddress();
                    setText( IPv4E.getIPAddressAsString(), juce::sendNotification );
                    if( onIPv4Changed )
                        onIPv4Changed( editorValues, getText() );
                }
            } );
        }
    }

}
