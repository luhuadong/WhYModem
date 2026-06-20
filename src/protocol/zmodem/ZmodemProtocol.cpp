#include "protocol/zmodem/ZmodemProtocol.h"
#include "protocol/zmodem/qzmodem/qrecvzmodem.h"
#include "protocol/zmodem/qzmodem/qsendzmodem.h"
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStringList>

ZmodemProtocol::ZmodemProtocol() :
    mode(ModeIdle),
    started(false),
    completionReported(false),
    lastProgressBytes(0)
{
}

ZmodemProtocol::~ZmodemProtocol()
{
    abort();
    cleanupWorkers();
}

const char *ZmodemProtocol::name() const
{
    return "Zmodem";
}

void ZmodemProtocol::setProtocolCallback(const ProtocolCallback &callback)
{
    protocolCallback = callback;
}

void ZmodemProtocol::setIoCallbacks(const ReadCallback &read, const WriteCallback &write)
{
    readCallback = read;
    writeCallback = write;
}

void ZmodemProtocol::setFileName(const QString &name)
{
    fileName = name;
}

void ZmodemProtocol::setFilePath(const QString &path)
{
    filePath = QDir::cleanPath(path);
}

void ZmodemProtocol::receive()
{
    if(!started)
    {
        startReceiver();
    }
    pumpIo();
}

void ZmodemProtocol::transmit()
{
    if(!started)
    {
        startTransmitter();
    }
    pumpIo();
}

void ZmodemProtocol::abort()
{
    if(sender)
    {
        sender->requestStop();
    }
    if(receiver)
    {
        receiver->requestStop();
    }

    if(mode != ModeIdle && mode != ModeFinished && mode != ModeError)
    {
        reportStatus(StatusAbort);
    }

    mode = ModeIdle;
    started = false;
}

void ZmodemProtocol::startReceiver()
{
    cleanupWorkers();
    receiver.reset(new QRecvZmodem(1));
    if(!filePath.isEmpty())
    {
        receiver->setFileDirPath(filePath);
    }

    QObject::connect(receiver.get(), &QRecvZmodem::approver, this,
        [](const char *, size_t, time_t, bool *ret) {
            if(ret)
            {
                *ret = true;
            }
        }, Qt::DirectConnection);
    QObject::connect(receiver.get(), &QRecvZmodem::sendData, this, &ZmodemProtocol::enqueueOutput, Qt::QueuedConnection);
    QObject::connect(receiver.get(), &QRecvZmodem::flushSend, this, &ZmodemProtocol::drainOutput, Qt::QueuedConnection);
    QObject::connect(receiver.get(), &QRecvZmodem::flushRecv, this, &ZmodemProtocol::discardInput, Qt::QueuedConnection);
    QObject::connect(receiver.get(), &QRecvZmodem::resetRecv, this, &ZmodemProtocol::discardInput, Qt::QueuedConnection);
    QObject::connect(receiver.get(), &QRecvZmodem::complete, this, [this](QString, int result, size_t, time_t) {
        if(result != 0)
        {
            mode = ModeError;
            reportStatus(StatusError);
        }
    }, Qt::QueuedConnection);
    QObject::connect(receiver.get(), &QRecvZmodem::tick, this,
        [this](const char *, long bytesDone, long bytesTotal, long, int, int, bool *ret) {
            Q_UNUSED(ret);
            reportProgress(bytesDone, bytesTotal);
        }, Qt::QueuedConnection);
    QObject::connect(receiver.get(), &QRecvZmodem::finished, this, [this]() {
        if(mode != ModeError && !completionReported)
        {
            mode = ModeFinished;
            reportStatus(StatusFinish);
        }
    }, Qt::QueuedConnection);

    mode = ModeReceiving;
    started = true;
    completionReported = false;
    lastProgressBytes = 0;
    reportStatus(StatusEstablish);
    receiver->start();
}

