/*
 * Copyright (c) 2024 MUSIC Tribe
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

#include "dcl_contracts_helper.h"
#include "dcl_ipc_keys.h"
#include "dcl_ipc_message_types.h"
#include "ipc/utils/mt_ipc_json.h"
#include "ipc/utils/mt_ipc_message_constructor.h"
#include "dcl_contracts.h"

namespace contracts
{
    namespace
    {
        // Payload length varies by message (a button frame is 2 bytes, a pad frame 4), so every
        // accessor goes through here. A missing byte reads as 0 instead of past the vector's end.
        int dataAt( const CPktMgr::HWMsg_t& hwMsg, size_t index )
        {
            return index < hwMsg.data.size() ? hwMsg.data[index] : 0;
        }
    } // namespace

    std::atomic<bool> shiftBtnState{ false };
    std::atomic<bool> halfLevelState{ false };

    bool DclContractsHelper::isShiftPressed() { return shiftBtnState.load(); }
    void DclContractsHelper::setHalfLevel( bool state ) { halfLevelState.store( state ); }
    bool DclContractsHelper::isHalfLevel() { return halfLevelState.load(); }
    bool DclContractsHelper::isButtonMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_BUTTON ) && ( hwMsg.pktCmd == contracts::DeviceControlContracts::BtnCmd::EN_BTN_STATUS ); }
    bool DclContractsHelper::isPadButtonMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_PAD ) && ( hwMsg.pktCmd == contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE ); }
    bool DclContractsHelper::isKnobMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_KNOBS ); }
    bool DclContractsHelper::isCenterKnobClickMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( isButtonMessage( hwMsg ) && getButtonCode( hwMsg ) == contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER ); }
    bool DclContractsHelper::isCenterKnobRotateMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( isKnobMessage( hwMsg ) && getButtonCode( hwMsg ) == contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER ); }
    bool DclContractsHelper::isSliderMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_SLIDER ) && ( hwMsg.pktCmd == contracts::DeviceControlContracts::SliderCmd::EN_SLIDER_TOUCH_INPUT ); }
    bool DclContractsHelper::isBankMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_BUTTON ) && ( dataAt( hwMsg, 0 ) < NUM_OF_BANKS ); }
    bool DclContractsHelper::isButtonRelevantForControl( const CPktMgr::HWMsg_t& hwMsg )
    {
        if( isButtonMessage( hwMsg ) )
        {
            switch( dataAt( hwMsg, 0 ) )
            {
                case contracts::DeviceControlContracts::EN_PLUS:
                case contracts::DeviceControlContracts::EN_MINUS:
                case contracts::DeviceControlContracts::EN_KNOB_ENTER:
                case contracts::DeviceControlContracts::EN_KNOB_CCW:
                case contracts::DeviceControlContracts::EN_KNOB_CW:
                case contracts::DeviceControlContracts::EN_BACK:
                case contracts::DeviceControlContracts::EN_ERASE:
                    return true;
                default:
                    return false;
            }
        }
        return false;
    }
    bool DclContractsHelper::isNavigationMessage( const CPktMgr::HWMsg_t& hwMsg ) { return isButtonRelevantForControl( hwMsg ) || isPadButtonMessage( hwMsg ) || isKnobMessage( hwMsg ) || isSliderMessage( hwMsg ); }
    bool DclContractsHelper::isCenterKnobMessage( const CPktMgr::HWMsg_t& hwMsg ) { return isCenterKnobClickMessage( hwMsg ) || isCenterKnobRotateMessage( hwMsg ); }
    bool DclContractsHelper::isSystemMessage( const CPktMgr::HWMsg_t& hwMsg ) { return hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM; }
    bool DclContractsHelper::isSystemOrPadMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_PAD ) || ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM ); }
    bool DclContractsHelper::isSoftwareVersionMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM ) && ( hwMsg.pktCmd == contracts::DeviceControlContracts::SysCmd::EN_SYS_GET_SW_VER ); }
    bool DclContractsHelper::isSoftwareUpdateMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM ) && ( hwMsg.pktCmd == contracts::DeviceControlContracts::SysCmd::EN_SYS_SW_UPDATE ); }
    bool DclContractsHelper::isSystemCommInit( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM ) && ( hwMsg.pktCmd == contracts::DeviceControlContracts::SysCmd::EN_SYS_CHECK_COMM_STATUS ) && ( dataAt( hwMsg, 0 ) == 1 ); }
    bool DclContractsHelper::isSystemCommLive( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM ) && ( hwMsg.pktCmd == contracts::DeviceControlContracts::SysCmd::EN_SYS_CHECK_COMM_STATUS ) && ( dataAt( hwMsg, 0 ) == 2 ); }

    bool DclContractsHelper::isLEDMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_LED ); }
    bool DclContractsHelper::isLEDSingleMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_LED ) && ( hwMsg.pktCmd == contracts::DeviceControlContracts::EN_LED_SET_SINGLE_LED ); }
    bool DclContractsHelper::isLEDAllMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_LED ) && ( hwMsg.pktCmd == contracts::DeviceControlContracts::EN_LED_SET_ALL ); }
    bool DclContractsHelper::isLEDBlinkMessage( const CPktMgr::HWMsg_t& hwMsg ) { return ( hwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_LED ) && ( hwMsg.pktCmd == contracts::DeviceControlContracts::EN_LED_BLINK_SINGLE ); }

    int DclContractsHelper::getButtonCode( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 0 ); }
    int DclContractsHelper::getButtonState( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 1 ); }

    int DclContractsHelper::getPadNumber( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 0 ); }
    int DclContractsHelper::getPadState( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 1 ); }
    int DclContractsHelper::getPadVelocity( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 2 ); }
    int DclContractsHelper::getPadAftertouch( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 3 ); }

    int DclContractsHelper::getKnobCode( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 0 ); }
    int DclContractsHelper::getKnobDirection( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 1 ); }
    int DclContractsHelper::getKnobRotation( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 2 ); }
    int DclContractsHelper::getKnobValue( const CPktMgr::HWMsg_t& hwMsg ) { return ( contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE == getKnobDirection( hwMsg ) ) ? getKnobRotation( hwMsg ) : -( getKnobRotation( hwMsg ) ); }

    int DclContractsHelper::getSliderValue( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 0 ); }

    int DclContractsHelper::getSwUpdateCode( const CPktMgr::HWMsg_t& hwMsg ) { return dataAt( hwMsg, 0 ); }

    void DclContractsHelper::hWMessageToIPCMessage( const CPktMgr::HWMsg_t& hwMessage, std::shared_ptr<ipc::MTIPCJson> ipcMessage )
    {
        ipcMessage->setProperty( contracts::DCL_IPC_KEY_PACKET_TYPE, hwMessage.pktType );
        ipcMessage->setProperty( contracts::DCL_IPC_KEY_PACKET_COMMAND, hwMessage.pktCmd );
        ipcMessage->setProperty( contracts::DCL_IPC_KEY_PACKET_DIRECTION, hwMessage.pktDir );
        juce::Array<juce::var> arr;
        for( const int value : hwMessage.data )
        {
            arr.add( value );
        }
        ipcMessage->setProperty( contracts::DCL_IPC_KEY_PACKET_DATA, arr );
    }

    void DclContractsHelper::ipcMessageToHWMessage( std::shared_ptr<ipc::MTIPCJson> ipcMessage, CPktMgr::HWMsg_t& hwMessage )
    {
        hwMessage.pktType            = static_cast<CPktMgr::enumPacketType>( int( ipcMessage->getProperty( contracts::DCL_IPC_KEY_PACKET_TYPE ) ) );
        hwMessage.pktCmd             = ipcMessage->getProperty( contracts::DCL_IPC_KEY_PACKET_COMMAND );
        hwMessage.pktDir             = static_cast<CPktMgr::enumPacketDir>( int( ipcMessage->getProperty( contracts::DCL_IPC_KEY_PACKET_DIRECTION ) ) );
        // A frame without a payload array is a frame without payload, not a null dereference.
        const juce::var         dataVar = ipcMessage->getProperty( contracts::DCL_IPC_KEY_PACKET_DATA, juce::var( juce::Array<juce::var>() ) );
        juce::Array<juce::var>* data    = dataVar.getArray();
        if( data != nullptr )
        {
            for( const juce::var& item : *data )
                hwMessage.data.push_back( item );
        }
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::reconnectMessage()
    {
        return prepareHWMessage( CPktMgr::EN_PKT_TYPE_SYSTEM, CPktMgr::EN_PKT_REQUEST, contracts::DeviceControlContracts::EN_SYS_RECONNECT );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::shutdownMessage()
    {
        return prepareHWMessage( CPktMgr::EN_PKT_TYPE_SYSTEM, CPktMgr::EN_PKT_REQUEST, contracts::DeviceControlContracts::EN_SYS_SHUTDOWN );
    }
    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::SendSwUpdateCmdMessage( int f_data )
    {
        std::vector<int> data{ f_data };
        return prepareHWMessage( CPktMgr::EN_PKT_TYPE_SYSTEM, CPktMgr::EN_PKT_REQUEST, contracts::DeviceControlContracts::EN_SYS_SW_UPDATE, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setPlayStateMessage( contracts::DeviceControlContracts::BtnLedSequence button )
    {
        std::vector<int> data{ button, contracts::DeviceControlContracts::BTN_PRESSED };
        return prepareHWMessage( CPktMgr::EN_PKT_TYPE_BUTTON, CPktMgr::EN_PKT_REQUEST, contracts::DeviceControlContracts::EN_BTN_STATUS, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::clearAllPadsMessage()
    {
        std::vector<int> data{ 0, contracts::DeviceControlContracts::PadColourTrans };
        return prepareHWMessage( CPktMgr::EN_PKT_TYPE_PAD, CPktMgr::EN_PKT_REQUEST, contracts::DeviceControlContracts::EN_PAD_SET_ALL_SINGLE_COLOUR, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setLEDONMessage( int hwId, int colourId )
    {
        std::vector<int> data{ hwId, contracts::DeviceControlContracts::PadButtonState::StateOn, colourId };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_LED, CPktMgr::enumPacketDir::EN_PKT_REQUEST, contracts::DeviceControlContracts::EN_LED_SET_SINGLE_LED, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setLEDOFFMessage( int hwId, int colourId )
    {
        std::vector<int> data{ hwId, contracts::DeviceControlContracts::PadButtonState::StateOff, colourId };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_LED, CPktMgr::enumPacketDir::EN_PKT_REQUEST, contracts::DeviceControlContracts::EN_LED_SET_SINGLE_LED, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::sendSliderValueInPercentMessage( int valueInPercent, DeviceControlContracts::SliderCmd cmd )
    {
        std::vector<int> data{ valueInPercent };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_SLIDER, CPktMgr::enumPacketDir::EN_PKT_REQUEST, cmd, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setPadDataMessage( int PadID, int State, int ColourId )
    {
        std::vector<int> data{ PadID, State, ColourId };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_SET_SINGLE_PAD, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setPadBlinkingMessage( int PadID, int State, int OnColourId, int OffColourId )
    {
        std::vector<int> data{ PadID,
                               State,
                               OnColourId,
                               OffColourId };

        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_SET_BLINKING, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setPadSettingsMessage( const std::vector<int>& f_data )
    {
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_SETTINGS, f_data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setTempoMessage( int tempo )
    {
        uint8_t          higherByte = ( tempo >> 8 );
        uint8_t          lowerByte  = tempo & 0x00FF;
        std::vector<int> data{ higherByte, lowerByte };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::SysCmd::EN_SYS_BPM, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setBankMessage( int bankId )
    {
        std::vector<int> data{ (int)DeviceControlContracts::BtnLedSequence::EN_BANK_A + bankId, 1 };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_LED, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::LedCmd::EN_LED_SET_SINGLE_LED, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setMediaControlMessage( int btnId )
    {
        std::vector<int> data{ btnId, DeviceControlContracts::PadButtonState::StateOn, 1 };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_LED, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::LedCmd::EN_LED_SET_SINGLE_LED, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setAllPadDataColourInfoMessage( int LedStateType, int ColourIdData )
    {
        std::vector<int> padData{ LedStateType, ColourIdData };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_SET_ALL_SINGLE_COLOUR, padData );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setAllPadDataColourInfoMessage( int LedStateType, const std::vector<int>& ColourIdData )
    {
        std::vector<int> padData{ LedStateType };

        for( auto temp = 0; temp < ColourIdData.size(); temp++ )
            padData.push_back( ColourIdData[temp] );
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_SET_ALL_MULTI_COLOUR, padData );
    }

std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setPadColourAndStateInfoMessage( const std::vector<PadData>& padDataList )
    {
        std::shared_ptr<ipc::MTIPCJson> message = nullptr;
        if( padDataList.size() > 0 )
        {
            std::vector<int> padData;
            for( auto i = 0; i < padDataList.size(); i++ )
                padData.push_back( padDataList[i].colourId );

            for( auto i = 0; i < padDataList.size(); i++ )
                padData.push_back( padDataList[i].state );

           message = prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_SET_COLOUR_STATE, padData );
        }
        return message;
      }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::enableAfterTouchMessage( bool state )
    {
        std::vector<int> data{ state };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_SET_AFTER_TOUCH, data );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setPadColorModeMessage( int colourMode )
    {
        std::vector<int> padData{ colourMode };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_COLOR_MODE, padData );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setPadColourByVelocityMessage( int PadID, int State, int ColourId, int Velocity )
    {
        std::vector<int> padData{ PadID, State, ColourId, Velocity };
        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_SET_STEP_SINGLE_PAD, padData );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setPadColourByVelocityMessage( int State, const std::vector<int>& ColourId, const std::vector<int>& Velocity )
    {
        std::vector<int> padData{ State };

        for( auto temp = 0; temp < Velocity.size(); temp++ )
            padData.push_back( ColourId[temp] );

        for( auto temp = 0; temp < Velocity.size(); temp++ )
            padData.push_back( Velocity[temp] );

        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_SET_STEP_ALL_PAD, padData );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::setAllPadColourByVelocityMessage( int State, int ColourId, int Velocity )
    {
        std::vector<int> padData;
        padData.push_back( State );

        for( auto temp = 0; temp < 16; temp++ )
            padData.push_back( ColourId );

        for( auto temp = 0; temp < 16; temp++ )
            padData.push_back( Velocity );

        return prepareHWMessage( CPktMgr::enumPacketType::EN_PKT_TYPE_PAD, CPktMgr::enumPacketDir::EN_PKT_REQUEST, DeviceControlContracts::PadCmd::EN_PAD_SET_STEP_ALL_PAD, padData );
    }

    std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::prepareHWMessage( const int pktType, const int pktDir, const int pktCmd, const std::vector<int>& data )
    {
        std::shared_ptr<ipc::MTIPCJson> ipcMessage =
            ipc::MTIPCMessageConstructor::constructRequestMessage( contracts::DclIPCMessageType::DCL_IPC_MESSAGE_TYPE_HW_COMMAND );
        ipcMessage->setProperty( contracts::DCL_IPC_KEY_PACKET_TYPE, pktType );
        ipcMessage->setProperty( contracts::DCL_IPC_KEY_PACKET_COMMAND, pktCmd );
        ipcMessage->setProperty( contracts::DCL_IPC_KEY_PACKET_DIRECTION, pktDir );
        juce::Array<juce::var> arr;
        for( const int value : data )
        {
            arr.add( value );
        }
        ipcMessage->setProperty( contracts::DCL_IPC_KEY_PACKET_DATA, arr );

        return ipcMessage;
    }
}
