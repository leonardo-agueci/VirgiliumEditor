
#include <iostream>
#include "client/view/WelcomePage.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    WelcomePage w;
    w.show();
    return a.exec();
}