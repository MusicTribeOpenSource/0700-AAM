#include "kick_HardwareMode.h"
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
#include "kick_FirmwareUpdate.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/dcl/dcl_signals.h"

namespace mt
{

    FirmwareUpdate::FirmwareUpdate()
    {
        hwMessage.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM;
        hwMessage.pktCmd  = contracts::DeviceControlContracts::SysCmd::EN_SYS_SW_UPDATE;
        hwMessage.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    }

    int FirmwareUpdate::connect_ipc( bool reconnect )
    {
        int fd = -1;
#if defined( __linux__ ) || defined( __linux ) || defined( linux )
        DBG( "IN CONNECT_IPC " );
        struct sockaddr_un servaddr;
        fd = socket( AF_LOCAL, SOCK_STREAM, 0 );
        bzero( &servaddr, sizeof( servaddr ) );
        servaddr.sun_family = AF_LOCAL;
        strncpy( servaddr.sun_path, socketpath, sizeof( servaddr.sun_path ) - 1 );

        /*
         * Check to get a valid socket
         */
        if( fd < 0 )
            return -1;

        do
        {
            if( connect( fd, (struct sockaddr*)&servaddr, sizeof( servaddr ) ) == 0 )
            {
                break;
            }
            if( !reconnect )
            {
                DBG( "cannot communicate with SWUpdate via " << socketpath );
                exit( 1 );
            }
            usleep( 1000 );
        } while( true );

        DBG( "Connected to SWUpdate via=" << socketpath );
#endif
        return fd;
    }

    int FirmwareUpdate::progress_ipc_receive( int* connfd, struct progress_msg* msg )
    {
        int ret = -1;
#if defined( __linux__ ) || defined( __linux ) || defined( linux )
        ret = read( *connfd, msg, sizeof( *msg ) );
        DBG( "ret-> " << ret );
        if( ret != sizeof( *msg ) )
        {
            DBG( "Connection closing..\n" );
            close( *connfd );
            *connfd = -1;
            return -1;
        }
#endif
        return ret;
    }

    MCUUpdateAction FirmwareUpdate::getMcuUpdateState( int f_data )
    {
        MCUUpdateAction returnData = MCUUpdateAction::EN_SW_UPDATE_END;
        if( f_data == 0 )
        {
            returnData = MCUUpdateAction::EN_SW_UPDATE_END;
        }
        else if( f_data > 0 )
        {
            returnData = MCUUpdateAction::EN_SW_UPDATE_DATA;
        }
        else if( f_data == -2 )
        {
            returnData = MCUUpdateAction::EN_SW_UPDATE_DATABASE;
        }
        else
        {
            returnData = MCUUpdateAction::EN_SW_UPDATE_END;
        }
        return ( returnData );
    }

    void FirmwareUpdate::updateFirmware()
    {
        if (!hardwareEnabled()) return;
        bool mcuFwStatus = ( isMCUImagePresent() && mt::HwPref::getHwPref().getSurfaceVersion().isNotEmpty() );

#if defined( __linux__ ) || defined( __linux ) || defined( linux )
        if( juce::File( "/tmp/fwupdate/Kick.swu" ).exists() )
        {
            DBG( "In Firmware Upgrade" );

            int status = linuxUpdate();
            if( status != SUCCESS )
            {
                DBG( " Linux FwUpdate SuccessFul" );
                return;
            }
            if( mcuFwStatus )
            {
                DBG( "IN MCU UPDATE NOW" );

                int mcuUpdate = MCUUpdate( mcuFilePath );
                if( mcuUpdate == 1 )
                {
                    DBG( "MCUUPDATE DONE" );
                }
                else if( mcuUpdate == -1 )
                {
                    DBG( "MCUUPDATE FAILED" );
                }
            }
        }
#endif
        DBG( "FirmWare upgrade Successful" );
    }

