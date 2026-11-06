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

#if( JUCE_LINUX || __linux__ )
    #include <iostream>
    #include <fcntl.h>
    #include <linux/i2c-dev.h>
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

#include <sys/stat.h>

#define WIFI_CRED_FILE "/home/Kick/wifi_credentials.txt"
#define WIFI_CONF_FILE "/home/Kick/wpa_supplicant.conf"

namespace mt
{
    WifiManager::WifiManager()
    {
    }

    WifiManager::~WifiManager()
    {
    }

    juce::String WifiManager::executeCommand( const juce::String& command )
    {
        juce::ChildProcess process;
        process.start( command );
        process.waitForProcessToFinish( -1 );
        return process.readAllProcessOutput();
    }

    bool WifiManager::fileExists( const std::string& filename )
    {
        struct stat buffer;
        return ( stat( filename.c_str(), &buffer ) == 0 );
    }

    std::map<std::string, std::string> WifiManager::loadWifiCredentials()
    {
        std::ifstream                      file( WIFI_CRED_FILE );
        std::map<std::string, std::string> wifiMap;
        std::string                        line;

        while( std::getline( file, line ) )
        {
            std::stringstream ss( line );
            std::string       ssid, password;
            if( std::getline( ss, ssid, ':' ) && std::getline( ss, password ) )
            {
                wifiMap[ssid] = password;
            }
        }
        return wifiMap;
    }

    void WifiManager::saveWifiCredential( const std::string& ssid, const std::string& password )
    {
        if( !fileExists( WIFI_CRED_FILE ) )
        {
            std::ofstream file( WIFI_CRED_FILE );
            file.close();
        }

        std::ofstream file( WIFI_CRED_FILE, std::ios::app );
        file << ssid << ":" << password << std::endl;
    }

    const std::vector<juce::String> WifiManager::scanWiFiNetworks()
    {
        juce::String                                      currentSSID;
        juce::String                                      currentFrequency;
        std::map<juce::String, std::vector<juce::String>> wifiNetworks;
        std::vector<juce::String>                         scanResults;
        std::vector<juce::String>                         _networks;
        juce::String                                      command = "iwlist wlan0 scan";
        juce::ChildProcess                                childProcess;

        DBG( "-------------------------scanning-----------------------" );
        if( childProcess.start( command ) )
        {
            juce::StringArray tempResults;
            tempResults.addLines( childProcess.readAllProcessOutput() );
            scanResults.assign( tempResults.begin(), tempResults.end() );
            childProcess.waitForProcessToFinish( 10000 );
        }

        for( const auto& line : scanResults )
        {
            if( line.contains( "ESSID:" ) )
            {
                currentSSID = line.fromFirstOccurrenceOf( "ESSID:", false, true ).trim().unquoted();
            }
            else if( line.contains( "Frequency:" ) )
            {
                currentFrequency = line.fromFirstOccurrenceOf( "Frequency:", false, true ).upToFirstOccurrenceOf( " ", false, true ).trim();
            }

            if( !currentSSID.isEmpty() && !currentFrequency.isEmpty() )
            {
                wifiNetworks[currentSSID].push_back( currentFrequency );
                currentSSID.clear();
                currentFrequency.clear();
            }
        }

        for( const auto& entry : wifiNetworks )
        {
            juce::StringArray frequencyArray;

            // Manually add elements from the vector to the StringArray
            for( const auto& freq : entry.second )
            {
                frequencyArray.add( freq );
            }

            _networks.push_back( entry.first );
            DBG( "SSID: " + entry.first );
            DBG( "Frequencies: " + frequencyArray.joinIntoString( ", " ) );
            DBG( "-------------------------" );
        }
        displayWiFiNetworks( _networks );
        return _networks;
    }

    bool WifiManager::enableWiFi()
    {
        juce::String command = "ip link set wlan0 up";
        juce::String result  = executeCommand( command );
        return result.isEmpty();
    }

    bool WifiManager::disableWiFi()
    {
        juce::String command = "ip link set wlan0 down";
        juce::String result  = executeCommand( command );
        return result.isEmpty();
    }

    void WifiManager::displayWiFiNetworks( const std::vector<juce::String>& Networks )
    {
        for( const auto& network : Networks )
        {
            std::cout << "Network: " << network << std::endl;
        }
    }

    bool WifiManager::connectToWiFi( const juce::String& ssid, const juce::String& password, int priority )
    {
        std::cout << "Network: connecting to wifi SSID: " << ssid << std::endl;
        std::map<std::string, std::string> wifiMap = loadWifiCredentials();
        std::string                        ssid1   = ssid.toStdString();
        juce::String                       result  = "";

#if defined( __linux__ ) || defined( __linux ) || defined( linux )
        auto it = wifiMap.find( ssid1 );
        if( it != wifiMap.end() )
        {
            std::string password1 = wifiMap[ssid1];
            addSSID( ssid, juce::String( password1 ), priority );
            std::cout << "SSID found in file. Using stored password." << std::endl;
        }
        else
        {
            addSSID( ssid, password, priority );
            saveWifiCredential( ssid.toStdString(), password.toStdString() );
        }

        juce::String command = "killall -9 wpa_supplicant dhclient";
        result               = executeCommand( command );
        sleep( 1 );
        command = "wpa_supplicant -B -i wlan0 -c /home/Kick/wpa_supplicant.conf";
        result  = executeCommand( command );
        sleep( 2 );
        command = "dhclient wlan0";
        result  = executeCommand( command );
#endif
        return result.isEmpty();
    }

    bool WifiManager::disconnectWiFi()
    {
        juce::String command = "killall -9 wpa_supplicant dhclient";
        juce::String result  = executeCommand( command );
        return result.isEmpty();
    }

    void WifiManager::addSSID( const juce::String& ssid, const juce::String& password, int priority )
    {
        if( !fileExists( WIFI_CONF_FILE ) )
        {
            std::ofstream file( WIFI_CONF_FILE );
            file.close();
        }
        juce::File configFile( WIFI_CONF_FILE );
        if( configFile.existsAsFile() )
        {
            juce::String networkConfig =
                "\nnetwork={\n"
                "    ssid=\""
                + ssid + "\"\n"
                         "    psk=\""
                + password + "\"\n"
                             "    key_mgmt=WPA-PSK  "
                + "\n"
                  "}\n\n";

            configFile.replaceWithText( networkConfig );
        }
        else
        {
            std::cerr << "Error: wpa_supplicant.conf file not found or not accessible." << std::endl;
        }
    }
}
