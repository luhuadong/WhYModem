#include "FileTransmitter.h"
#include "protocol/ymodem/Ymodem.h"
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
    protocolKind(ProtocolKind::Ymodem),
    progress(0),
    status(ITransferProtocol::StatusEstablish),
    fileSize(0),
    fileCount(0),
    firstDataDelayMs(500),
    interPacketDelayMs(500),
    txEchoOffset(0)
{
    serialPort->setPortName("COM1");
    serialPort->setBaudRate(115200);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    connect(readTimer, SIGNAL(timeout()), this, SLOT(readTimeOut()));
    connect(writeTimer, SIGNAL(timeout()), this, SLOT(writeTimeOut()));
    configureProtocol();
}

FileTransmitter::~FileTransmitter()
{
    delete file;
    delete readTimer;
    delete writeTimer;
    delete serialPort;
}

void FileTransmitter::setProtocolKind(ProtocolKind kind)
{
    if(protocolKind == kind && protocol)
    {
        return;
    }

    protocolKind = kind;
    configureProtocol();
}

void FileTransmitter::setFileName(const QString &name)
{
    file->setFileName(name);
    if(protocol)
    {
        protocol->setFileName(name);
    }
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
    status   = ITransferProtocol::StatusEstablish;
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
    if(protocol)
    {
        protocol->abort();
    }
    status = ITransferProtocol::StatusAbort;
    writeTimer->start(WRITE_TIME_OUT);
}

int FileTransmitter::getTransmitProgress()
{
    return progress;
}

FileTransmitter::Status FileTransmitter::getTransmitStatus()
{
    return status;
}

void FileTransmitter::readTimeOut()
{
    readTimer->stop();

    if(protocol)
    {
        protocol->transmit();
    }

    if((status == ITransferProtocol::StatusEstablish) || (status == ITransferProtocol::StatusTransmit))
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

ITransferProtocol::Reply FileTransmitter::callback(Status status, uint8_t *buff, uint32_t *len)
{
    switch(status)
    {
        case ITransferProtocol::StatusEstablish:
        {
            if(protocolKind == ProtocolKind::Zmodem)
            {
                FileTransmitter::status = ITransferProtocol::StatusEstablish;
                transmitStatus(ITransferProtocol::StatusEstablish);
                return ITransferProtocol::ReplyAck;
            }

            if(file->open(QFile::ReadOnly) == true)
            {
                QFileInfo fileInfo(*file);

                fileSize  = fileInfo.size();
                fileCount = 0;

                if(buff != 0 && len != 0)
                {
                    strcpy((char *)buff, fileInfo.fileName().toLocal8Bit().data());
                    strcpy((char *)buff + fileInfo.fileName().toLocal8Bit().size() + 1, QByteArray::number(fileInfo.size()).data());

                    *len = YMODEM_PACKET_SIZE;
                }

                FileTransmitter::status = ITransferProtocol::StatusEstablish;

                transmitStatus(ITransferProtocol::StatusEstablish);

                return ITransferProtocol::ReplyAck;
            }
            else
            {
                FileTransmitter::status = ITransferProtocol::StatusError;

                writeTimer->start(WRITE_TIME_OUT);

                return ITransferProtocol::ReplyCancel;
            }
        }

        case ITransferProtocol::StatusTransmit:
        {
            if(protocolKind == ProtocolKind::Zmodem)
            {
                if(len != 0 && *len > 0)
                {
                    fileCount += *len;
                    if(buff != 0)
                    {
                        const long total = *reinterpret_cast<long *>(buff);
                        if(total > 0)
                        {
                            fileSize = static_cast<uint64_t>(total);
                        }
                    }
                    if(fileSize > 0)
                    {
                        progress = (int)(fileCount * 100 / fileSize);
                    }
                }
                FileTransmitter::status = ITransferProtocol::StatusTransmit;
                transmitProgress(progress);
                transmitStatus(ITransferProtocol::StatusTransmit);
                return ITransferProtocol::ReplyAck;
            }

            if(fileSize != fileCount)
            {
                uint32_t requestSize = 0;
                if(protocolKind == ProtocolKind::Ymodem)
                {
                    requestSize = (fileSize - fileCount) > YMODEM_PACKET_SIZE ? YMODEM_PACKET_1K_SIZE : YMODEM_PACKET_SIZE;
                }
                else
                {
                    requestSize = (len != 0 && *len > 0) ? *len : YMODEM_PACKET_SIZE;
                }

                const qint64 readCount = file->read((char *)buff, requestSize);
                if(readCount <= 0)
                {
                    *len = 0;
                    return ITransferProtocol::ReplyEot;
                }

                fileCount += static_cast<uint64_t>(readCount);
                *len = protocolKind == ProtocolKind::Ymodem ? requestSize : static_cast<uint32_t>(readCount);

                progress = (int)(fileCount * 100 / fileSize);

                FileTransmitter::status = ITransferProtocol::StatusTransmit;

                transmitProgress(progress);
                transmitStatus(ITransferProtocol::StatusTransmit);

                return ITransferProtocol::ReplyAck;
            }
            else
            {
                FileTransmitter::status = ITransferProtocol::StatusTransmit;

                transmitStatus(ITransferProtocol::StatusTransmit);

                return ITransferProtocol::ReplyEot;
            }
        }

        case ITransferProtocol::StatusFinish:
        {
            if(protocolKind != ProtocolKind::Zmodem)
            {
                file->close();
            }

            FileTransmitter::status = ITransferProtocol::StatusFinish;

            writeTimer->start(WRITE_TIME_OUT);

            return ITransferProtocol::ReplyAck;
        }

        case ITransferProtocol::StatusAbort:
        {
            file->close();

            FileTransmitter::status = ITransferProtocol::StatusAbort;

            writeTimer->start(WRITE_TIME_OUT);

            return ITransferProtocol::ReplyCancel;
        }

        case ITransferProtocol::StatusTimeout:
        {
            FileTransmitter::status = ITransferProtocol::StatusTimeout;

            writeTimer->start(WRITE_TIME_OUT);

            return ITransferProtocol::ReplyCancel;
        }

        default:
        {
            file->close();

            FileTransmitter::status = ITransferProtocol::StatusError;

            writeTimer->start(WRITE_TIME_OUT);

            return ITransferProtocol::ReplyCancel;
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
        if(protocolKind == ProtocolKind::Zmodem)
        {
            filteredRx.append(data);
        }
        else
        {
            appendFilteredRx(data);
        }
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

    if(protocolKind != ProtocolKind::Zmodem)
    {
        delayBeforePacket(buff, len);
    }

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

    if(written == len && protocolKind != ProtocolKind::Zmodem)
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

    const bool ymodemPacket = (buff[0] == 0x01) || (buff[0] == 0x02);
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

void FileTransmitter::configureProtocol()
{
    protocol = ProtocolFactory::create(protocolKind);
    protocol->setFileName(file->fileName());
    protocol->setProtocolCallback([this](Status status, uint8_t *buff, uint32_t *len) {
        return callback(status, buff, len);
    });
    protocol->setIoCallbacks(
        [this](uint8_t *buff, uint32_t len) {
            return read(buff, len);
        },
        [this](uint8_t *buff, uint32_t len) {
            return write(buff, len);
        });
}
