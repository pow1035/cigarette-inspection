#pragma once
#include <QObject>
#include <QRunnable>
#include <atomic>

//#include <opencv2/opencv.hpp>
//#include <opencv2/core.hpp>
#include <Windows.h>//计算用时
#include <vector>
//#include"USB5841.h"
#include <qthread.h>
#include <ctime>


#include <bdaqctrl.h>
#define  deviceDescription  L"PCIE-1730,BID#0"
//using namespace Automation::BDaq;

class CigVision;
class readIOTask : public QObject, public QRunnable
{
	Q_OBJECT
public:
	readIOTask(CigVision* pUser);
	~readIOTask();
	bool initialize();
	bool prepareStart();
	void requestStop();
signals:
	void fatalReadError();
protected:
	void run();
private:
	//HANDLE artCard;//IO卡句柄
	Automation::BDaq::InstantDiCtrl* instantDiCtrl = nullptr;//研华卡句柄
	Automation::BDaq::ErrorCode  cardRet = Automation::BDaq::ErrorCode::Success;
	CigVision* mainDlg;//窗体句柄
	std::atomic_bool readEnable{ false };
	std::atomic_bool initialized{ false };
	int component1ToReject = 0;
	int component2ToReject = 0;
	Automation::BDaq::uint8  bufferForReading[2] = { 0 };//the first element of this array is used for start port
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
