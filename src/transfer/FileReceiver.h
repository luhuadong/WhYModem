#ifndef FILERECEIVER_H
#define FILERECEIVER_H

#include <QFile>
#include <QByteArray>
#include <QTimer>
#include <QObject>
#include <QSerialPort>
#include <memory>
#include "protocol/ITransferProtocol.h"
#include "transfer/ProtocolFactory.h"

class FileReceiver : public QObject
{
    Q_OBJECT

public:
    typedef ITransferProtocol::Status Status;

    explicit FileReceiver(QObject *parent = 0);
    ~FileReceiver();

    void setProtocolKind(ProtocolKind kind);
    void setFilePath(const QString &path);

    void setPortName(const QString &name);
    void setPortBaudRate(qint32 baudrate);

    bool startReceive();
    void stopReceive();

    int getReceiveProgress();
    Status getReceiveStatus();

signals:
    void receiveProgress(int progress);
    void receiveStatus(FileReceiver::Status status);
    void rawDataReceived(const QByteArray &data);

private slots:
    void readTimeOut();
    void writeTimeOut();

private:
    ITransferProtocol::Reply callback(Status status, uint8_t *buff, uint32_t *len);

    uint32_t read(uint8_t *buff, uint32_t len);
    uint32_t write(uint8_t *buff, uint32_t len);
    void configureProtocol();

    QFile       *file;
    QTimer      *readTimer;
    QTimer      *writeTimer;
    QSerialPort *serialPort;
    std::unique_ptr<ITransferProtocol> protocol;
    ProtocolKind protocolKind;

    int      progress;
    Status   status;
    QString  filePath;
    QString  fileName;
    uint64_t fileSize;
    uint64_t fileCount;
};

#endif // FILERECEIVER_H
