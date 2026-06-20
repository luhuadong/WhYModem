#include "ui/settingsdialog.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    firstDataDelay(new QSpinBox(this)),
    interPacketDelay(new QSpinBox(this))
{
    setWindowTitle(u8"设置");
    setMinimumWidth(430);

    const TransmitDelayConfig delays = AppConfig::transmitDelays();

    firstDataDelay->setRange(0, 60000);
    firstDataDelay->setSuffix(" ms");
    firstDataDelay->setValue(delays.firstDataDelayMs);

    interPacketDelay->setRange(0, 60000);
    interPacketDelay->setSuffix(" ms");
    interPacketDelay->setValue(delays.interPacketDelayMs);

    QLineEdit *configPath = new QLineEdit(AppConfig::configFilePath(), this);
    configPath->setReadOnly(true);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->addRow(u8"首包延迟：", firstDataDelay);
    formLayout->addRow(u8"包间延迟：", interPacketDelay);
    formLayout->addRow(u8"配置文件：", configPath);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Save)->setText(u8"保存");
    buttons->button(QDialogButtonBox::Close)->setText(u8"关闭");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, this, &SettingsDialog::saveConfig);
}

void SettingsDialog::saveConfig()
{
    TransmitDelayConfig delays;
    delays.firstDataDelayMs = firstDataDelay->value();
    delays.interPacketDelayMs = interPacketDelay->value();

    AppConfig::setTransmitDelays(delays);
    emit transmitDelaySaved(delays);
}
