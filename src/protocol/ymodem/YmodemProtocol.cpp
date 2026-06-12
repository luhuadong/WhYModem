#include "protocol/ymodem/YmodemProtocol.h"

YmodemProtocol::YmodemProtocol()
{
}

const char *YmodemProtocol::name() const
{
    return "Ymodem";
}

void YmodemProtocol::setProtocolCallback(const ProtocolCallback &callback)
{
    protocolCallback = callback;
}

void YmodemProtocol::setIoCallbacks(const ReadCallback &read, const WriteCallback &write)
{
    readCallback = read;
    writeCallback = write;
}

void YmodemProtocol::receive()
{
    Ymodem::receive();
}

void YmodemProtocol::transmit()
{
    Ymodem::transmit();
}

void YmodemProtocol::abort()
{
    Ymodem::abort();
}

Ymodem::Code YmodemProtocol::callback(Ymodem::Status status, uint8_t *buff, uint32_t *len)
{
    if(!protocolCallback)
    {
        return CodeCan;
    }

    return toYmodemCode(protocolCallback(toTransferStatus(status), buff, len));
}

uint32_t YmodemProtocol::read(uint8_t *buff, uint32_t len)
{
    if(!readCallback)
    {
        return 0;
    }

    return readCallback(buff, len);
}

uint32_t YmodemProtocol::write(uint8_t *buff, uint32_t len)
{
    if(!writeCallback)
    {
        return 0;
    }

    return writeCallback(buff, len);
}

Ymodem::Code YmodemProtocol::toYmodemCode(Reply reply) const
{
    switch(reply)
    {
        case ReplyAck:
            return CodeAck;
        case ReplyEot:
            return CodeEot;
        default:
            return CodeCan;
    }
}

ITransferProtocol::Status YmodemProtocol::toTransferStatus(Ymodem::Status status) const
{
    switch(status)
    {
        case Ymodem::StatusEstablish:
            return ITransferProtocol::StatusEstablish;
        case Ymodem::StatusTransmit:
            return ITransferProtocol::StatusTransmit;
        case Ymodem::StatusFinish:
            return ITransferProtocol::StatusFinish;
        case Ymodem::StatusAbort:
            return ITransferProtocol::StatusAbort;
        case Ymodem::StatusTimeout:
            return ITransferProtocol::StatusTimeout;
        default:
            return ITransferProtocol::StatusError;
    }
}
