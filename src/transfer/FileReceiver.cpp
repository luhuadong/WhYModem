#include "FileReceiver.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#define READ_TIME_OUT   (10)
#define WRITE_TIME_OUT  (100)

FileReceiver::FileReceiver(QObject *parent) :
    QObject(parent),
    file(new QFile),
    readTimer(new QTimer),
    writeTimer(new QTimer),
    serialPort(new QSerialPort),
    protocolKind(ProtocolKind::Ymodem),
    progress(0),
    status(ITransferProtocol::StatusEstablish),
    fileSize(0),
    fileCount(0)
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

FileReceiver::~FileReceiver()
{
    delete file;
    delete readTimer;
    delete writeTimer;
    delete serialPort;
}

void FileReceiver::setProtocolKind(ProtocolKind kind)
{
    if(protocolKind == kind && protocol)
    {
        return;
    }

    protocolKind = kind;
    configureProtocol();
}

void FileReceiver::setFilePath(const QString &path)
{
    filePath = QDir::cleanPath(path);
    if(filePath == ".")
    {
        filePath.clear();
    }

    if(protocol)
    {
        protocol->setFilePath(filePath);
    }
}

void FileReceiver::setPortName(const QString &name)
{
    serialPort->setPortName(name);
}

void FileReceiver::setPortBaudRate(qint32 baudrate)
{
    serialPort->setBaudRate(baudrate);
}

