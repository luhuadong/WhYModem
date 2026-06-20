#include "ui/widget.h"
#include <QCoreApplication>
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("GeekAt");
    QCoreApplication::setApplicationName("WhYModem");
    QCoreApplication::setApplicationVersion(WHYMODEM_VERSION);
    a.setWindowIcon(QIcon(":/icons/logo.png"));

    Widget w;
    w.show();

    return a.exec();
}
