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

#pragma once

#include <memory>
#include <atomic>
#include <PktMgr/PktMgr.hpp>
#include "dcl_contracts.h"

namespace ipc
{
    class MTIPCJson;
}

namespace contracts
{
    extern std::atomic<bool> shiftBtnState;
    extern std::atomic<bool> halfLevelState;
}

namespace contracts
{
    struct PadData
    {
        int                                               colourIndex;
        contracts::DeviceControlContracts::PadButtonState state;
        int                                               colourId;
    };

    class DclContractsHelper
    {
    public:
        // Incoming events must satisfy the payload sizes used by UI consumers.
        // Pad aftertouch is optional; command packets may legitimately be empty.
        static bool isValidHardwareInput( const CPktMgr::HWMsg_t& hwMsg );
        static bool isShiftPressed();
        static void setHalfLevel( bool state );
        static bool isHalfLevel();
        static bool isButtonMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isPadButtonMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isKnobMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isCenterKnobClickMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isCenterKnobRotateMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isSliderMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isBankMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isButtonRelevantForControl( const CPktMgr::HWMsg_t& hwMsg );
        static bool isNavigationMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isCenterKnobMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isSystemMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isSystemOrPadMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isSoftwareVersionMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isSoftwareUpdateMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isSystemCommInit( const CPktMgr::HWMsg_t& hwMsg );
        static bool isSystemCommLive( const CPktMgr::HWMsg_t& hwMsg );

        static bool isLEDMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isLEDSingleMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isLEDAllMessage( const CPktMgr::HWMsg_t& hwMsg );
        static bool isLEDBlinkMessage( const CPktMgr::HWMsg_t& hwMsg );

        static int getButtonCode( const CPktMgr::HWMsg_t& hwMsg );

        static int getButtonState( const CPktMgr::HWMsg_t& hwMsg );

        static int getPadNumber( const CPktMgr::HWMsg_t& hwMsg );

        static int getPadState( const CPktMgr::HWMsg_t& hwMsg );
        static int getPadVelocity( const CPktMgr::HWMsg_t& hwMsg );
        static int getPadAftertouch( const CPktMgr::HWMsg_t& hwMsg );

        static int getKnobCode( const CPktMgr::HWMsg_t& hwMsg );
        static int getKnobDirection( const CPktMgr::HWMsg_t& hwMsg );
        static int getKnobRotation( const CPktMgr::HWMsg_t& hwMsg );
        static int getKnobValue( const CPktMgr::HWMsg_t& hwMsg );

        static int getSliderValue( const CPktMgr::HWMsg_t& hwMsg );

        static int getSwUpdateCode( const CPktMgr::HWMsg_t& hwMsg );

        static void                            hWMessageToIPCMessage( const CPktMgr::HWMsg_t& hwMessage, std::shared_ptr<ipc::MTIPCJson> ipcMessage );
        static bool                            ipcMessageToHWMessage( std::shared_ptr<ipc::MTIPCJson> ipcMessage, CPktMgr::HWMsg_t& hwMessage );
        static std::shared_ptr<ipc::MTIPCJson> prepareHWMessage( const int pktType, const int pktDir, const int pktCmd, const std::vector<int>& data = std::vector<int>() );

        static std::shared_ptr<ipc::MTIPCJson> reconnectMessage();
        static std::shared_ptr<ipc::MTIPCJson> shutdownMessage();
        static std::shared_ptr<ipc::MTIPCJson> SendSwUpdateCmdMessage( int f_data );
        static std::shared_ptr<ipc::MTIPCJson> setPlayStateMessage( contracts::DeviceControlContracts::BtnLedSequence button );
        static std::shared_ptr<ipc::MTIPCJson> clearAllPadsMessage();
        static std::shared_ptr<ipc::MTIPCJson> setLEDONMessage( int hwId, int colourId = 0 );
        static std::shared_ptr<ipc::MTIPCJson> setLEDOFFMessage( int hwId, int colourId = 0 );
        static std::shared_ptr<ipc::MTIPCJson> sendSliderValueInPercentMessage( int valueInPercent, DeviceControlContracts::SliderCmd cmd = DeviceControlContracts::SliderCmd::EN_SLIDER_BAR_LED );
        static std::shared_ptr<ipc::MTIPCJson> setPadDataMessage( int PadID, int State, int ColourId = 15 );
        static std::shared_ptr<ipc::MTIPCJson> setPadBlinkingMessage( int PadID, int State, int OnColourId, int OffColourId = contracts::DeviceControlContracts::PadColourIds::PadColourTrans );
        static std::shared_ptr<ipc::MTIPCJson> setPadSettingsMessage( const std::vector<int>& f_data );
        static std::shared_ptr<ipc::MTIPCJson> setTempoMessage( int tempo );
        static std::shared_ptr<ipc::MTIPCJson> setBankMessage( int bankId );
        static std::shared_ptr<ipc::MTIPCJson> setMediaControlMessage( int btnId );
        static std::shared_ptr<ipc::MTIPCJson> setAllPadDataColourInfoMessage( int LedStateType, int ColourIdData );
        static std::shared_ptr<ipc::MTIPCJson> setAllPadDataColourInfoMessage( int LedStateType, const std::vector<int>& ColourIdData );
        static std::shared_ptr<ipc::MTIPCJson> setPadColourAndStateInfoMessage( const std::vector<PadData>& padDataList );
        static std::shared_ptr<ipc::MTIPCJson> enableAfterTouchMessage( bool state );
        static std::shared_ptr<ipc::MTIPCJson> setPadColorModeMessage( int colourMode );
        static std::shared_ptr<ipc::MTIPCJson> setPadColourByVelocityMessage( int PadID, int State, int ColourId = contracts::DeviceControlContracts::PadColourTrans, int Velocity = 127 );
        static std::shared_ptr<ipc::MTIPCJson> setPadColourByVelocityMessage( int State, const std::vector<int>& ColourId, const std::vector<int>& Velocity );
        static std::shared_ptr<ipc::MTIPCJson> setAllPadColourByVelocityMessage( int State, int ColourId, int Velocity = 127 );
    };
}
