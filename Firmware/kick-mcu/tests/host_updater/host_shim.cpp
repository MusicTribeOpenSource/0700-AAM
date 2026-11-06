// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Compiles the host's firmware updater as it is in the tree and puts it behind HostShim.
//
// kick_FirmwareUpdate.cpp is included, not copied, so a change to it is a change to what this
// harness runs. What replaces the rest of the host:
//   - the DclSignals statics and DclContractsHelper::SendSwUpdateCmdMessage, which in the real
//     build reach the packet layer through ipc::MTIPCJson and the DCL process. The stand-ins build
//     the same HWMsg_t (SYSTEM, request, SW_UPDATE, data) and encode it with the real PktMgr.
//   - USBFunctionFS and the fs alias, which only the Linux image-scan code reads.
//   - HwPref's constructor and run(): MCUUpdate does not use them, and the real ones open the
//     serial port and a power-button thread.
//   - the receive side of SWUpdateScreen::handleCommand, copied below with its source named, since
//     that class is a JUCE component.
//
// The MCUUpdate state machine is private, so the header is included with private made public
// after every system header has been read.

// JUCE and the standard library first, with private still private.
#include "juce_core/juce_core.h"

#include <atomic>
#include <cstdlib>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>

#include "PktMgr/PktMgr.hpp"
#include "contracts/dcl/dcl_contracts.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/dcl/dcl_signals.h"
#include "kick_Hwcommon.h"
#include "kick_Hwpref.h"

#include "host_shim.h"

// ipc::MTIPCJson is only forward declared by the host headers. The stand-in carries what the real
// one is built from in prepareHWMessage: the command's data.
namespace ipc
{
class MTIPCJson
{
public:
    std::vector<int> data;
};
} // namespace ipc

namespace contracts
{
sigslot::signal<std::shared_ptr<ipc::MTIPCJson>> DclSignals::sendCommandToDcl;
sigslot::signal<CPktMgr::HWMsg_t&>               DclSignals::createAndSendCommandToDcl;
sigslot::signal<CPktMgr::HWMsg_t&>               DclSignals::hardwareInputTriggered;
sigslot::signal<>                                DclSignals::refreshPadLeds;
sigslot::signal<int>                             DclSignals::onMainModeTabSelected;

// Same message as dcl_contracts_helper.cpp: prepareHWMessage( SYSTEM, REQUEST, EN_SYS_SW_UPDATE, { f_data } ).
std::shared_ptr<ipc::MTIPCJson> DclContractsHelper::SendSwUpdateCmdMessage( int f_data )
{
    auto message  = std::make_shared<ipc::MTIPCJson>();
    message->data = { f_data };
    return message;
}
} // namespace contracts

namespace mt
{
HwPref::HwPref()
    : juce::Thread( "host-updater-harness-hwpref" )
{
}
void HwPref::run() {}

// The Linux image handling in FirmwareUpdate (decrypt, extract, scan, compare). MCUUpdate never
// reaches it; the definitions exist so the file links, and a call is a harness bug.
bool HwPref::extractImage( std::string ) { std::abort(); }
bool HwPref::getLinuxVersion( std::string&, linuxFwInfo& ) { std::abort(); }
bool HwPref::getVersionfromFile( std::string, linuxFwInfo& ) { std::abort(); }
bool HwPref::decryptopenSSLImage( std::string ) { std::abort(); }
std::vector<std::pair<std::string, linuxFwInfo>> HwPref::getSWUFileswithVersion( const std::string&, const std::string& ) { std::abort(); }
VersionComparisonResult HwPref::isLinuxFirmwareVersionLess( const linuxFwInfo&, const linuxFwInfo& ) { std::abort(); }

// Only the PC-mode status poll in validateImage() asks the USB gadget anything.
class USBFunctionFS
{
public:
    static USBFunctionFS& getUSBInstance()
    {
        static USBFunctionFS instance;
        return instance;
    }
    USB_STATUS getStatus() { return U_INIT; }
};
} // namespace mt

// The real build has this alias from a header this harness does not pull in.
namespace fs = std::filesystem;

#define private public
#include "../../../../Kick/modules/hardware/source/kick_FirmwareUpdate.cpp"
#undef private

namespace host_updater
{
namespace
{
constexpr size_t kTrailer = 1; ///< the MCU ends every frame with '\n'; the host drops it before PktMgr
}

struct HostShim::Impl
{
    // The host's PktMgr, built the way input_events.cpp builds it.
    uint8_t rx[20]{};
    uint8_t tx[20]{};
    CPktMgr mgr{ CPktMgr::EN_COM_CHANNEL_USB, rx, 20, tx, 20, 60 };

    std::mutex                                encodeMutex;
    std::function<void( const Bytes& )>       transmit;
    sigslot::scoped_connection                initiate;
    sigslot::scoped_connection                data;
    mutable std::mutex                        logMutex;
    std::vector<SentFrame>                    sent;
    std::vector<StatusEvent>                  events;
    Bytes                                     inbox; ///< bytes of a frame still arriving

    std::thread       worker;
    std::atomic<bool> done{ false };
    std::atomic<int>  returned{ 0 };

    Impl() { mgr.Init(); }

    void emit( CPktMgr::HWMsg_t& message )
    {
        std::vector<char> bytes;
        {
            std::lock_guard<std::mutex> lock( encodeMutex );
            mgr.BuildAndTransmitPacket( message, bytes );
        }
        {
            std::lock_guard<std::mutex> lock( logMutex );
            sent.push_back( { (int)message.pktType, message.pktCmd, message.data } );
        }
        if( transmit )
            transmit( Bytes( bytes.begin(), bytes.end() ) );
    }

