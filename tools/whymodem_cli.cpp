#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QTimer>

#include "transfer/FileReceiver.h"
#include "transfer/FileTransmitter.h"
#include "transfer/ProtocolFactory.h"

namespace {

QString statusName(ITransferProtocol::Status status)
{
    switch(status)
    {
        case ITransferProtocol::StatusEstablish:
            return "establish";
        case ITransferProtocol::StatusTransmit:
            return "transmit";
        case ITransferProtocol::StatusFinish:
            return "finish";
        case ITransferProtocol::StatusAbort:
            return "abort";
        case ITransferProtocol::StatusTimeout:
            return "timeout";
        default:
            return "error";
    }
}

bool isTerminalStatus(ITransferProtocol::Status status)
{
    return status == ITransferProtocol::StatusFinish ||
           status == ITransferProtocol::StatusAbort ||
           status == ITransferProtocol::StatusTimeout ||
           status == ITransferProtocol::StatusError;
}

int exitCodeForStatus(ITransferProtocol::Status status, bool timedOut)
{
    if(timedOut)
    {
        return 124;
    }

    switch(status)
    {
        case ITransferProtocol::StatusFinish:
            return 0;
        case ITransferProtocol::StatusAbort:
            return 3;
        case ITransferProtocol::StatusTimeout:
            return 4;
        default:
            return 5;
    }
}

void logStatus(QTextStream &err, const QString &prefix, ITransferProtocol::Status status)
{
    err << QDateTime::currentDateTime().toString(Qt::ISODate) << ' '
        << prefix << " status=" << statusName(status) << Qt::endl;
}

int parseIntOption(const QCommandLineParser &parser, const QCommandLineOption &option, int fallback)
{
    bool ok = false;
    const int value = parser.value(option).toInt(&ok);
    return ok ? value : fallback;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("WhYModemCli");
    QCoreApplication::setApplicationVersion(WHYMODEM_VERSION);

    QTextStream out(stdout);
    QTextStream err(stderr);

    QCommandLineParser parser;
    parser.setApplicationDescription("Command line XMODEM/YMODEM/ZMODEM transfer tool using WhYModem protocol code.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("mode", "Transfer mode: send or receive.");

    const QCommandLineOption modeOpt(QStringList() << "m" << "mode", "Transfer mode: send or receive.", "mode");
    const QCommandLineOption protocolOpt(QStringList() << "p" << "protocol", "Protocol: xmodem, ymodem, or zmodem.", "protocol", "ymodem");
    const QCommandLineOption portOpt(QStringList() << "P" << "port", "Serial port path or name.", "port");
    const QCommandLineOption baudOpt(QStringList() << "b" << "baud", "Serial baud rate.", "baud", "115200");
    const QCommandLineOption fileOpt(QStringList() << "f" << "file", "File to send.", "file");
    const QCommandLineOption outputOpt(QStringList() << "o" << "output", "Receive output file for XMODEM, or output directory for YMODEM/ZMODEM.", "path");
    const QCommandLineOption timeoutOpt(QStringList() << "timeout-ms", "Overall operation timeout in milliseconds.", "ms", "60000");
    const QCommandLineOption firstDelayOpt(QStringList() << "first-delay-ms", "YMODEM first data packet delay in milliseconds for send mode.", "ms", "0");
    const QCommandLineOption interDelayOpt(QStringList() << "inter-delay-ms", "YMODEM data packet interval in milliseconds for send mode.", "ms", "0");
    const QCommandLineOption rawLogOpt(QStringList() << "raw-log", "Write raw serial bytes received by WhYModem to this file.", "file");
    const QCommandLineOption quietOpt(QStringList() << "q" << "quiet", "Only print terminal status.");

    parser.addOption(modeOpt);
    parser.addOption(protocolOpt);
    parser.addOption(portOpt);
    parser.addOption(baudOpt);
    parser.addOption(fileOpt);
    parser.addOption(outputOpt);
    parser.addOption(timeoutOpt);
    parser.addOption(firstDelayOpt);
    parser.addOption(interDelayOpt);
    parser.addOption(rawLogOpt);
    parser.addOption(quietOpt);
    parser.process(app);

    QString mode = parser.value(modeOpt).toLower();
    if(mode.isEmpty() && !parser.positionalArguments().isEmpty())
    {
        mode = parser.positionalArguments().first().toLower();
    }
    const QString port = parser.value(portOpt);
    const bool quiet = parser.isSet(quietOpt);

    if(mode != "send" && mode != "receive")
    {
        err << "error: --mode must be send or receive" << Qt::endl;
        parser.showHelp(2);
    }

    if(port.isEmpty())
    {
        err << "error: --port is required" << Qt::endl;
        parser.showHelp(2);
    }

    bool baudOk = false;
    const int baud = parser.value(baudOpt).toInt(&baudOk);
    if(!baudOk || baud <= 0)
    {
        err << "error: invalid --baud" << Qt::endl;
        return 2;
    }

    const ProtocolKind protocol = ProtocolFactory::fromName(parser.value(protocolOpt));
    const int timeoutMs = parseIntOption(parser, timeoutOpt, 60000);
    const int firstDelayMs = parseIntOption(parser, firstDelayOpt, 0);
    const int interDelayMs = parseIntOption(parser, interDelayOpt, 0);

    QFile rawLog;
    if(parser.isSet(rawLogOpt))
    {
        rawLog.setFileName(parser.value(rawLogOpt));
        if(!rawLog.open(QFile::WriteOnly))
        {
            err << "error: failed to open raw log: " << rawLog.fileName() << Qt::endl;
            return 2;
        }
    }

    bool timedOut = false;
    int lastProgress = -1;

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    if(mode == "send")
    {
        const QString fileName = parser.value(fileOpt);
        if(fileName.isEmpty())
        {
            err << "error: --file is required in send mode" << Qt::endl;
            return 2;
        }

        if(!QFileInfo::exists(fileName))
        {
            err << "error: send file does not exist: " << fileName << Qt::endl;
            return 2;
        }

        FileTransmitter transmitter;
        transmitter.setProtocolKind(protocol);
        transmitter.setFileName(fileName);
        transmitter.setPortName(port);
        transmitter.setPortBaudRate(baud);
        transmitter.setTransferDelays(firstDelayMs, interDelayMs);

        QObject::connect(&transmitter, &FileTransmitter::transmitProgress, &app, [&](int progress) {
            if(!quiet && progress != lastProgress)
            {
                lastProgress = progress;
                err << "progress=" << progress << '%' << Qt::endl;
            }
        });

        QObject::connect(&transmitter, &FileTransmitter::transmitStatus, &app, [&](FileTransmitter::Status status) {
            if(!quiet || isTerminalStatus(status))
            {
                logStatus(err, "send", status);
            }

            if(isTerminalStatus(status))
            {
                app.exit(exitCodeForStatus(status, timedOut));
            }
        });

        QObject::connect(&transmitter, &FileTransmitter::rawDataReceived, &app, [&](const QByteArray &data) {
            if(rawLog.isOpen())
            {
                rawLog.write(data);
            }
        });

        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, [&]() {
            timedOut = true;
            err << "error: send timeout" << Qt::endl;
            transmitter.stopTransmit();
            QTimer::singleShot(250, &app, [&]() { app.exit(124); });
        });

        if(!transmitter.startTransmit())
        {
            err << "error: failed to open serial port: " << port << Qt::endl;
            return 2;
        }

        timeoutTimer.start(timeoutMs);
        return app.exec();
    }

    const QString outputPath = parser.value(outputOpt);
    if(outputPath.isEmpty())
    {
        err << "error: --output is required in receive mode" << Qt::endl;
        return 2;
    }

    if(protocol != ProtocolKind::Xmodem)
    {
        QDir().mkpath(outputPath);
    }
    else
    {
        const QFileInfo info(outputPath);
        if(!info.absoluteDir().exists())
        {
            QDir().mkpath(info.absolutePath());
        }
    }

    FileReceiver receiver;
    receiver.setProtocolKind(protocol);
    receiver.setFilePath(outputPath);
    receiver.setPortName(port);
    receiver.setPortBaudRate(baud);

    QObject::connect(&receiver, &FileReceiver::receiveProgress, &app, [&](int progress) {
        if(!quiet && progress != lastProgress)
        {
            lastProgress = progress;
            err << "progress=" << progress << '%' << Qt::endl;
        }
    });

    QObject::connect(&receiver, &FileReceiver::receiveStatus, &app, [&](FileReceiver::Status status) {
        if(!quiet || isTerminalStatus(status))
        {
            logStatus(err, "receive", status);
        }

        if(isTerminalStatus(status))
        {
            app.exit(exitCodeForStatus(status, timedOut));
        }
    });

    QObject::connect(&receiver, &FileReceiver::rawDataReceived, &app, [&](const QByteArray &data) {
        if(rawLog.isOpen())
        {
            rawLog.write(data);
        }
    });

    QObject::connect(&timeoutTimer, &QTimer::timeout, &app, [&]() {
        timedOut = true;
        err << "error: receive timeout" << Qt::endl;
        receiver.stopReceive();
        QTimer::singleShot(250, &app, [&]() { app.exit(124); });
    });

    if(!receiver.startReceive())
    {
        err << "error: failed to open serial port: " << port << Qt::endl;
        return 2;
    }

    timeoutTimer.start(timeoutMs);
    return app.exec();
}
