#ifndef XMODEMPROTOCOL_H
#define XMODEMPROTOCOL_H

#include "protocol/ITransferProtocol.h"

class XmodemProtocol : public ITransferProtocol
{
public:
    XmodemProtocol();

    const char *name() const override;
    void setProtocolCallback(const ProtocolCallback &callback) override;
    void setIoCallbacks(const ReadCallback &read, const WriteCallback &write) override;

    void receive() override;
    void transmit() override;
    void abort() override;

private:
    enum Code
    {
        CodeNone = 0x00,
        CodeSoh  = 0x01,
        CodeStx  = 0x02,
        CodeEot  = 0x04,
        CodeAck  = 0x06,
        CodeNak  = 0x15,
        CodeCan  = 0x18,
        CodeC    = 0x43
    };

    enum Stage
    {
        StageNone,
        StageEstablishing,
        StageTransmitting,
        StageFinishing,
        StageFinished
    };

    Code receivePacket();
    uint32_t packetDataSize(Code code) const;
    bool isPreviousPacket(uint8_t seq) const;
    bool readExact(uint8_t *buff, uint32_t len);
    void sendCode(Code code);
    uint16_t crc16(uint8_t *buff, uint32_t len);
    uint8_t checksum(uint8_t *buff, uint32_t len);
    void reset();

    void receiveStageNone();
    void receiveStageTransmitting();
    void transmitStageNone();
    void transmitStageEstablishing();
    void transmitStageTransmitting();
    void transmitStageFinishing();

    ProtocolCallback protocolCallback;
    ReadCallback readCallback;
    WriteCallback writeCallback;

    Stage stage;
    uint32_t timeCount;
    uint32_t errorCount;
    uint32_t cancelCount;
    uint8_t packetNumber;
    bool useCrc;
    bool receiverOpened;
    bool packetPending;
    uint32_t currentPacketSize;
    uint32_t txPacketSize;
    uint32_t txLength;
    uint8_t rxBuffer[1029];
    uint8_t txBuffer[1029];
};

#endif // XMODEMPROTOCOL_H
