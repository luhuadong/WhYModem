#ifndef ITRANSFERPROTOCOL_H
#define ITRANSFERPROTOCOL_H

#include <stdint.h>
#include <functional>

class ITransferProtocol
{
public:
    enum Status
    {
        StatusEstablish,
        StatusTransmit,
        StatusFinish,
        StatusAbort,
        StatusTimeout,
        StatusError
    };

    enum Reply
    {
        ReplyAck,
        ReplyCancel,
        ReplyEot
    };

    typedef std::function<Reply(Status status, uint8_t *buff, uint32_t *len)> ProtocolCallback;
    typedef std::function<uint32_t(uint8_t *buff, uint32_t len)> ReadCallback;
    typedef std::function<uint32_t(uint8_t *buff, uint32_t len)> WriteCallback;

    virtual ~ITransferProtocol() {}

    virtual const char *name() const = 0;
    virtual void setProtocolCallback(const ProtocolCallback &callback) = 0;
    virtual void setIoCallbacks(const ReadCallback &read, const WriteCallback &write) = 0;

    virtual void receive() = 0;
    virtual void transmit() = 0;
    virtual void abort() = 0;
};

#endif // ITRANSFERPROTOCOL_H
