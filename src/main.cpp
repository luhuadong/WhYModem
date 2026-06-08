#include "ui/widget.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icons/logo.png"));

    Widget w;
    w.show();

    return a.exec();
}
