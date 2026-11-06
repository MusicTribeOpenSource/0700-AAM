#include "Defines.hpp"
//#include <optional>
#include <stdio.h>
#include <vector>

#ifndef PKTMGR_H
#define PKTMGR_H

#define PKT_HEADER_AVAILABLE
#define PKT_CRC_AVAILABLE

class CPktMgr
{
public:
    enum enumComChannel
    {
        EN_COM_CHANNEL_USB = 0,
        EN_NUM_COM_CHANNELS
    };

    enum enumFrameState
    {
        EN_FRM_STATE_INIT_1 = 0,
        EN_FRM_STATE_INIT_2 = 1,
        EN_FRM_STATE_TYPE = 2,
        EN_FRM_STATE_DIR = 3,
        EN_FRM_STATE_ID = 4,
        EN_FRM_STATE_SIZE = 5,
        EN_FRM_STATE_DATA = 6,
        EN_FRM_STATE_CRC = 7,
        EN_FRM_EOF = 8
    };

    enum enumPacketType
    {
        EN_PKT_TYPE_SYSTEM = 0,
        EN_PKT_TYPE_BUTTON,
        EN_PKT_TYPE_LED,
        EN_PKT_TYPE_PAD,
        EN_PKT_TYPE_KNOBS,
        EN_PKT_TYPE_MIDI,
        EN_PKT_TYPE_CV,
        EN_PKT_TYPE_SLIDER,
    };

    enum enumPacketDir
    {
        EN_PKT_REQUEST = 0,
        EN_PKT_RESPONSE
    };

    struct HWMsg_t
    {
        enumPacketType   pktType{ enumPacketType::EN_PKT_TYPE_BUTTON };
        enumPacketDir    pktDir{ enumPacketDir::EN_PKT_REQUEST };
        int              pktCmd{ 0 };
        std::vector<int> data;
    };

private:
#define PKTMGR_START_BYTE           5
#define PKTMGR_CRC_BYTE             1
#define PKTMGR_HEADER_EXCLUDING_CRC ( PKTMGR_START_BYTE )
#define PKTMGR_HEADER_INCLUDING_CRC ( PKTMGR_START_BYTE + PKTMGR_CRC_BYTE )

    enum enumComStatus
    {
        EN_COMM_STATUS_SUCCESS = 0,
        EN_COMM_STATUS_TIMEOUT = 1,
        EN_COMM_STATUS_INIT_INVALID = 2,
        EN_COMM_STATUS_CHECKSUM_ERROR = 3,
        EN_COMM_STATUS_CMD_UNKNOWN = 4
    };

    enumComChannel         m_enComChannel;
    enumFrameState         m_enFrmState;
    enumPacketType         m_enPktType;
    uint8_t                m_ui8FrameSize;
    volatile uint8_t       m_ui8RxCommand;
    uint8_t                m_ui8TxCommand;
    volatile enumPacketDir m_enPktDir;

    uint8_t        ui8RxDataFifo[30];
    const uint8_t  mc_ui8RxFifoSize;
    const uint8_t  mc_ui8TxFifoSize;
    uint8_t* const m_ptrui8RxFifo;
    uint8_t* const m_ptrui8TxFifo;

    volatile bool    m_boFrameRcvComplete;
    volatile uint8_t m_ui8RxFifoWritePos;
    volatile uint8_t m_ui8RxFifoReadPos;

    volatile uint8_t m_ui8TxFifoWritePos;
    volatile uint8_t m_ui8TxFifoReadPos;
    uint8_t          m_ui8ArrTransmitBuffer[40];

    // Bytes of the frame being received, from 'M' up to the last byte before the CRC.
    // Per instance: a static here let one link's abandoned frame corrupt another's CRC.
    std::vector<char> m_rxCrcBytes;

public:
    void    PushByteToRxFifo( uint8_t f_ui8Data );
    uint8_t PopUint8FromRxFifo( void );
    uint8_t PopUint8FromTxFifo( void );

    void PushUint8ToTxFifo( uint8_t f_ui8Data );

    void StartTransmissionAndClearRxFifo( void );
    bool IsFrameRcvComplete() { return m_boFrameRcvComplete; }
    void SetFrameRcvComplete( bool f_boState ) { m_boFrameRcvComplete = f_boState; }
    void Init();
    void Reset( CPktMgr::HWMsg_t& f_HwMsg );
    void ClearReceiver( void );
    void BuildAndTransmitPacket( CPktMgr::HWMsg_t& f_HwMsg, std::vector<char>& );

    CPktMgr( const enumComChannel& f_enComChannel, ///< Communication channel to use
        uint8_t* const        f_ptrui8RxFifo, ///< Buffer for receiving of data
        const uint16_t& f_ui8RxFifoSize, ///< Size of send buffer in byte
        uint8_t* const        f_ptrui8TxFifo, ///< Buffer for sending of data
        const uint16_t& f_ui8TxFifoSize, ///< Size of send buffer in byte
        const uint16_t& f_ui16ByteTimeoutMs ///< Timeout for reception of one byte
    )
        : m_enComChannel( f_enComChannel )
        , mc_ui8RxFifoSize( f_ui8RxFifoSize > 255 ? 255 : (uint8_t)f_ui8RxFifoSize )
        , mc_ui8TxFifoSize( f_ui8TxFifoSize > 255 ? 255 : (uint8_t)f_ui8TxFifoSize )
        , m_ptrui8RxFifo( f_ptrui8RxFifo )
        , m_ptrui8TxFifo( f_ptrui8TxFifo )
    {}

    static uint8_t                 CrcGen( std::vector<char>& );

    inline uint8_t                 GetRxCommand( void ) { return m_ui8RxCommand; }
    inline const enumPacketType& GetRxPktType( void ) const { return m_enPktType; }
    inline const uint8_t& GetTxCommand( void ) const { return m_ui8TxCommand; }
    inline volatile uint8_t& GetRxReadPoint( void ) { return m_ui8RxFifoReadPos; }
    inline volatile uint8_t& GetRxWritePoint( void ) { return m_ui8RxFifoWritePos; }
    inline const uint8_t& GetRxFrameSize( void ) const { return m_ui8FrameSize; }
    inline volatile enumPacketDir& GetPktDir( void ) { return m_enPktDir; }
    inline void                    SetPktDir( enumPacketDir f_enPktDir ) { m_enPktDir = f_enPktDir; }
};

#endif
