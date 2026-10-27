
#include "../audio_components/gridview/kick_GridData.h"

namespace kick_ui
{
#define stpButtonWidth 119
#define boxButtonWidth 320
#define bbtButtonWidth 60
#define buttonHeight   44
#define controlY       7
#define sequenceX      20
#define trackX         160
#define programX       300
#define boxX           440
#define barX           780
#define beatX          860
#define tickX          940

    AppStatusbar& AppStatusbar::Instance()
    {
        // Register the shared look-and-feel before this dependent component.
        ui::KickLooknFeel::getLnf();
        static auto* instance = new AppStatusbar;
        return *instance;
    };

    AppStatusbar::AppStatusbar()
    {
        setName( "Statusbar" );

        boxInfo.setValueJustification( juce::Justification::centred );
        boxInfo.setColours( ui::Theme::textColour( 2 ), ui::Theme::backgroundColour( 5 ) );
        boxInfo.setFontHeight( 18 );
        // Progress Bar...
        progressBar.setTextToDisplay( "" );
        progressBar.setColour( juce::ProgressBar::backgroundColourId, ui::Theme::backgroundColour( 12 ) );
        progressBar.setColour( juce::ProgressBar::foregroundColourId, ui::Theme::foregroundColour( 2 ) );

        setInterceptsMouseClicks( false, true );

        mt::utils::addChildComponents( this, progressBar, boxInfo, left, right );

        auto& grid{ GridData::Instance() };

        grid.gridPlayHeadChanged.connect( [&]() {
            right.bbt.updatePlayHead();
        } );

        grid.gridChanged.connect( [&]() {
            left.stp.updateSelection();
            right.bbt.updatePlayHead();
        } );

        left.stp.updateSelection();
        right.bbt.updatePlayHead();
    }

    //==============================================================================
    void AppStatusbar::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 1 ) );
        g.fillRect( getLocalBounds() );
        mt::utils::fillRoundedRect( g, boxInfo.getBounds(), ui::Theme::backgroundColour( 5 ), false, false, false, false );
    }

    void AppStatusbar::resized()
    {
        progressBar.setBoundsRelative( .0f, .0f, 1.0f, .094 );
        left.setBoundsRelative( .0097f, .2031f, .4091f, .6875f );
        boxInfo.setBoundsRelative( .4296f, .2031f, .4225f, .6875f );
        right.setBoundsRelative( .7617f, .2031f, .2382f, .6875f );

        init = true;
    }


    void AppStatusbar::setBoxInfo( juce::String BoxInfo )
    {
        boxInfo.setText( BoxInfo, juce::dontSendNotification );
    }

    void AppStatusbar::visibilityChanged()
    {
        auto visible = isVisible();

        for( auto& it : getChildren() )
            it->setVisible( visible );
        right.bbt.refresh();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    //////////////Left Control
    //////////////////////////////////////////////////////////////////////////////////////////////

    LeftControl::LeftControl()
    {
        mt::utils::addAndMakeVisible( this, stp );
        setInterceptsMouseClicks( true, false );
    }

    void LeftControl::resized()
    {
        auto b = getLocalBounds();
        stp.setBoundsRelative( 0.F, 0.F, 1.F, 1.F );
    }
    void LeftControl::mouseDown( const juce::MouseEvent& e )
    {
        stp.displaySTPPopup();
    }
    //////////////////////////////////////////////////////////////////////////////////////////////
    //////////////Right Control
    //////////////////////////////////////////////////////////////////////////////////////////////

    RightControl::RightControl()
    {
        mt::utils::addAndMakeVisible( this, bbt );
        setInterceptsMouseClicks( true, false );
    }

    void RightControl::resized()
    {
        auto b = getLocalBounds();
        bbt.setBoundsRelative( 0.F, 0.F, 1.F, 1.F );
    }
    void RightControl::mouseDown( const juce::MouseEvent& e )
    {
        bbt.displayPopup();
    }
}