bool FileReceiver::startReceive()
{
    readTimer->stop();
    writeTimer->stop();
    serialPort->close();
    file->close();
    configureProtocol();

    progress = 0;
    status   = ITransferProtocol::StatusEstablish;
    fileSize = 0;
    fileCount = 0;
    pendingXmodemBlock.clear();

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

void FileReceiver::stopReceive()
{
    file->close();
    pendingXmodemBlock.clear();
    if(protocol)
    {
        protocol->abort();
    }
    status = ITransferProtocol::StatusAbort;
    writeTimer->start(WRITE_TIME_OUT);
}

int FileReceiver::getReceiveProgress()
{
    return progress;
}

FileReceiver::Status FileReceiver::getReceiveStatus()
{
    return status;
}

void FileReceiver::readTimeOut()
{
    readTimer->stop();

    if(protocol)
    {
        protocol->receive();
    }

    if((status == ITransferProtocol::StatusEstablish) || (status == ITransferProtocol::StatusTransmit))
    {
        readTimer->start(READ_TIME_OUT);
    }
}

void FileReceiver::writeTimeOut()
{
    writeTimer->stop();
    serialPort->close();
    receiveStatus(status);
}

ITransferProtocol::Reply FileReceiver::callback(Status status, uint8_t *buff, uint32_t *len)
{
    switch(status)
    {
        case ITransferProtocol::StatusEstablish:
        {
            if(protocolKind == ProtocolKind::Zmodem)
            {
                FileReceiver::status = ITransferProtocol::StatusEstablish;
                receiveStatus(ITransferProtocol::StatusEstablish);
                return ITransferProtocol::ReplyAck;
            }

            if(buff != 0 && buff[0] != 0)
            {
                int  i         =  0;
                char name[128] = {0};
                char size[128] = {0};

                for(int j = 0; buff[i] != 0; i++, j++)
                {
                    name[j] = buff[i];
                }

                i++;

                for(int j = 0; buff[i] != 0; i++, j++)
                {
                    size[j] = buff[i];
                }

                fileName  = QString::fromLocal8Bit(name);
                QString file_desc(size);
                QString sizeStr = file_desc.left(file_desc.indexOf(' '));
                fileSize  = sizeStr.toULongLong();
                fileCount = 0;

                file->setFileName(QDir(filePath).filePath(fileName));

                if(file->open(QFile::WriteOnly) == true)
                {
                    FileReceiver::status = ITransferProtocol::StatusEstablish;

                    receiveStatus(ITransferProtocol::StatusEstablish);

                    return ITransferProtocol::ReplyAck;
                }
                else
                {
                    FileReceiver::status = ITransferProtocol::StatusError;

                    writeTimer->start(WRITE_TIME_OUT);

                    return ITransferProtocol::ReplyCancel;
                }
            }
            else
            {
                fileName = ProtocolFactory::displayName(protocolKind).toLower() + "-" +
                           QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss") + ".bin";
                fileSize = 0;
                fileCount = 0;

                if(protocolKind == ProtocolKind::Xmodem && filePath.isEmpty() != true && QFileInfo(filePath).isDir() != true)
                {
                    file->setFileName(filePath);
                    fileName = QFileInfo(filePath).fileName();
                }
                else
                {
                    file->setFileName(QDir(filePath).filePath(fileName));
                }

                if(file->open(QFile::WriteOnly) == true)
                {
                    FileReceiver::status = ITransferProtocol::StatusEstablish;

                    receiveStatus(ITransferProtocol::StatusEstablish);

                    return ITransferProtocol::ReplyAck;
                }
                else
                {
                    FileReceiver::status = ITransferProtocol::StatusError;

                    writeTimer->start(WRITE_TIME_OUT);

                    return ITransferProtocol::ReplyCancel;
                }
            }
        }

        case ITransferProtocol::StatusTransmit:
        {
            if(protocolKind == ProtocolKind::Zmodem)
            {
                if(len != 0 && *len > 0)
                {
                    fileCount += *len;
                }
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
                FileReceiver::status = ITransferProtocol::StatusTransmit;
                receiveProgress(progress);
                receiveStatus(ITransferProtocol::StatusTransmit);
                return ITransferProtocol::ReplyAck;
            }

            if(buff == 0 || len == 0)
            {
                return ITransferProtocol::ReplyCancel;
            }

            if(protocolKind == ProtocolKind::Xmodem)
            {
                if(pendingXmodemBlock.isEmpty() != true)
                {
                    file->write(pendingXmodemBlock);
                    fileCount += static_cast<uint64_t>(pendingXmodemBlock.size());
                }

                pendingXmodemBlock = QByteArray(reinterpret_cast<const char *>(buff), static_cast<int>(*len));
            }
            else if(fileSize > 0 && (fileSize - fileCount) <= *len)
            {
                file->write((char *)buff, fileSize - fileCount);

                fileCount += fileSize - fileCount;
            }
            else
            {
                file->write((char *)buff, *len);

                fileCount += *len;
            }

            if(fileSize > 0)
            {
                progress = (int)(fileCount * 100 / fileSize);
            }

            FileReceiver::status = ITransferProtocol::StatusTransmit;

            receiveProgress(progress);
            receiveStatus(ITransferProtocol::StatusTransmit);

            return ITransferProtocol::ReplyAck;
        }

        case ITransferProtocol::StatusFinish:
        {
            if(protocolKind == ProtocolKind::Xmodem && pendingXmodemBlock.isEmpty() != true)
            {
                int validSize = pendingXmodemBlock.size();
                while(validSize > 0 && static_cast<unsigned char>(pendingXmodemBlock.at(validSize - 1)) == 0x1A)
                {
                    --validSize;
                }

                if(validSize > 0)
                {
                    file->write(pendingXmodemBlock.constData(), validSize);
                    fileCount += static_cast<uint64_t>(validSize);
                }
                pendingXmodemBlock.clear();
            }

            if(protocolKind != ProtocolKind::Zmodem)
            {
                file->close();
            }
            progress = 100;

            FileReceiver::status = ITransferProtocol::StatusFinish;
            receiveProgress(progress);

            writeTimer->start(WRITE_TIME_OUT);

            return ITransferProtocol::ReplyAck;
        }

        case ITransferProtocol::StatusAbort:
        {
            file->close();
            pendingXmodemBlock.clear();

            FileReceiver::status = ITransferProtocol::StatusAbort;

            writeTimer->start(WRITE_TIME_OUT);

            return ITransferProtocol::ReplyCancel;
        }

        case ITransferProtocol::StatusTimeout:
        {
            file->close();
            pendingXmodemBlock.clear();

            FileReceiver::status = ITransferProtocol::StatusTimeout;

            writeTimer->start(WRITE_TIME_OUT);

            return ITransferProtocol::ReplyCancel;
        }

        default:
        {
            file->close();
            pendingXmodemBlock.clear();

            FileReceiver::status = ITransferProtocol::StatusError;

            writeTimer->start(WRITE_TIME_OUT);

            return ITransferProtocol::ReplyCancel;
        }
    }
}

uint32_t FileReceiver::read(uint8_t *buff, uint32_t len)
{
    const qint64 n = serialPort->read((char *)buff, len);
    if(n <= 0)
    {
        return 0;
    }
    emit rawDataReceived(QByteArray(reinterpret_cast<const char *>(buff), static_cast<int>(n)));
    return static_cast<uint32_t>(n);
}

uint32_t FileReceiver::write(uint8_t *buff, uint32_t len)
{
    const qint64 n = serialPort->write((char *)buff, len);
    if(n <= 0)
    {
        return 0;
    }
    serialPort->waitForBytesWritten(3000);
    serialPort->flush();
    return static_cast<uint32_t>(n);
}

void FileReceiver::configureProtocol()
{
    protocol = ProtocolFactory::create(protocolKind);
    protocol->setFilePath(filePath);
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
