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
    a.setDesktopFileName("io.geekat.whymodem");
    const QIcon appIcon(":/icons/logo.png");
    a.setWindowIcon(appIcon);

    Widget w;
    w.setWindowIcon(appIcon);
    w.show();

    return a.exec();
}
