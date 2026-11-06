#pragma once
// Test harness around CPktMgr: owns the FIFOs the constructor requires and
// offers helpers to build wire frames and feed them byte by byte.

#include <cstdint>
#include <vector>

#include "PktMgr.hpp"

namespace kicktest
{
    class PktHarness
    {
    public:
        PktHarness()
            : mgr( CPktMgr::EN_COM_CHANNEL_USB, rx, sizeof( rx ), tx, sizeof( tx ), 10 )
        {
            mgr.Init();
            mgr.ClearReceiver();
        }

        // Serialize a message exactly as the device-side transmit path does.
        std::vector<char> encode( CPktMgr::enumPacketType type, CPktMgr::enumPacketDir dir, int cmd, std::vector<int> data = {} )
        {
            CPktMgr::HWMsg_t msg;
            msg.pktType = type;
            msg.pktDir  = dir;
            msg.pktCmd  = cmd;
            msg.data    = std::move( data );
            std::vector<char> wire;
            mgr.BuildAndTransmitPacket( msg, wire );
            return wire;
        }

        void feed( const std::vector<char>& bytes )
        {
            for( char b : bytes )
                mgr.PushByteToRxFifo( static_cast<uint8_t>( b ) );
        }

        // Pop the payload of a completed frame. Frame size on the wire is data+CRC.
        std::vector<uint8_t> drainPayload()
        {
            std::vector<uint8_t> out;
            const int            n = mgr.GetRxFrameSize() - 1;
            for( int i = 0; i < n; ++i )
                out.push_back( mgr.PopUint8FromRxFifo() );
            return out;
        }

        CPktMgr mgr;

    private:
        uint8_t rx[30]{};
        uint8_t tx[40]{};
    };
}
