#include "CigVision.h"
#include <QtWidgets/QApplication>
#include <qwidget.h>
#include<qvboxlayout>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CigVision w;
    
    //运行界面
    QVBoxLayout* layout = new QVBoxLayout(&w);
    


    //参数设置界面

    //统计查询界面 

    //系统设置 界面

    //w.showFullScreen();
    w.resize(1920, 1080);
    w.setFixedSize(1920, 1080);
    w.show();
    return a.exec();
}

