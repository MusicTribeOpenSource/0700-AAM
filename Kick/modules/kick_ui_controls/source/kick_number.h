/*
 * Copyright (c) 2021 MUSIC Tribe
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

#pragma once

#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/dcl/dcl_signals.h"

namespace ui
{
    inline static constexpr int buttonSpace1    = 17;
    inline static constexpr int buttonSpace2    = 10;
    inline static constexpr int buttonWidth1    = 120;
    inline static constexpr int buttonHeight1   = 50;
    inline static constexpr int buttonHeight2   = 44;
    inline static constexpr int spaceAround1    = 46;
    inline static constexpr int spaceAround2    = 19;
    inline static constexpr int inputTextHeight = 80;
    inline static constexpr int buttonWidth3    = 80;
    inline static constexpr int buttonWidth4    = 44;
    inline static constexpr int buttonHeight4   = 44;
    inline static constexpr int numpadWidth     = 650;
    inline static constexpr int tempoWidth      = 800;
    inline static constexpr int numpadHeight    = 520;

    ////////////////////////////////////////////////
    //   Class: Kick Number
    ////////////////////////////////////////////////
    enum class ViewType
    {
        withTitleTop,
        withTitleBottom
    };
    template<typename Type>
    class KickNumPad;

    template<typename Type>
    class KickNumberT
        : public KickControlBase<juce::Component>
        , public mt::MTProperties
        , private juce::AsyncUpdater
    {
    public:
        std::function<void( Type )> onValueChanged;

        explicit KickNumberT( const juce::String& Name, Type Min = -9999, Type Max = 9999, Type Default = 0, Type StepSize = 1, bool ShowTitle = true, bool ShowBorder = false )
            : mt::MTProperties( this )
            , min_( Min )
            , max_( Max )
            , val_( Default )
            , step_( StepSize )
        {
            setTitle( Name );
            setName( Name );
            addMouseListener( this, true );
            setValueBounds( Min, Max, Default, StepSize );

            setTitleColour( ui::Theme::textColour() );
            setValueColour( ui::Theme::textColour( 2 ) );
            setTitleBackgroundColour( ui::Theme::backgroundColour( 9 ) );
            setValueBackgroundColour( ui::Theme::backgroundColour( 3 ) );
            setBorderColour( ui::Theme::outlineColour() );
            setValueFontHeight( 26.f );
            setTitleJustification( juce::Justification::centredTop );
            showTitle( ShowTitle );
            showBorder( ShowBorder );
        }

        ~KickNumberT()
        {
            removeMouseListener( this );
        }

        class Listener
        {
        public:
            virtual ~Listener()                                          = default; // Destructor.
            virtual void numberValueChanged( KickNumberT<Type>* Number ) = 0;
        };

        /** Adds a listener to be called when this component's value changes. */
        void addListener( KickNumberT<Type>::Listener* l ) { listeners.add( l ); }
        /** Removes a previously-registered listener. */
        void removeListener( KickNumberT<Type>::Listener* l ) { listeners.remove( l ); }

        void setDisplayFormat( ViewType viewType )
        {
            type = viewType;
        }

        void paint( juce::Graphics& g ) override
        {
            KickControlBase::paint( g );
            auto b = getLocalBounds();

            // Draw Border.
            if( isShowingBorder() )
            {
                mt::utils::renderRoundedRect( g, b, getBorderColour(), getBorderThickness(), getCornerRadius() );
                b = b.reduced( getBorderThickness() );
            }

            // Draw Title text.
            if( isShowingTitle() )
            {
                auto height = (int)( b.proportionOfHeight( getTitleProportion() ) );
                auto bTitle = getLocalBounds();
                if( type == ViewType::withTitleTop )
                    bTitle = b.removeFromTop( height );
                if( type == ViewType::withTitleBottom )
                    bTitle = b.removeFromBottom( height );
                mt::utils::fillRoundedRect( g, b, getTitleBackgroundColour(), getCornerRadius() );
                mt::utils::renderText( g, getTitle().toUpperCase(), bTitle, getTitleColour(), getTitleJustification(), getTitleFontHeight() );
            }

            // Draw Value.
            juce::String str( val_ );
            if( getPadLen() )
                str = str.paddedLeft( getPadChar(), getPadLen() );

            mt::utils::fillRoundedRect( g, b, getValueBackgroundColour(), getCornerRadius() );
            if( isAlternateText && val_ == 0 )
            {
                mt::utils::renderText( g, alternateText, b, getValueColour(), getValueJustification(), getValueFontHeight() );
            }
            else
                mt::utils::renderText( g, str + getPostFix(), b, getValueColour(), getValueJustification(), getValueFontHeight() );
        }

        inline void setRange( Type Min, Type Max, Type StepSize = 1 ) { setValueBounds( Min, Max, val_, StepSize ); }

        void setValueBounds( Type Min, Type Max, Type Val, Type StepSize = 1 )
        {
            min_  = Min;
            max_  = Max;
            step_ = StepSize;
            norm_ = juce::NormalisableRange<float>( (float)min_, (float)max_ );
            setValue( Val, juce::dontSendNotification );
        }

        Type getMin( void ) const { return min_; };
        Type getMax( void ) const { return max_; };
        Type getValue( void ) const { return val_; };
        Type getStep( void ) const { return step_; };

        void setMin( Type V ) { setValueBounds( V, max_, val_, step_ ); }
        void setMax( Type V ) { setValueBounds( min_, V, val_, step_ ); }
        void setStep( Type V ) { setValueBounds( min_, max_, val_, V ); }
        void setValue( Type V, juce::NotificationType notification = juce::sendNotification )
        {
            if( V != val_ )
            {
                val_ = juce::jlimit<Type>( min_, max_, V );
                if( juce::dontSendNotification != notification )
                {
                    if( onValueChanged )
                    {
                        onValueChanged( val_ );
                    }

                    Component::BailOutChecker checker( this );
                    listeners.callChecked( checker, [&]( KickNumberT<Type>::Listener& l ) { l.numberValueChanged( this ); } );
                }
                triggerAsyncUpdate();
                sendPercentValueToHardware( val_ );
            }
        }

        void  setNormalized( float Value ) { setValue( norm_.convertFrom0to1( Value ), juce::sendNotification ); }
        float getNormalized() { return norm_.convertTo0to1( val_ ); }

        void setPadding( const juce::String& PadChar, int PadLen )
        {
            setPadChar( PadChar );
            setPadLen( PadLen );
        }

        double getIncrement() override
        {
            return (double)step_;
        }

        void knobRotated( double deltaValue ) override
        {
            setValue( val_ + (Type)( deltaValue ) );
        }

        void sliderMoved( float percentValue ) override
        {
            double tempValue = norm_.convertFrom0to1( ( percentValue ) );
            setValue( static_cast<Type>( tempValue ) );
        }

        void sendPercentValueToHardware( Type value )
        {
            auto valueInPercent = (int)( 100.0 * norm_.convertTo0to1( (float)value ) );
            //TODO - sNeed to find a way to avoid calling this function before the project is loaded.

            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::sendSliderValueInPercentMessage( valueInPercent ) );
        }

        void knobClicked() override
        {
            showOptionsDlg();
        }

        void setAlternateText( bool state, juce::String text )
        {
            isAlternateText = state;
            alternateText   = text;
            triggerAsyncUpdate();
        }

        void handleAsyncUpdate() override
        {
            if( !isUpdatePending() )
                repaint();
        }

    private:
        void showOptionsDlg()
        {
            if( !isEditing && isEnabled() )
            {
                isEditing = true;

                KickLooknFeel::getLnf();
                static auto& pad = *new KickNumPad<Type>{ 0, 1, 1 };
                pad.setValueBounds( min_, max_, val_, step_ );
                pad.show( getTitle(), [&]( int R ) {
                    isEditing = false;
                    if( R == UI::BUTTONS::APPLY )
                    {
                        setValue( pad.getValue() );
                    }
                    else
                    {
                        sendPercentValueToHardware( getValue() );
                    }
                } );
                pad.setFocus( true );
            }
        }

        void mouseDoubleClick( const juce::MouseEvent& /*e*/ ) override
        {
            showOptionsDlg();
        }

        void mouseDown( const juce::MouseEvent& event )
        {
            Component::BailOutChecker checker( this );
            if( checker.shouldBailOut() )
                return;

            auto currentTime = juce::Time::getCurrentTime();

            if( ( currentTime - lastClickTime ).inMilliseconds() <= doubleClickTimeoutMs && currentTime != lastClickTime )
                mouseDoubleClick( event );
            else
                lastClickTime = currentTime;
        }

    private:
        bool                           isEditing{ false };
        Type                           min_{ 0 };
        Type                           max_{ 0 };
        Type                           val_{ 0 };
        Type                           step_{ 1 };
        juce::NormalisableRange<float> norm_;
        ViewType                       type{ ViewType::withTitleTop };
        bool                           isAlternateText{ false };
        juce::String                   alternateText{ "" };

        juce::ListenerList<KickNumberT<Type>::Listener> listeners;

        juce::Time lastClickTime;
        float      doubleClickTimeoutMs{ 400 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickNumberT<Type> )
    };

    ////////////////////////////////////////////////
    //   Class: Kick Num Pad
    ////////////////////////////////////////////////

    template<typename Type>
    class KickNumPad
        : public KickControlBase<juce::Component>
        , public mt::MTProperties
        , public juce::DeletedAtShutdown
    {
    public:
        explicit KickNumPad( Type min, Type max, Type defaultVal, Type stepSize = 1 )
            : mt::MTProperties( this )
        {
            setLookAndFeel( KickLooknFeel::getLnf() );
            setValueBounds( min, max, defaultVal, stepSize );

            slider.setMaxFontSize( 100 );
            slider.showValue( true, juce::Justification::centred, juce::Slider::NoTextBox );
            //auto& vl = slider.getValueLabel();
            //vl.setColour( juce::Label::textColourId, ui::Theme::textColour( 2 ) );

            mt::utils::addAndMakeVisible( this, slider, num1, num2, num3, num4, num5, num6, num7, num8, num9, num0, dot, inc, dec, mul, div, del, clr, sign, inputTextLabel );

            inc.setConnectedEdges( (int)juce::Button::ConnectedEdgeFlags::ConnectedOnRight );
            dec.setConnectedEdges( (int)juce::Button::ConnectedEdgeFlags::ConnectedOnLeft );

            inputTextLabel.setFont( 24 );
            inputTextLabel.setColour( juce::TextEditor::backgroundColourId, ui::Theme::backgroundColour( 13 ) );
            inputTextLabel.setColour( juce::TextEditor::textColourId, ui::Theme::textColour( 2 ) );
            inputTextLabel.setCaretVisible( true );
            inputTextLabel.setFocusContainerType( juce::Component::FocusContainerType::focusContainer );
            inputTextLabel.setBorder( { 22, 2, 20, 2 } );
            inputTextLabel.setJustification( juce::Justification::centred );
            inputTextLabel.setText( slider.getTextFromValue( slider.getValue() ) );
            mt::utils::setPlacementFlag( juce::Justification::centred, num0, num1, num2, num3, num4, num5, num6, num7, num8, num9 );

            num0.onClick = [&]() { addInput( 0 ); };
            num1.onClick = [&]() { addInput( 1 ); };
            num2.onClick = [&]() { addInput( 2 ); };
            num3.onClick = [&]() { addInput( 3 ); };
            num4.onClick = [&]() { addInput( 4 ); };
            num5.onClick = [&]() { addInput( 5 ); };
            num6.onClick = [&]() { addInput( 6 ); };
            num7.onClick = [&]() { addInput( 7 ); };
            num8.onClick = [&]() { addInput( 8 ); };
            num9.onClick = [&]() { addInput( 9 ); };
            sign.onClick = [&]() { intermidentValue *= -1; refreshDisplay( ); };
            del.onClick  = [&]() { intermidentValue /= 10; refreshDisplay( ); };
            mul.onClick  = [&]() { intermidentValue *= 2;  refreshDisplay( ); };
            div.onClick  = [&]() { intermidentValue /= 2;  if(intermidentValue == 0){ intermidentValue = 1;} refreshDisplay( ); };
            inc.onClick  = [&]() { intermidentValue += (int)stepValue; refreshDisplay( ); };
            dec.onClick  = [&]() { intermidentValue -= (int)stepValue; refreshDisplay( ); };
            clr.onClick  = [&]() { intermidentValue =0; refreshDisplay( ); };

            slider.onValueChanged = [&]( double V ) {
                intermidentValue = (int)V;
                slider.sendPercentValueToHardware( (int)intermidentValue );
                refreshDisplay();
            };

            for( auto it : getChildren() )
            {
                it->setWantsKeyboardFocus( false );
            }
            slider.setSelectionEnabled( false );

            themeChanged.connect( [&]() {
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
                dot.setImages( "numPad_Dot", "numPad_Dot", "numPad_Dot_Active" );
                del.setImages( "numPad_D", "numPad_D", "numPad_D_Active" );
                clr.setImages( "numPad_C", "numPad_C", "numPad_C_Active" );
                inc.setImages( "numPad_P", "numPad_P", "numPad_P_Active" );
                dec.setImages( "numPad_M", "numPad_M", "numPad_M_Active" );
                mul.setImages( "numPad_x2", "numPad_x2", "numPad_x2_Active" );
                div.setImages( "numPad_/2", "numPad_/2", "numPad_/2_Active" );
                sign.setImages( "numPad_+", "numPad_+", " numPad_ + _Active " );
            } );
        }

        void setValueBounds( Type min, Type max, Type defaultVal, Type stepSize = 1 )
        {
            minValue         = min;
            maxValue         = max;
            defaultValue     = defaultVal;
            stepValue        = stepSize;
            intermidentValue = 0;

            if( minValue > 0 )
                sign.setEnabled( false );

            slider.setRange( (double)minValue, (double)maxValue, (double)stepValue );
            setValue( defaultVal );
        }

        Type getValue( void ) const
        {
            return (Type)slider.getValue();
        }

        void setValue( Type Value )
        {
            intermidentValue = (int)validate( Value );
            refreshDisplay();
        }

        void show( const juce::String& Title, std::function<void( int )> CallbackOnExit )
        {
            mt::utils::showOptions( Title, this, CallbackOnExit, { 0, 0, numpadWidth, numpadHeight } );
        }

        double getIncrement() override { return (double)stepValue; }
        void   sliderMoved( float v ) override { slider.sliderMoved( v ); };
        void   knobRotated( double v ) override { slider.knobRotated( v ); };
        void   knobClicked() override { mt::utils::exitDialog( this, UI::APPLY ); };
        void   mouseDoubleClick( const juce::MouseEvent& ) override
        {
            setValue( defaultValue );
        }

    private:
        KickSlider       slider;
        juce::TextEditor inputTextLabel;

        int                                                intermidentValue{ 0 };
        std::chrono::time_point<std::chrono::steady_clock> lastInputTime;
        std::chrono::milliseconds                          timeoutDuration{ 1000 }; // 2 seconds

        KickImageButton num1{ "1", "numPad_1", "numPad_1", "numPad_1_Active" };
        KickImageButton num2{ "2", "numPad_2", "numPad_2", "numPad_2_Active" };
        KickImageButton num3{ "3", "numPad_3", "numPad_3", "numPad_3_Active" };
        KickImageButton num4{ "4", "numPad_4", "numPad_4", "numPad_4_Active" };
        KickImageButton num5{ "5", "numPad_5", "numPad_5", "numPad_5_Active" };
        KickImageButton num6{ "6", "numPad_6", "numPad_6", "numPad_6_Active" };
        KickImageButton num7{ "7", "numPad_7", "numPad_7", "numPad_7_Active" };
        KickImageButton num8{ "8", "numPad_8", "numPad_8", "numPad_8_Active" };
        KickImageButton num9{ "9", "numPad_9", "numPad_9", "numPad_9_Active" };
        KickImageButton num0{ "0", "numPad_0", "numPad_0", "numPad_0_Active" };
        KickImageButton dot{ ".", "numPad_Dot", "numPad_Dot", "numPad_Dot_Active" };
        KickImageButton del{ "Del", "numPad_D", "numPad_D", "numPad_D_Active" };
        KickImageButton clr{ "Clear", "numPad_C", "numPad_C", "numPad_C_Active" };
        KickImageButton inc{ "+", "numPad_P", "numPad_P", "numPad_P_Active" };
        KickImageButton dec{ "-", "numPad_M", "numPad_M", "numPad_M_Active" };
        KickImageButton mul{ "x2", "numPad_x2", "numPad_x2", "numPad_x2_Active" };
        KickImageButton div{ "/2", "numPad_/2", "numPad_/2", "numPad_/2_Active" };
        KickImageButton sign{ "+/-", "numPad_+", "numPad_+", "numPad_+_Active" };

        Type minValue;
        Type maxValue;
        Type stepValue;
        Type defaultValue;

        void addInput( int i )
        {
            auto now = std::chrono::steady_clock::now();
            if( now - lastInputTime > timeoutDuration )
            {
                intermidentValue = 0; // Reset current value if timeout
            }
            lastInputTime = now;

            intermidentValue = intermidentValue * 10 + i;
            if( intermidentValue > maxValue )
            {
                intermidentValue = (int)maxValue; // Ensure value does not exceed max
            }
            refreshDisplay();
        }

        void checkMaxnReset( int newInput )
        {
            static int attempts = 0;
            auto       value    = juce::jlimit<Type>( minValue, maxValue, intermidentValue );
            if( value == maxValue )
            {
                if( ++attempts >= 2 )
                {
                    intermidentValue = newInput;
                }
            }
            else
            {
                attempts = 0;
            }
        }

        inline Type validate( Type Val )
        {
            return juce::jlimit( minValue, maxValue, Val );
        }

        void refreshDisplay()
        {
            auto val = validate( (Type)intermidentValue );
            slider.setValue( (double)val, juce::dontSendNotification );
            inputTextLabel.setText( juce::String( val ) );
        };

        void resized() override
        {
            auto b = getLocalBounds();
            b.removeFromTop( buttonSpace1 );
            inputTextLabel.setBounds( b.removeFromTop( inputTextHeight ).reduced( spaceAround1, 0 ) );

            b.removeFromTop( buttonSpace1 );
            auto row1 = b.removeFromTop( buttonHeight2 ).reduced( spaceAround1, 0 );
            b.removeFromTop( buttonSpace1 );
            auto row2 = b.removeFromTop( buttonHeight1 ).reduced( spaceAround1, 0 );
            b.removeFromTop( buttonSpace2 );
            auto row3 = b.removeFromTop( buttonHeight1 ).reduced( spaceAround1, 0 );
            b.removeFromTop( buttonSpace2 );
            auto row4 = b.removeFromTop( buttonHeight1 ).reduced( spaceAround1, 0 );
            b.removeFromTop( buttonSpace2 );
            auto row5 = b.removeFromTop( buttonHeight1 ).reduced( spaceAround1, 0 );

            dec.setBounds( row1.removeFromLeft( buttonWidth3 ) );
            row1.removeFromRight( 3 );
            inc.setBounds( row1.removeFromRight( buttonWidth3 ) );
            slider.setBounds( row1.reduced( buttonSpace2, 0 ) );

            num7.setBounds( row2.removeFromLeft( buttonWidth1 ) );
            row2.removeFromLeft( buttonSpace2 );
            num8.setBounds( row2.removeFromLeft( buttonWidth1 ) );
            row2.removeFromLeft( buttonSpace2 );
            num9.setBounds( row2.removeFromLeft( buttonWidth1 ) );
            row2.removeFromLeft( buttonSpace2 );
            del.setBounds( row2 );

            num4.setBounds( row3.removeFromLeft( buttonWidth1 ) );
            row3.removeFromLeft( buttonSpace2 );
            num5.setBounds( row3.removeFromLeft( buttonWidth1 ) );
            row3.removeFromLeft( buttonSpace2 );
            num6.setBounds( row3.removeFromLeft( buttonWidth1 ) );
            row3.removeFromLeft( buttonSpace2 );
            clr.setBounds( row3 );

            num1.setBounds( row4.removeFromLeft( buttonWidth1 ) );
            row4.removeFromLeft( buttonSpace2 );
            num2.setBounds( row4.removeFromLeft( buttonWidth1 ) );
            row4.removeFromLeft( buttonSpace2 );
            num3.setBounds( row4.removeFromLeft( buttonWidth1 ) );
            row4.removeFromLeft( buttonSpace2 );
            mul.setBounds( row4 );

            sign.setBounds( row5.removeFromLeft( buttonWidth1 ) );
            row5.removeFromLeft( buttonSpace2 );
            num0.setBounds( row5.removeFromLeft( buttonWidth1 ) );
            row5.removeFromLeft( buttonSpace2 );
            dot.setBounds( row5.removeFromLeft( buttonWidth1 ) );
            row5.removeFromLeft( buttonSpace2 );
            div.setBounds( row5 );
        };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickNumPad<Type> )
    };

    using KickNumber   = KickNumberT<int>;
    using KickNumber64 = KickNumberT<juce::int64>;
    using KickNumberf  = KickNumberT<float>;
}
