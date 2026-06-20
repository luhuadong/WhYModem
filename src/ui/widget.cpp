#include "widget.h"
#include "ui_widget.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QSerialPortInfo>
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

#ifndef WHYMODEM_VERSION
#define WHYMODEM_VERSION "dev"
#endif

namespace {

const int MaxRxLogLines = 500;
const int MaxRxLineBytes = 16 * 1024;
const int MaxPendingRxRenderBytes = 8 * 1024;
const int RxRenderIntervalMs = 30;

void AppendHexByte(QString &text, unsigned char ch)
{
    static const char digits[] = "0123456789ABCDEF";
    text.append(QLatin1Char(digits[(ch >> 4) & 0x0f]));
    text.append(QLatin1Char(digits[ch & 0x0f]));
}

void ShowMessage(QWidget *parent, const QString &title, const QString &text, QMessageBox::Icon icon)
{
    QMessageBox box(parent);
    box.setIcon(icon);
    box.setWindowTitle(title);
    box.setText(text);
    box.addButton(u8"关闭", QMessageBox::AcceptRole);
    box.exec();
}

}

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    serialPort(new QSerialPort),
    fileTransmitter(new FileTransmitter),
    fileReceiver(new FileReceiver),
    rxLog(new QPlainTextEdit),
    rxHexCheckBox(new QCheckBox(u8"十六进制显示")),
    rxRenderTimer(new QTimer(this)),
    rxPaused(false)
{
    transmitButtonStatus = false;
    receiveButtonStatus  = false;

    ui->setupUi(this);
    ui->versionLabel->setText(QString("V%1").arg(WHYMODEM_VERSION));
    setMinimumSize(860, 500);
    resize(900, 560);

    QGroupBox *rxGroup = new QGroupBox(u8"接收窗口", this);
    QVBoxLayout *rxLayout = new QVBoxLayout(rxGroup);
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    QPushButton *pauseRx = new QPushButton(u8"暂停", rxGroup);
    QPushButton *clearRx = new QPushButton(u8"清空", rxGroup);
    rxGroup->setMinimumHeight(135);
    rxGroup->setMaximumHeight(300);
    rxLayout->setSpacing(8);
    currentRxLine.reserve(MaxRxLineBytes);
    pendingRxRender.reserve(MaxPendingRxRenderBytes);
    rxHexCheckBox->setParent(rxGroup);
    rxHexCheckBox->setChecked(false);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(rxHexCheckBox);
    toolbarLayout->addWidget(pauseRx);
    toolbarLayout->addWidget(clearRx);
    rxLog->setReadOnly(true);
    rxLog->setUndoRedoEnabled(false);
    rxLog->setMinimumHeight(68);
    rxLog->document()->setMaximumBlockCount(MaxRxLogLines);
    rxLog->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    rxLog->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rxLayout->addWidget(rxLog, 1);
    rxLayout->addLayout(toolbarLayout);
    ui->verticalLayout_3->addWidget(rxGroup);

    ui->comPort->setEditable(true);
    refreshSerialPorts();

    serialPort->setPortName("COM1");
    serialPort->setBaudRate(115200);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    connect(fileTransmitter, &FileTransmitter::transmitProgress, this, &Widget::transmitProgress);
    connect(fileReceiver, &FileReceiver::receiveProgress, this, &Widget::receiveProgress);
    connect(fileTransmitter, &FileTransmitter::transmitStatus, this, &Widget::transmitStatus);
    connect(fileReceiver, &FileReceiver::receiveStatus, this, &Widget::receiveStatus);
    connect(fileTransmitter, SIGNAL(rawDataReceived(QByteArray)), this, SLOT(appendRawData(QByteArray)));
    connect(fileReceiver, SIGNAL(rawDataReceived(QByteArray)), this, SLOT(appendRawData(QByteArray)));
    connect(serialPort, SIGNAL(readyRead()), this, SLOT(readMonitorData()));
    rxRenderTimer->setSingleShot(true);
    connect(rxRenderTimer, SIGNAL(timeout()), this, SLOT(flushRxRender()));
    connect(rxHexCheckBox, SIGNAL(toggled(bool)), this, SLOT(refreshRxLog()));
    connect(pauseRx, &QPushButton::clicked, this, [this, pauseRx]() {
        rxPaused = !rxPaused;
        pauseRx->setText(rxPaused ? u8"继续" : u8"暂停");
        if(rxPaused)
        {
            rxRenderTimer->stop();
            pendingRxRender.clear();
            return;
        }
        if(!rxPaused)
        {
            renderRxCache();
        }
    });
    connect(ui->receivePath, &QLineEdit::textChanged, this, [this](const QString &text) {
        if(receiveButtonStatus == false)
        {
            ui->receiveButton->setEnabled(text.isEmpty() != true);
        }
    });

    connect(clearRx, &QPushButton::clicked, this, [this]() {
        rxLines.clear();
        currentRxLine.clear();
        currentRxLine.reserve(MaxRxLineBytes);
        pendingRxRender.clear();
        pendingRxRender.reserve(MaxPendingRxRenderBytes);
        rxRenderTimer->stop();
        rxLog->clear();
    });
}