void ZmodemProtocol::startTransmitter()
{
    cleanupWorkers();
    sender.reset(new QSendZmodem(1));
    sender->setFilePath(QStringList() << fileName, QStringList() << QFileInfo(fileName).fileName());

    QObject::connect(sender.get(), &QSendZmodem::sendData, this, &ZmodemProtocol::enqueueOutput, Qt::QueuedConnection);
    QObject::connect(sender.get(), &QSendZmodem::flushSend, this, &ZmodemProtocol::drainOutput, Qt::QueuedConnection);
    QObject::connect(sender.get(), &QSendZmodem::flushRecv, this, &ZmodemProtocol::discardInput, Qt::QueuedConnection);
    QObject::connect(sender.get(), &QSendZmodem::resetRecv, this, &ZmodemProtocol::discardInput, Qt::QueuedConnection);
    QObject::connect(sender.get(), &QSendZmodem::complete, this, [this](QString, int result, size_t, time_t) {
        if(result != 0)
        {
            mode = ModeError;
            reportStatus(StatusError);
        }
    }, Qt::QueuedConnection);
    QObject::connect(sender.get(), &QSendZmodem::tick, this,
        [this](const char *, long bytesDone, long bytesTotal, long, int, int, bool *ret) {
            Q_UNUSED(ret);
            reportProgress(bytesDone, bytesTotal);
        }, Qt::QueuedConnection);
    QObject::connect(sender.get(), &QSendZmodem::finished, this, [this]() {
        if(mode != ModeError && !completionReported)
        {
            mode = ModeFinished;
            reportStatus(StatusFinish);
        }
    }, Qt::QueuedConnection);

    mode = ModeTransmitting;
    started = true;
    completionReported = false;
    lastProgressBytes = 0;
    reportStatus(StatusEstablish);
    sender->start();
}

void ZmodemProtocol::pumpIo()
{
    if(!readCallback || !writeCallback)
    {
        return;
    }

    uint8_t buff[4096];
    for(;;)
    {
        const uint32_t n = readCallback(buff, sizeof(buff));
        if(n == 0)
        {
            break;
        }
        feedInput(QByteArray(reinterpret_cast<const char *>(buff), static_cast<int>(n)));
    }

    drainOutput();
}

void ZmodemProtocol::enqueueOutput(const QByteArray &data)
{
    QMutexLocker locker(&outgoingMutex);
    outgoing.append(data);
}

void ZmodemProtocol::drainOutput()
{
    if(!writeCallback)
    {
        return;
    }

    QByteArray data;
    {
        QMutexLocker locker(&outgoingMutex);
        if(outgoing.isEmpty())
        {
            return;
        }
        data = outgoing;
        outgoing.clear();
    }

    writeCallback(reinterpret_cast<uint8_t *>(data.data()), static_cast<uint32_t>(data.size()));
}

void ZmodemProtocol::discardInput()
{
    if(!readCallback)
    {
        return;
    }

    uint8_t buff[4096];
    while(readCallback(buff, sizeof(buff)) > 0)
    {
    }
}

void ZmodemProtocol::feedInput(QByteArray data)
{
    if(sender)
    {
        sender->onRecvData(data);
    }
    if(receiver)
    {
        receiver->onRecvData(data);
    }
}

void ZmodemProtocol::cleanupWorkers()
{
    if(sender)
    {
        sender->requestStop();
        sender->wait();
        sender.reset();
    }
    if(receiver)
    {
        receiver->requestStop();
        receiver->wait();
        receiver.reset();
    }

    QMutexLocker locker(&outgoingMutex);
    outgoing.clear();
}

void ZmodemProtocol::reportStatus(Status status)
{
    if(!protocolCallback)
    {
        return;
    }

    if(status == StatusFinish || status == StatusAbort || status == StatusError || status == StatusTimeout)
    {
        completionReported = true;
    }

    protocolCallback(status, 0, 0);
}

void ZmodemProtocol::reportProgress(long bytesDone, long bytesTotal)
{
    if(!protocolCallback)
    {
        return;
    }

    uint32_t delta = 0;
    if(bytesDone > lastProgressBytes)
    {
        delta = static_cast<uint32_t>(bytesDone - lastProgressBytes);
        lastProgressBytes = bytesDone;
    }
    else if(bytesTotal > 0 && bytesDone >= bytesTotal)
    {
        delta = 0;
    }

    protocolCallback(StatusTransmit, reinterpret_cast<uint8_t *>(&bytesTotal), &delta);
}