    // The receive side of SWUpdateScreen::handleCommand (kick_ui/swupdate/kick_SWUpdateScreen.cpp):
    // 1 starts the transfer, 6 is a failure, anything else is an acknowledgement.
    void handleCommand( const CPktMgr::HWMsg_t& message )
    {
        const bool isSwUpdate = message.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM && message.pktCmd == contracts::DeviceControlContracts::SysCmd::EN_SYS_SW_UPDATE;
        if( !isSwUpdate )
            return;

        mt::MCUUpdateAction flag = mt::MCUUpdateAction::EN_SW_UPDATE_FAILURE;
        if( message.data[0] == 1 )
            flag = mt::MCUUpdateAction::EN_SW_UPDATE_BEGIN_TRANSFER;
        else if( message.data[0] == 6 )
            flag = mt::MCUUpdateAction::EN_SW_UPDATE_FAILURE;
        else
            flag = mt::MCUUpdateAction::EN_SW_UPDATE_ACK;
        mt::FirmwareUpdate::getSwupdateInstance().setMcuUpdateState( flag );
    }
};

HostShim::HostShim()
    : impl_( std::make_unique<Impl>() )
{
    Impl* impl = impl_.get();
    impl->initiate = contracts::DclSignals::sendCommandToDcl.connect( [impl]( std::shared_ptr<ipc::MTIPCJson> command ) {
        CPktMgr::HWMsg_t message;
        message.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM;
        message.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        message.pktCmd  = contracts::DeviceControlContracts::SysCmd::EN_SYS_SW_UPDATE;
        message.data    = command->data;
        impl->emit( message );
    } );
    impl->data = contracts::DclSignals::createAndSendCommandToDcl.connect( [impl]( CPktMgr::HWMsg_t& message ) { impl->emit( message ); } );

    mt::FirmwareUpdate::getSwupdateInstance().setSwUpdatecallback( [impl]( mt::swupdatemsg_t& status ) {
        std::lock_guard<std::mutex> lock( impl->logMutex );
        impl->events.push_back( { (int)status.command_status, (double)status.totalper } );
    } );
}

HostShim::~HostShim()
{
    if( impl_->worker.joinable() )
        impl_->worker.join();
    impl_->initiate.disconnect();
    impl_->data.disconnect();
    mt::FirmwareUpdate::getSwupdateInstance().setSwUpdatecallback( nullptr );
}

void HostShim::setTransmit( std::function<void( const Bytes& )> transmit ) { impl_->transmit = std::move( transmit ); }

void HostShim::onMcuBytes( const Bytes& bytes )
{
    Impl& s = *impl_;
    s.inbox.insert( s.inbox.end(), bytes.begin(), bytes.end() );

    // A frame is header(5) + length + payload + crc(1) + trailer, which input_events.cpp reads as
    // size + 7 bytes. The length is the sixth byte.
    for( ;; )
    {
        if( s.inbox.size() < 6 )
            return;
        const size_t total = (size_t)s.inbox[5] + 7;
        if( s.inbox.size() < total )
            return;

        s.mgr.ClearReceiver();
        s.mgr.SetFrameRcvComplete( FALSE );
        for( size_t i = 0; i + kTrailer < total; ++i )
            s.mgr.PushByteToRxFifo( s.inbox[i] );
        if( s.mgr.IsFrameRcvComplete() )
        {
            CPktMgr::HWMsg_t message;
            message.pktType = s.mgr.GetRxPktType();
            message.pktDir  = CPktMgr::enumPacketDir::EN_PKT_RESPONSE;
            message.pktCmd  = s.mgr.GetRxCommand();
            for( int n = (int)s.mgr.GetRxFrameSize() - 1; n > 0; --n )
                message.data.push_back( s.mgr.PopUint8FromRxFifo() );
            s.mgr.ClearReceiver();
            s.mgr.SetFrameRcvComplete( FALSE );
            if( !message.data.empty() )
                s.handleCommand( message );
        }
        s.inbox.erase( s.inbox.begin(), s.inbox.begin() + (std::ptrdiff_t)total );
    }
}

void HostShim::start( const std::string& hexPath )
{
    Impl* impl    = impl_.get();
    impl->done    = false;
    impl->worker  = std::thread( [impl, hexPath] {
        juce::String path( hexPath );
        impl->returned = mt::FirmwareUpdate::getSwupdateInstance().MCUUpdate( path );
        impl->done     = true;
    } );
}

bool HostShim::finished() const { return impl_->done.load(); }
int  HostShim::result() const { return impl_->returned.load(); }

void HostShim::giveUp() { mt::FirmwareUpdate::getSwupdateInstance().setMcuUpdateState( mt::MCUUpdateAction::EN_SW_UPDATE_FAILURE ); }

void HostShim::join()
{
    if( impl_->worker.joinable() )
        impl_->worker.join();
}

std::vector<StatusEvent> HostShim::statusEvents() const
{
    std::lock_guard<std::mutex> lock( impl_->logMutex );
    return impl_->events;
}

std::vector<HostShim::SentFrame> HostShim::sentFrames() const
{
    std::lock_guard<std::mutex> lock( impl_->logMutex );
    return impl_->sent;
}

} // namespace host_updater
