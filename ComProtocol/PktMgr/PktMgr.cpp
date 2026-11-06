#include "PktMgr.hpp"

#ifdef KICK_MCU
    #include <Hardware.hpp>
    #include <TaskHdl.hpp>
    #include <ComHdl.hpp>
#endif

void CPktMgr::Init( void )
{
    m_ui8FrameSize       = 0;
    m_ui8RxCommand       = 0;
    m_ui8TxCommand       = 0;
    m_ui8RxFifoWritePos  = 0;
    m_ui8RxFifoReadPos   = 0;
    m_ui8TxFifoWritePos  = 0;
    m_ui8TxFifoReadPos   = 0;
    m_boFrameRcvComplete = FALSE;
    m_enPktDir           = EN_PKT_REQUEST;
    m_rxCrcBytes.clear();
    // Allocate once here so the receive path never allocates (ISR/driver context on the MCU).
    m_rxCrcBytes.reserve( (size_t)mc_ui8RxFifoSize + 6 );

#ifdef PKT_HEADER_AVAILABLE
    m_enFrmState = EN_FRM_STATE_INIT_1;
#else
    m_enFrmState = EN_FRM_STATE_TYPE;
#endif
}

void CPktMgr::ClearReceiver( void )
{
    m_ui8RxFifoWritePos  = 0;
    m_ui8RxFifoReadPos   = 0;
    m_enPktDir           = EN_PKT_REQUEST;
    m_boFrameRcvComplete = FALSE;
    m_enFrmState         = EN_FRM_STATE_INIT_1;
    m_rxCrcBytes.clear();
}

void CPktMgr::PushByteToRxFifo( uint8_t f_ui8Data )
{
    std::vector<char>& rxBytes           = m_rxCrcBytes;
    bool               boRcvdCompleteMsg = FALSE;

    switch( m_enFrmState )
    {
        case EN_FRM_STATE_INIT_1:
            // Line noise before the start byte is skipped, not folded into the CRC.
            // While a complete frame awaits the consumer, a new frame is ignored whole so
            // it cannot overwrite the pending frame's header or payload.
            if( f_ui8Data == 'M' && !m_boFrameRcvComplete )
            {
                rxBytes.clear();
                rxBytes.push_back( f_ui8Data );
                m_enFrmState = EN_FRM_STATE_INIT_2;
            }
            break;
        case EN_FRM_STATE_INIT_2:
            if( f_ui8Data == 'T' )
            {
                rxBytes.push_back( f_ui8Data );
                m_enFrmState = EN_FRM_STATE_TYPE;
            }
            else if( f_ui8Data != 'M' )
            {
                // 'M' followed by 'M' is still a possible start; anything else is noise.
                rxBytes.clear();
                m_enFrmState = EN_FRM_STATE_INIT_1;
            }
            break;
        case EN_FRM_STATE_TYPE:
            m_enPktType  = (enumPacketType)f_ui8Data;
            m_enFrmState = EN_FRM_STATE_DIR;
            rxBytes.push_back( f_ui8Data );
            break;
        case EN_FRM_STATE_DIR:
            rxBytes.push_back( f_ui8Data );
            m_enPktDir   = (enumPacketDir)f_ui8Data;
            m_enFrmState = EN_FRM_STATE_ID;
            break;
        case EN_FRM_STATE_ID:
            rxBytes.push_back( f_ui8Data );
            m_ui8RxCommand = f_ui8Data;
            m_enFrmState   = EN_FRM_STATE_SIZE;
            break;
        case EN_FRM_STATE_SIZE:
            rxBytes.push_back( f_ui8Data );
            m_ui8FrameSize = f_ui8Data;
            if( m_ui8FrameSize > mc_ui8RxFifoSize + PKTMGR_CRC_BYTE )
            {
                // Payload would overrun the RX FIFO: drop the frame and resync.
                rxBytes.clear();
                m_enFrmState = EN_FRM_STATE_INIT_1;
            }
            else
            {
                // INIT_1 admits a frame only when none is pending, so the FIFO is free.
                m_ui8RxFifoWritePos = 0;
                m_ui8RxFifoReadPos  = 0;
                // The size byte counts payload plus the CRC byte, so 0 or 1 means no payload.
                m_enFrmState = ( m_ui8FrameSize <= PKTMGR_CRC_BYTE ) ? EN_FRM_STATE_CRC : EN_FRM_STATE_DATA;
            }
            break;
        case EN_FRM_STATE_DATA:
            rxBytes.push_back( f_ui8Data );
            m_ptrui8RxFifo[m_ui8RxFifoWritePos] = f_ui8Data;
            m_ui8RxFifoWritePos++;
            if( m_ui8RxFifoWritePos == ( m_ui8FrameSize - 1 ) )
            {
                m_enFrmState = EN_FRM_STATE_CRC;
            }
            break;
        case EN_FRM_STATE_CRC:
            boRcvdCompleteMsg = ( CrcGen( rxBytes ) == f_ui8Data );
            rxBytes.clear();
            // Good or bad, the frame is over: hunt for the next start byte so a
            // rejected frame cannot wedge the receiver.
            m_enFrmState = EN_FRM_STATE_INIT_1;
            break;
    }

    if( boRcvdCompleteMsg )
    {
        m_boFrameRcvComplete = TRUE;
    }
}