Widget::~Widget()
{
    delete ui;
    delete serialPort;
    delete fileTransmitter;
    delete fileReceiver;
}

void Widget::on_comButton_clicked()
{
    static bool button_status = false;

    if(button_status == false)
    {
        serialPort->setPortName(ui->comPort->currentText());
        serialPort->setBaudRate(ui->comBaudRate->currentText().toInt());

        if(serialPort->open(QSerialPort::ReadWrite) == true)
        {
            button_status = true;

            ui->comPort->setDisabled(true);
            ui->comBaudRate->setDisabled(true);
            ui->protocol->setDisabled(true);
            ui->refreshButton->setDisabled(true);
            ui->comButton->setText(u8"关闭串口");
        }
        else
        {
            ShowMessage(this, u8"串口打开失败", u8"请检查串口是否已被占用！", QMessageBox::Warning);
        }
    }
    else
    {
        button_status = false;

        serialPort->close();

        ui->comPort->setEnabled(true);
        ui->comBaudRate->setEnabled(true);
        ui->protocol->setEnabled(true);
        ui->refreshButton->setEnabled(true);
        ui->comButton->setText(u8"打开串口");
    }
}

void Widget::on_refreshButton_clicked()
{
    refreshSerialPorts();
}

void Widget::on_transmitBrowse_clicked()
{
    ui->transmitPath->setText(QFileDialog::getOpenFileName(this, u8"打开文件", ".", u8"任意文件 (*.*)"));

    if(ui->transmitPath->text().isEmpty() != true)
    {
        ui->transmitButton->setEnabled(true);
    }
    else
    {
        ui->transmitButton->setDisabled(true);
    }
}

void Widget::on_receiveBrowse_clicked()
{
    QString path;
    if(ProtocolFactory::fromName(ui->protocol->currentText()) == ProtocolKind::Xmodem)
    {
        path = QFileDialog::getSaveFileName(this,
                                            u8"选择保存文件",
                                            ui->receivePath->text().isEmpty() ? "." : ui->receivePath->text(),
                                            u8"任意文件 (*.*)");
    }
    else
    {
        path = QFileDialog::getExistingDirectory(this,
                                                u8"选择保存目录",
                                                ui->receivePath->text().isEmpty() ? "." : ui->receivePath->text(),
                                                QFileDialog::ShowDirsOnly);
    }

    if(path.isEmpty() != true)
    {
        ui->receivePath->setText(path);
    }

    if(ui->receivePath->text().isEmpty() != true)
    {
        ui->receiveButton->setEnabled(true);
    }
    else
    {
        ui->receiveButton->setDisabled(true);
    }
}

