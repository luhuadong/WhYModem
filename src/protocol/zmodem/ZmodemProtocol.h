#ifndef ZMODEMPROTOCOL_H
#define ZMODEMPROTOCOL_H

#include "protocol/ITransferProtocol.h"

class ZmodemProtocol : public ITransferProtocol
{
public:
    ZmodemProtocol();

    const char *name() const override;
    void setProtocolCallback(const ProtocolCallback &callback) override;
    void setIoCallbacks(const ReadCallback &read, const WriteCallback &write) override;

    void receive() override;
    void transmit() override;
    void abort() override;

private:
    void reportUnsupported();

    ProtocolCallback protocolCallback;
    ReadCallback readCallback;
    WriteCallback writeCallback;
    bool reported;
};

#endif // ZMODEMPROTOCOL_H
