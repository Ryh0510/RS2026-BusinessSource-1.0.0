#include "CdfDemoWindow.h"

#include <QApplication>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    cdf_gui::CdfDemoWindow window;
    window.show();
    return app.exec();
}