void Widget::on_transmitButton_clicked()
{
    if(transmitButtonStatus == false)
    {
        serialPort->close();

        fileTransmitter->setProtocolKind(ProtocolFactory::fromName(ui->protocol->currentText()));
        fileTransmitter->setFileName(ui->transmitPath->text());
        fileTransmitter->setPortName(ui->comPort->currentText());
        fileTransmitter->setPortBaudRate(ui->comBaudRate->currentText().toInt());

        if(fileTransmitter->startTransmit() == true)
        {
            transmitButtonStatus = true;

            ui->comButton->setDisabled(true);
            ui->refreshButton->setDisabled(true);

            ui->receivePath->setDisabled(true);
            ui->receiveBrowse->setDisabled(true);
            ui->receiveButton->setDisabled(true);

            ui->transmitBrowse->setDisabled(true);
            ui->transmitButton->setText(u8"取消");
            ui->transmitProgress->setValue(0);
        }
        else
        {
            ShowMessage(this, u8"失败", u8"文件发送失败！", QMessageBox::Warning);
        }
    }
    else
    {
        fileTransmitter->stopTransmit();
    }
}

void Widget::on_receiveButton_clicked()
{
    if(receiveButtonStatus == false)
    {
        serialPort->close();

        const ProtocolKind receiveProtocol = ProtocolFactory::fromName(ui->protocol->currentText());
        fileReceiver->setProtocolKind(receiveProtocol);
        fileReceiver->setFilePath(ui->receivePath->text());
        fileReceiver->setPortName(ui->comPort->currentText());
        fileReceiver->setPortBaudRate(ui->comBaudRate->currentText().toInt());

        if(fileReceiver->startReceive() == true)
        {
            receiveButtonStatus = true;

            ui->comButton->setDisabled(true);
            ui->refreshButton->setDisabled(true);

            ui->transmitBrowse->setDisabled(true);
            ui->transmitButton->setDisabled(true);

            ui->receivePath->setDisabled(true);
            ui->receiveBrowse->setDisabled(true);
            ui->receiveButton->setText(u8"取消");
            if(receiveProtocol == ProtocolKind::Xmodem)
            {
                ui->receiveProgress->setRange(0, 0);
            }
            else
            {
                ui->receiveProgress->setRange(0, 100);
                ui->receiveProgress->setValue(0);
            }
        }
        else
        {
            ui->receiveProgress->setRange(0, 100);
            ui->receiveProgress->setValue(0);

            ShowMessage(this, u8"失败", u8"文件接收失败！", QMessageBox::Warning);
        }
    }
    else
    {
        fileReceiver->stopReceive();
    }
}

void Widget::transmitProgress(int progress)
{
    ui->transmitProgress->setValue(progress);
}

void Widget::receiveProgress(int progress)
{
    ui->receiveProgress->setValue(progress);
}

void Widget::transmitStatus(FileTransmitter::Status status)
{
    switch(status)
    {
        case ITransferProtocol::StatusEstablish:
        {
            break;
        }

        case ITransferProtocol::StatusTransmit:
        {
            break;
        }

        case ITransferProtocol::StatusFinish:
        {
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);
            ui->refreshButton->setEnabled(true);

            ui->receivePath->setEnabled(true);
            ui->receiveBrowse->setEnabled(true);

            if(ui->receivePath->text().isEmpty() != true)
            {
                ui->receiveButton->setEnabled(true);
            }

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"发送");

            ShowMessage(this, u8"成功", u8"文件发送成功！", QMessageBox::Information);

            break;
        }

        case ITransferProtocol::StatusAbort:
        {
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);
            ui->refreshButton->setEnabled(true);

            ui->receivePath->setEnabled(true);
            ui->receiveBrowse->setEnabled(true);

            if(ui->receivePath->text().isEmpty() != true)
            {
                ui->receiveButton->setEnabled(true);
            }

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"发送");

            ShowMessage(this, u8"失败", u8"文件发送失败！", QMessageBox::Warning);

            break;
        }

        case ITransferProtocol::StatusTimeout:
        {
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);
            ui->refreshButton->setEnabled(true);

            ui->receivePath->setEnabled(true);
            ui->receiveBrowse->setEnabled(true);

            if(ui->receivePath->text().isEmpty() != true)
            {
                ui->receiveButton->setEnabled(true);
            }

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"发送");

            ShowMessage(this, u8"失败", u8"文件发送失败！", QMessageBox::Warning);

            break;
        }

        default:
        {
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);
            ui->refreshButton->setEnabled(true);

            ui->receivePath->setEnabled(true);
            ui->receiveBrowse->setEnabled(true);

            if(ui->receivePath->text().isEmpty() != true)
            {
                ui->receiveButton->setEnabled(true);
            }

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"发送");

            ShowMessage(this, u8"失败", u8"文件发送失败！", QMessageBox::Warning);
        }
    }
}

