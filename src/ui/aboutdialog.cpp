#include "ui/aboutdialog.h"
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QVBoxLayout>

#ifndef WHYMODEM_VERSION
#define WHYMODEM_VERSION "dev"
#endif

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(u8"关于 WhYModem");
    setMinimumWidth(430);

    QLabel *title = new QLabel(QString("WhYModem %1").arg(WHYMODEM_VERSION), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);

    QLabel *logo = new QLabel(this);
    logo->setPixmap(QPixmap(":/icons/logo.png").scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setFixedSize(48, 48);
    logo->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QHBoxLayout *titleLayout = new QHBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->addWidget(title, 1);
    titleLayout->addWidget(logo);

    QLabel *description = new QLabel(u8"WhYModem 是一个通过串口 XMODEM/YMODEM/ZMODEM 协议进行文件传输的 Qt 工具软件。", this);
    description->setWordWrap(true);

    QLabel *details = new QLabel(this);
    details->setText(u8"作者：<a href=\"https://github.com/luhuadong\">luhuadong</a> &lt;luhuadong@163.com&gt;<br>"
                     u8"项目地址：<a href=\"https://github.com/luhuadong/WhYModem\">https://github.com/luhuadong/WhYModem</a>");
    details->setTextFormat(Qt::RichText);
    details->setTextInteractionFlags(Qt::TextBrowserInteraction);
    details->setOpenExternalLinks(true);
    details->setWordWrap(true);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(u8"关闭");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->addLayout(titleLayout);
    mainLayout->addWidget(description);
    mainLayout->addWidget(details);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &AboutDialog::reject);
}
