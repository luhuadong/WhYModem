#ifndef FILETRANSMITTER_H
#define FILETRANSMITTER_H

#include <QFile>
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

    bool startTransmit();
    void stopTransmit();

    int getTransmitProgress();
    Status getTransmitStatus();

signals:
    void transmitProgress(int progress);
    void transmitStatus(FileTransmitter::Status status);

private slots:
    void readTimeOut();
    void writeTimeOut();

private:
    Code callback(Status status, uint8_t *buff, uint32_t *len);

    uint32_t read(uint8_t *buff, uint32_t len);
    uint32_t write(uint8_t *buff, uint32_t len);

    QFile       *file;
    QTimer      *readTimer;
    QTimer      *writeTimer;
    QSerialPort *serialPort;

    int      progress;
    Status   status;
    uint64_t fileSize;
    uint64_t fileCount;
};

#endif // FILETRANSMITTER_H