void Widget::receiveStatus(FileReceiver::Status status)
{
    switch(status)
    {
        case ITransferProtocol::StatusEstablish:
        {
            break;
        }

        case ITransferProtocol::StatusTransmit:
        {
            break;
        }

        case ITransferProtocol::StatusFinish:
        {
            receiveButtonStatus = false;

            ui->comButton->setEnabled(true);
            ui->refreshButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);

            if(ui->transmitPath->text().isEmpty() != true)
            {
                ui->transmitButton->setEnabled(true);
            }

            ui->receiveProgress->setRange(0, 100);
            ui->receiveProgress->setValue(100);
            ui->receivePath->setEnabled(true);
            ui->receiveBrowse->setEnabled(true);
            ui->receiveButton->setText(u8"接收");

            ShowMessage(this, u8"成功", u8"文件接收成功！", QMessageBox::Information);

            break;
        }

        case ITransferProtocol::StatusAbort:
        {
            receiveButtonStatus = false;

            ui->comButton->setEnabled(true);
            ui->refreshButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);

            if(ui->transmitPath->text().isEmpty() != true)
            {
                ui->transmitButton->setEnabled(true);
            }

            ui->receiveProgress->setRange(0, 100);
            ui->receivePath->setEnabled(true);
            ui->receiveBrowse->setEnabled(true);
            ui->receiveButton->setText(u8"接收");

            ui->receiveProgress->setValue(0);

            ShowMessage(this, u8"失败", u8"文件接收失败！", QMessageBox::Warning);

            break;
        }

        case ITransferProtocol::StatusTimeout:
        {
            receiveButtonStatus = false;

            ui->comButton->setEnabled(true);
            ui->refreshButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);

            if(ui->transmitPath->text().isEmpty() != true)
            {
                ui->transmitButton->setEnabled(true);
            }

            ui->receiveProgress->setRange(0, 100);
            ui->receivePath->setEnabled(true);
            ui->receiveBrowse->setEnabled(true);
            ui->receiveButton->setText(u8"接收");

            ui->receiveProgress->setValue(0);

            ShowMessage(this, u8"失败", u8"文件接收失败！", QMessageBox::Warning);

            break;
        }

        default:
        {
            receiveButtonStatus = false;

            ui->comButton->setEnabled(true);
            ui->refreshButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);

            if(ui->transmitPath->text().isEmpty() != true)
            {
                ui->transmitButton->setEnabled(true);
            }

            ui->receiveProgress->setRange(0, 100);
            ui->receivePath->setEnabled(true);
            ui->receiveBrowse->setEnabled(true);
            ui->receiveButton->setText(u8"接收");

            ui->receiveProgress->setValue(0);

            ShowMessage(this, u8"失败", u8"文件接收失败！", QMessageBox::Warning);
        }
    }
}

void Widget::readMonitorData()
{
    const QByteArray data = serialPort->readAll();
    if(data.isEmpty() != true)
    {
        appendRawData(data);
    }
}

