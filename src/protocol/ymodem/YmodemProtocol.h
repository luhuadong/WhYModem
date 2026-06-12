#ifndef YMODEMPROTOCOL_H
#define YMODEMPROTOCOL_H

#include "protocol/ITransferProtocol.h"
#include "protocol/ymodem/Ymodem.h"

class YmodemProtocol : public ITransferProtocol, private Ymodem
{
public:
    YmodemProtocol();

    const char *name() const override;
    void setProtocolCallback(const ProtocolCallback &callback) override;
    void setIoCallbacks(const ReadCallback &read, const WriteCallback &write) override;

    void receive() override;
    void transmit() override;
    void abort() override;

private:
    Code callback(Ymodem::Status status, uint8_t *buff, uint32_t *len) override;
    uint32_t read(uint8_t *buff, uint32_t len) override;
    uint32_t write(uint8_t *buff, uint32_t len) override;

    Code toYmodemCode(Reply reply) const;
    ITransferProtocol::Status toTransferStatus(Ymodem::Status status) const;

    ProtocolCallback protocolCallback;
    ReadCallback readCallback;
    WriteCallback writeCallback;
};

#endif // YMODEMPROTOCOL_H
