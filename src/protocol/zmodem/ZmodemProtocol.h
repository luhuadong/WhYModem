#ifndef ZMODEMPROTOCOL_H
#define ZMODEMPROTOCOL_H

#include "protocol/ITransferProtocol.h"
#include <QObject>
#include <QByteArray>
#include <QMutex>
#include <QString>
#include <memory>

class QRecvZmodem;
class QSendZmodem;

class ZmodemProtocol : public QObject, public ITransferProtocol
{
    Q_OBJECT

public:
    ZmodemProtocol();
    ~ZmodemProtocol();

    const char *name() const override;
    void setProtocolCallback(const ProtocolCallback &callback) override;
    void setIoCallbacks(const ReadCallback &read, const WriteCallback &write) override;
    void setFileName(const QString &name) override;
    void setFilePath(const QString &path) override;

    void receive() override;
    void transmit() override;
    void abort() override;

private:
    enum Mode
    {
        ModeIdle,
        ModeReceiving,
        ModeTransmitting,
        ModeFinished,
        ModeError
    };

    void startReceiver();
    void startTransmitter();
    void pumpIo();
    void enqueueOutput(const QByteArray &data);
    void drainOutput();
    void discardInput();
    void feedInput(QByteArray data);
    void cleanupWorkers();
    void reportStatus(Status status);
    void reportProgress(long bytesDone, long bytesTotal);

    ProtocolCallback protocolCallback;
    ReadCallback readCallback;
    WriteCallback writeCallback;

    QString fileName;
    QString filePath;
    Mode mode;
    bool started;
    bool completionReported;
    std::unique_ptr<QSendZmodem> sender;
    std::unique_ptr<QRecvZmodem> receiver;
    QByteArray outgoing;
    QMutex outgoingMutex;
    long lastProgressBytes;
    bool transferCompleted;
};

#endif // ZMODEMPROTOCOL_H
