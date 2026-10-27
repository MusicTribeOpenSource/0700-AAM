
namespace ui
{
    inline static constexpr int spaceButtonWidth    = 480;
    inline static constexpr int controlButtonsWidth = 120;
    inline static constexpr int buttonSpacing       = 10;
    inline static constexpr int buttonWidth         = 60;
    inline static constexpr int buttonHeight        = 50;
    inline static constexpr int inputLabelHeight    = 80;
    inline static constexpr int compSpaceAround     = 19;
    inline static constexpr int row3ButtonSpace     = 75;
    inline static constexpr int row2spaceAround     = 35;

    IKeyBoardBase::IKeyBoardBase()
    {
        setInterceptsMouseClicks( false, true );
        setPaintingIsUnclipped( true );
        setBufferedToImage( true );
    }

    IKeyBoardBase::~IKeyBoardBase()
    {
        removeAllChildren();
        for( int idx = 0; idx < 4; ++idx )
        {
            row[idx].clear();
        }
    }

    void IKeyBoardBase::addAllMakeVisibleAndAddListener()
    {
        for( int idx = 0; idx < 4; ++idx )
        {
            for( auto& button : row[idx] )
            {
                addAndMakeVisible( button.get() );
                button->setColour( juce::TextButton::buttonColourId, ui::Theme::backgroundColour( 0 ) );
                button->setWantsKeyboardFocus( false );

                button->onClick = [&, button = button.get()]() {
                    if( onKeyPressed_ )
                    {
                        //if( button->getButtonText() == "Enter" )
                        if( button->getName() == "Enter" )
                        {
                            if( onEnterPressed )
                                onEnterPressed( UI::BUTTONS::APPLY );
                        }
                        else
                        {
                            //onKeyPressed_( button->getButtonText() );
                            onKeyPressed_( button->getName() );
                        }
                    }
                };
            }
        }
    }

    void IKeyBoardBase::resized()
    {
        auto bounds = getLocalBounds();

        for( int idx = 0; idx < 4; ++idx )
        {
            auto rBounds     = bounds.removeFromTop( buttonHeight );
            auto numElements = (int)row[idx].size();
            if( numElements )
            {
                auto index = 0;
                if( idx == 0 || idx == 1 )
                    rBounds.removeFromLeft( row2spaceAround );

                for( auto& button : row[idx] )
                {
                    if( idx == 0 )
                    {
                        button->setBounds( rBounds.removeFromLeft( buttonWidth ) );
                        rBounds.removeFromLeft( buttonSpacing );
                    }
                    else if( idx == 1 )
                    {
                        if( ( row[1].size() < row[0].size() ) && index == 0 )
                            rBounds.removeFromLeft( row2spaceAround );
                        button->setBounds( rBounds.removeFromLeft( buttonWidth ) );
                        rBounds.removeFromLeft( buttonSpacing );
                    }
                    else if( idx == 2 )
                    {
                        if( index == 0 || ( index == ( row[idx].size() - 1 ) ) )
                        {
                            if( ( index == ( row[idx].size() - 1 ) ) && row[idx].size() == 7 )
                                button->setBounds( rBounds.removeFromLeft( row3ButtonSpace ) );

                            else if( ( index == ( row[idx].size() - 1 ) ) && ( row[idx].size() == 9 ) )
                                rBounds.removeFromLeft( 5 );

                            button->setBounds( rBounds.removeFromLeft( controlButtonsWidth ) );
                        }
                        else
                        {
                            if( index == 1 && row[idx].size() == 7 )
                            {
                                rBounds.removeFromLeft( row3ButtonSpace );
                                button->setBounds( rBounds.removeFromLeft( buttonWidth ) );
                            }
                            else if( ( index == 1 ) && ( row[idx].size() == 9 ) )
                            {
                                rBounds.removeFromLeft( 5 );
                                button->setBounds( rBounds.removeFromLeft( buttonWidth ) );
                            }
                            else
                                button->setBounds( rBounds.removeFromLeft( buttonWidth ) );
                        }
                        rBounds.removeFromLeft( buttonSpacing );
                    }
                    else if( idx == 3 )
                    {
                        if( index == 0 || ( index == ( row[idx].size() - 1 ) ) )
                            button->setBounds( rBounds.removeFromLeft( controlButtonsWidth ) );
                        else
                            button->setBounds( rBounds.removeFromLeft( spaceButtonWidth ) );
                        rBounds.removeFromLeft( buttonSpacing + 5 );
                    }
                    index++;
                }
                bounds.removeFromTop( buttonSpacing );
            }
        }
    }

