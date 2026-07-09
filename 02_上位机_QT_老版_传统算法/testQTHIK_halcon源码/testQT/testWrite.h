#pragma once
#include <QObject>
#include <QRunnable>

#include "testQT.h"
//#include <opencv2/opencv.hpp>
//#include <opencv2/core.hpp>
#include <Windows.h>//计算用时
#include <vector>
//#include"USB5841.h"
#include <qthread.h>
class testQT;
class testWrite:public QObject, public QRunnable
{
	Q_OBJECT
public:
	testWrite(testQT* pUser);
	testWrite(testQT* pUser,int sidPicture);
protected:
	void run();
private:
	HANDLE instantDiCtrl;//IO卡输入句柄
	testQT* mainDlg;//窗体句柄
	bool writeEnable;
	BYTE byte_PC[8];
	BYTE byte_PD[8];
	BYTE byte_PC0[8];
	BYTE byte_PD0[8];
	int sidPicture;
	//int number_Camera;
};