void Widget::appendRawData(const QByteArray &data)
{
    if(data.isEmpty())
    {
        return;
    }

    const bool segmented = appendToRxLineCache(data);
    if(rxPaused)
    {
        return;
    }

    if(segmented)
    {
        pendingRxRender.clear();
        rxRenderTimer->stop();
        renderRxCache();
        return;
    }

    pendingRxRender.append(data);
    if(pendingRxRender.size() >= MaxPendingRxRenderBytes)
    {
        flushRxRender();
        return;
    }

    if(!rxRenderTimer->isActive())
    {
        rxRenderTimer->start(RxRenderIntervalMs);
    }
}

void Widget::refreshRxLog()
{
    pendingRxRender.clear();
    rxRenderTimer->stop();
    renderRxCache();
}

void Widget::flushRxRender()
{
    if(rxPaused || pendingRxRender.isEmpty())
    {
        return;
    }

    const QByteArray data = pendingRxRender;
    pendingRxRender.clear();
    renderRawData(data);
}

void Widget::refreshSerialPorts()
{
    const QString currentPort = ui->comPort->currentText();

    ui->comPort->blockSignals(true);
    ui->comPort->clear();

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for(const QSerialPortInfo &serialPortInfo : ports)
    {
        ui->comPort->addItem(serialPortInfo.portName());
    }

    const int currentIndex = ui->comPort->findText(currentPort);
    if(currentIndex >= 0)
    {
        ui->comPort->setCurrentIndex(currentIndex);
    }
    else if(currentPort.isEmpty() != true)
    {
        ui->comPort->addItem(currentPort);
        ui->comPort->setCurrentText(currentPort);
    }
    else if(ui->comPort->count() > 0)
    {
        ui->comPort->setCurrentIndex(0);
    }

    ui->comPort->blockSignals(false);
}

bool Widget::appendToRxLineCache(const QByteArray &data)
{
    bool segmented = false;

    for(char ch : data)
    {
        currentRxLine.append(ch);

        if(ch == '\n')
        {
            rxLines.append(currentRxLine);
            currentRxLine.clear();
            trimRxLineCache();
            continue;
        }

        if(currentRxLine.size() >= MaxRxLineBytes)
        {
            currentRxLine.append('\n');
            rxLines.append(currentRxLine);
            currentRxLine.clear();
            trimRxLineCache();
            segmented = true;
        }
    }

    return segmented;
}

void Widget::trimRxLineCache()
{
    const int overflow = rxLines.size() - MaxRxLogLines;
    if(overflow > 0)
    {
        rxLines.erase(rxLines.begin(), rxLines.begin() + overflow);
    }
}

void Widget::renderRxCache()
{
    rxLog->clear();
    for(const QByteArray &line : rxLines)
    {
        renderRawData(line);
    }
    renderRawData(currentRxLine);
}

void Widget::renderRawData(const QByteArray &data)
{
    if(data.isEmpty())
    {
        return;
    }

    QString text;
    text.reserve(rxHexCheckBox->isChecked() ? data.size() * 3 : data.size());
    for(unsigned char ch : data)
    {
        if(rxHexCheckBox->isChecked())
        {
            AppendHexByte(text, ch);
            text.append(QLatin1Char(' '));
            if(ch == '\n')
            {
                text.append(QLatin1Char('\n'));
            }
            continue;
        }

        if(ch == '\n')
        {
            text.append(QLatin1Char('\n'));
            continue;
        }
        if(ch == '\r')
        {
            continue;
        }
        if(ch >= 0x20 && ch <= 0x7e)
        {
            text.append(QLatin1Char(ch));
        }
        else
        {
            text.append(QLatin1Char('<'));
            AppendHexByte(text, ch);
            text.append(QLatin1Char('>'));
        }
    }

    QTextCursor cursor = rxLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    rxLog->setTextCursor(cursor);
    rxLog->ensureCursorVisible();
}
