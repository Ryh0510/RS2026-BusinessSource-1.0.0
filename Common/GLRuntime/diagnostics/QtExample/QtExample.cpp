#include <QApplication>
#include "GLWidget.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    GLWidget w;
    w.resize(800,600);
    w.show();
    return a.exec();
}