    int FirmwareUpdate::MCUUpdate( juce::String& McuFilePath )
    {
        if (!hardwareEnabled()) return -1;
        int               SwUpdateSuccess = -1;
        mt::swupdatemsg_t data;

        DBG( "MCU UPDATE" );
        static bool        boSwUpdateStartUpCheck = false;
        static char        binaryData[32]         = { 0 };
        static int         totalRead              = 0;
        static juce::int64 currentPos             = 0;
        static int         hexRecSize             = 43;
        static juce::int64 fileSize               = 0;

        //if(CheckForVersion() == 1)
        {
            if( !boSwUpdateStartUpCheck )
            {
                boSwUpdateStartUpCheck = true;
                //DBG( "First Time" );
                parser.AddPath( McuFilePath );
                fileSize = parser.getSize() / hexRecSize;
                //DBG("FileSize" << fileSize);
                if( fileSize == 0 )
                    jassertfalse;

                currentPos += 1;
                totalProgress    = (int)( 100 * currentPos / fileSize );
                boUpdateComplete = false;
                mcuUpdateState   = MCUUpdateAction::EN_SW_UPDATE_INITIATE;
            }

            while( boDataTransferPending )
            {
                //DBG("Sw Update State Machine");
                switch( mcuUpdateState )
                {
                    ////////////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_INITIATE:
                        ////////////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "Initiate" );
#endif
                            auto hwMsg =
                                contracts::DclContractsHelper::SendSwUpdateCmdMessage( (int)MCUUpdateAction::EN_SW_UPDATE_INITIATE );

                            contracts::DclSignals::sendCommandToDcl( hwMsg );
                            mcuUpdateState = MCUUpdateAction::EN_SW_UPDATE_IDLE;
                            break;
                        }
                    /////////////////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_BEGIN_TRANSFER:
                        /////////////////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "Begin Transfer" );
#endif
                            mcuUpdateState = MCUUpdateAction::EN_SW_UPDATE_ACK;
                        }
                        break;
                    ///////////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_DATA:
                        ///////////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "Data" );
#endif
                            std::vector<int> sendData;
                            sendData.push_back( MCUUpdateAction::EN_SW_UPDATE_DATA );
                            for( unsigned int i = 0; i <= (unsigned int)totalRead; i++ )
                            {
                                sendData.push_back( binaryData[i] );
                            }
                            mcuUpdateState = MCUUpdateAction::EN_SW_UPDATE_IDLE;
                            hwMessage.data = sendData;

                            contracts::DclSignals::createAndSendCommandToDcl( hwMessage );

                            currentPos += 1;
                            // Update the progres bar
                            data.command_status = mt::RECOVERY_STATUS::RUN;
                            data.nsteps         = linuxsteps; //last step
                            data.curstep        = linuxsteps;
                            double _progress    = ( (float)currentPos / (float)fileSize ) * 100;

                            // Calculate the remaining percentage
                            double remainingPercentage = 100.0 - linuxPercentage;

                            // Calculate the remaining percentage based on _progress
                            double remainingFromProgress = remainingPercentage * ( _progress / 100.0 );

                            // Update total progress by adding the remaining percentage from _progress
                            totalProgress = linuxPercentage + remainingFromProgress;

                            data.totalper = totalProgress;

#ifdef DEBUG_FWUPDATE
                            DBG( "Linux Stpes -> " << linuxsteps );
                            DBG( "remaining Percentage" << remainingPercentage );
                            DBG( "remaining From Progress" << remainingFromProgress );
                            DBG( "Total Progress with MCU " << totalProgress );
#endif
                            if( swupdateStatus )
                                swupdateStatus( data );

                            break;
                        }
                    ////////////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_DATABASE:
                        ///////////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "DataBase" );
#endif
                            std::vector<int> sendBaseData;
                            sendBaseData.push_back( MCUUpdateAction::EN_SW_UPDATE_DATABASE );
                            for( unsigned int i = 0; i < 2; i++ )
                            {
                                sendBaseData.push_back( (int)binaryData[i] );
                            }
                            hwMessage.data = sendBaseData;
                            mcuUpdateState = MCUUpdateAction::EN_SW_UPDATE_IDLE;
                            contracts::DclSignals::createAndSendCommandToDcl( hwMessage );
                            break;
                        }
                    ///////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_END:
                        ///////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "SUCCESS -> END" );
#endif
                            std::vector<int> sendBaseData;
                            sendBaseData.push_back( MCUUpdateAction::EN_SW_UPDATE_END );
                            hwMessage.data = sendBaseData;
                            contracts::DclSignals::createAndSendCommandToDcl( hwMessage );
                            boUpdateComplete       = true;
                            boSwUpdateStartUpCheck = false;
                            boDataTransferPending  = false;
                            mcuUpdateState         = MCUUpdateAction::EN_SW_UPDATE_IDLE;
                            SwUpdateSuccess        = 1;
                            data.command_status    = mt::RECOVERY_STATUS::SUCCESS;
                            data.totalper          = 100.0;
                            if( swupdateStatus )
                                swupdateStatus( data );

