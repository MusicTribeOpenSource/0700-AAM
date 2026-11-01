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
#include "kick_I2c.h"

#if( JUCE_LINUX || __linux__ )
    #include <iostream>
    #include <fcntl.h>
    #include <linux/i2c-dev.h>
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

namespace mt
{
    I2CDevice::I2CDevice()
        : devicePath_( "/dev/i2c-3" ), address_( 0xc ), file_( -1 ), isDeviceOpen_( false )
    {
        if( openDevice() )
        {
            isDeviceOpen_ = true;
        }
    }

    I2CDevice::~I2CDevice()
    {
    }

    bool I2CDevice::isOpen() const
    {
        return isDeviceOpen_;
    }

    bool I2CDevice::openDevice()
    {
#if( JUCE_LINUX || __linux__ )
        file_ = open( devicePath_.c_str(), O_RDWR );
        if( file_ < 0 )
        {
            DBG( "Failed to open the I2C device" );
            return false;
        }
        else
            DBG( "SUCCCESS to open the I2C device" );

        if( ioctl( file_, I2C_SLAVE, address_ ) < 0 )
        {
            DBG( "Failed to select I2C device" );
            closeDevice();
            return false;
        }
        else
            DBG( "SUCCCESS to SET Slave Address" );
#endif
        return true;
    }

    void I2CDevice::closeDevice()
    {
#if( JUCE_LINUX || __linux__ )
        if( file_ >= 0 )
        {
            close( file_ );
            file_ = -1;
        }
#endif
        isDeviceOpen_ = false;
    }

    bool I2CDevice::writeBytes( const uint8_t* data, int length )
    {
#if( JUCE_LINUX || __linux__ )
        if( !isOpen() )
        {
            DBG( "I2C device is not open" );
            return false;
        }

        //  DBG( "data[0]=" << juce::String::toHexString( data[0] ) );
        //DBG( "data[1]=" << juce::String::toHexString( data[1] ) );
        //DBG( "data[2]=" << juce::String::toHexString( data[2] ) );
        ssize_t bytesWritten = write( file_, data, length );
        if( bytesWritten != length )
        {
            DBG( "Failed to write to the I2C device" );
            return false;
        }
#endif
        return true;
    }

    bool I2CDevice::readBytes( uint8_t* buffer, int length )
    {
#if( JUCE_LINUX || __linux__ )
        if( !isOpen() )
        {
            DBG( "I2C device is not open" );
            return false;
        }

        ssize_t bytesRead = read( file_, buffer, length );
        if( bytesRead != length )
        {
            DBG( "Failed to read from the I2C device" );
            return false;
        }
#endif
        return true;
    }

    bool CV::setCentVoltage( uint8_t channel, uint16_t centValue )
    {
        bool retVal = false;
        if( juce::isPositiveAndBelow( channel, MaxCVPorts ) )
        {
            auto startNote = (int)getChannelNoteMap( channel );
            auto checkCent = (int)( centValue - ( startNote * 100 ) );
            auto noteIndex = centValue / 100;
            if( juce::isPositiveAndBelow( checkCent, 6000 ) )
            {
                uint16_t regValue = claibValue[noteIndex] + ( claibValue[noteIndex + 1] - claibValue[noteIndex] ) * std::fmod( centValue, 100 ) / 100;
                uint8_t  buffer[3];
                buffer[0] = ( 0x30 | ( toChannelmap( channel ) ) );
                buffer[1] = ( regValue >> 4 ) & 0xFF;
                buffer[2] = ( regValue << 4 ) & 0xF0;

                if( i2c.writeBytes( buffer, sizeof( buffer ) ) )
                    retVal = true;
                else
                    DBG( "setVoltage Failed to set Voltage Channel->" << channel );
            }
            else
                setVoltageOff( channel );
        }
        return retVal;
    }

    bool CV::setVoltage( uint8_t channel, uint8_t Note )
    {
        bool retVal = false;
        if( juce::isPositiveAndBelow( channel, MaxCVPorts ) )
        {
            auto startNote = (int)getChannelNoteMap( channel );
            auto note      = (int)Note - startNote;
            if( juce::isPositiveAndBelow( note, 61 ) )
            {
                uint8_t  buffer[3];
                uint16_t regValue = claibValue[note];
                buffer[0]         = ( 0x30 | ( toChannelmap( channel ) ) );
                buffer[1]         = ( regValue >> 4 ) & 0xFF;
                buffer[2]         = ( regValue << 4 ) & 0xF0;

                if( i2c.writeBytes( buffer, sizeof( buffer ) ) )
                    retVal = true;
                else
                    DBG( "setVoltage Failed to set Voltage Channel->" << channel );
            }
            else
                setVoltageOff( channel );
        }
        return retVal;
    }

