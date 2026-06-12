#include "protocol/zmodem/ZmodemProtocol.h"

ZmodemProtocol::ZmodemProtocol() :
    reported(false)
{
}

const char *ZmodemProtocol::name() const
{
    return "Zmodem";
}

void ZmodemProtocol::setProtocolCallback(const ProtocolCallback &callback)
{
    protocolCallback = callback;
}

void ZmodemProtocol::setIoCallbacks(const ReadCallback &read, const WriteCallback &write)
{
    readCallback = read;
    writeCallback = write;
}

void ZmodemProtocol::receive()
{
    reportUnsupported();
}

void ZmodemProtocol::transmit()
{
    reportUnsupported();
}

void ZmodemProtocol::abort()
{
    reported = false;
    if(protocolCallback)
    {
        protocolCallback(StatusAbort, 0, 0);
    }
}

void ZmodemProtocol::reportUnsupported()
{
    if(reported)
    {
        return;
    }

    reported = true;
    if(protocolCallback)
    {
        protocolCallback(StatusError, 0, 0);
    }
}
