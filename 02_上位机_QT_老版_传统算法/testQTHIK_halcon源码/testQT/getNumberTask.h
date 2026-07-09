
#pragma once
#include <QObject>
#include <QRunnable>
#include "testQT.h"
//#include <opencv2/opencv.hpp>
//#include <opencv2/core.hpp>
#include <Windows.h>//计算用时
#include <vector>
class testQT;
class getNumberTask : public QObject, public QRunnable
{
    Q_OBJECT

    getNumberTask();
    ~getNumberTask();
protected:
    void run();

public:

    int camera_number;//相机编号
    int photo_number;//图片编号

    //bool getAllMask(InputArray _originalMat, OutputArray _allMask);
//signals:

    //void mySignal();
};