    bool CV::setstepVoltage( uint8_t channel, uint16_t step )
    {
        if( juce::isPositiveAndBelow( channel, MaxCVPorts ) )
        {
            if( step == 0 )
                return setVoltageOff( channel );

            uint8_t buffer[3];
            int bufVal = juce::jlimit( 0, 4095, (int)( step * 4095 / 127 ) );
            buffer[0]  = ( 0x30 | ( toChannelmap( channel ) ) );
            buffer[1]  = ( bufVal >> 4 ) & 0xFF;
            buffer[2]  = ( bufVal << 4 ) & 0xF0;

            if( !i2c.writeBytes( buffer, sizeof( buffer ) ) )
            {
                DBG( "setVoltage Failed to set Voltage Channel->" << channel );
                return false;
            }
            return true;
        }
        return false;
    }

    bool CV::setVoltageOn( uint8_t channel )
    {
        if( juce::isPositiveAndBelow( channel, MaxCVPorts ) )
        {
            uint8_t buffer[3];
            buffer[0] = ( 0x30 | ( toChannelmap( channel ) ) ); //beacuse cv channle start with 0
            buffer[1] = 0xFF;
            buffer[2] = 0xFF;
            if( !i2c.writeBytes( buffer, sizeof( buffer ) ) )
            {
                DBG( "setVoltage Failed to set Voltage Channel->" << channel );
                return false;
            }
            return true;
        }
        return false;
    }

    bool CV::setVoltageOff( uint8_t channel )
    {
        if( juce::isPositiveAndBelow( channel, MaxCVPorts ) )
        {
            uint8_t buffer[3];
            buffer[0] = ( 0x30 | ( toChannelmap( channel ) ) );
            buffer[1] = 0x00;
            buffer[2] = 0x00;

            if( !i2c.writeBytes( buffer, sizeof( buffer ) ) )
            {
                DBG( "setVoltage Failed to set Voltage Channel->" << channel );
                return false;
            }
            return true;
        }
        return false;
    }

    bool CV::calibrateCV( uint8_t index, int step )
    {
        if( index < 5 && index > 0 ) //ui sending 1 to 4 1v,2v,3v,4v
        {
            uint16_t value = defaultCV[index] + step;
            writeAllChannelVoltage( value );
            return true;
        }
        return false;
    }

    bool CV::resetCalibCV( uint8_t index )
    {
        if( index < 5 && index > 0 ) //ui sending 1 to 4 1v,2v,3v,4v
        {
            calibCV[index] = defaultCV[index];
            return true;
        }
        return false;
    }

    bool CV::applyCalibration( const std::array<uint16_t, 6>& index )
    {
        std::array<uint16_t, 6> calib = { 0 };
        calib[1]                      = defaultCV[1] + index[1];
        calib[2]                      = defaultCV[2] + index[2];
        calib[3]                      = defaultCV[3] + index[3];
        calib[4]                      = defaultCV[4] + index[4];
        calib[5]                      = 4095;

        // Apply calibration to the array
        int array_index = 0;
        for( int i = 0; i < 5; i++ )
        {
            // Calculate the step size between each pair of calibration values
            float step_size = static_cast<float>( calib[i + 1] - calib[i] ) / 12.0f;

            // Distribute calibration values between array indices
            for( int j = 0; j < 12; j++ )
            {
                claibValue[array_index++] = calib[i] + static_cast<uint16_t>( step_size * j );
            }
        }
        claibValue[60] = 4095;

        return true; // Calibration applied successfully
    }

    bool CV::setChannelNoteMap( uint8_t channel, int startNote )
    {
        uint8_t mappedchannel         = toChannelmap( channel );
        channelNoteMap[mappedchannel] = startNote;
        return true; // Indicate successful operation
    }

    // Function to get the note range for a given channel
    inline uint8_t CV::getChannelNoteMap( uint8_t channel )
    {
        return channelNoteMap[channel];
    }

