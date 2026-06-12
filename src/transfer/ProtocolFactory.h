#ifndef PROTOCOLFACTORY_H
#define PROTOCOLFACTORY_H

#include <memory>
#include <QString>

class ITransferProtocol;

enum class ProtocolKind
{
    Ymodem,
    Xmodem,
    Zmodem
};

class ProtocolFactory
{
public:
    static ProtocolKind fromName(const QString &name);
    static QString displayName(ProtocolKind kind);
    static std::unique_ptr<ITransferProtocol> create(ProtocolKind kind);
};

#endif // PROTOCOLFACTORY_H
