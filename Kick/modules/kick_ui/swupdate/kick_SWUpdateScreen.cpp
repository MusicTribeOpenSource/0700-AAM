#include "kick_SWUpdateScreen.h"
#if KICK_EMU
    #include "kick_emu_hal.h"
#endif
namespace kick_ui
{
    SWUpdateScreen::SWUpdateScreen()
        : kick_ui::IScreen( kick_ui::Screen::SWUpdateWindow )
    {
        mt::utils::addAndMakeVisible( this, updateLabel, restartLabel, poweroffLabel, stepProgressbar, overallProgressbar, backBtn, logo, stepLabel, overallLabel, header, logo1 );

        updateLabel.setValueJustification( juce::Justification::centred );
        restartLabel.setValueJustification( juce::Justification::centred );
        poweroffLabel.setValueJustification( juce::Justification::centred );
        header.showOptions( false, false );
        header.showBackButton( false );
        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };
        updateLabel.setValueColour( ui::Theme::textColour( 2 ) );
        restartLabel.setValueColour( ui::Theme::textColour( 1 ) );
        poweroffLabel.setValueColour( ui::Theme::textColour( 2 ) );
        mt::utils::setValueFontHeight( 20, updateLabel, poweroffLabel );
        updateLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        restartLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        poweroffLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        updateLabel.setFontHeight( 20 );
        restartLabel.setFontHeight( 18 );
        poweroffLabel.setFontHeight( 20 );

        stepLabel.setFontHeight( 18 );
        overallLabel.setFontHeight( 18 );

        stepLabel.setValueJustification( juce::Justification::centredTop );
        overallLabel.setValueJustification( juce::Justification::centredTop );

        stepLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        overallLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        stepLabel.setValueColour( ui::Theme::textColour( 1 ) );
        overallLabel.setValueColour( ui::Theme::textColour( 1 ) );

