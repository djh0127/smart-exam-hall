#include "mainwindow.h"
#include "courcedialog.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    CourceDialog c;
    c.show();
    return a.exec();
}
