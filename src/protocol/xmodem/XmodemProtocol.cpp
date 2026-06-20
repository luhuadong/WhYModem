#include "protocol/xmodem/XmodemProtocol.h"
#include <string.h>

namespace {
const uint32_t PacketSize = 128;
const uint32_t Packet1KSize = 1024;
const uint32_t PacketOverheadCrc = 5;
const uint32_t PacketOverheadChecksum = 4;
const uint32_t TimeMax = 600;
const uint32_t ErrorMax = 10;
const uint32_t HandshakeInterval = 100;
const uint32_t CanAbortCount = 2;
}

XmodemProtocol::XmodemProtocol()
{
    reset();
}

const char *XmodemProtocol::name() const
{
    return "Xmodem";
}

void XmodemProtocol::setProtocolCallback(const ProtocolCallback &callback)
{
    protocolCallback = callback;
}

void XmodemProtocol::setIoCallbacks(const ReadCallback &read, const WriteCallback &write)
{
    readCallback = read;
    writeCallback = write;
}

void XmodemProtocol::receive()
{
    switch(stage)
    {
        case StageNone:
            receiveStageNone();
            break;
        case StageTransmitting:
            receiveStageTransmitting();
            break;
        default:
            break;
    }
}

void XmodemProtocol::transmit()
{
    switch(stage)
    {
        case StageNone:
            transmitStageNone();
            break;
        case StageEstablishing:
            transmitStageEstablishing();
            break;
        case StageTransmitting:
            transmitStageTransmitting();
            break;
        case StageFinishing:
            transmitStageFinishing();
            break;
        default:
            break;
    }
}

void XmodemProtocol::abort()
{
    uint8_t buff[5] = {CodeCan, CodeCan, CodeCan, CodeCan, CodeCan};
    if(writeCallback)
    {
        writeCallback(buff, sizeof(buff));
    }
    reset();
}

void XmodemProtocol::reset()
{
    stage = StageNone;
    timeCount = 0;
    errorCount = 0;
    cancelCount = 0;
    packetNumber = 1;
    useCrc = true;
    receiverOpened = false;
    packetPending = false;
    currentPacketSize = PacketSize;
    txPacketSize = PacketSize;
    txLength = 0;
    memset(rxBuffer, 0, sizeof(rxBuffer));
    memset(txBuffer, 0, sizeof(txBuffer));
}

void XmodemProtocol::receiveStageNone()
{
    stage = StageTransmitting;
    timeCount = 0;
    errorCount = 0;
    cancelCount = 0;
    useCrc = true;
    sendCode(CodeC);
}

void XmodemProtocol::receiveStageTransmitting()
{
    switch(receivePacket())
    {
        case CodeSoh:
        case CodeStx:
        {
            const uint8_t seq = rxBuffer[1];
            const uint8_t seqNeg = rxBuffer[2];
            const bool seqOk = (seq == packetNumber) && ((uint8_t)(seq + seqNeg) == 0xFF);
            const uint32_t crcIndex = 3 + currentPacketSize;
            uint16_t rxCrc = ((uint16_t)rxBuffer[crcIndex] << 8) | rxBuffer[crcIndex + 1];
            const bool crcOk = useCrc ? (rxCrc == crc16(&(rxBuffer[3]), currentPacketSize))
                                      : (rxBuffer[crcIndex] == checksum(&(rxBuffer[3]), currentPacketSize));

            if(seqOk && crcOk && protocolCallback)
            {
                if(!receiverOpened)
                {
                    uint32_t len = 0;
                    if(protocolCallback(StatusEstablish, 0, &len) != ReplyAck)
                    {
                        sendCode(CodeCan);
                        protocolCallback(StatusError, 0, 0);
                        reset();
                        return;
                    }
                    receiverOpened = true;
                }

                uint32_t len = currentPacketSize;
                if(protocolCallback(StatusTransmit, &(rxBuffer[3]), &len) == ReplyAck)
                {
                    sendCode(CodeAck);
                    ++packetNumber;
                    timeCount = 0;
                    errorCount = 0;
                    cancelCount = 0;
                }
                else
                {
                    sendCode(CodeCan);
                    protocolCallback(StatusError, 0, 0);
                    reset();
                }
            }
            else if(isPreviousPacket(seq) && crcOk)
            {
                sendCode(CodeAck);
                timeCount = 0;
                cancelCount = 0;
            }
            else
            {
                sendCode(CodeNak);
                if(++errorCount > ErrorMax)
                {
                    protocolCallback(StatusTimeout, 0, 0);
                    reset();
                }
            }
            break;
        }

        case CodeEot:
            sendCode(CodeAck);
            if(protocolCallback)
            {
                protocolCallback(StatusFinish, 0, 0);
            }
            reset();
            break;

        case CodeCan:
            if(++cancelCount >= CanAbortCount)
            {
                if(protocolCallback)
                {
                    protocolCallback(StatusAbort, 0, 0);
                }
                reset();
            }
            break;

        default:
            ++timeCount;
            if(receiverOpened && timeCount > TimeMax)
            {
                if(protocolCallback)
                {
                    protocolCallback(StatusTimeout, 0, 0);
                }
                reset();
            }
            else if((timeCount % HandshakeInterval) == 0)
            {
                sendCode(CodeC);
            }
            break;
    }
}