    uint16_t CV::calculateValue( uint8_t note )
    {
        double factor   = 68.33;
        int    noteBase = ( note / 12 );

        if( noteBase && ( noteBase != 5 ) )
        {
            factor = calibCV[noteBase + 1] - calibCV[noteBase];
        }
        else if( noteBase == 5 )
        {
            factor = calibCV[noteBase] - calibCV[noteBase - 1];
            ;
        }
        else
            factor = ( calibCV[0] / 12 );

        if( noteBase >= 0 && noteBase < 5 )
        { // Assuming 5 is the maximum possible value of noteBase
            return ( factor * ( note - ( noteBase * 12 ) ) + calibCV[noteBase] );
        }
        else
        {
            // Handle invalid noteBase or note here
            // You may return a default value or throw an exception, depending on your requirements
            return 0; // Default value
        }
    }

    bool CV::initRead( uint8_t channel )
    {
        uint8_t buffer[3];
        buffer[0] = ( 0x90 | channel );
        buffer[1] = 0;
        buffer[2] = 0;
        if( !i2c.writeBytes( buffer, sizeof( buffer ) ) )
        {
            DBG( "Failed to initRead Voltage channal-> " << channel );
            return false;
        }
        return true;
    }

    inline int CV::toChannelmap( int port )
    {
        return juce::isPositiveAndBelow( port, 9 ) ? CVPortIndex[port] : -1;
    }

    bool CV::readVoltage( uint8_t channel, float& voltage )
    {
        if( juce::isPositiveAndBelow( channel, MaxCVPorts ) )
        {
            uint8_t rxBuffer[2] = { 0, 0 }; // To store the read data
            if( initRead( ( toChannelmap( channel ) ) ) )
            {
                if( !i2c.readBytes( rxBuffer, sizeof( rxBuffer ) ) )
                {
                    DBG( "read Failed to read Voltage channal-> " << channel );
                    return false;
                }
                uint16_t regValue = static_cast<uint16_t>( rxBuffer[0] ) << 8 | rxBuffer[1];
                voltage           = ( ( (float)regValue * 5.f ) / 4096.f );
            }
            return true;
        }
        return false;
    }

    bool CV::readAllchannelVoltage( std::vector<float>& data )
    {
        data.clear();
        uint8_t rxBuffer[16] = { 0 }; // To store the read data
        if( initRead( 0 ) ) // Reading from channel 0
        {
            if( !i2c.readBytes( rxBuffer, sizeof( rxBuffer ) ) )
            {
                DBG( "read Failed to read Voltage channal-> " );
                return false;
            }
            uint16_t regValue = 0;
            for( int i = 8; i < 16; )
            {
                regValue = ( ( static_cast<uint16_t>( rxBuffer[i] ) << 8 | rxBuffer[i + 1] ) >> 4 );
                data.push_back( ( ( (float)regValue * 5.f ) / 4095.f ) );
                i = i + 2;
            }
            for( int i = 0; i < 8; )
            {
                regValue = ( ( static_cast<uint16_t>( rxBuffer[i] ) << 8 | rxBuffer[i + 1] ) >> 4 );
                data.push_back( ( ( (float)regValue * 5.f ) / 4095.f ) );
                i = i + 2;
            }
        }
        return true;
    }

    bool CV::writeAllChannelVoltage( const std::vector<uint16_t>& data )
    {
        uint8_t buffer[3];
        for( int channel = 0; channel < 8; channel++ )
        {
            buffer[0] = ( 0x30 | channel );
            buffer[1] = ( data[channel] >> 4 ) & 0xFF;
            buffer[2] = ( data[channel] << 4 ) & 0xF0;
            if( !i2c.writeBytes( buffer, sizeof( buffer ) ) )
            {
                DBG( "writeAllChannelVoltage -> Failed to Write SetVoltage channel ->" << channel );
                return false;
            }
        }
        return true;
    }

    bool CV::writeAllChannelVoltage( const uint16_t& data )
    {
        uint8_t buffer[3];
        for( int channel = 0; channel < 8; channel++ )
        {
            buffer[0] = ( 0x30 | channel );
            buffer[1] = ( data >> 4 ) & 0xFF;
            buffer[2] = ( data << 4 ) & 0xF0;
            if( !i2c.writeBytes( buffer, sizeof( buffer ) ) )
            {
                DBG( "writeAllChannelVoltage -> Failed to Write SetVoltage channel ->" << channel );
                return false;
            }
        }
        return true;
    }
}
