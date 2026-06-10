#include "FileTransmitter.h"
#include <QFileInfo>
#include <QThread>
#include <algorithm>
#include <cstring>

#define READ_TIME_OUT   (10)
#define WRITE_TIME_OUT  (100)

FileTransmitter::FileTransmitter(QObject *parent) :
    QObject(parent),
    file(new QFile),
    readTimer(new QTimer),
    writeTimer(new QTimer),
    serialPort(new QSerialPort),
    firstDataDelayMs(500),
    interPacketDelayMs(500),
    txEchoOffset(0)
{
    setTimeDivide(499);
    setTimeMax(5);
    setErrorMax(999);

    serialPort->setPortName("COM1");
    serialPort->setBaudRate(115200);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    connect(readTimer, SIGNAL(timeout()), this, SLOT(readTimeOut()));
    connect(writeTimer, SIGNAL(timeout()), this, SLOT(writeTimeOut()));
}

FileTransmitter::~FileTransmitter()
{
    delete file;
    delete readTimer;
    delete writeTimer;
    delete serialPort;
}

void FileTransmitter::setFileName(const QString &name)
{
    file->setFileName(name);
}

void FileTransmitter::setPortName(const QString &name)
{
    serialPort->setPortName(name);
}

void FileTransmitter::setPortBaudRate(qint32 baudrate)
{
    serialPort->setBaudRate(baudrate);
}

void FileTransmitter::setTransferDelays(int firstDataDelayMs, int interPacketDelayMs)
{
    this->firstDataDelayMs = std::max(0, firstDataDelayMs);
    this->interPacketDelayMs = std::max(0, interPacketDelayMs);
}

bool FileTransmitter::startTransmit()
{
    progress = 0;
    status   = StatusEstablish;
    filteredRx.clear();
    txEcho.clear();
    txEchoOffset = 0;

    if(serialPort->open(QSerialPort::ReadWrite) == true)
    {
        readTimer->start(READ_TIME_OUT);

        return true;
    }
    else
    {
        return false;
    }
}

void FileTransmitter::stopTransmit()
{
    file->close();
    abort();
    status = StatusAbort;
    writeTimer->start(WRITE_TIME_OUT);
}

int FileTransmitter::getTransmitProgress()
{
    return progress;
}

Ymodem::Status FileTransmitter::getTransmitStatus()
{
    return status;
}

void FileTransmitter::readTimeOut()
{
    readTimer->stop();

    transmit();

    if((status == StatusEstablish) || (status == StatusTransmit))
    {
        readTimer->start(READ_TIME_OUT);
    }
}

void FileTransmitter::writeTimeOut()
{
    writeTimer->stop();
    serialPort->close();
    transmitStatus(status);
}

Ymodem::Code FileTransmitter::callback(Status status, uint8_t *buff, uint32_t *len)
{
    switch(status)
    {
        case StatusEstablish:
        {
            if(file->open(QFile::ReadOnly) == true)
            {
                QFileInfo fileInfo(*file);

                fileSize  = fileInfo.size();
                fileCount = 0;

                strcpy((char *)buff, fileInfo.fileName().toLocal8Bit().data());
                strcpy((char *)buff + fileInfo.fileName().toLocal8Bit().size() + 1, QByteArray::number(fileInfo.size()).data());

                *len = YMODEM_PACKET_SIZE;

                FileTransmitter::status = StatusEstablish;

                transmitStatus(StatusEstablish);

                return CodeAck;
            }
            else
            {
                FileTransmitter::status = StatusError;

                writeTimer->start(WRITE_TIME_OUT);

                return CodeCan;
            }
        }

        case StatusTransmit:
        {
            if(fileSize != fileCount)
            {
                if((fileSize - fileCount) > YMODEM_PACKET_SIZE)
                {
                    fileCount += file->read((char *)buff, YMODEM_PACKET_1K_SIZE);

                    *len = YMODEM_PACKET_1K_SIZE;
                }
                else
                {
                    fileCount += file->read((char *)buff, YMODEM_PACKET_SIZE);

                    *len = YMODEM_PACKET_SIZE;
                }

                progress = (int)(fileCount * 100 / fileSize);

                FileTransmitter::status = StatusTransmit;

                transmitProgress(progress);
                transmitStatus(StatusTransmit);

                return CodeAck;
            }
            else
            {
                FileTransmitter::status = StatusTransmit;

                transmitStatus(StatusTransmit);

                return CodeEot;
            }
        }

        case StatusFinish:
        {
            file->close();

            FileTransmitter::status = StatusFinish;

            writeTimer->start(WRITE_TIME_OUT);

            return CodeAck;
        }

        case StatusAbort:
        {
            file->close();

            FileTransmitter::status = StatusAbort;

            writeTimer->start(WRITE_TIME_OUT);

            return CodeCan;
        }

        case StatusTimeout:
        {
            FileTransmitter::status = StatusTimeout;

            writeTimer->start(WRITE_TIME_OUT);

            return CodeCan;
        }

        default:
        {
            file->close();

            FileTransmitter::status = StatusError;

            writeTimer->start(WRITE_TIME_OUT);

            return CodeCan;
        }
    }
}

uint32_t FileTransmitter::read(uint8_t *buff, uint32_t len)
{
    if(len == 0)
    {
        return 0;
    }

    if(filteredRx.isEmpty())
    {
        const QByteArray data = serialPort->readAll();
        if(data.isEmpty())
        {
            return 0;
        }
        emit rawDataReceived(data);
        appendFilteredRx(data);
    }

    const int n = std::min<int>(static_cast<int>(len), filteredRx.size());
    if(n <= 0)
    {
        return 0;
    }
    std::memcpy(buff, filteredRx.constData(), static_cast<size_t>(n));
    filteredRx.remove(0, n);
    return static_cast<uint32_t>(n);
}

uint32_t FileTransmitter::write(uint8_t *buff, uint32_t len)
{
    if(len == 0)
    {
        return 0;
    }

    delayBeforePacket(buff, len);

    uint32_t written = 0;
    while(written < len)
    {
        const qint64 n = serialPort->write(reinterpret_cast<const char *>(buff + written), len - written);
        if(n <= 0)
        {
            break;
        }
        written += static_cast<uint32_t>(n);
        if(serialPort->waitForBytesWritten(3000) == false)
        {
            break;
        }
    }
    serialPort->flush();

    if(written == len)
    {
        txEcho = QByteArray(reinterpret_cast<const char *>(buff), static_cast<int>(len));
        txEchoOffset = 0;
    }

    return written;
}

void FileTransmitter::appendFilteredRx(const QByteArray &data)
{
    for(char ch : data)
    {
        if(!txEcho.isEmpty())
        {
            if(txEchoOffset < txEcho.size() && ch == txEcho.at(txEchoOffset))
            {
                ++txEchoOffset;
                if(txEchoOffset >= txEcho.size())
                {
                    txEcho.clear();
                    txEchoOffset = 0;
                }
                continue;
            }

            txEcho.clear();
            txEchoOffset = 0;
        }

        filteredRx.append(ch);
    }
}

void FileTransmitter::delayBeforePacket(const uint8_t *buff, uint32_t len)
{
    if(len < 3)
    {
        return;
    }

    const bool ymodemPacket = (buff[0] == CodeSoh) || (buff[0] == CodeStx);
    if(!ymodemPacket)
    {
        return;
    }

    if(buff[1] == 1)
    {
        QThread::msleep(static_cast<unsigned long>(firstDataDelayMs));
    }
    else if(buff[1] > 1)
    {
        QThread::msleep(static_cast<unsigned long>(interPacketDelayMs));
    }
}