void XmodemProtocol::transmitStageNone()
{
    stage = StageEstablishing;
    timeCount = 0;
    errorCount = 0;
    packetNumber = 1;
    useCrc = true;
}

void XmodemProtocol::transmitStageEstablishing()
{
    switch(receivePacket())
    {
        case CodeC:
        case CodeNak:
            useCrc = (rxBuffer[0] == CodeC);
            if(protocolCallback && protocolCallback(StatusEstablish, 0, 0) == ReplyAck)
            {
                stage = StageTransmitting;
                timeCount = 0;
                errorCount = 0;
                cancelCount = 0;
            }
            else
            {
                sendCode(CodeCan);
                if(protocolCallback)
                {
                    protocolCallback(StatusError, 0, 0);
                }
                reset();
            }
            break;

        case CodeCan:
            if(++cancelCount >= CanAbortCount)
            {
                if(protocolCallback)
                {
                    protocolCallback(StatusAbort, 0, 0);
                }
                reset();
            }
            break;

        default:
            if(++timeCount > TimeMax)
            {
                if(protocolCallback)
                {
                    protocolCallback(StatusTimeout, 0, 0);
                }
                reset();
            }
            break;
    }
}

void XmodemProtocol::transmitStageTransmitting()
{
    if(!protocolCallback)
    {
        reset();
        return;
    }

    if(packetPending)
    {
        switch(receivePacket())
        {
            case CodeAck:
                packetPending = false;
                txLength = 0;
                txPacketSize = PacketSize;
                ++packetNumber;
                timeCount = 0;
                errorCount = 0;
                cancelCount = 0;
                break;

            case CodeNak:
                writeCallback(txBuffer, txLength);
                timeCount = 0;
                if(++errorCount > ErrorMax)
                {
                    protocolCallback(StatusTimeout, 0, 0);
                    reset();
                }
                break;

            case CodeCan:
                if(++cancelCount >= CanAbortCount)
                {
                    protocolCallback(StatusAbort, 0, 0);
                    reset();
                }
                break;

            default:
                if(++timeCount > TimeMax)
                {
                    writeCallback(txBuffer, txLength);
                    timeCount = 0;
                    if(++errorCount > ErrorMax)
                    {
                        protocolCallback(StatusTimeout, 0, 0);
                        reset();
                    }
                }
                break;
        }
        return;
    }

    txPacketSize = PacketSize;
    memset(&(txBuffer[3]), 0x1A, txPacketSize);
    uint32_t len = txPacketSize;
    const Reply reply = protocolCallback(StatusTransmit, &(txBuffer[3]), &len);
    if(reply == ReplyEot || len == 0)
    {
        txBuffer[0] = CodeEot;
        writeCallback(txBuffer, 1);
        stage = StageFinishing;
        return;
    }

    if(reply != ReplyAck)
    {
        abort();
        protocolCallback(StatusError, 0, 0);
        return;
    }

    if(len <= PacketSize)
    {
        txPacketSize = PacketSize;
    }
    else
    {
        txPacketSize = Packet1KSize;
    }

    if(len < txPacketSize)
    {
        memset(&(txBuffer[3 + len]), 0x1A, txPacketSize - len);
    }

    txBuffer[0] = txPacketSize == Packet1KSize ? CodeStx : CodeSoh;
    txBuffer[1] = packetNumber;
    txBuffer[2] = 0xFF - packetNumber;
    if(useCrc)
    {
        const uint16_t crc = crc16(&(txBuffer[3]), txPacketSize);
        txBuffer[3 + txPacketSize] = (uint8_t)(crc >> 8);
        txBuffer[3 + txPacketSize + 1] = (uint8_t)(crc >> 0);
        txLength = txPacketSize + PacketOverheadCrc;
    }
    else
    {
        txBuffer[3 + txPacketSize] = checksum(&(txBuffer[3]), txPacketSize);
        txLength = txPacketSize + PacketOverheadChecksum;
    }

    writeCallback(txBuffer, txLength);
    packetPending = true;
}