                            break;
                        }
                    ///////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_ACK:
                        ///////////////////////////////////
                        {
#ifdef DEBUG_FWUPDATE
                            DBG( "Ack" );
#endif
                            totalRead      = parser.readNext( binaryData );
                            mcuUpdateState = getMcuUpdateState( totalRead );

                            break;
                        }
                    ///////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_IDLE:
                        ///////////////////////////////////
                        {
                            //DBG("Idle");
                            break;
                        }
                    ///////////////////////////////////
                    case MCUUpdateAction::EN_SW_UPDATE_FAILURE:
                        ///////////////////////////////////
                        {
                            SwUpdateSuccess        = -1;
                            boSwUpdateStartUpCheck = false;
                            boDataTransferPending  = false;
                            data.command_status    = mt::RECOVERY_STATUS::FAILURE;
                            if( swupdateStatus )
                                swupdateStatus( data );
                            DBG( "Failure" );
                            break;
                        }
                }
            }
        }
        return SwUpdateSuccess;
    }

    void FirmwareUpdate::HexParser::AddPath( const juce::String& filePath )
    {
        fileStream.reset( new juce::FileInputStream( juce::File( filePath ) ) );
        if( !fileStream->openedOk() )
        {
#ifdef PREF_DBG
            DBG( "failed to open firmware file" );
#endif()
        }
    }

    bool FirmwareUpdate::isMCUImagePresent()
    {
        if( juce::File( mcuFilePath ).exists() )
        {
            return true;
        }
        return false;
    }

    int FirmwareUpdate::HexParser::readNext( char redData[] )
    {
        if( !fileStream->getStatus().wasOk() || fileStream->isExhausted() )
            return -1;

        char tmp = fileStream->readByte();
        while( tmp == '\r' || tmp == '\n' )
            tmp = fileStream->readByte();

        if( tmp != ':' )
            jassertfalse;

        // total bytes
        char tempData[5] = { 0 };
        fileStream->read( tempData, 2 );
        int totalDataBytes = juce::String( tempData ).getHexValue32();

        // read two bytes address
        memset( tempData, 0, sizeof( tempData ) );
        fileStream->read( tempData, 2 );
        redData[0] = (char)juce::String( tempData ).getHexValue32();

        memset( tempData, 0, sizeof( tempData ) );
        fileStream->read( tempData, 2 );
        redData[1] = (char)juce::String( tempData ).getHexValue32();

        //RECORD TYPE
        memset( tempData, 0, sizeof( tempData ) );
        fileStream->read( tempData, 2 );
        int recordType = juce::String( tempData ).getHexValue32();

        // No need to read, if not data. end of data
        if( totalDataBytes == 0 && recordType == 1 )
            return -1;

        if( recordType == 4 )
        {
            // read data + checksum to forward the stream to next line
            for( int i = 0; i < totalDataBytes; i++ )
            {
                memset( tempData, 0, sizeof( tempData ) );
                fileStream->read( tempData, 2 );
                redData[i] = (char)juce::String( tempData ).getHexValue32();
            }
            // read checksum
            fileStream->read( tempData, 2 );

            return -2;
        }
        if( recordType != 0 )
        {
            // read data + checksum to forward the stream to next line
            fileStream->read( tempData, totalDataBytes * 2 + 2 );
            return 0;
        }

        for( int i = 0; i < totalDataBytes; i++ )
        {
            memset( tempData, 0, sizeof( tempData ) );
            fileStream->read( tempData, 2 );
            redData[i + 2] = (char)juce::String( tempData ).getHexValue32();
        }
        // read checksum
        fileStream->read( tempData, 2 );

        return totalDataBytes + 2;
    }

    juce::int64 FirmwareUpdate::HexParser::getSize()
    {
        if( !fileStream->getStatus().wasOk() )
            return 0;

        return fileStream->getTotalLength();
    }

    int FirmwareUpdate::linuxUpdate( bool status )
    {
        if (!hardwareEnabled()) return -1;
        int fwstatus = SUCCESS;
        DBG( "IN FIRMWRE UPDATE" );
        swupdatemsg_t data;
        data.imageType               = SwupdateType_t::LINUXUPDATE;
        static double _totalProgress = 0.0;
        bool          mcustaus       = ( isMCUImagePresent() && mt::HwPref::getHwPref().getSurfaceVersion().isNotEmpty() );
        ;
#if defined( __linux__ ) || defined( __linux ) || defined( linux )
        DBG( "Firmware update confirmed by user... Startting upgrade script..." );
        int                 connfd = -1;
        struct progress_msg msg;

        std::string filePath = "/tmp/fwupdate/Kick.swu -v &";
        std::string command  = "/usr/bin/swupdate" + std::string( " -i " ) + filePath;

        std::future<void> future = std::async( std::launch::async, [&command] {
            // Execute the command
            std::system( command.c_str() );
        } );

        sleep( 1 );
        while( 1 )
        {
            if( juce::File( socketpath ).exists() )
            {
                DBG( "FIle Present" );
                break;
            }
            else
            {
                DBG( "File Not present" );
            }
            sleep( 100 );
        }

        while( 1 )
        {
            if( connfd < 0 )
            {
                connfd = connect_ipc( true );
            }
            /*
              * if still fails, try later
              */
            if( connfd < 0 )
            {
                sleep( 1 );
                continue;
            }

            if( progress_ipc_receive( &connfd, &msg ) == -1 )
            {
                continue;
            }

            /*
             * Be sure that string in message are Null terminated
             */
            if( msg.infolen > 0 )
            {
                if( msg.infolen >= sizeof( msg.info ) - 1 )
                {
                    msg.infolen                      = sizeof( msg.info ) - 1;
                    msg.info[sizeof( msg.info ) - 1] = '\0';
                }
                fprintf( stdout, "INFO : %s\n\n", msg.info );
            }
            msg.cur_image[sizeof( msg.cur_image ) - 1] = '\0';

            data.command_status = msg.status;
            std::strncpy( data.curimage, msg.cur_image, sizeof( data.curimage ) - 1 );
            data.curimage[sizeof( data.curimage ) - 1] = '\0'; // Ensure null-termination

            data.cur_percent = msg.cur_percent;
            if( mcustaus )
            {
                data.nsteps = ( msg.nsteps + 1 );
            }
            else
            {
                data.nsteps = msg.nsteps;
            }
            data.curstep = msg.cur_step;
    #ifdef DEBUG_FWUPDATE
            DBG( "========================END========================" );
            DBG( "msg.cur_percent-> " << juce::String( msg.cur_percent ) );
            DBG( "msg.cur_image-> " << juce::String( msg.cur_image ) );
            DBG( "msg.cur_step-> " << juce::String( msg.cur_step ) );
            DBG( "msg.nsteps-> " << juce::String( msg.nsteps ) );

            DBG( "========================END========================" );
    #endif
            if( msg.status != mt::RECOVERY_STATUS::IDLE )
            {
                // Calculate overall progress
                _totalProgress = ( static_cast<double>( msg.cur_step - 1 ) / data.nsteps ) * 100.0;
                _totalProgress = _totalProgress + ( ( ( static_cast<double>( msg.cur_percent ) / 100.0 ) ) * static_cast<double>( 100.0 / data.nsteps ) );
                data.totalper  = _totalProgress;

                // DBG( "Progress ->  " << juce::String( data.totalper ) );
                if( swupdateStatus )
                {
                    if( msg.status == mt::RECOVERY_STATUS::SUCCESS )
                    {
                        /* if mcu image is not present we need to send file status as failure*/
                        if( mcustaus ) { data.command_status = mt::RECOVERY_STATUS::LINUX_SUCCESS; }
                        else { data.command_status = mt::RECOVERY_STATUS::SUCCESS; }

                        linuxsteps      = data.nsteps;
                        linuxPercentage = _totalProgress;
                        swupdateStatus( data );
                        break;
                    }
                    else if( msg.status == mt::RECOVERY_STATUS::FAILURE )
                    {
                        /* if mcu image is not present we need to send file status as failure*/
                        if( mcustaus ) { data.command_status = mt::RECOVERY_STATUS::FAILURE; }
                        else { data.command_status = mt::RECOVERY_STATUS::LINUX_FAILURE; }

                        swupdateStatus( data );
                        fwstatus = FAILURE;
                        break;
                    }
                    swupdateStatus( data );
                }
            }
        }
#endif
        return fwstatus;
    }

    SwUpdateErrorCode_t FirmwareUpdate::validateImage( bool downloaded )
    {
        SwUpdateMode_t mode   = getSwUpdateMode();
        std::string    fwpath = getFwFilepath( mode );

        if( mode == SwUpdateMode_t::PC )
        {
            if( downloaded )
            {
                std::vector<std::pair<std::string, mt::linuxFwInfo>> usbFiles = mt::HwPref::getHwPref().getSWUFileswithVersion( PCMODE_PATH );
                if( !usbFiles.empty() )
                    fwpath = usbFiles[0].first;
            }
            else
            {
                DBG( "Firmware is not downloaded yet" );
                return SwUpdateErrorCode_t::SW_SUCCESS;
            }
        }

        if( mt::HwPref::getHwPref().decryptopenSSLImage( fwpath ) )
        {
            if( mt::HwPref::getHwPref().extractImage() )
            {
                DBG( "Extarction of Image Success" );
                return SwUpdateErrorCode_t::SW_SUCCESS;
            }
            else
            {
                DBG( "Extarction of Image Failed" );
                return SwUpdateErrorCode_t::IMAGE_EXTRACTION_E;
            }
        }
        else
        {
            DBG( "Image Validation Failed ERROR Code" << juce::String::formatted( "0x%04X", mt::SwUpdateErrorCode_t::IMAGE_DYCRYPTION_E ) );
            return SwUpdateErrorCode_t::IMAGE_DYCRYPTION_E;
        }

        DBG( "IMage Not found in " << fwpath );
        return SwUpdateErrorCode_t::IMAGE_NOTFOUND;
    }

    SwUpdateMode_t FirmwareUpdate::getSwUpdateModeFromString( const juce::String& deviceName )
    {
        if( deviceName == "PC" )
            return SwUpdateMode_t::PC;
        else if( deviceName == "SDCard" )
            return SwUpdateMode_t::SDCARD;
        else if( deviceName == "USBDisk" )
            return SwUpdateMode_t::USBDISK;
        else if( deviceName == "OTA" )
            return SwUpdateMode_t::OTA;
        else
            return SwUpdateMode_t::NONE;
    }

    SwUpdateErrorCode_t FirmwareUpdate::setSwUpdateMode( SwUpdateMode_t id )
    {
        switch( id )
        {
            case SwUpdateMode_t::SDCARD:
                DBG( "SDCARD MODE IS SELECTED" );
                break;
            case SwUpdateMode_t::USBDISK:
                DBG( "USBDISK MODE IS SELECTED" );
                break;
            case SwUpdateMode_t::PC:
                DBG( "PC MODE IS SELECTED" );
                break;
            default:
                DBG( "Invalid SwUpdate Mode" );
                return mt::SwUpdateErrorCode_t::SW_FAILED;
        }
        swUpdateMode = id;
        return mt::SwUpdateErrorCode_t::SW_SUCCESS;
    }

    std::string FirmwareUpdate::getFwFilepath( SwUpdateMode_t id )
    {
        switch( id )
        {
            case SwUpdateMode_t::SDCARD:
                DBG( "SDCARD MODE IS SELECTED" );
                if( !sd_card_firmware_files.empty() )
                    return sd_card_firmware_files[0].first; // Assuming the first file in the vector is the desired one
                else
                    return "";
            case SwUpdateMode_t::USBDISK:
                DBG( "USBDISK MODE IS SELECTED" );
                if( !usb_firmware_files.empty() )
                    return usb_firmware_files[0].first; // Assuming the first file in the vector is the desired one
                else
                    return "";
            case SwUpdateMode_t::PC:
                return PCMODE_PATH;
            default:
                DBG( "Invalid SwUpdate Mode" );
                return "";
        }
    }

    std::string FirmwareUpdate::getFwFileVersion( SwUpdateMode_t id )
    {
        switch( id )
        {
            case SwUpdateMode_t::SDCARD:
                DBG( "SDCARD MODE IS SELECTED" );
                if( !sd_card_firmware_files.empty() )
                {
                    return "V" + std::to_string( sd_card_firmware_files.at( 0 ).second.major ) + "." + std::to_string( sd_card_firmware_files.at( 0 ).second.minor ) + "." + std::to_string( sd_card_firmware_files.at( 0 ).second.patch );
                }
                else
                {
                    return "";
                }
            case SwUpdateMode_t::USBDISK:
                DBG( "USBDISK MODE IS SELECTED" );
                if( !usb_firmware_files.empty() )
                {
                    return "V" + std::to_string( usb_firmware_files.at( 0 ).second.major ) + "." + std::to_string( usb_firmware_files.at( 0 ).second.minor ) + "." + std::to_string( usb_firmware_files.at( 0 ).second.patch );
                }
                else
                {
                    return "";
                }
            case SwUpdateMode_t::PC:
                DBG( "PC MODE IS SELECTED" );
                return PCMODE_PATH; // Assuming PCMODE_PATH is a string constant representing the PC mode path
            default:
                DBG( "Invalid SwUpdate Mode" );
                return "";
        }
    }
    VersionComparisonResult FirmwareUpdate::compareVersion( std::string filePath )
    {
        std::string path = "/etc/";
        linuxFwInfo linuxinfo_f, linuxinfo;

        if( !HwPref::getHwPref().getLinuxVersion( path, linuxinfo ) )
        {
            DBG( "failed to get current fwversion" );
            return VersionComparisonResult::Error;
        }
        if( HwPref::getHwPref().getVersionfromFile( filePath, linuxinfo_f ) )
        {
            DBG( "Got the version from file" );
        }
        else
        {
            return VersionComparisonResult::Error;
        }
        return HwPref::getHwPref().isLinuxFirmwareVersionLess( linuxinfo, linuxinfo_f );
    }

    VersionComparisonResult FirmwareUpdate::compareVersion( std::string filePath, mt::linuxFwInfo finfo )
    {
        std::string path = "/etc/";
        linuxFwInfo linuxinfo_f, linuxinfo;

        if( !HwPref::getHwPref().getLinuxVersion( path, linuxinfo ) )
        {
            DBG( "failed to get current fwversion" );
            return VersionComparisonResult::Error;
        }
        return HwPref::getHwPref().isLinuxFirmwareVersionLess( linuxinfo, finfo );
    }
    std::string FirmwareUpdate::getPcSwVersion()
    {
        return pcFwVersion;
    }

    void FirmwareUpdate::setPcSwVersion( std::string version )
    {
        pcFwVersion = version;
    }

    void FirmwareUpdate::startFirmwareScan()
    {
        DBG( "Scannning firmware" );
        pcFwVersion.clear();
        usb_firmware_files.clear();
        sd_card_firmware_files.clear();
        pcAppStatus = false;
        // Start firmware scan on USB drives
        usb_firmware_files = scanFirmwareOnUSBDrives();

        // Start firmware scan on SD cards
        sd_card_firmware_files = scanFirmwareOnSDCards();
        // Start firmware scan in PC app
        pcAppStatus = scanFirmwareOnPCApp();
    }

    bool FirmwareUpdate::isFirmwarePresent( SwUpdateMode_t mode )
    {
        switch( mode )
        {
            case SwUpdateMode_t::SDCARD:
                if( !sd_card_firmware_files.empty() )
                    return true; // Assuming the first file in the vector is the desired one
            case SwUpdateMode_t::USBDISK:
                DBG( "USBDISK MODE IS SELECTED" );
                if( !usb_firmware_files.empty() )
                    return true; // Assuming the first file in the vector is the desired one
            case SwUpdateMode_t::PC:
                DBG( "PC MODE IS SELECTED" );
                if( !pcFwFile.empty() )
                    return true;
            default:
                DBG( "Invalid SwUpdate Mode" );
                return false;
        }
        return false;
    }

    bool FirmwareUpdate::scanFirmwareOnPCApp()
    {
        bool status = false;
        for( int i = 0; i < 6; i++ )
        {
            status = mt::USBFunctionFS::getUSBInstance().getStatus();
            if( status )
            {
                DBG( "iTS CONNENCTED" );
                return status;
            }
#if defined( __linux__ ) || defined( __linux ) || defined( linux )
            sleep( 1 );
#endif
        }
        DBG( "iTS not CONNENCTED" );
        return status;
    }

    std::vector<std::pair<std::string, mt::linuxFwInfo>> FirmwareUpdate::scanFirmwareOnUSBDrives()
    {
        std::vector<std::pair<std::string, mt::linuxFwInfo>> swuFiles;

        // Validate USB disk path
        if( !fs::exists( USBDISK_PATH ) || !fs::is_directory( USBDISK_PATH ) )
        {
            DBG( "Invalid USB disk path: " << USBDISK_PATH );
            return swuFiles;
        }

        // Get firmware files with version information from USB disk
        std::vector<std::pair<std::string, mt::linuxFwInfo>> allFiles = mt::HwPref::getHwPref().getSWUFileswithVersion( USBDISK_PATH );

        // Check if no firmware files were found
        if( allFiles.empty() )
        {
            DBG( "No firmware files found on USB drives." );
            return swuFiles;
        }

        // Iterate through firmware files and compare versions
        for( const auto& file : allFiles )
        {
            try
            {
                // Compare the version of the firmware file
                mt::VersionComparisonResult comparisonResult = mt::FirmwareUpdate::compareVersion( file.first, file.second );

                // Handle the result of the version comparison
                if( comparisonResult == mt::VersionComparisonResult::Less )
                {
                    DBG( "Found higher version: " << file.first );
                    swuFiles.push_back( file );
                }
                else if( comparisonResult == mt::VersionComparisonResult::Equal )
                {
                    DBG( "Already on updated version: " << file.first );
                }
                else
                {
                    DBG( "Found lower version: " << file.first );
                }
            } catch( const std::exception& e )
            {
                DBG( "Error comparing versions: " << e.what() );
                // Handle the error (e.g., continue iteration, log the error, or throw an exception)
            }
        }

        // Return the vector of firmware files with higher versions
        return std::move( swuFiles );
    }
    std::vector<std::pair<std::string, mt::linuxFwInfo>> FirmwareUpdate::scanFirmwareOnSDCards()
    {
        std::vector<std::pair<std::string, mt::linuxFwInfo>> swuFiles;

        // Validate USB disk path
        if( !fs::exists( SDCARD_PATH ) || !fs::is_directory( SDCARD_PATH ) )
        {
            DBG( "Invalid SDCARD disk path: " << SDCARD_PATH );
            return swuFiles;
        }

        // Get firmware files with version information from USB disk
        std::vector<std::pair<std::string, mt::linuxFwInfo>> allFiles = mt::HwPref::getHwPref().getSWUFileswithVersion( SDCARD_PATH );

        // Check if no firmware files were found
        if( allFiles.empty() )
        {
            DBG( "No firmware files found on SDcard drives." );
            return swuFiles;
        }

        // Iterate through firmware files and compare versions
        for( const auto& file : allFiles )
        {
            try
            {
                // Compare the version of the firmware file
                mt::VersionComparisonResult comparisonResult = mt::FirmwareUpdate::compareVersion( file.first, file.second );

                // Handle the result of the version comparison
                if( comparisonResult == mt::VersionComparisonResult::Less )
                {
                    DBG( "Found higher version: " << file.first );
                    swuFiles.push_back( file );
                }
                else if( comparisonResult == mt::VersionComparisonResult::Equal )
                {
                    DBG( "Already on updated version: " << file.first );
                }
                else
                {
                    DBG( "Found lower version: " << file.first );
                }
            } catch( const std::exception& e )
            {
                DBG( "Error comparing versions: " << e.what() );
                // Handle the error (e.g., continue iteration, log the error, or throw an exception)
            }
        }

        // Return the vector of firmware files with higher versions
        return std::move( swuFiles );
    }
