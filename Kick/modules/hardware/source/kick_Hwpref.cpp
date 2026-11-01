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

#include "kick_Hwpref.h"

#if KICK_EMU
    #include "kick_emu_hal.h"
    // The emulator never runs shell commands; it logs them and answers success.
    #define KICK_SYSTEM( cmd ) kick_emu::shell( cmd )
#else
    #define KICK_SYSTEM( cmd ) std::system( cmd )
#endif

namespace mt
{
    namespace fs = std::filesystem;

    const char* FACTORY_RESET_CMD = "/bin/rm -rf /home/Kick/projects* /home/Kick/prefs";
    const char* POWEROFF          = "/sbin/poweroff";

    HwPref::HwPref()
        : juce::Thread( "powerbutton" )
    {
        DBG( "HwPref Constructor called" );
        //startThread();
    }

    void HwPref::run()
    {
        const char* devicePath = "/dev/input/event2"; // Replace X with the appropriate event number

        while( !threadShouldExit() )
        {
#if defined( __linux__ ) || defined( __linux ) || defined( linux )
            if( !inputDevice.is_open() ) // Check if inputDevice is not valid
            {
                // Open the input device file
                inputDevice.open( devicePath, std::ios::binary );
                if( !inputDevice.is_open() )
                {
                    DBG( "Error opening Input device" );
                    // Handle the error (e.g., return, continue loop, or retry opening)
                    continue; // Continue the loop if opening failed
                }
            }

            // Read the input event
            struct input_event ev;
            if( inputDevice.read( reinterpret_cast<char*>( &ev ), sizeof( struct input_event ) ) )
            {
                // Process the input event
                if( ev.type == EV_KEY && ev.code == KEY_STOP )
                {
                    if( ev.value == 1 )
                    {
                        DBG( "KEY_STOP pressed" );
                    }
                    else if( ev.value == 0 )
                    {
                        DBG( "KEY_STOP released" );
                        if( powerButtonUI )
                            powerButtonUI();
                    }
                }
            }
            else
            {
                continue; // Continue the loop if reading failed
            }
#endif
            juce::Thread::sleep( 300 );
        }
    }

    std::string HwPref::executeCommand( const std::string& command )
    {
        std::string result = "";
#if( JUCE_LINUX || __linux__ )
        char buffer[128];

        FILE* pipe = popen( command.c_str(), "r" );
        if( !pipe )
        {
            return "Error";
        }

        while( !feof( pipe ) )
        {
            if( fgets( buffer, 128, pipe ) != nullptr )
            {
                result += buffer;
            }
        }

        pclose( pipe );
#endif
        return result;
    }

    int HwPref::executeSetCommand( const std::string& command )
    {
        int result = 0;
#if KICK_EMU || JUCE_LINUX || __linux__
        result = KICK_SYSTEM( command.c_str() );
#endif
        return result;
    }

    int HwPref::startDhcp( void )
    {
	    DBG("Starting DHCP");
        std::string command = "/usr/sbin/udhcpc -i eth0 &";
        return executeSetCommand( command.c_str() );
    }

    int HwPref::stopDhcp( void )
    {
	    DBG("Stoping DHCP");
        std::string command = "killall udhcpc";
        return executeSetCommand( command.c_str() );
    }

    int HwPref::factoryReset( void )
    {
        return executeSetCommand( FACTORY_RESET_CMD );
    }

    int HwPref::shutdown( void )
    {
        return executeSetCommand( POWEROFF );
    }

    // Function to compare firmware versions
    mt::VersionComparisonResult HwPref::isLinuxFirmwareVersionLess( const mt::linuxFwInfo& current, const mt::linuxFwInfo& stored )
    {
        if( current.major < stored.major )
        {
            return mt::VersionComparisonResult::Less;
        }
        else if( current.major == stored.major )
        {
            if( current.minor < stored.minor )
            {
                return mt::VersionComparisonResult::Less;
            }
            else if( current.minor == stored.minor )
            {
                if( current.patch < stored.patch )
                {
                    return mt::VersionComparisonResult::Less;
                }
                else if( current.patch == stored.patch )
                {
                    return mt::VersionComparisonResult::Equal;
                }
            }
        }
        return mt::VersionComparisonResult::Greater;
    }

