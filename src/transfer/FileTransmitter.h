#ifndef FILETRANSMITTER_H
#define FILETRANSMITTER_H

#include <QFile>
#include <QByteArray>
#include <QTimer>
#include <QObject>
#include <QSerialPort>
#include "protocol/ymodem/Ymodem.h"

class FileTransmitter : public QObject, public Ymodem
{
    Q_OBJECT

public:
    explicit FileTransmitter(QObject *parent = 0);
    ~FileTransmitter();

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
    Code callback(Status status, uint8_t *buff, uint32_t *len);

    uint32_t read(uint8_t *buff, uint32_t len);
    uint32_t write(uint8_t *buff, uint32_t len);
    void appendFilteredRx(const QByteArray &data);
    void delayBeforePacket(const uint8_t *buff, uint32_t len);

    QFile       *file;
    QTimer      *readTimer;
    QTimer      *writeTimer;
    QSerialPort *serialPort;

    int      progress;
    Status   status;
    uint64_t fileSize;
    uint64_t fileCount;
    int firstDataDelayMs;
    int interPacketDelayMs;
    QByteArray filteredRx;
    QByteArray txEcho;
    int txEchoOffset;
};

#endif // FILETRANSMITTER_H
