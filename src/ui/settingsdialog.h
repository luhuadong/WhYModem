#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "config/AppConfig.h"
#include <QDialog>

class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = 0);

signals:
    void transmitDelaySaved(const TransmitDelayConfig &config);

private:
    void saveConfig();

    QSpinBox *firstDataDelay;
    QSpinBox *interPacketDelay;
};

#endif // SETTINGSDIALOG_H
