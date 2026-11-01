
#include <moodycamel/moodycamel.h>
#include <juce_serialport/juce_serialport.h>

#include "input_events.h"
#include "contracts/dcl/dcl_contracts.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "kick_SimulatorInterface.h"

#include "ComConfig.hpp"

namespace Controls
{
    std::atomic<bool> shiftBtnState{ false };
    std::atomic<bool> halfLevelState{ false };

    ///
    /// Static members...
    static std::array<uint8_t, BUFFER_RX_SIZE> ms_arrui8RxBufferUsb;
    static std::array<uint8_t, BUFFER_RX_SIZE> ms_arrui8TxBufferUsb;
    static SimulatorInterface*                 simInterface{ nullptr };
    static CPktMgr                             ms_objPktMgrUsb{ CPktMgr::EN_COM_CHANNEL_USB, ms_arrui8RxBufferUsb.data(), BUFFER_RX_SIZE, ms_arrui8TxBufferUsb.data(), BUFFER_RX_SIZE, 60 };

    sigslot::signal<CPktMgr::HWMsg_t&> Controls::InputEvents::dispatchHardwareEvent = {};

    ///
    /// IMPL...
    class HwSerialData : private juce::ChangeListener
        , private juce::Thread
        , private juce::AsyncUpdater
    {
    public:
        HwSerialData()
            : Thread( "Queue" ) { startThread(); }

        ~HwSerialData()
        {
#if !defined( __APPLE__ ) || KICK_EMU
            close();
#endif
        }

        void connect()
        {
#if( JUCE_LINUX || __linux__ || KICK_EMU )
#if KICK_EMU
            // The emulator's control surface is a pty; KICK_SERIAL_PORT names its slave side.
            juce::String serialDev = juce::SystemStats::getEnvironmentVariable( "KICK_SERIAL_PORT", {} );
            if( serialDev.isNotEmpty() && juce::File( serialDev ).exists() )
            {
#else
            if( juce::File( "/dev/ttymxc0" ).exists() )
            {
                juce::StringPairArray portlist = SerialPort::getSerialPortPaths();

                if( !portlist.size() )
                    return;

                bool         hasSerialPort = false;
                juce::String serialDev     = "/dev/ttymxc0";
                serialDev                  = "/dev/ttymxc0";
#endif

                auto serialDebug = []( juce::String str1, juce::String str2 ) {
                    DBG( str1 );
                    DBG( str2 );
                };
                SerialPortConfig config( 230400, 8, SerialPortConfig::SERIALPORT_PARITY_NONE, SerialPortConfig::STOPBITS_1, SerialPortConfig::FLOWCONTROL_NONE );

    #if( JUCE_LINUX || __linux__ || KICK_EMU )
                juce::String devPath = serialDev;
    #endif
                if( sPort )
                {
                    input.reset( nullptr );
                    output.reset( nullptr );
                    sPort.reset( nullptr );
                }
                {
    #if( JUCE_LINUX || __linux__ || KICK_EMU )
                    CPktMgr::HWMsg_t hwMsg;
                    hwMsg.pktType = CPktMgr::EN_PKT_TYPE_SYSTEM;
                    hwMsg.pktCmd  = contracts::DeviceControlContracts::SysCmd::EN_SYS_COMM_INIT;
                    hwMsg.pktDir  = CPktMgr::EN_PKT_REQUEST;

                    sPort.reset( new SerialPort( devPath, config, serialDebug ) );
                    input.reset( new SerialPortInputStream( sPort.get() ) );
                    input->addChangeListener( this );
                    input->setNotify( SerialPortInputStream::NOTIFY_ALWAYS );

                    hwMsg.data.push_back( (int)contracts::DeviceControlContracts::SysCommCmd::EN_SYS_COMM_CONNECTED );
                    output.reset( new SerialPortOutputStream( sPort.get() ) );
                    Controls::InputEvents::sendMessageToHardware( hwMsg );
    #endif
                }
            }
            else
            {
                DBG( "Serial Not Found" );
#if KICK_EMU
                juce::Logger::writeToLog( "[emu] KICK_SERIAL_PORT not set or missing; start emu/run.sh or KICK Surface first" );
#endif
            }
#endif
        }

        void close()
        {
            if( sPort ) sPort->close();
            stopThread( 1000 );
            waitForThreadToExit( 1000 );
        }

        void reconnect()
        {
#if !defined( __APPLE__ ) || KICK_EMU
            close();
            connect();
#endif
        }

        void sendMessageToControlSurface( CPktMgr::HWMsg_t& f_HwMsg )
        { // Enqueue in the queue...
            txQueue.enqueue( f_HwMsg );
            //semaphoreQueue.signal();
            triggerAsyncUpdate();
        }

        void messageReceivedFromControlSurface( CPktMgr::HWMsg_t& f_rxMsg )
        {
            juce::String log;
            log += " Data[0] : " + juce::String( f_rxMsg.data[0] ) + " Cmd : " + juce::String( f_rxMsg.pktCmd ) + +" type : " + juce::String( f_rxMsg.pktType );
            juce::Logger::writeToLog( log );

            Controls::InputEvents::dispatchHardwareEvent( f_rxMsg );
        }

    private:
        void handleAsyncUpdate() override { txEvent.signal(); }
        void sendSerialOutData( const std::vector<char>& data )
        {
#if( JUCE_LINUX || __linux__ || KICK_EMU )
            if( !output )
                return;

            juce::MemoryBlock outData;
            uint8_t           f_txSize = data.size();
            //if(f_txSize > 0)
            //{
            //p + (4 int size) + (hw id) + data + \n
            outData.ensureSize( f_txSize );
            //DBG("======TX==Message======");
            for( unsigned int i = 0; i < f_txSize; i++ )
            {
                outData[i] = data[i];
                //DBG("TxData: " << (int)outData[i]);
            }

            output->write( outData.getData(), outData.getSize() );
            output->flush();
            //}
#endif
        }

        void changeListenerCallback( juce::ChangeBroadcaster* source ) override
        {
            int rxLength = (int)input->getTotalLength();
            // DBG("Receive Length: " << rxLength);
            if( !input->getTotalLength() || input->getTotalLength() < 6 )
                return;

            juce::MemoryBlock serialData( (size_t)input->getTotalLength(), true );
            size_t            len = (size_t)input->getTotalLength();

            input->read( serialData.getData(), (int)input->getTotalLength() );

            if( len < 4 )
            {
                DBG( "Size is less then 3 bytes" );
                return;
            }

            char* byteArray = serialData.begin();

            CPktMgr& refobjFrameHdl = ms_objPktMgrUsb;

            while( rxLength )
            {
                int pktIndex     = 0;
                int pktOffset    = 0;
                int packetLength = byteArray[5 + pktIndex] + 7;
                // DBG("Packet Length: " << packetLength);

                if( ( rxLength >= packetLength ) && ( byteArray[0] == 'M' ) && ( byteArray[1] == 'T' ) )
                {
                    while( packetLength )
                    {
                        if( packetLength > 1 )
                        {
                            refobjFrameHdl.PushByteToRxFifo( byteArray[0] );
                        }
                        byteArray++;
                        packetLength--;
                        rxLength--;
                    }
                }
                else
                {
                    rxLength = 0;
                }

                if( refobjFrameHdl.IsFrameRcvComplete() == TRUE )
                {
                    refobjFrameHdl.SetFrameRcvComplete( FALSE );
                    CPktMgr::HWMsg_t rxMsg;
                    std::vector<int> tempData;
                    tempData.push_back( 1 );
                    rxMsg.pktType = refobjFrameHdl.GetRxPktType();
                    rxMsg.pktCmd  = refobjFrameHdl.GetRxCommand();
                    rxMsg.pktDir  = refobjFrameHdl.GetPktDir();

                    int size = (int)refobjFrameHdl.GetRxFrameSize();

                    /*DBG( "===========RX Message==========" );
                    DBG( "Rx Type: " << rxMsg.pktType << "Rx pktId: " << rxMsg.pktCmd << "Rx size: " << size );
                   */
                    //juce::Logger::writeToLog( "Rx Type: " + juce::String(rxMsg.pktType) + "Rx pktId: " + juce::String(rxMsg.pktCmd) + "Rx size: " + juce::String(size ));

                    int index = 0;
                    while( size - 1 )
                    {
                        rxMsg.data.push_back( (int)refobjFrameHdl.PopUint8FromRxFifo() );
                        index++;
                        size--;
                    }
                    refobjFrameHdl.ClearReceiver();
                    // DBG("Pad Num: "<<  rxMsg.data[0]);

                    if( rxMsg.pktDir == CPktMgr::enumPacketDir::EN_PKT_REQUEST )
                    {
                        CPktMgr::HWMsg_t ackTxMsg;
                        //ackTxMsg.pktType = rxMsg.pktType;
                        //ackTxMsg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_RESPONSE;
                        //ackTxMsg.pktId     = rxMsg.pktId;
                        //ackTxMsg.data      = rxMsg.data;

                        //DBG("Ack Msg");
                        //sendMessageToHardware( ackTxMsg );

                        if( contracts::DclContractsHelper::isButtonMessage( rxMsg ) )
                        {
                            DBG( "Button state = " << contracts::DclContractsHelper::getButtonState( rxMsg ) );
                            if( ( contracts::DeviceControlContracts::EN_SHIFT == contracts::DclContractsHelper::getButtonCode( rxMsg ) ) )
                                Controls::shiftBtnState.store( contracts::DclContractsHelper::getButtonState( rxMsg ) );
                        }
                        // juce::String log;
                        // log += "Cmd:" + juce::String( rxMsg.pktCmd ) + " " + "type:" + juce::String( rxMsg.pktType );
                        // juce::Logger::writeToLog( log );
                        messageReceivedFromControlSurface( rxMsg );
                    }
                    else
                    {
                        //If the data is Ack (response) for the App message, ignore
                        // Do Nothing
                    }
                }
                else
                {
#if defined( DEBUG_PRINTS ) && ( DEBUG_PRINTS > 0 )
                    DBG( "Data Frame Error" );
#endif
                    refobjFrameHdl.ClearReceiver();
                }
            }
        }

        void run() override
        {
            static CPktMgr::HWMsg_t  item;
            static std::vector<char> txData;
            while( !threadShouldExit() )
            {
                txEvent.wait( 1000 );
                //semaphoreQueue.wait( 1 );

                while( txQueue.try_dequeue( item ) && !threadShouldExit() )
                {
#if( JUCE_LINUX || __linux__ || KICK_EMU )
                    txData.clear();
                    ms_objPktMgrUsb.BuildAndTransmitPacket( item, txData );
                    sendSerialOutData( txData );

                    item.data.clear();
#endif
                }
            } //DBG("Out of While");
        }

    private:
        juce::WaitableEvent txEvent;

        std::unique_ptr<SerialPort>             sPort;
        std::unique_ptr<SerialPortInputStream>  input;
        std::unique_ptr<SerialPortOutputStream> output;

        moodycamel::ConcurrentQueue<CPktMgr::HWMsg_t> txQueue;
    };
    static std::unique_ptr<HwSerialData> _FPPort;

    /// <summary>
    ///
    /// Class Controls::InputEvents
    ///
    /// </summary>

    void Controls::InputEvents::initialize()
    {
#ifdef SIM_BUILD
        //creating multiple instance of SimulatorInterface using factoty is somehow clearing the connected signals need to find the reason
        simInterface = Factory<SimulatorInterface>::makeObject( 0 );
        simInterface->hwInputReceived.connect( []( int id, const std::vector<char>& data ) {
            Controls::InputEvents::dispatchHardwareEvent( id, data );
        } );
#else
        if( !_FPPort )
        {
            _FPPort.reset( new HwSerialData );
            ms_objPktMgrUsb.Init();
        }
        _FPPort->connect();
        // Send Rebooted command to GD
#endif
    }

    void Controls::InputEvents::reconnect()
    {
#if !defined( __APPLE__ ) || KICK_EMU
        if( _FPPort ) _FPPort->reconnect();
#endif
    }

    void Controls::InputEvents::shutdown()
    {
        _FPPort.reset();
    }

    void Controls::InputEvents::messageReceived( CPktMgr::HWMsg_t& f_rxMsg )
    {
        if( _FPPort ) _FPPort->messageReceivedFromControlSurface( f_rxMsg );
    }

    void Controls::InputEvents::sendMessageToHardware( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( f_HwMsg.pktType == CPktMgr::enumPacketType::EN_PKT_TYPE_SYSTEM )
        {
            if( f_HwMsg.pktCmd == contracts::DeviceControlContracts::EN_SYS_RECONNECT )
            {
                reconnect();
                return;
            }
            if( f_HwMsg.pktCmd == contracts::DeviceControlContracts::EN_SYS_SHUTDOWN )
            {
                shutdown();
                return;
            }
        }

#ifdef SIM_BUILD
        if( simInterface ) simInterface->sendMessageToHW( f_HwMsg );
#endif
        if( _FPPort ) _FPPort->sendMessageToControlSurface( f_HwMsg );
        else
        {
            juce::Logger::outputDebugString( "Null Serial Port " );
        }
    }
}
