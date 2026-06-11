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
#include <QVBoxLayout>

namespace {

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
    rxHexCheckBox(new QCheckBox(u8"十六进制显示"))
{
    transmitButtonStatus = false;
    receiveButtonStatus  = false;

    ui->setupUi(this);
    setMinimumSize(760, 620);
    resize(800, 660);

    QGroupBox *rxGroup = new QGroupBox(u8"接收窗口", this);
    QVBoxLayout *rxLayout = new QVBoxLayout(rxGroup);
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    QPushButton *clearRx = new QPushButton(u8"清空", rxGroup);
    rxGroup->setMinimumHeight(220);
    rxLayout->setSpacing(8);
    rxHexCheckBox->setParent(rxGroup);
    rxHexCheckBox->setChecked(false);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(rxHexCheckBox);
    toolbarLayout->addWidget(clearRx);
    rxLog->setReadOnly(true);
    rxLog->setMinimumHeight(120);
    rxLog->setLineWrapMode(QPlainTextEdit::NoWrap);
    rxLayout->addWidget(rxLog, 1);
    rxLayout->addLayout(toolbarLayout);
    ui->verticalLayout_3->addWidget(rxGroup, 1);

    QSerialPortInfo serialPortInfo;

    foreach(serialPortInfo, QSerialPortInfo::availablePorts())
    {
        ui->comPort->addItem(serialPortInfo.portName());
    }

    serialPort->setPortName("COM1");
    serialPort->setBaudRate(115200);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    connect(fileTransmitter, SIGNAL(transmitProgress(int)), this, SLOT(transmitProgress(int)));
    connect(fileReceiver, SIGNAL(receiveProgress(int)), this, SLOT(receiveProgress(int)));
    connect(fileTransmitter, SIGNAL(transmitStatus(FileTransmitter::Status)), this, SLOT(transmitStatus(FileTransmitter::Status)));
    connect(fileReceiver, SIGNAL(receiveStatus(FileReceiver::Status)), this, SLOT(receiveStatus(FileReceiver::Status)));
    connect(fileTransmitter, SIGNAL(rawDataReceived(QByteArray)), this, SLOT(appendRawData(QByteArray)));
    connect(fileReceiver, SIGNAL(rawDataReceived(QByteArray)), this, SLOT(appendRawData(QByteArray)));
    connect(serialPort, SIGNAL(readyRead()), this, SLOT(readMonitorData()));
    connect(rxHexCheckBox, SIGNAL(toggled(bool)), this, SLOT(refreshRxLog()));
    connect(clearRx, &QPushButton::clicked, this, [this]() {
        rxBuffer.clear();
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
            ui->comButton->setText(u8"关闭串口");

            ui->transmitBrowse->setEnabled(true);
            ui->receiveBrowse->setEnabled(true);

            if(ui->transmitPath->text().isEmpty() != true)
            {
                ui->transmitButton->setEnabled(true);
            }

            if(ui->receivePath->text().isEmpty() != true)
            {
                ui->receiveButton->setEnabled(true);
            }
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
        ui->comButton->setText(u8"打开串口");

        ui->transmitBrowse->setDisabled(true);
        ui->transmitButton->setDisabled(true);

        ui->receiveBrowse->setDisabled(true);
        ui->receiveButton->setDisabled(true);
    }
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
    ui->receivePath->setText(QFileDialog::getExistingDirectory(this, u8"选择目录", ".", QFileDialog::ShowDirsOnly));

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

        fileTransmitter->setFileName(ui->transmitPath->text());
        fileTransmitter->setPortName(ui->comPort->currentText());
        fileTransmitter->setPortBaudRate(ui->comBaudRate->currentText().toInt());

        if(fileTransmitter->startTransmit() == true)
        {
            transmitButtonStatus = true;

            ui->comButton->setDisabled(true);

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

        fileReceiver->setFilePath(ui->receivePath->text());
        fileReceiver->setPortName(ui->comPort->currentText());
        fileReceiver->setPortBaudRate(ui->comBaudRate->currentText().toInt());

        if(fileReceiver->startReceive() == true)
        {
            receiveButtonStatus = true;

            ui->comButton->setDisabled(true);

            ui->transmitBrowse->setDisabled(true);
            ui->transmitButton->setDisabled(true);

            ui->receiveBrowse->setDisabled(true);
            ui->receiveButton->setText(u8"取消");
            ui->receiveProgress->setValue(0);
        }
        else
        {
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

void Widget::transmitStatus(Ymodem::Status status)
{
    switch(status)
    {
        case FileTransmitter::StatusEstablish:
        {
            break;
        }

        case FileTransmitter::StatusTransmit:
        {
            break;
        }

        case FileTransmitter::StatusFinish:
        {
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);

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

        case FileTransmitter::StatusAbort:
        {
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);

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

        case FileTransmitter::StatusTimeout:
        {
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);

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
        case FileReceiver::StatusEstablish:
        {
            break;
        }

        case FileReceiver::StatusTransmit:
        {
            break;
        }

        case FileReceiver::StatusFinish:
        {
            receiveButtonStatus = false;

            ui->comButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);

            if(ui->transmitPath->text().isEmpty() != true)
            {
                ui->transmitButton->setEnabled(true);
            }

            ui->receiveBrowse->setEnabled(true);
            ui->receiveButton->setText(u8"接收");

            ShowMessage(this, u8"成功", u8"文件接收成功！", QMessageBox::Information);

            break;
        }

        case FileReceiver::StatusAbort:
        {
            receiveButtonStatus = false;

            ui->comButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);

            if(ui->transmitPath->text().isEmpty() != true)
            {
                ui->transmitButton->setEnabled(true);
            }

            ui->receiveBrowse->setEnabled(true);
            ui->receiveButton->setText(u8"接收");

            ShowMessage(this, u8"失败", u8"文件接收失败！", QMessageBox::Warning);

            break;
        }

        case FileReceiver::StatusTimeout:
        {
            receiveButtonStatus = false;

            ui->comButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);

            if(ui->transmitPath->text().isEmpty() != true)
            {
                ui->transmitButton->setEnabled(true);
            }

            ui->receiveBrowse->setEnabled(true);
            ui->receiveButton->setText(u8"接收");

            ShowMessage(this, u8"失败", u8"文件接收失败！", QMessageBox::Warning);

            break;
        }

        default:
        {
            receiveButtonStatus = false;

            ui->comButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);

            if(ui->transmitPath->text().isEmpty() != true)
            {
                ui->transmitButton->setEnabled(true);
            }

            ui->receiveBrowse->setEnabled(true);
            ui->receiveButton->setText(u8"接收");

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
    rxBuffer.append(data);
    renderRawData(data);
}

void Widget::refreshRxLog()
{
    rxLog->clear();
    renderRawData(rxBuffer);
}

void Widget::renderRawData(const QByteArray &data)
{
    if(data.isEmpty())
    {
        return;
    }

    QTextCursor cursor = rxLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    for(unsigned char ch : data)
    {
        if(rxHexCheckBox->isChecked())
        {
            cursor.insertText(QString("%1 ").arg(static_cast<unsigned>(ch), 2, 16, QChar('0')).toUpper());
            if(ch == '\n')
            {
                cursor.insertBlock();
            }
            continue;
        }

        if(ch == '\n')
        {
            cursor.insertBlock();
            continue;
        }
        if(ch == '\r')
        {
            continue;
        }
        if(ch >= 0x20 && ch <= 0x7e)
        {
            cursor.insertText(QString(QChar(ch)));
        }
        else
        {
            cursor.insertText(QString("<%1>").arg(static_cast<unsigned>(ch), 2, 16, QChar('0')).toUpper());
        }
    }
    rxLog->setTextCursor(cursor);
    rxLog->ensureCursorVisible();
}