    // Function to compare mcu versions
    mt::VersionComparisonResult HwPref::isMCUFirmwareVersionLess( const mt::mcuFwInfo& current, const mt::mcuFwInfo& stored )
    {
        if( current.major < stored.major )
        {
            return mt::VersionComparisonResult::Less;
        }
        else if( current.major == stored.major )
        {
            if( current.minor < stored.minor )
            {
                return mt::VersionComparisonResult::Less;
            }
            else if( current.minor == stored.minor )
            {
                return mt::VersionComparisonResult::Equal;
            }
        }
        return mt::VersionComparisonResult::Greater;
    }

    std::string HwPref::isImageFound( juce::String Path, juce::String format )
    {
        DBG( "isImageFound checking in Folder path " << Path << "Format of file " << format );
        std::vector<std::string> swuFiles = getSWUFiles( Path.toStdString(), format.toStdString() );
        if( swuFiles.empty() )
        {
            DBG( "No SWU files found in the folder." );
            return "";
        }
        else
        {
            DBG( "SWU Files in the folder:" );
            for( const auto& swuFile : swuFiles )
            {
                DBG( swuFile );
            }
        }
        fwFileName     = swuFiles.at( 0 ); // taking whichever is first
        fwFileFullpath = Path.toStdString() + "/" + swuFiles.at( 0 );
        DBG( "FWFile Name -> " << fwFileName );
        DBG( "FWFile Name -> " << fwFileFullpath );
        return fwFileFullpath;
    }

    bool HwPref::extractImage( std::string path )
    {
        bool        status         = true;
        std::string imageFormat    = ".swu";
        std::string creatFolderCmd = "mkdir " + path;
        DBG( "FwUpdate ->extractImage->  " << creatFolderCmd );
        KICK_SYSTEM( creatFolderCmd.c_str() );
        std::string outFullpath = decrptionDir + decryptedFwname;
        std::string cmd         = "mount -o loop -t ext4  " + outFullpath + " " + extractDir;
        DBG( "FwUpdate ->extractImage->  " << cmd );
        KICK_SYSTEM( cmd.c_str() );
        std::vector<std::string> swuFiles = getSWUFiles( extractDir, imageFormat );
        if( swuFiles.empty() )
        {
            DBG( "FwUpdate -> After exraction image file In " << extractDir << " Is not Found" );
            return false;
        }
        else
        {
            DBG( "FwUpdate -> After exraction image file In " << extractDir << " Is Found" );
            for( const auto& swuFile : swuFiles )
            {
                DBG( swuFile );
            }
        }
        return status;
    }
    juce::String HwPref::getDevSerial()
    {
        std::string serialNo = "000000000";
#if KICK_EMU
        std::string path = kick_emu::devicePath( "/sys/devices/soc0/serial_number" );
        serialNo         = "KICKEMU0001";
#elif defined( __linux__ ) || defined( __linux ) || defined( linux )
        std::string path = "/sys/devices/soc0/serial_number";

        std::ifstream serialFile( path.c_str() );
        if( serialFile.is_open() )
        {
            serialFile >> serialNo;
        }
#endif
        return juce::String( serialNo );
    }

    juce::String HwPref::getFirmwareVersion()
    {
        std::string     fwVersion;
        std::string     path = "/etc/";
        mt::linuxFwInfo linfo;

        getLinuxVersion( path, linfo );
        juce::String FwVersion = juce::String::formatted( "%d.%d.%d",linfo.major, linfo.minor, linfo.patch );
        return FwVersion;
    }

    bool HwPref::getLinuxVersion( std::string& path, mt::linuxFwInfo& linfo )
    {
        std::string   filePath = path + "version.txt";
        std::ifstream file( filePath );

        if( !file.is_open() )
        {
            DBG( "FwUpdate -> getFwVersion Error opening file: " << filePath );
            return false;
        }

        std::string line;

        // Read the first line for Linux firmware information
        if( std::getline( file, line ) )
        {
            // Define a regular expression for parsing Linux firmware information
            std::regex  linuxRegex( "^([a-zA-Z]+) (\\d+)\\.(\\d+)\\.(\\d+)$" );
            std::smatch match;

            // Try to match the line against the regular expression
            if( std::regex_match( line, match, linuxRegex ) )
            {
                linfo.linuxname = match[1];
                linfo.major     = std::stoi( match[2] );
                linfo.minor     = std::stoi( match[3] );
                linfo.patch     = std::stoi( match[4] );
                return true;
                // Display parsed information
            }
            else
            {
                DBG( "Error parsing Linux firmware information. Invalid format." );
                file.close();
                return false;
            }
        }
        else
        {
            DBG( "Error reading Linux firmware information from file." );
            file.close();
            return false;
        }
    }

