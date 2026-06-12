#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QByteArray>
#include <QCheckBox>
#include <QPlainTextEdit>
#include "transfer/FileTransmitter.h"
#include "transfer/FileReceiver.h"

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

private slots:
    void on_comButton_clicked();
    void on_refreshButton_clicked();
    void on_transmitBrowse_clicked();
    void on_receiveBrowse_clicked();
    void on_transmitButton_clicked();
    void on_receiveButton_clicked();
    void transmitProgress(int progress);
    void receiveProgress(int progress);
    void transmitStatus(FileTransmitter::Status status);
    void receiveStatus(FileReceiver::Status status);
    void readMonitorData();
    void appendRawData(const QByteArray &data);
    void refreshRxLog();

private:
    void refreshSerialPorts();
    void renderRawData(const QByteArray &data);

    Ui::Widget *ui;
    QSerialPort *serialPort;
    FileTransmitter *fileTransmitter;
    FileReceiver *fileReceiver;
    QPlainTextEdit *rxLog;
    QCheckBox *rxHexCheckBox;
    QByteArray rxBuffer;

    bool transmitButtonStatus;
    bool receiveButtonStatus;
};

#endif // WIDGET_H
