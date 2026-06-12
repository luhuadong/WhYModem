#include "transfer/ProtocolFactory.h"
#include "protocol/ITransferProtocol.h"
#include "protocol/ymodem/YmodemProtocol.h"
#include "protocol/xmodem/XmodemProtocol.h"
#include "protocol/zmodem/ZmodemProtocol.h"

ProtocolKind ProtocolFactory::fromName(const QString &name)
{
    if(name.compare("Xmodem", Qt::CaseInsensitive) == 0)
    {
        return ProtocolKind::Xmodem;
    }

    if(name.compare("Zmodem", Qt::CaseInsensitive) == 0)
    {
        return ProtocolKind::Zmodem;
    }

    return ProtocolKind::Ymodem;
}

QString ProtocolFactory::displayName(ProtocolKind kind)
{
    switch(kind)
    {
        case ProtocolKind::Xmodem:
            return "Xmodem";
        case ProtocolKind::Zmodem:
            return "Zmodem";
        default:
            return "Ymodem";
    }
}

std::unique_ptr<ITransferProtocol> ProtocolFactory::create(ProtocolKind kind)
{
    switch(kind)
    {
        case ProtocolKind::Xmodem:
            return std::unique_ptr<ITransferProtocol>(new XmodemProtocol);
        case ProtocolKind::Zmodem:
            return std::unique_ptr<ITransferProtocol>(new ZmodemProtocol);
        default:
            return std::unique_ptr<ITransferProtocol>(new YmodemProtocol);
    }
}