    bool HwPref::getFwVersion( std::string& path, mt::linuxFwInfo& linfo, mt::mcuFwInfo& minfo )
    {
        std::string   filePath = path + "version.txt";
        std::ifstream file( filePath );

        if( !file.is_open() )
        {
            DBG( "FwUpdate -> getFwVersion Error opening file: " << filePath );
            return false;
        }

        std::string line;

        // Read the first line for Linux firmware information
        if( std::getline( file, line ) )
        {
            // Define a regular expression for parsing Linux firmware information
            std::regex  linuxRegex( "^([a-zA-Z]+) (\\d+)\\.(\\d+)\\.(\\d+)$" );
            std::smatch match;

            // Try to match the line against the regular expression
            if( std::regex_match( line, match, linuxRegex ) )
            {
                linfo.linuxname = match[1];
                linfo.major     = std::stoi( match[2] );
                linfo.minor     = std::stoi( match[3] );
                linfo.patch     = std::stoi( match[4] );

                // Display parsed information
            }
            else
            {
                DBG( "Error parsing Linux firmware information. Invalid format." );
                file.close();
                return false;
            }
        }
        else
        {
            DBG( "Error reading Linux firmware information from file." );
            file.close();
            return false;
        }

        // Read the second line for MCU firmware information
        if( std::getline( file, line ) )
        {
            // Define a regular expression for parsing MCU firmware information
            std::regex  mcuRegex( "^([a-zA-Z]+) (\\d+)\\.(\\d+)$" );
            std::smatch mcuMatch;

            // Try to match the line against the regular expression
            if( std::regex_match( line, mcuMatch, mcuRegex ) )
            {
                minfo.mcuname = mcuMatch[1];
                minfo.major   = std::stoi( mcuMatch[2] );
                minfo.minor   = std::stoi( mcuMatch[3] );

                // Display parsed information
            }
            else
            {
                DBG( "Error parsing MCU firmware information. Invalid format." );
                file.close();
                return false;
            }
        }
        else
        {
            DBG( "Error reading MCU firmware information from file." );
            file.close();
            return false;
        }

        file.close();
        return true;
    }

    bool HwPref::addVersionNotoFile( std::string filename )
    {
        return true;
    }

    bool HwPref::getVersionfromFile( std::string filename, linuxFwInfo& info )
    {
        fw_ver_index = -1;
        // Open the encrypted file
        std::ifstream encrypted_file( filename, std::ios::binary );
        if( !encrypted_file )
        {
            DBG( "Error opening encrypted filei" );
            return false;
        }

        // Define buffer to store the version information
        char version_buffer[20]; // 20 characters for version, 1 for null terminator

        // Read the version information from the file
        if( !encrypted_file.read( version_buffer, 20 ) )
        {
            DBG( "Error reading version information." );
            return false;
        }

        // Close the file
        encrypted_file.close();

        for( auto i = 0; i < 20; i++ )
            if( ( version_buffer[i] == 'E' ) || ( version_buffer[i] == 'R' ) )
                fw_ver_index = ( i + 2 );

        if( fw_ver_index < 0 )
        {
            DBG( "Version is not correct" );
            return false;
        }
        version_buffer[fw_ver_index] = '\0';

        // Parse the version buffer and fill the linuxFwInfo structure
        std::istringstream version_stream( std::string( version_buffer, fw_ver_index ) );

        if( !( version_stream >> info.major ) )
        {
            DBG( "Error parsing major version." );
            return false;
        }

        if( version_stream.get() != '.' || !( version_stream >> info.minor ) )
        {
            DBG( "Error parsing minor version." );
            return false;
        }

        if( version_stream.get() != '.' || !( version_stream >> info.patch ) )
        {
            DBG( "Error parsing patch version." );
            return false;
        }

        // Output the version information
        DBG( "Version from future Update File is=" << filename << ":" << info.major << "." << info.minor << "." << info.patch );
        return true;
    }

    bool HwPref::isImgFile( std::string filepath )
    {
        // Check if the file extension is ".img"
        return fs::path( filepath ).extension() == ".img";
    }