#if 0
    std::vector<std::pair<std::string, mt::linuxFwInfo>> FirmwareUpdate::scanFirmwareOnUSBDrives()
    {
        std::vector<std::pair<std::string, mt::linuxFwInfo>> swuFiles;
        std::vector<std::pair<std::string, mt::linuxFwInfo>> allFiles = mt::HwPref::getHwPref().getSWUFileswithVersion( USBDISK_PATH );
        for( const auto& file : allFiles )
        {
            mt::VersionComparisonResult comparisonResult = mt::FirmwareUpdate::compareVersion( file.first, file.second );
            if( comparisonResult == mt::VersionComparisonResult::Less )
            {
                DBG( "Got Higher Version so pushing to vector -> " << file.first );
                swuFiles.push_back( file );
            }
            else
            {
                DBG( "already on updated version" );
            }
        }
        return swuFiles;
    }

    std::vector<std::pair<std::string, linuxFwInfo>> FirmwareUpdate::scanFirmwareOnSDCards()
    {
        std::vector<std::pair<std::string, mt::linuxFwInfo>> swuFiles;
        std::vector<std::pair<std::string, mt::linuxFwInfo>> allFiles = mt::HwPref::getHwPref().getSWUFileswithVersion( SDCARD_PATH );
        for( const auto& file : allFiles )
        {
            mt::VersionComparisonResult comparisonResult = mt::FirmwareUpdate::compareVersion( file.first, file.second );
            if( comparisonResult == mt::VersionComparisonResult::Less )
            {
                DBG( "Version is higher so pushing to vector -> " << file.first );
                swuFiles.push_back( file );
            }
            else
            {
                DBG( "already on updated version" );
            }
        }
        return swuFiles;
    }
#endif
}