    KickVirtualKeyBoard::SpecialSymbols::SpecialSymbols()
    {
        row[0].emplace_back( std::make_unique<KickImageButton>( "[", "Virtual_s1", "Virtual_s1", "Virtual_s1_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "]", "Virtual_s2", "Virtual_s2", "Virtual_s2_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "{", "Virtual_s3", "Virtual_s3", "Virtual_s3_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "}", "Virtual_s4", "Virtual_s4", "Virtual_s4_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "#", "Virtual_s5", "Virtual_s5", "Virtual_s5_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "%", "Virtual_s6", "Virtual_s6", "Virtual_s6_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "^", "Virtual_s7", "Virtual_s7", "Virtual_s7_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "*", "Virtual_s8", "Virtual_s8", "Virtual_s8_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "+", "Virtual_s9", "Virtual_s9", "Virtual_s9_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "=", "Virtual_s10", "Virtual_s10", "Virtual_s10_Active" ) );

        row[1].emplace_back( std::make_unique<KickImageButton>( "_", "Virtual_s11", "Virtual_s11", "Virtual_s11_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "\\", "Virtual_s12", "Virtual_s12", "Virtual_s12_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "|", "Virtual_s13", "Virtual_s13", "Virtual_s13_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "~", "Virtual_s14", "Virtual_s14", "Virtual_s14_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "<", "Virtual_s15", "Virtual_s15", "Virtual_s15_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( ">", "Virtual_s16", "Virtual_s16", "Virtual_s16_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "$", "Virtual_s17", "Virtual_s17", "Virtual_s17_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "&", "Virtual_s18", "Virtual_s18", "Virtual_s18_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "@", "Virtual_s19", "Virtual_s19", "Virtual_s19_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "\"", "Virtual_s32", "Virtual_s32 ", "Virtual_s32_Active" ) );

        row[2].emplace_back( std::make_unique<KickImageButton>( "#123", "Virtual_#123", "Virtual_#123", "Virtual_#123_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( ".", "Virtual_s20", "Virtual_s20", "Virtual_s20_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( ",", "Virtual_s21", "Virtual_s21", "Virtual_s21_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "?", "Virtual_s22", "Virtual_s22", "Virtual_s22_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "!", "Virtual_s23", "Virtual_s23", "Virtual_s23_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "'", "Virtual_s24", "Virtual_s24", "Virtual_s24_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "clear", "Virtual_Clear", "Virtual_Clear", "Virtual_Clear_Active" ) );

        row[3].emplace_back( std::make_unique<KickImageButton>( "ABC", "Virtual_ABC", "Virtual_ABC", "Virtual_ABC_Active" ) );
        row[3].emplace_back( std::make_unique<KickImageButton>( " ", "Virtual_Space", "Virtual_Space", "Virtual_Space_Active" ) );
        row[3].emplace_back( std::make_unique<KickImageButton>( "Enter", "Virtual_Enter", "Virtual_Enter", "Virtual_Enter_Active" ) );

        addAllMakeVisibleAndAddListener();

        themeChanged.connect( [&]() {
            dynamic_cast<KickImageButton*>( row[0][0].get() )->setImages( "Virtual_s1", "Virtual_s1", "Virtual_s1_Active" );
            dynamic_cast<KickImageButton*>( row[0][1].get() )->setImages( "Virtual_s2", "Virtual_s2", "Virtual_s2_Active" );
            dynamic_cast<KickImageButton*>( row[0][2].get() )->setImages( "Virtual_s3", "Virtual_s3", "Virtual_s3_Active" );
            dynamic_cast<KickImageButton*>( row[0][3].get() )->setImages( "Virtual_s4", "Virtual_s4", "Virtual_s4_Active" );
            dynamic_cast<KickImageButton*>( row[0][4].get() )->setImages( "Virtual_s5", "Virtual_s5", "Virtual_s5_Active" );
            dynamic_cast<KickImageButton*>( row[0][5].get() )->setImages( "Virtual_s6", "Virtual_s6", "Virtual_s6_Active" );
            dynamic_cast<KickImageButton*>( row[0][6].get() )->setImages( "Virtual_s7", "Virtual_s7", "Virtual_s7_Active" );
            dynamic_cast<KickImageButton*>( row[0][7].get() )->setImages( "Virtual_s8", "Virtual_s8", "Virtual_s8_Active" );
            dynamic_cast<KickImageButton*>( row[0][8].get() )->setImages( "Virtual_s9", "Virtual_s9", "Virtual_s9_Active" );
            dynamic_cast<KickImageButton*>( row[0][9].get() )->setImages( "Virtual_s10", "Virtual_s10", "Virtual_s10_Active" );

            dynamic_cast<KickImageButton*>( row[1][0].get() )->setImages( "Virtual_s11", "Virtual_s11", "Virtual_s11_Active" );
            dynamic_cast<KickImageButton*>( row[1][1].get() )->setImages( "Virtual_s12", "Virtual_s12", "Virtual_s12_Active" );
            dynamic_cast<KickImageButton*>( row[1][2].get() )->setImages( "Virtual_s13", "Virtual_s13", "Virtual_s13_Active" );
            dynamic_cast<KickImageButton*>( row[1][3].get() )->setImages( "Virtual_s14", "Virtual_s14", "Virtual_s14_Active" );
            dynamic_cast<KickImageButton*>( row[1][4].get() )->setImages( "Virtual_s15", "Virtual_s15", "Virtual_s15_Active" );
            dynamic_cast<KickImageButton*>( row[1][5].get() )->setImages( "Virtual_s16", "Virtual_s16", "Virtual_s16_Active" );
            dynamic_cast<KickImageButton*>( row[1][6].get() )->setImages( "Virtual_s17", "Virtual_s17", "Virtual_s17_Active" );
            dynamic_cast<KickImageButton*>( row[1][7].get() )->setImages( "Virtual_s18", "Virtual_s18", "Virtual_s18_Active" );
            dynamic_cast<KickImageButton*>( row[1][8].get() )->setImages( "Virtual_s19", "Virtual_s19", "Virtual_s19_Active" );
            dynamic_cast<KickImageButton*>( row[1][9].get() )->setImages( "Virtual_s32", "Virtual_s32 ", "Virtual_s32_Active" );

            dynamic_cast<KickImageButton*>( row[2][0].get() )->setImages( "Virtual_#123", "Virtual_#123", "Virtual_#123_Active" );
            dynamic_cast<KickImageButton*>( row[2][1].get() )->setImages( "Virtual_s20", "Virtual_s20", "Virtual_s20_Active" );
            dynamic_cast<KickImageButton*>( row[2][2].get() )->setImages( "Virtual_s21", "Virtual_s21", "Virtual_s21_Active" );
            dynamic_cast<KickImageButton*>( row[2][3].get() )->setImages( "Virtual_s22", "Virtual_s22", "Virtual_s22_Active" );
            dynamic_cast<KickImageButton*>( row[2][4].get() )->setImages( "Virtual_s23", "Virtual_s23", "Virtual_s23_Active" );
            dynamic_cast<KickImageButton*>( row[2][5].get() )->setImages( "Virtual_s24", "Virtual_s24", "Virtual_s24_Active" );
            dynamic_cast<KickImageButton*>( row[2][6].get() )->setImages( "Virtual_Clear", "Virtual_Clear", "Virtual_Clear_Active" );

            dynamic_cast<KickImageButton*>( row[3][0].get() )->setImages( "Virtual_ABC", "Virtual_ABC", "Virtual_ABC_Active" );
            dynamic_cast<KickImageButton*>( row[3][1].get() )->setImages( "Virtual_Space", "Virtual_Space", "Virtual_Space_Active" );
            dynamic_cast<KickImageButton*>( row[3][2].get() )->setImages( "Virtual_Enter", "Virtual_Enter", "Virtual_Enter_Active" );
        } );
    }

    KickVirtualKeyBoard::LowerCaseLetterComponent::LowerCaseLetterComponent()
    {
        row[0].emplace_back( std::make_unique<KickImageButton>( "q", "Virtual_q", "Virtual_q", "Virtual_q_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "w", "Virtual_w", "Virtual_w", "Virtual_w_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "e", "Virtual_e", "Virtual_e", "Virtual_e_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "r", "Virtual_r", "Virtual_r", "Virtual_r_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "t", "Virtual_t", "Virtual_t", "Virtual_t_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "y", "Virtual_y", "Virtual_y", "Virtual_y_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "u", "Virtual_u", "Virtual_u", "Virtual_u_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "i", "Virtual_i", "Virtual_i", "Virtual_i_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "o", "Virtual_o", "Virtual_o", "Virtual_o_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "p", "Virtual_p", "Virtual_p", "Virtual_p_Active" ) );

        row[1].emplace_back( std::make_unique<KickImageButton>( "a", "Virtual_a", "Virtual_a", "Virtual_a_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "s", "Virtual_s", "Virtual_s", "Virtual_s_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "d", "Virtual_d", "Virtual_d", "Virtual_d_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "f", "Virtual_f", "Virtual_f", "Virtual_f_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "g", "Virtual_g", "Virtual_g", "Virtual_g_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "h", "Virtual_h", "Virtual_h", "Virtual_h_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "j", "Virtual_j", "Virtual_j", "Virtual_j_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "k", "Virtual_k", "Virtual_k", "Virtual_k_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "l", "Virtual_l", "Virtual_l", "Virtual_l_Active" ) );

        row[2].emplace_back( std::make_unique<KickImageButton>( "Shift1", "Virtual_Shift" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "z", "Virtual_z", "Virtual_z", "Virtual_z_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "x", "Virtual_x", "Virtual_x", "Virtual_x_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "c", "Virtual_c", "Virtual_c", "Virtual_c_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "v", "Virtual_v", "Virtual_v", "Virtual_v_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "b", "Virtual_b", "Virtual_b", "Virtual_b_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "n", "Virtual_n", "Virtual_n", "Virtual_n_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "m", "Virtual_m", "Virtual_m", "Virtual_m_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "clear", "Virtual_Clear", "Virtual_Clear", "Virtual_Clear_Active" ) );

        row[3].emplace_back( std::make_unique<KickImageButton>( "#123", "Virtual_#123", "Virtual_#123", "Virtual_#123_Active" ) );
        row[3].emplace_back( std::make_unique<KickImageButton>( " ", "Virtual_Space", "Virtual_Space", "Virtual_Space_Active" ) );
        row[3].emplace_back( std::make_unique<KickImageButton>( "Enter", "Virtual_Enter", "Virtual_Enter", "Virtual_Enter_Active" ) );

        addAllMakeVisibleAndAddListener();

        themeChanged.connect( [&]() {
            dynamic_cast<KickImageButton*>( row[0][0].get() )->setImages( "Virtual_q", "Virtual_q", "Virtual_q_Active" );
            dynamic_cast<KickImageButton*>( row[0][1].get() )->setImages( "Virtual_w", "Virtual_w", "Virtual_w_Active" );
            dynamic_cast<KickImageButton*>( row[0][2].get() )->setImages( "Virtual_e", "Virtual_e", "Virtual_e_Active" );
            dynamic_cast<KickImageButton*>( row[0][3].get() )->setImages( "Virtual_r", "Virtual_r", "Virtual_r_Active" );
            dynamic_cast<KickImageButton*>( row[0][4].get() )->setImages( "Virtual_t", "Virtual_t", "Virtual_t_Active" );
            dynamic_cast<KickImageButton*>( row[0][5].get() )->setImages( "Virtual_y", "Virtual_y", "Virtual_y_Active" );
            dynamic_cast<KickImageButton*>( row[0][6].get() )->setImages( "Virtual_u", "Virtual_u", "Virtual_u_Active" );
            dynamic_cast<KickImageButton*>( row[0][7].get() )->setImages( "Virtual_i", "Virtual_i", "Virtual_i_Active" );
            dynamic_cast<KickImageButton*>( row[0][8].get() )->setImages( "Virtual_o", "Virtual_o", "Virtual_o_Active" );
            dynamic_cast<KickImageButton*>( row[0][9].get() )->setImages( "Virtual_p", "Virtual_p", "Virtual_p_Active" );

            dynamic_cast<KickImageButton*>( row[1][0].get() )->setImages( "Virtual_a", "Virtual_a", "Virtual_a_Active" );
            dynamic_cast<KickImageButton*>( row[1][1].get() )->setImages( "Virtual_s", "Virtual_s", "Virtual_s_Active" );
            dynamic_cast<KickImageButton*>( row[1][2].get() )->setImages( "Virtual_d", "Virtual_d", "Virtual_d_Active" );
            dynamic_cast<KickImageButton*>( row[1][3].get() )->setImages( "Virtual_f", "Virtual_f", "Virtual_f_Active" );
            dynamic_cast<KickImageButton*>( row[1][4].get() )->setImages( "Virtual_g", "Virtual_g", "Virtual_g_Active" );
            dynamic_cast<KickImageButton*>( row[1][5].get() )->setImages( "Virtual_h", "Virtual_h", "Virtual_h_Active" );
            dynamic_cast<KickImageButton*>( row[1][6].get() )->setImages( "Virtual_j", "Virtual_j", "Virtual_j_Active" );
            dynamic_cast<KickImageButton*>( row[1][7].get() )->setImages( "Virtual_k", "Virtual_k", "Virtual_k_Active" );
            dynamic_cast<KickImageButton*>( row[1][8].get() )->setImages( "Virtual_l", "Virtual_l", "Virtual_l_Active" );

            dynamic_cast<KickImageButton*>( row[2][0].get() )->setImages( "Virtual_Shift" );
            dynamic_cast<KickImageButton*>( row[2][1].get() )->setImages( "Virtual_z", "Virtual_z", "Virtual_z_Active" );
            dynamic_cast<KickImageButton*>( row[2][2].get() )->setImages( "Virtual_x", "Virtual_x", "Virtual_x_Active" );
            dynamic_cast<KickImageButton*>( row[2][3].get() )->setImages( "Virtual_c", "Virtual_c", "Virtual_c_Active" );
            dynamic_cast<KickImageButton*>( row[2][4].get() )->setImages( "Virtual_v", "Virtual_v", "Virtual_v_Active" );
            dynamic_cast<KickImageButton*>( row[2][5].get() )->setImages( "Virtual_b", "Virtual_b", "Virtual_b_Active" );
            dynamic_cast<KickImageButton*>( row[2][6].get() )->setImages( "Virtual_n", "Virtual_n", "Virtual_n_Active" );
            dynamic_cast<KickImageButton*>( row[2][7].get() )->setImages( "Virtual_m", "Virtual_m", "Virtual_m_Active" );
            dynamic_cast<KickImageButton*>( row[2][8].get() )->setImages( "Virtual_Clear", "Virtual_Clear", "Virtual_Clear_Active" );

            dynamic_cast<KickImageButton*>( row[3][0].get() )->setImages( "Virtual_#123", "Virtual_#123", "Virtual_#123_Active" );
            dynamic_cast<KickImageButton*>( row[3][1].get() )->setImages( "Virtual_Space", "Virtual_Space", "Virtual_Space_Active" );
            dynamic_cast<KickImageButton*>( row[3][2].get() )->setImages( "Virtual_Enter", "Virtual_Enter", "Virtual_Enter_Active" );
        } );
    }

    KickVirtualKeyBoard::UpperCaseLetterComponent::UpperCaseLetterComponent()
    {
        row[0].emplace_back( std::make_unique<KickImageButton>( "Q", "Virtual_Q", "Virtual_Q", "Virtual_Q_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "W", "Virtual_W", "Virtual_W", "Virtual_W_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "E", "Virtual_E", "Virtual_E", "Virtual_E_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "R", "Virtual_R", "Virtual_R", "Virtual_R_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "T", "Virtual_T", "Virtual_T", "Virtual_T_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "Y", "Virtual_Y", "Virtual_Y", "Virtual_Y_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "U", "Virtual_U", "Virtual_U", "Virtual_U_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "I", "Virtual_I", "Virtual_I", "Virtual_I_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "O", "Virtual_O", "Virtual_O", "Virtual_O_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "P", "Virtual_P", "Virtual_P", "Virtual_P_Active" ) );

        row[1].emplace_back( std::make_unique<KickImageButton>( "A", "Virtual_A", "Virtual_A", "Virtual_A_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "S", "Virtual_S", "Virtual_S", "Virtual_S_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "D", "Virtual_D", "Virtual_D", "Virtual_D_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "F", "Virtual_F", "Virtual_F", "Virtual_F_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "G", "Virtual_G", "Virtual_G", "Virtual_G_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "H", "Virtual_H", "Virtual_H", "Virtual_H_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "J", "Virtual_J", "Virtual_J", "Virtual_J_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "K", "Virtual_K", "Virtual_K", "Virtual_K_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "L", "Virtual_L", "Virtual_L", "Virtual_L_Active" ) );

        row[2].emplace_back( std::make_unique<KickImageButton>( "Shift2", "Virtual_Shift_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "Z", "Virtual_Z", "Virtual_Z", "Virtual_Z_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "X", "Virtual_X", "Virtual_X", "Virtual_X_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "C", "Virtual_C", "Virtual_C", "Virtual_C_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "V", "Virtual_V", "Virtual_V", "Virtual_V_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "B", "Virtual_B", "Virtual_B", "Virtual_B_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "N", "Virtual_N", "Virtual_N", "Virtual_N_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "M", "Virtual_M", "Virtual_M", "Virtual_M_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "clear", "Virtual_Clear", "Virtual_Clear", "Virtual_Clear_Active" ) );

        row[3].emplace_back( std::make_unique<KickImageButton>( "#123", "Virtual_#123", "Virtual_#123", "Virtual_#123_Active" ) );
        row[3].emplace_back( std::make_unique<KickImageButton>( " ", "Virtual_Space", "Virtual_Space", "Virtual_Space_Active" ) );
        row[3].emplace_back( std::make_unique<KickImageButton>( "Enter", "Virtual_Enter", "Virtual_Enter", "Virtual_Enter_Active" ) );

        addAllMakeVisibleAndAddListener();

        themeChanged.connect( [&]() {
            dynamic_cast<KickImageButton*>( row[0][0].get() )->setImages( "Virtual_Q", "Virtual_Q", "Virtual_Q_Active" );
            dynamic_cast<KickImageButton*>( row[0][1].get() )->setImages( "Virtual_W", "Virtual_W", "Virtual_W_Active" );
            dynamic_cast<KickImageButton*>( row[0][2].get() )->setImages( "Virtual_E", "Virtual_E", "Virtual_E_Active" );
            dynamic_cast<KickImageButton*>( row[0][3].get() )->setImages( "Virtual_R", "Virtual_R", "Virtual_R_Active" );
            dynamic_cast<KickImageButton*>( row[0][4].get() )->setImages( "Virtual_T", "Virtual_T", "Virtual_T_Active" );
            dynamic_cast<KickImageButton*>( row[0][5].get() )->setImages( "Virtual_Y", "Virtual_Y", "Virtual_Y_Active" );
            dynamic_cast<KickImageButton*>( row[0][6].get() )->setImages( "Virtual_U", "Virtual_U", "Virtual_U_Active" );
            dynamic_cast<KickImageButton*>( row[0][7].get() )->setImages( "Virtual_I", "Virtual_I", "Virtual_I_Active" );
            dynamic_cast<KickImageButton*>( row[0][8].get() )->setImages( "Virtual_O", "Virtual_O", "Virtual_O_Active" );
            dynamic_cast<KickImageButton*>( row[0][9].get() )->setImages( "Virtual_P", "Virtual_P", "Virtual_P_Active" );

            dynamic_cast<KickImageButton*>( row[1][0].get() )->setImages( "Virtual_A", "Virtual_A", "Virtual_A_Active" );
            dynamic_cast<KickImageButton*>( row[1][1].get() )->setImages( "Virtual_S", "Virtual_S", "Virtual_S_Active" );
            dynamic_cast<KickImageButton*>( row[1][2].get() )->setImages( "Virtual_D", "Virtual_D", "Virtual_D_Active" );
            dynamic_cast<KickImageButton*>( row[1][3].get() )->setImages( "Virtual_F", "Virtual_F", "Virtual_F_Active" );
            dynamic_cast<KickImageButton*>( row[1][4].get() )->setImages( "Virtual_G", "Virtual_G", "Virtual_G_Active" );
            dynamic_cast<KickImageButton*>( row[1][5].get() )->setImages( "Virtual_H", "Virtual_H", "Virtual_H_Active" );
            dynamic_cast<KickImageButton*>( row[1][6].get() )->setImages( "Virtual_J", "Virtual_J", "Virtual_J_Active" );
            dynamic_cast<KickImageButton*>( row[1][7].get() )->setImages( "Virtual_K", "Virtual_K", "Virtual_K_Active" );
            dynamic_cast<KickImageButton*>( row[1][8].get() )->setImages( "Virtual_L", "Virtual_L", "Virtual_L_Active" );

            dynamic_cast<KickImageButton*>( row[2][0].get() )->setImages( "Virtual_Shift_Active" );
            dynamic_cast<KickImageButton*>( row[2][1].get() )->setImages( "Virtual_Z", "Virtual_Z", "Virtual_Z_Active" );
            dynamic_cast<KickImageButton*>( row[2][2].get() )->setImages( "Virtual_X", "Virtual_X", "Virtual_X_Active" );
            dynamic_cast<KickImageButton*>( row[2][3].get() )->setImages( "Virtual_C", "Virtual_C", "Virtual_C_Active" );
            dynamic_cast<KickImageButton*>( row[2][4].get() )->setImages( "Virtual_V", "Virtual_V", "Virtual_V_Active" );
            dynamic_cast<KickImageButton*>( row[2][5].get() )->setImages( "Virtual_B", "Virtual_B", "Virtual_B_Active" );
            dynamic_cast<KickImageButton*>( row[2][6].get() )->setImages( "Virtual_N", "Virtual_N", "Virtual_N_Active" );
            dynamic_cast<KickImageButton*>( row[2][7].get() )->setImages( "Virtual_M", "Virtual_M", "Virtual_M_Active" );
            dynamic_cast<KickImageButton*>( row[2][8].get() )->setImages( "Virtual_Clear", "Virtual_Clear", "Virtual_Clear_Active" );

            dynamic_cast<KickImageButton*>( row[3][0].get() )->setImages( "Virtual_#123", "Virtual_#123", "Virtual_#123_Active" );
            dynamic_cast<KickImageButton*>( row[3][1].get() )->setImages( "Virtual_Space", "Virtual_Space", "Virtual_Space_Active" );
            dynamic_cast<KickImageButton*>( row[3][2].get() )->setImages( "Virtual_Enter", "Virtual_Enter", "Virtual_Enter_Active" );
        } );
    }

    KickVirtualKeyBoard::NumberComponent::NumberComponent()
    {
        row[0].emplace_back( std::make_unique<KickImageButton>( "1", "Virtual_1", "Virtual_1", "Virtual_1_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "2", "Virtual_2", "Virtual_2", "Virtual_2_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "3", "Virtual_3", "Virtual_3", "Virtual_3_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "4", "Virtual_4", "Virtual_4", "Virtual_4_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "5", "Virtual_5", "Virtual_5", "Virtual_5_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "6", "Virtual_6", "Virtual_6", "Virtual_6_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "7", "Virtual_7", "Virtual_7", "Virtual_7_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "8", "Virtual_8", "Virtual_8", "Virtual_8_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "9", "Virtual_9", "Virtual_9", "Virtual_9_Active" ) );
        row[0].emplace_back( std::make_unique<KickImageButton>( "0", "Virtual_0", "Virtual_0", "Virtual_0_Active" ) );

        row[1].emplace_back( std::make_unique<KickImageButton>( "-", "Virtual_s25", "Virtual_s25", "Virtual_s25_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "/", "Virtual_s26", "Virtual_s26", "Virtual_s26_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( ":", "Virtual_s27", "Virtual_s27", "Virtual_s27_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( ";", "Virtual_s28", "Virtual_s28", "Virtual_s28_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "(", "Virtual_s29", "Virtual_s29", "Virtual_s29_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( ")", "Virtual_s30", "Virtual_s30", "Virtual_s30_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "$", "Virtual_s17", "Virtual_s17", "Virtual_s17_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "&", "Virtual_s18", "Virtual_s18", "Virtual_s18_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "@", "Virtual_s19", "Virtual_s19", "Virtual_s19_Active" ) );
        row[1].emplace_back( std::make_unique<KickImageButton>( "\"", "Virtual_s32", "Virtual_s32 ", "Virtual_s32_Active" ) );

        row[2].emplace_back( std::make_unique<KickImageButton>( "#+=", "Virtual_s31", "Virtual_s31", "Virtual_s31_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( ".", "Virtual_s20", "Virtual_s20", "Virtual_s20_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( ",", "Virtual_s21", "Virtual_s21", "Virtual_s21_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "?", "Virtual_s22", "Virtual_s22", "Virtual_s22_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "!", "Virtual_s23", "Virtual_s23", "Virtual_s23_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "'", "Virtual_s24", "Virtual_s24", "Virtual_s24_Active" ) );
        row[2].emplace_back( std::make_unique<KickImageButton>( "clear", "Virtual_Clear", "Virtual_Clear", "Virtual_Clear_Active" ) );

        row[3].emplace_back( std::make_unique<KickImageButton>( "ABC", "Virtual_ABC", "Virtual_ABC", "Virtual_ABC_Active" ) );
        row[3].emplace_back( std::make_unique<KickImageButton>( " ", "Virtual_Space", "Virtual_Space", "Virtual_Space_Active" ) );
        row[3].emplace_back( std::make_unique<KickImageButton>( "Enter", "Virtual_Enter", "Virtual_Enter", "Virtual_Enter_Active" ) );

        addAllMakeVisibleAndAddListener();

        themeChanged.connect( [&]() {
            dynamic_cast<KickImageButton*>( row[0][0].get() )->setImages( "Virtual_1", "Virtual_1", "Virtual_1_Active" );
            dynamic_cast<KickImageButton*>( row[0][1].get() )->setImages( "Virtual_2", "Virtual_2", "Virtual_2_Active" );
            dynamic_cast<KickImageButton*>( row[0][2].get() )->setImages( "Virtual_3", "Virtual_3", "Virtual_3_Active" );
            dynamic_cast<KickImageButton*>( row[0][3].get() )->setImages( "Virtual_4", "Virtual_4", "Virtual_4_Active" );
            dynamic_cast<KickImageButton*>( row[0][4].get() )->setImages( "Virtual_5", "Virtual_5", "Virtual_5_Active" );
            dynamic_cast<KickImageButton*>( row[0][5].get() )->setImages( "Virtual_6", "Virtual_6", "Virtual_6_Active" );
            dynamic_cast<KickImageButton*>( row[0][6].get() )->setImages( "Virtual_7", "Virtual_7", "Virtual_7_Active" );
            dynamic_cast<KickImageButton*>( row[0][7].get() )->setImages( "Virtual_8", "Virtual_8", "Virtual_8_Active" );
            dynamic_cast<KickImageButton*>( row[0][8].get() )->setImages( "Virtual_9", "Virtual_9", "Virtual_9_Active" );
            dynamic_cast<KickImageButton*>( row[0][9].get() )->setImages( "Virtual_0", "Virtual_0", "Virtual_0_Active" );

            dynamic_cast<KickImageButton*>( row[1][0].get() )->setImages( "Virtual_s25", "Virtual_s25", "Virtual_s25_Active" );
            dynamic_cast<KickImageButton*>( row[1][1].get() )->setImages( "Virtual_s26", "Virtual_s26", "Virtual_s26_Active" );
            dynamic_cast<KickImageButton*>( row[1][2].get() )->setImages( "Virtual_s27", "Virtual_s27", "Virtual_s27_Active" );
            dynamic_cast<KickImageButton*>( row[1][3].get() )->setImages( "Virtual_s28", "Virtual_s28", "Virtual_s28_Active" );
            dynamic_cast<KickImageButton*>( row[1][4].get() )->setImages( "Virtual_s29", "Virtual_s29", "Virtual_s29_Active" );
            dynamic_cast<KickImageButton*>( row[1][5].get() )->setImages( "Virtual_s30", "Virtual_s30", "Virtual_s30_Active" );
            dynamic_cast<KickImageButton*>( row[1][6].get() )->setImages( "Virtual_s17", "Virtual_s17", "Virtual_s17_Active" );
            dynamic_cast<KickImageButton*>( row[1][7].get() )->setImages( "Virtual_s18", "Virtual_s18", "Virtual_s18_Active" );
            dynamic_cast<KickImageButton*>( row[1][8].get() )->setImages( "Virtual_s19", "Virtual_s19", "Virtual_s19_Active" );
            dynamic_cast<KickImageButton*>( row[1][9].get() )->setImages( "Virtual_s32", "Virtual_s32 ", "Virtual_s32_Active" );

            dynamic_cast<KickImageButton*>( row[2][0].get() )->setImages( "Virtual_s31", "Virtual_s31", "Virtual_s31_Active" );
            dynamic_cast<KickImageButton*>( row[2][1].get() )->setImages( "Virtual_s20", "Virtual_s20", "Virtual_s20_Active" );
            dynamic_cast<KickImageButton*>( row[2][2].get() )->setImages( "Virtual_s21", "Virtual_s21", "Virtual_s21_Active" );
            dynamic_cast<KickImageButton*>( row[2][3].get() )->setImages( "Virtual_s22", "Virtual_s22", "Virtual_s22_Active" );
            dynamic_cast<KickImageButton*>( row[2][4].get() )->setImages( "Virtual_s23", "Virtual_s23", "Virtual_s23_Active" );
            dynamic_cast<KickImageButton*>( row[2][5].get() )->setImages( "Virtual_s24", "Virtual_s24", "Virtual_s24_Active" );
            dynamic_cast<KickImageButton*>( row[2][6].get() )->setImages( "Virtual_Clear", "Virtual_Clear", "Virtual_Clear_Active" );

            dynamic_cast<KickImageButton*>( row[3][0].get() )->setImages( "Virtual_ABC", "Virtual_ABC", "Virtual_ABC_Active" );
            dynamic_cast<KickImageButton*>( row[3][1].get() )->setImages( "Virtual_Space", "Virtual_Space", "Virtual_Space_Active" );
            dynamic_cast<KickImageButton*>( row[3][2].get() )->setImages( "Virtual_Enter", "Virtual_Enter", "Virtual_Enter_Active" );
        } );
    }

    KickVirtualKeyBoard::KickVirtualKeyBoard( const juce::String& initialKeyvalue )
        : inputTextLabel( initialKeyvalue )
    {
        mt::utils::addChildComponents( this, inputTextLabel, letterComp, numComp, speSym, upperCaseComp );

        setInterceptsMouseClicks( false, true );
        setBufferedToImage( true );

        juce::Font fnt( 36.f );
        inputTextLabel.clear();
        inputTextLabel.setFont( fnt );
        inputTextLabel.setColour( juce::TextEditor::backgroundColourId, ui::Theme::backgroundColour( 13 ) );
        inputTextLabel.setColour( juce::TextEditor::textColourId, ui::Theme::textColour( 2 ) );
        inputTextLabel.setColour( juce::TextEditor::outlineColourId, ui::Theme::outlineColour( 0 ) );
        inputTextLabel.setColour( juce::TextEditor::focusedOutlineColourId, ui::Theme::outlineColour( 0 ) );
        inputTextLabel.setText( initialKeyvalue, juce::dontSendNotification );
        inputTextLabel.setCaretVisible( true );
        inputTextLabel.selectAll();
        inputTextLabel.setFocusContainerType( juce::Component::FocusContainerType::focusContainer );
        inputTextLabel.setJustification( juce::Justification::centredLeft );

        auto keyEntered = [this]( const juce::String& key ) {
            if( !key.compareIgnoreCase( "clear" ) )
            {
                juce::KeyPress keyj( juce::KeyPress::backspaceKey );
                inputTextLabel.keyPressed( keyj );
            }
            else if( !key.compareIgnoreCase( "ABC" ) )
            {
                showKeyboard( Type::KEYS_LOWERCASE_ALPHABETIC );
            }
            else if( !key.compareIgnoreCase( "#123" ) )
            {
                showKeyboard( Type::KEYS_NUMERIC );
            }
            else if( !key.compareIgnoreCase( "#+=" ) )
            {
                showKeyboard( Type::KEYS_SPECIAL );
            }
            else if( !key.compareIgnoreCase( "Shift1" ) )
            {
                showKeyboard( Type::KEYS_UPPERCASE_ALPHABETIC );
            }
            else if( !key.compareIgnoreCase( "Shift2" ) )
            {
                showKeyboard( Type::KEYS_LOWERCASE_ALPHABETIC );
            }
            else
            {
                inputTextLabel.insertTextAtCaret( key );
            }
        };

        letterComp.onKeyPressed_    = keyEntered;
        numComp.onKeyPressed_       = keyEntered;
        speSym.onKeyPressed_        = keyEntered;
        upperCaseComp.onKeyPressed_ = keyEntered;

        auto enterKeyHandler = [this]( int R ) {
            onEnterPressed( R );
        };

        letterComp.onEnterPressed    = enterKeyHandler;
        numComp.onEnterPressed       = enterKeyHandler;
        speSym.onEnterPressed        = enterKeyHandler;
        upperCaseComp.onEnterPressed = enterKeyHandler;

        showKeyboard( currentType );
    }

    KickVirtualKeyBoard& KickVirtualKeyBoard::getWithText( const juce::String& InitalValue )
    {
        KickLooknFeel::getLnf();
        static auto* instance = new KickVirtualKeyBoard( "" );
        instance->setInputText( InitalValue );
        return *instance;
    }

    void KickVirtualKeyBoard::showKeyboard( Type type )
    {
        currentType = type;

        inputTextLabel.setVisible( true );
        letterComp.setVisible( false );
        numComp.setVisible( false );
        speSym.setVisible( false );
        upperCaseComp.setVisible( false );

        if( Type::KEYS_LOWERCASE_ALPHABETIC == currentType ) letterComp.setVisible( true );
        if( Type::KEYS_NUMERIC == currentType ) numComp.setVisible( true );
        if( Type::KEYS_SPECIAL == currentType ) speSym.setVisible( true );
        if( Type::KEYS_UPPERCASE_ALPHABETIC == currentType ) upperCaseComp.setVisible( true );
    }

    void KickVirtualKeyBoard::show( const juce::String& Title, Type KeyboardType, std::function<void( int )> Callback, juce::Rectangle<int> Bounds )
    {
        showKeyboard( KeyboardType );
        mt::utils::showOptions( Title, this, std::forward<std::function<void( int )>>( Callback ), Bounds );
    }

    void KickVirtualKeyBoard::close()
    {
        mt::utils::exitDialog( getCurrentlyFocusedComponent(), UI::APPLY );
    }

    void KickVirtualKeyBoard::resized()
    {
        auto b = getLocalBounds().reduced( compSpaceAround );
        inputTextLabel.setBounds( b.removeFromTop( inputLabelHeight ) );
        b.removeFromTop( compSpaceAround );
        auto kBounds = b;
        letterComp.setBounds( kBounds );
        numComp.setBounds( kBounds );
        speSym.setBounds( kBounds );
        upperCaseComp.setBounds( kBounds );
    }

}
