#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QByteArray>
#include <QCheckBox>
#include <QList>
#include <QPlainTextEdit>
#include "transfer/FileTransmitter.h"
#include "transfer/FileReceiver.h"

namespace Ui {
class Widget;
}

class QTimer;
class QEvent;
class QPushButton;

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

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
    void flushRxRender();

private:
    void refreshSerialPorts();
    void applyTransmitConfig();
    void openSettingsDialog();
    void updateSettingsIcon();
    bool appendToRxLineCache(const QByteArray &data);
    void trimRxLineCache();
    void renderRxCache();
    void renderRawData(const QByteArray &data);

    Ui::Widget *ui;
    QSerialPort *serialPort;
    FileTransmitter *fileTransmitter;
    FileReceiver *fileReceiver;
    QPlainTextEdit *rxLog;
    QCheckBox *rxHexCheckBox;
    QPushButton *settingsButton;
    QTimer *rxRenderTimer;
    QList<QByteArray> rxLines;
    QByteArray currentRxLine;
    QByteArray pendingRxRender;
    bool rxPaused;
    bool settingsButtonHovered;

    bool transmitButtonStatus;
    bool receiveButtonStatus;
};

#endif // WIDGET_H
