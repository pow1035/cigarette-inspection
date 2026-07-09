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
#include "testWrite.h"
#include <ctime>

#include"C:/Advantech/DAQNavi/Inc/bdaqctrl.h"

using namespace Automation::BDaq;

class testQT;
class readIOTask : public QObject, public QRunnable
{
	Q_OBJECT
public:
	readIOTask(testQT* pUser);
protected:
	void run();
private:
	//HANDLE artCard;//IO卡句柄
	InstantDiCtrl* instantDiCtrl;//研华卡句柄
	ErrorCode  cardRet = Success;
	testQT* mainDlg;//窗体句柄
	bool readEnable;
	uint8  bufferForReading[2] = { 0 };//the first element of this array is used for start port
	//BYTE byte_PA[8];
	//BYTE byte_PB[8];
	int sidPicture;
	int number_Camera;
	unsigned long readErrorTimes = 0;
	//时间测试
	LARGE_INTEGER litmp;
	LONGLONG QStartCount, expendTime;//毫秒ms
	double dfFreq;//CPU频率
	
	LONGLONG initTime();
	LONGLONG getExpendTime(LONGLONG startQpart);

};