        backBtn.onClick = [&]() {
            closeScreen();
        };
        stepProgressbar.setVisible( false );
        stepLabel.setVisible( false );
        setSwUpdateCallback();
        setUsbCallback();
        startTimerHz( 10 );
    }

    void SWUpdateScreen::paint( juce::Graphics& g )
    {
        // g.setColour( ui::Theme::backgroundColour( 1 ) );
        // g.fillAll();
        mt::utils::fillRoundedRect( g, centreBounds, ui::Theme::backgroundColour( 1 ), 6.0f, false, false, true, true );
    }

    void SWUpdateScreen::visibilityChanged()
    {
        setUsbCallback();
        _overallLabel      = "";
        _overallpercentage = 0.0;
        if( isVisible() )
        {
            DBG( "SWUPDATE SCREEN IS VISIBILE" );
            if( mt::FirmwareUpdate::getSwupdateInstance().getSwUpdateMode() == mt::SwUpdateMode_t::PC )
            {
                stepProgressbar.setVisible( false );
                _overallLabel = "Waiting for connection";
                // mt::USBFunctionFS::getUSBInstance().start();
                DBG( "its in PC Mode" );
            }
            else
            {
                std::thread( [&]() {
                    mt::FirmwareUpdate::getSwupdateInstance().updateFirmware();
                } ).detach();
            }
        }
    }

    void SWUpdateScreen::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        logo1.setBounds( 410, 100, 186, 30 );
        logo.setBounds( b.removeFromTop( 153 ).withTrimmedTop( 100 ) );
        updateLabel.setBounds( b.removeFromTop( 80 ).withTrimmedTop( 25 ) );
        poweroffLabel.setBounds( b.removeFromTop( 25 ).withTrimmedTop( 5 ) );
        restartLabel.setBounds( b.removeFromTop( 50 ).withTrimmedTop( 10 ) );

        stepProgressbar.setBounds( 300, 320, 425, 40 );
        stepLabel.setBounds( b.removeFromTop( 20 ) );
        overallProgressbar.setBounds( b.removeFromTop( 130 ).withTrimmedTop( 20 ) );
        overallLabel.setBounds( b.removeFromTop( 20 ) );
        //  backBtn.setBounds( 10, 556, 120, 44 );
    }

    void SWUpdateScreen::timerCallback()
    {
        overallProgressbar.updateProgress( _overallpercentage );
        overallLabel.setText( _overallLabel, juce::dontSendNotification );
    }

    void SWUpdateScreen::getSwupdateStatus( mt::swupdatemsg_t& data )
    {
        if( data.command_status == mt::RECOVERY_STATUS::RUN )
        {
            _overallpercentage = data.totalper;
            _step_percentage   = data.cur_percent;
            _overallLabel      = juce::String( data.curstep ) + "/" + juce::String( data.nsteps );
        }
        else if( data.command_status == mt::RECOVERY_STATUS::LINUX_SUCCESS )
        {
            _overallpercentage = data.totalper;
            _step_percentage   = data.cur_percent;
            _overallLabel      = juce::String( data.curstep ) + "/" + juce::String( data.nsteps );
        }
        else if( data.command_status == mt::RECOVERY_STATUS::SUCCESS )
        {
            _overallpercentage = 100.0;
            DBG( "Firmware Upgrade SUCCESS " );
            auto warningPopup = new ui::FilePopup( "Firmware Upgrade SUCCESS! Do you want to reboot a device ?", false, true, true );
            mt::utils::showDialog( "SUCCESS!", warningPopup, [&]( int R ) {
                if( R == UI::APPLY )
                {
                    #if KICK_EMU
                    if( kick_emu::shell( "reboot -nf" ) == 0 )
#else
                    if( system( "reboot -nf" ) == 0 )
#endif
                        DBG( "Rebooting the device so new firmware will apply" );
                }
                else if( R == UI::CANCEL )
                    closeScreen();
                if( warningPopup ) delete warningPopup;
            } );
        }
        else if( data.command_status == mt::RECOVERY_STATUS::FAILURE || data.command_status == mt::RECOVERY_STATUS::LINUX_FAILURE )
        {
            auto warningPopup = new ui::FilePopup( "Firmware Update Failed", true, false, false );
            mt::utils::showDialog( "WARNING!", warningPopup, [&]( int R ) {
                if( R == UI::CANCEL )
                    closeScreen();
                if( warningPopup ) delete warningPopup;
            } );
            mt::USBFunctionFS::getUSBInstance().stop();
            DBG( "Firmware Upgrade FAILED" );
            header.showBackButton( true );
        }
    }

    void SWUpdateScreen::getUsbupdateStatus( mt::usbmsg_t& data )
    {
        if( data.command_status == mt::USB_STATUS::U_INIT )
        {
            DBG( "INIT" );
            _overallLabel      = "Waiting for connection";
            _overallpercentage = 0.0;
            DBG( "Firmware is Getting Download Done " << data.totalfilesize );
        }
        else if( data.command_status == mt::USB_STATUS::U_DOWNLOAD )
        {
            DBG( "DOWNLOAD" );
            DBG( "Firmware is Getting Download Done " << data.downloadfilesize );
            DBG( "percentage " << data.percentage );
            _overallpercentage = data.percentage;
            _overallLabel      = "Downloading ..";
        }
        else if( data.command_status == mt::USB_STATUS::U_DONE )
        {
            _overallLabel = "Download done..";
            DBG( "USB Download DONE<<" << juce::String( data.percentage ) );
            _overallLabel      = "Validating Image....";
            _overallpercentage = 0;
            mt::USBFunctionFS::getUSBInstance().stop();
            mt::SwUpdateErrorCode_t status = mt::FirmwareUpdate::getSwupdateInstance().validateImage( true );
            if( status == mt::SwUpdateErrorCode_t::SW_FAILED )
            {
                _overallLabel = "Image Validation failed";
                DBG( "IMAGE VALIDATION FAILED" );
            }
            else if( status == mt::SwUpdateErrorCode_t::IMAGE_EXTRACTION_E || mt::SwUpdateErrorCode_t::IMAGE_EXTRACTION_E )
            {
                _overallLabel = "InValid Image..";
            }
            if( status == mt::SwUpdateErrorCode_t::SW_SUCCESS )
            {
#if 0
                mt::VersionComparisonResult currentver = mt::FirmwareUpdate::getSwupdateInstance().compareVerison();
                if( currentver == mt::VersionComparisonResult::Equal ) //both image version is same
                {
                    DBG( "You are already updated image" );
                }
                else if( currentver == mt::VersionComparisonResult::Greater ) //current version is higher then image placed
                {
                    DBG( "Sw version is older then current version" );
                }
                else if( currentver == mt::VersionComparisonResult::Error ) //current version is higher then image placed
                {
                    DBG( "Failed to get SwVersion" );
                }
                else
                {
#endif
                DBG( "Firmware Image Varification successFul" );
                std::thread( [&]() {
                    mt::FirmwareUpdate::getSwupdateInstance().updateFirmware();
                } ).detach();
                //}
            }
        }
        else if( data.command_status == mt::USB_STATUS::U_FAILED )
        {
            DBG( "Firmware Download FAILED" );
            _overallLabel      = "Download done..";
            _overallpercentage = 0.0;
            mt::USBFunctionFS::getUSBInstance().stop();
        }
        else if( data.command_status == mt::USB_STATUS::U_TIMEOUT )
        {
            DBG( "U_TIMEOUT" );
            _overallLabel      = "Connection Timed Out";
            _overallpercentage = 0.0;
            mt::USBFunctionFS::getUSBInstance().stop();
            closeScreen();
            //add popup with timeout msg;
        }
        else
        {
            DBG( "UNKONWN COMMAND" );
        }
    }

    void SWUpdateScreen::handleCommand( CPktMgr::HWMsg_t& f_hwMsg )
    {
        mt::MCUUpdateAction flag = mt::MCUUpdateAction::EN_SW_UPDATE_FAILURE;
        if( contracts::DclContractsHelper::isSoftwareUpdateMessage( f_hwMsg ) )
        {
#ifdef DEBUG_FWUPDATE
            DBG( "SwUtility - Handle Command Enter" );
#endif

            if( f_hwMsg.data[0] == 1 )
            {
                flag = mt::MCUUpdateAction::EN_SW_UPDATE_BEGIN_TRANSFER;
#ifdef DEBUG_FWUPDATE
                DBG( "Begin Transfer" );
#endif
            }
            else if( f_hwMsg.data[0] == 6 )
            {
                flag = mt::MCUUpdateAction::EN_SW_UPDATE_FAILURE;
            }
            else
            {
                flag = mt::MCUUpdateAction::EN_SW_UPDATE_ACK;
#ifdef DEBUG_FWUPDATE
                DBG( "Send Ack" );
#endif
            }
            mt::FirmwareUpdate::getSwupdateInstance().setMcuUpdateState( flag );
        }
    }
}