    bool HwPref::removeHeaderFromImage( std::string fwfilepath )
    {
        std::string cmd = "tail -c +" + std::to_string( fw_ver_index ) + " " + fwfilepath + " > " + decrptionDir + fwHeaderFile;
        DBG( "removing Header from image" << cmd );
        int result = KICK_SYSTEM( cmd.c_str() ); // Execute the command using system call

	if ( result != 0 )
    {
        DBG( "Failed to remove header from image." );
        return false;
    }

    // Now remove the original file
    if ( std::remove( fwfilepath.c_str() ) != 0 )
    {
        DBG( "Error deleting original file: " << fwfilepath );
        return false;
    }
    else
    {
        DBG( "Successfully deleted original file: " << fwfilepath );
    }
    	return true;

    }

    bool HwPref::decryptopenSSLImage( std::string fwfilepath )
    {
        bool        status        = false;
        std::string outFwfilepath = decrptionDir + decryptedFwname;
        std::string inFwfilepath  = decrptionDir + fwHeaderFile;
        std::string cmd           = "mkdir -p " + decrptionDir; // Create decryption directory if not exists
#if( JUCE_LINUX || __linux__ )
        KICK_SYSTEM( cmd.c_str() ); // Execute the command using system call to create directory

        if( isImgFile( fwfilepath ) )
        { // Check if it's an img file
            if( removeHeaderFromImage( fwfilepath ) )
            { // Remove header from image
                std::string command = "openssl enc -aes-256-cbc -pbkdf2 -d -salt -in " + inFwfilepath + " -out " + outFwfilepath + " -k '" + decrptKey + "'";

                std::cout << "Decrypting image: " << command << std::endl;
                int result = KICK_SYSTEM( command.c_str() ); // Execute the command using system call

                if( result == 0 )
                {
                    std::cout << "Decryption successful" << std::endl;
                    return true;
                }
                else
                {
                    std::cout << "Incorrect password or decryption failed" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cout << "Header removal failed" << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << "Not an IMG file" << std::endl;
            return false;
        }
#endif
        return true;
    }
    std::vector<std::pair<std::string, linuxFwInfo>> HwPref::getSWUFileswithVersion( const std::string& folderPath, const std::string& imageFormat )
    {
        std::vector<std::pair<std::string, linuxFwInfo>> swuFiles;
        if( fs::exists( folderPath ) && fs::is_directory( folderPath ) )
        {
            try
            {
                bool foundSWU = false;
                for( const auto& entry : fs::directory_iterator( folderPath ) )
                {
                    if( entry.path().extension() == imageFormat )
                    {
                        std::string fullpath = folderPath + "/" + entry.path().filename().string();
                        linuxFwInfo info;
                        if( getVersionfromFile( fullpath, info ) )
                        {
                            swuFiles.push_back( std::make_pair( fullpath, info ) );
                            DBG( "FwUpdate file -> " << fullpath << " Version: " << info.major << "." << info.minor << "." << info.patch );
                            foundSWU = true;
                        }
                        else
                        {
                            DBG( "Error extracting version information from file: " << entry.path().string() );
                        }
                    }
                }
                if( !foundSWU )
                {
                    DBG( "No SWU files found in the specified directory." );
                }
            } catch( const std::filesystem::filesystem_error& e )
            {
                DBG( "Error accessing directory: " << e.what() );
            }
        }
        else
        {
            DBG( "The path is not valid or does not exist." );
        }
        return swuFiles;
    }

    std::vector<std::string> HwPref::getSWUFiles( const std::string& folderPath, const std::string& imageFormat )
    {
        std::vector<std::string> swuFiles;
        if( fs::exists( folderPath ) && fs::is_directory( folderPath ) )
        {
            try
            {
                for( const auto& entry : fs::directory_iterator( folderPath ) )
                {
                    if( entry.path().extension() == imageFormat )
                    {
                        swuFiles.push_back( entry.path().filename().string() );
                        DBG( "FwUpate files -> " << entry.path().filename().string() );
                    }
                }
            } catch( const std::filesystem::filesystem_error& e )
            {
                DBG( "Error accessing directory: " << e.what() );
            }
        }
        else
        {
            DBG( "The path is not valid or does not exist." );
        }
        return swuFiles;
    }

    bool HwPref::isEthernetCableConnected()
    {
#if( JUCE_LINUX || __linux__ )
        std::string ethtoolOutput = executeCommand( "ethtool eth0 | grep 'Link detected'" );
        return ( ethtoolOutput.find( "yes" ) != std::string::npos );
#endif
        return false;
    }

    bool HwPref::ethernetEnable( bool status )
    {
#if( JUCE_LINUX || __linux__ )
        std::string command = status ? "ifconfig eth0 up" : "ifconfig eth0 down";
        int         result  = KICK_SYSTEM( command.c_str() ); // Execute the command using system call
        if( result == 0 )
        {
            return true; // Command executed successfully
        }
        else
        {
            return false; // Command execution failed
        }
#else
        return true; // Not supported on non-Linux systems
#endif
    }

    bool HwPref::isInterfaceUp()
    {
#if( JUCE_LINUX || __linux__ )
        std::string   path = "/sys/class/net/eth0/operstate";
        std::ifstream file( path );

        if( !file.is_open() )
        {
            std::cerr << "Error: Could not open " << path << std::endl;
            return false;
        }

        std::string state;
        file >> state;
        file.close();

        return state == "up";
#else
        return true;
#endif
    }

    int HwPref::setIP( const std::string& interface, const std::string& ipAddress, const std::string& subnetMask, const std::string& gateway )
    {
        DBG( "SetIP Fucntion Called" );
        int         result  = 0;
        std::string command = "ifconfig " + interface + " " + ipAddress + " netmask " + subnetMask;
	DBG("cmd = " <<command);
#if( JUCE_LINUX || __linux__ )
        result = executeSetCommand( command.c_str() );
#endif
        return result;
    }

    std::string HwPref::getSubnetMask( const std::string& interface )
    {
        std::string subnetMask = "0.0.0.0";
#if( JUCE_LINUX || __linux__ )
        int          sockfd;
        struct ifreq ifr;

        // Create a socket to use for the ioctl call
        sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
        if( sockfd == -1 )
        {
            std::cerr << "Failed to create socket\n";
            return "";
        }

        // Prepare the struct for the ioctl call
        ifr.ifr_addr.sa_family = AF_INET;
        std::strncpy( ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1 );

        // Perform the ioctl to get the subnet mask
        if( ioctl( sockfd, SIOCGIFNETMASK, &ifr ) == -1 )
        {
            std::cerr << "Failed to get subnet mask\n";
            close( sockfd );
            return "";
        }

        // Convert the result to a readable string
        struct sockaddr_in* netmask = reinterpret_cast<struct sockaddr_in*>( &ifr.ifr_netmask );
        subnetMask                  = inet_ntoa( netmask->sin_addr );

        // Close the socket
        close( sockfd );
#endif
        return subnetMask;
    }

    std::string HwPref::getGateway( const std::string& interface )
    {
#if( JUCE_LINUX || __linux__ )
        // Command to retrieve routing information
        std::string command = "ip route show";
        char        buffer[128];
        std::string result = "";

        // Open the command for reading
        FILE* pipe = popen( command.c_str(), "r" );
        if( !pipe )
        {
            std::cerr << "Failed to run command\n";
            return "";
        }

        // Read the output of the command
        while( fgets( buffer, sizeof( buffer ), pipe ) != nullptr )
        {
            result += buffer;
        }

        // Close the pipe
        pclose( pipe );

        // Parse the result to find the line corresponding to the desired interface
        std::istringstream iss( result );
        std::string        line;
        while( std::getline( iss, line ) )
        {
            if( line.find( "default via" ) != std::string::npos && line.find( interface ) != std::string::npos )
            {
                // Extract the gateway IP from the line
                std::istringstream lineStream( line );
                std::string        word;
                lineStream >> word; // Skip "default"
                lineStream >> word; // Skip "via"
                std::string gatewayIP;
                lineStream >> gatewayIP; // Gateway IP

                return gatewayIP;
            }
        }
#endif
        return "0.0.0.0";
    }

    void HwPref::getIP( const std::string interface, sysEthInfo& info )
    {
        DBG( "GetIP Fucntion Called" );
        std::string ipAddress = "0.0.0.0";

        std::string command = "ip -4 addr show " + interface + " | awk '/inet / {split($2, a, \"\\\\/\"); print a[1]}'";
#if( JUCE_LINUX || __linux__ )

        // Get the local IP address
        ipAddress = executeCommand( command );
        info.IP   = ipAddress;

        // Get Ethernet adapter status
        std::string ifconfigOutput = executeCommand( "ifconfig eth0" );
        if( ifconfigOutput.find( "UP" ) != std::string::npos )
        {
            info.status = true;
            DBG( "Ethernet Adapter Status: Operational" );
        }
        else
        {
            info.status = false;
            DBG( "Ethernet Adapter Status: Not Operational" );
        }

        // Get the gateway
        std::string gateway = getGateway( interface );
        DBG( "Gateway: " << gateway );
        info.gateway = gateway;

        // Get the subnet mask
        std::string subnetMask = getSubnetMask( interface );
        info.subnet            = subnetMask;
        DBG( "Subnet Mask: " << subnetMask );
#endif
    }

    int HwPref::getDateTime( dateInfo& date, timeInfo& time, const juce::String& timezone )
    {
        DBG( "GetDate Fucntion Called" );

#if defined( __linux__ ) || defined( __linux ) || defined( linux )
        setenv( "TZ", timezone.toRawUTF8(), 1 );
#endif

        // Use tzset to update the timezone information
        tzset();

        std::time_t currentTime = std::time( nullptr );

        // Convert the time to a struct tm
        std::tm* timeInfo = std::localtime( &currentTime );

        if( timeInfo != nullptr )
        {
            // Extract and display each component separately
            date.YYYY = timeInfo->tm_year + 1900; // Add 1900 to get the actual year
            date.MM   = timeInfo->tm_mon + 1; // Add 1 since months are 0-based
            date.DD   = timeInfo->tm_mday;
            time.HH   = timeInfo->tm_hour;
            time.MM   = timeInfo->tm_min;

            DBG( "Date-> " << date.YYYY << "/" << date.MM << "/" << date.DD );
            DBG( "Time-> " << time.HH << "/" << time.MM );

            return 1;
        }
        return 0;
    }

    int HwPref::setTimeZone( juce::String& str )
    {
#if defined( __linux__ ) || defined( __linux ) || defined( linux )
        std::string cmd = "timedatectl set-timezone " + str.toStdString();
        int         ret = KICK_SYSTEM( cmd.c_str() );
        if( ret == 0 )
        {
            DBG( "setting timezone Success" << cmd );
            return 1;
        }
        else
            DBG( "Failed to set timeZone" );
#endif
        return 0;
    }

    int HwPref::setBrightness( int value )
    {
#if KICK_EMU
        value = value < 20 ? 20 : value;
        kick_emu::notify( ( "backlight " + std::to_string( value ) ).c_str() );
        return 1;
#elif defined( __linux__ ) || defined( __linux ) || defined( linux )
        if( value < 20 )
        {
            value = 20;
        }
        std::string setBrightness = "echo  " + std::to_string( value ) + " > /sys/devices/platform/backlight/backlight/backlight/brightness";
        DBG( "Command = " << setBrightness );

        int returnCode = KICK_SYSTEM( setBrightness.c_str() );
        if( returnCode == 0 )
        {
            DBG( "Command executed successfully." );
            return 1;
        }
        else
        {
            DBG( "Command execution failed or returned non-zero: " );
        }
#endif
        return 0;
    }

    DeviceInfo HwPref::getDeviceInfo()
    {
        DeviceInfo info;
        //Serial number
        juce::String serialnumber = getDevSerial().toRawUTF8();
        std::strncpy( info.serialNumber, serialnumber.toRawUTF8(), sizeof( info.serialNumber ) );

        //FirmwareVersion
        juce::String fwVersion = getFirmwareVersion().toRawUTF8();
        std::strncpy( info.firmwareVersion, fwVersion.toRawUTF8(), sizeof( info.firmwareVersion ) );

        //buildDate
        juce::String buildStamp = juce::String( __DATE__ ) + " :: " + juce::String( __TIME__ );
        std::strncpy( info.firmwareBuildDate, buildStamp.toRawUTF8(), sizeof( info.firmwareBuildDate ) );

        mt::sysRamInfo rInfo;
        mt::SystemResource::getResourceInterface().ramInfo( rInfo );

        std::strncpy( info.memoryStatus.available, juce::String( rInfo.freeRam ).toRawUTF8(), sizeof( info.memoryStatus.available ) );
        std::strncpy( info.memoryStatus.total, juce::String( rInfo.totalRam ).toRawUTF8(), sizeof( info.memoryStatus.total ) );
        std::strncpy( info.memoryStatus.used, juce::String( rInfo.usedRam ).toRawUTF8(), sizeof( info.memoryStatus.used ) );

        mt::sysSizeInfo sInfo;
        std::string     internalPath = "/home/Kick";
        mt::SystemResource::getResourceInterface().sizeInfo( internalPath, sInfo );

        std::strncpy( info.internalStorageStatus.availableSpace, juce::String( sInfo.avaSize ).toRawUTF8(), sizeof( info.internalStorageStatus.availableSpace ) );
        std::strncpy( info.internalStorageStatus.totalSpace, juce::String( sInfo.totalSize ).toRawUTF8(), sizeof( info.internalStorageStatus.totalSpace ) );
        std::strncpy( info.internalStorageStatus.usedSpace, juce::String( sInfo.usedSize ).toRawUTF8(), sizeof( info.internalStorageStatus.usedSpace ) );

        internalPath = "/mnt/flash";

        if( isStorageconnected( "flash" ) )
        {
            mt::SystemResource::getResourceInterface().sizeInfo( internalPath, sInfo );

            std::strncpy( info.usbStorageStatus.availableSpace, juce::String( sInfo.avaSize ).toRawUTF8(), sizeof( info.usbStorageStatus.availableSpace ) );
            std::strncpy( info.usbStorageStatus.totalSpace, juce::String( sInfo.totalSize ).toRawUTF8(), sizeof( info.usbStorageStatus.totalSpace ) );
        }
        else
        {
            memcpy( info.usbStorageStatus.totalSpace, "usb not connected", sizeof( info.usbStorageStatus.totalSpace ) );
            memcpy( info.usbStorageStatus.availableSpace, "usb not connected", sizeof( info.usbStorageStatus.availableSpace ) );
        }

        return info;
    }

    bool HwPref::isStorageconnected( std::string id ) // flash-> usb , sdcard->SDCARD
    {
#if KICK_EMU
        return kick_emu::isMounted( id.c_str() );
#elif defined( __linux__ ) || defined( __linux ) || defined( linux )
        std::fstream versionFile;
        versionFile.open( "/proc/mounts", std::ios::in );

        if( versionFile.is_open() )
        {
            std::string str;
            while( getline( versionFile, str ) )
            {
                if( str.find( id ) != std::string::npos )
                {
                    DBG( "USB Storage is connected" );
                    return true;
                }
            }
        }
#endif
        DBG( " " << id << " Storage is not connected" );
        return false;
    }

    void HwPref::setDate( int day, int month, int year )
    {
        //date - s "2024-05-15"
        std::string cmd = std::to_string( year ) + "-" + std::to_string( month ) + "-" + std::to_string( day );
        DBG( "Setting Day -> " << cmd );
        // Execute the command
        int result = KICK_SYSTEM( cmd.c_str() );
        // Check if the command executed successfully
        if( result == 0 )
        {
            DBG( "Date successfully updated to: " << day << "/" << month << "/" << year );
        }
        else
        {
            DBG( "Failed to update date." );
        }
    }

    void HwPref::setTime( int hour, int minute )
    {
        std::string cmd = std::to_string( hour ) + ":" + std::to_string( minute ) + ":00";
        DBG( "Setting Day -> " << cmd );
        // Execute the command
        int result = KICK_SYSTEM( cmd.c_str() );
        // Check if the command executed successfully
        if( result == 0 )
        {
            DBG( "Time successfully updated to: " << hour << "/" << minute );
        }
        else
        {
            DBG( "Failed to update Time." );
        }

        DBG( "Setting Day -> " << hour << "::" << minute );
    }

    // Get the number of days in a month, considering leap years
    int HwPref::getDaysInMonth( int month, int year )
    {
        if( month == 2 )
        {
            if( ( year % 4 == 0 && year % 100 != 0 ) || ( year % 400 == 0 ) )
                return 29; // Leap year
            else
                return 28; // Non-leap year
        }
        else if( month == 4 || month == 6 || month == 9 || month == 11 )
        {
            return 30;
        }
        else
        {
            return 31;
        }
    }
}
