#ifndef FILETRANSMITTER_H
#define FILETRANSMITTER_H

#include <QFile>
#include <QByteArray>
#include <QTimer>
#include <QObject>
#include <QSerialPort>
#include <memory>
#include "protocol/ITransferProtocol.h"
#include "transfer/ProtocolFactory.h"

class FileTransmitter : public QObject
{
    Q_OBJECT

public:
    typedef ITransferProtocol::Status Status;

    explicit FileTransmitter(QObject *parent = 0);
    ~FileTransmitter();

    void setProtocolKind(ProtocolKind kind);
    void setFileName(const QString &name);

    void setPortName(const QString &name);
    void setPortBaudRate(qint32 baudrate);
    void setTransferDelays(int firstDataDelayMs, int interPacketDelayMs);

    bool startTransmit();
    void stopTransmit();

    int getTransmitProgress();
    Status getTransmitStatus();

signals:
    void transmitProgress(int progress);
    void transmitStatus(FileTransmitter::Status status);
    void rawDataReceived(const QByteArray &data);

private slots:
    void readTimeOut();
    void writeTimeOut();

private:
    ITransferProtocol::Reply callback(Status status, uint8_t *buff, uint32_t *len);

    uint32_t read(uint8_t *buff, uint32_t len);
    uint32_t write(uint8_t *buff, uint32_t len);
    void appendFilteredRx(const QByteArray &data);
    void delayBeforePacket(const uint8_t *buff, uint32_t len);
    void configureProtocol();

    QFile       *file;
    QTimer      *readTimer;
    QTimer      *writeTimer;
    QSerialPort *serialPort;
    std::unique_ptr<ITransferProtocol> protocol;
    ProtocolKind protocolKind;

    int      progress;
    Status   status;
    uint64_t fileSize;
    uint64_t fileCount;
    int firstDataDelayMs;    // 首包延时
    int interPacketDelayMs;  // 包间隔
    QByteArray filteredRx;
    QByteArray txEcho;
    int txEchoOffset;
};

#endif // FILETRANSMITTER_H
