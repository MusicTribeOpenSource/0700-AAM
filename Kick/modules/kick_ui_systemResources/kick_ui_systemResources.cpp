#ifdef KICK_UI_H_INCLUDE
    /* When you add this cpp file to your project, you mustn't include it in a file where you've
already included any other headers - just put it inside a file on its own, possibly with your config
flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
header files that the compiler may be using.
*/
    #error "Incorrect use of JUCE cpp file"
#endif

#include "hardware/source/kick_Resorces.h"
#include "kick_ui_systemResources.h"

namespace kick_ui
{
    KSystemResources& KSystemResources::Instance()
    {
        ui::KickLooknFeel::getLnf();
        static auto* resources = new KSystemResources;
        return *resources;
    }

    KSystemResources::KSystemResources()
    {
        setTitle( "SYSTEM RESOURCES" );

        mt::utils::addChildComponents( this, cpu, ram, flash, sdcard, usb );
        mt::utils::addAndMakeVisible( this, storage, purgeAll, purgeUnused, ok );

        cpu.setEnabled( false );
        cpu.setRange( 0.0, 100.0 );
        cpu.setTextValueSuffix( " %" );
        cpu.setTitleProportion( .35f );
        cpu.setValueProportion( .35f );
        cpu.showTitle( true, juce::Justification::centredTop );
        cpu.showValue( true, juce::Justification::centredBottom, juce::Slider::TextBoxRight );

        ram.setEnabled( false );
        ram.setRange( 0.0, 100.0 );
        ram.setTextValueSuffix( " %" );
        ram.setTitleProportion( .35f );
        ram.setValueProportion( .35f );
        ram.showTitle( true, juce::Justification::centred );
        ram.showValue( true, juce::Justification::centred, juce::Slider::TextBoxBelow );

        flash.setEnabled( false );
        flash.setRange( 0.0, 100.0 );
        flash.setTextValueSuffix( " %" );
        flash.setTitleProportion( .35f );
        flash.setValueProportion( .35f );
        flash.showTitle( true, juce::Justification::centred );
        flash.showValue( true, juce::Justification::centred, juce::Slider::TextBoxBelow );

        sdcard.setEnabled( false );
        sdcard.setRange( 0.0, 100.0 );
        sdcard.setTextValueSuffix( " %" );
        sdcard.setTitleProportion( .35f );
        sdcard.setValueProportion( .35f );
        sdcard.showTitle( true, juce::Justification::centred );
        sdcard.showValue( true, juce::Justification::centred, juce::Slider::TextBoxBelow );

        usb.setEnabled( false );
        usb.setRange( 0.0, 100.0 );
        usb.setTextValueSuffix( " %" );
        usb.setTitleProportion( .35f );
        usb.setValueProportion( .35f );
        usb.showTitle( true, juce::Justification::centred );
        usb.showValue( true, juce::Justification::centred, juce::Slider::TextBoxBelow );

        storage.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        storage.setValueColour( ui::Theme::textColour( 2 ) );
        storage.setValueJustification( juce::Justification::centredTop );
        storage.setValueFontHeight( 16 );

        purgeAll.setPlacementFlag( juce::Justification::centred );
        purgeUnused.setPlacementFlag( juce::Justification::centred );
        ok.setPlacementFlag( juce::Justification::centred );

        purgeAll.onClick    = [&]() {};
        purgeUnused.onClick = [&]() {};
        ok.onClick          = [&]() {
            mt::utils::exitDialog( this, UI::APPLY );
        };

        ui::themeChanged.connect( [&]() {
            purgeAll.setImages( "purge" );
            purgeUnused.setImages( "purgeused" );
            ok.setImages( "ok_resource" );
        } );
    }

    void KSystemResources::start()
    {
        startTimerHz( 1 );
    }

    void KSystemResources::stop()
    {
        stopTimer();
    }
    void KSystemResources::timerCallback()
    {
        double          cpuusage;
        mt::sysRamInfo  rInfo;
        mt::sysSizeInfo sInfo;
        mt::SystemResource::getResourceInterface().cpuInfo( cpuusage );
        mt::SystemResource::getResourceInterface().ramInfo( rInfo );

        mt::SystemResource::getResourceInterface().sizeInfo( internalPath, sInfo );
        cpu.setValue( cpuusage, juce::dontSendNotification );
        ram.setValue( rInfo.perRam, juce::dontSendNotification );
        flash.setValue( sInfo.perSize, juce::dontSendNotification );
        mt::SystemResource::getResourceInterface().sizeInfo( sdCardPath, sInfo );
        if( sInfo.status )
        {
            sdcard.setVisible( true );
            sdcard.setValue( sInfo.perSize, juce::dontSendNotification );
        }
        else
        {
            sdcard.setVisible( false );
        }
        mt::SystemResource::getResourceInterface().sizeInfo( usbDiskPath, sInfo );
        if( sInfo.status )
        {
            usb.setVisible( true );
            usb.setValue( sInfo.perSize, juce::dontSendNotification );
        }
        else
        {
            usb.setVisible( false );
        }
    }

    void KSystemResources::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 14 ) );
        g.drawLine( 20.f, 182.f, 237.f, 182.f );
        g.drawLine( 367.f, 180.f, 584.f, 182.f );
    }

    void KSystemResources::resized()
    {
        cpu.setBounds( 90, 10, 425, 76 );

        ram.setBounds( 90, 96, 425, 76 );
        storage.setBounds( 267, 176, 71, 22 );

        flash.setBounds( 90, 199, 425, 76 );
        sdcard.setBounds( 90, 275, 425, 76 );

        usb.setBounds( 90, 357, 425, 76 );

        purgeAll.setBounds( 20, 433, 140, 40 );
        purgeUnused.setBounds( 240, 433, 140, 40 );
        ok.setBounds( 430, 433, 140, 40 );
    }

    void KSystemResources::visibilityChanged()
    {
        auto visible = isVisible();
        cpu.setVisible( visible );
        ram.setVisible( visible );
        storage.setVisible( visible );
        flash.setVisible( visible );
        sdcard.setVisible( visible );
        usb.setVisible( visible );
    }
}