uint8_t CPktMgr::PopUint8FromRxFifo( void )
{
    return m_ptrui8RxFifo[m_ui8RxFifoReadPos++];
}

uint8_t CPktMgr::PopUint8FromTxFifo( void )
{
    return m_ptrui8TxFifo[m_ui8TxFifoReadPos++];
}

void CPktMgr::PushUint8ToTxFifo( uint8_t f_ui8Data )
{
    m_ptrui8TxFifo[m_ui8TxFifoWritePos++] = f_ui8Data;
}

uint8_t CPktMgr::CrcGen( std::vector<char>& f_txData )
{
    uint8_t ui8CrcValue = 0xff;
    uint8_t j;
    const size_t len = f_txData.size();

    for( size_t i = 0; i < len; i++ )
    {
        ui8CrcValue ^= f_txData[i];
        for( j = 0; j < 8; j++ )
        {
            if( ( ui8CrcValue & 0x80 ) != 0 )
                ui8CrcValue = (uint8_t)( ( ui8CrcValue << 1 ) ^ 0x31 );
            else
                ui8CrcValue <<= 1;
        }
    }
    return ui8CrcValue;
}

void CPktMgr::Reset( CPktMgr::HWMsg_t& f_HwMsg )
{
    f_HwMsg.pktType = (enumPacketType)0;
    f_HwMsg.pktDir  = (enumPacketDir)0;
    f_HwMsg.pktCmd  = 0;
    f_HwMsg.data.clear();
}

void CPktMgr::BuildAndTransmitPacket( CPktMgr::HWMsg_t& f_HwMsg, std::vector<char>& f_txData )
{
    uint8_t ui8TxBufferIndex = 0;
    int     sizeValue        = (int)f_HwMsg.data.size();
    int     txIndex          = 0;

    f_txData.push_back( 'M' );
    f_txData.push_back( 'T' );
    f_txData.push_back( f_HwMsg.pktType );
    f_txData.push_back( f_HwMsg.pktDir );
    f_txData.push_back( f_HwMsg.pktCmd );
    f_txData.push_back( sizeValue + PKTMGR_CRC_BYTE );

    while( sizeValue )
    {
        f_txData.push_back( f_HwMsg.data[txIndex] );
        sizeValue--;
        txIndex++;
    }

    f_txData.push_back( CrcGen( f_txData ) );
    //f_txData.push_back('\n');

    m_ui8TxCommand = m_ui8ArrTransmitBuffer[EN_FRM_STATE_ID];
}