void XmodemProtocol::transmitStageFinishing()
{
    switch(receivePacket())
    {
        case CodeAck:
            if(protocolCallback)
            {
                protocolCallback(StatusFinish, 0, 0);
            }
            reset();
            break;

        case CodeNak:
            txBuffer[0] = CodeEot;
            writeCallback(txBuffer, 1);
            timeCount = 0;
            if(++errorCount > ErrorMax)
            {
                if(protocolCallback)
                {
                    protocolCallback(StatusTimeout, 0, 0);
                }
                reset();
            }
            break;

        case CodeCan:
            if(++cancelCount >= CanAbortCount)
            {
                if(protocolCallback)
                {
                    protocolCallback(StatusAbort, 0, 0);
                }
                reset();
            }
            break;

        default:
            if(++timeCount > TimeMax)
            {
                if(protocolCallback)
                {
                    protocolCallback(StatusTimeout, 0, 0);
                }
                reset();
            }
            else if((timeCount % HandshakeInterval) == 0)
            {
                txBuffer[0] = CodeEot;
                writeCallback(txBuffer, 1);
            }
            break;
    }
}

XmodemProtocol::Code XmodemProtocol::receivePacket()
{
    if(!readCallback)
    {
        return CodeNone;
    }

    if(readCallback(rxBuffer, 1) == 0)
    {
        return CodeNone;
    }

    if(rxBuffer[0] == CodeSoh || rxBuffer[0] == CodeStx)
    {
        currentPacketSize = packetDataSize((Code)rxBuffer[0]);
        if(readExact(&(rxBuffer[1]), useCrc ? (currentPacketSize + 4) : (currentPacketSize + 3)))
        {
            return (Code)rxBuffer[0];
        }
        return CodeNone;
    }

    return (Code)rxBuffer[0];
}

uint32_t XmodemProtocol::packetDataSize(Code code) const
{
    return code == CodeStx ? Packet1KSize : PacketSize;
}

bool XmodemProtocol::isPreviousPacket(uint8_t seq) const
{
    return seq == (uint8_t)(packetNumber - 1);
}

bool XmodemProtocol::readExact(uint8_t *buff, uint32_t len)
{
    uint32_t count = 0;
    for(uint32_t i = 0; i < 20 && count < len; ++i)
    {
        count += readCallback(&(buff[count]), len - count);
    }
    return count == len;
}

void XmodemProtocol::sendCode(Code code)
{
    uint8_t buff[1] = {(uint8_t)code};
    if(writeCallback)
    {
        writeCallback(buff, 1);
    }
}

uint16_t XmodemProtocol::crc16(uint8_t *buff, uint32_t len)
{
    uint16_t crc = 0;
    for(uint32_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)buff[i] << 8;
        for(uint8_t j = 0; j < 8; ++j)
        {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint8_t XmodemProtocol::checksum(uint8_t *buff, uint32_t len)
{
    uint8_t sum = 0;
    for(uint32_t i = 0; i < len; ++i)
    {
        sum = (uint8_t)(sum + buff[i]);
    }
    return sum;
}
