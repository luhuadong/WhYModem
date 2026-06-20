#include "ui/versiondialog.h"
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#ifndef WHYMODEM_VERSION
#define WHYMODEM_VERSION "dev"
#endif

namespace {

QString ReadResourceText(const QString &path)
{
    QFile file(path);
    if(!file.open(QFile::ReadOnly | QFile::Text))
    {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QPlainTextEdit *CreateTextView(const QString &text, QWidget *parent)
{
    QPlainTextEdit *view = new QPlainTextEdit(parent);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    view->setPlainText(text);
    return view;
}

} // namespace

VersionDialog::VersionDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(u8"版本信息");
    resize(680, 520);

    QLabel *title = new QLabel(QString("WhYModem %1").arg(WHYMODEM_VERSION), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    title->setFont(titleFont);

    QTabWidget *tabs = new QTabWidget(this);
    tabs->addTab(CreateTextView(ReadResourceText(":/docs/CHANGELOG.md"), tabs), u8"Change Log");
    tabs->addTab(CreateTextView(ReadResourceText(":/docs/LICENSE"), tabs), u8"License");
    tabs->addTab(CreateTextView(ReadResourceText(":/docs/THIRD_PARTY_NOTICES.md"), tabs), u8"开源软件清单");

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(u8"关闭");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(title);
    mainLayout->addWidget(tabs, 1);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &VersionDialog::reject);
}
