#include "readIOTask.h"
#include "CigVision.h"
#include <qthread.h>
#include <QMutexLocker>

namespace
{
Automation::BDaq::uint8 reverseBits(Automation::BDaq::uint8 value)
{
	Automation::BDaq::uint8 reversed = 0;
	for (int bit = 0; bit < 8; ++bit)
	{
		reversed = static_cast<Automation::BDaq::uint8>((reversed << 1) | (value & 0x01));
		value = static_cast<Automation::BDaq::uint8>(value >> 1);
	}
	return reversed;
}

Automation::BDaq::uint8 wrapPictureNumber(int value)
{
	value %= 100;
	if (value < 0)
	{
		value += 100;
	}
	return static_cast<Automation::BDaq::uint8>(value);
}
}

readIOTask::readIOTask(CigVision* pUser)
{
	mainDlg = pUser;
	sidPicture=0;
	number_Camera=0;

}

readIOTask::~readIOTask()
{
	requestStop();
	if (instantDiCtrl != nullptr)
	{
		instantDiCtrl->Dispose();
		instantDiCtrl = nullptr;
	}
}

bool readIOTask::initialize()
{
	if (initialized.load())
	{
		return true;
	}
	instantDiCtrl = Automation::BDaq::InstantDiCtrl::Create();
	if (instantDiCtrl == nullptr)
	{
		qDebug() << "IOCardRead_create_fail" << endl;
		return false;
	}
	Automation::BDaq::DeviceInformation devInfo(deviceDescription);
	cardRet = instantDiCtrl->setSelectedDevice(devInfo);
	if (cardRet != Automation::BDaq::Success)
	{
		qDebug() << "IOCardRead_setSelectedDevice_fail" << endl;
		instantDiCtrl->Dispose();
		instantDiCtrl = nullptr;
		return false;
	}
	initialized.store(true);
	return true;
}

bool readIOTask::prepareStart()
{
	if (!initialized.load() || readEnable.load())
	{
		return false;
	}
	const int configuredComponent1 = mainDlg->params.systemParams.component1ToReject;
	const int configuredComponent2 = mainDlg->params.systemParams.component2ToReject;
	if (configuredComponent1 < 10 || configuredComponent1 > 30 ||
		configuredComponent2 < 20 || configuredComponent2 > 40 ||
		configuredComponent2 <= configuredComponent1)
	{
		qDebug() << "invalid component reject positions" << endl;
		return false;
	}
	component1ToReject = configuredComponent1;
	component2ToReject = configuredComponent2;
	readErrorTimes = 0;
	readEnable.store(true);
	return true;
}

void readIOTask::requestStop()
{
	readEnable.store(false);
}

void readIOTask::run()
{
	bool last_enable = false;
	bool enable = false;
	const int runComponent1ToReject = component1ToReject;
	const int runComponent2ToReject = component2ToReject;
	//clock_t t1 = clock();//开始时间
	QStartCount =initTime();
	if (!initialized.load() || instantDiCtrl == nullptr)
	{
		readEnable.store(false);
		return;
	}
	//LONGLONG expendTime = getExpendTime(QStartCount);
	//qDebug() << "task readIOTaskInitExpend expend" << expendTime << endl;
	while (readEnable.load())
	{
		QThread::usleep(50);

		if (instantDiCtrl->Read(0, 2, bufferForReading))
		{
			//如果失败
			readErrorTimes = readErrorTimes + 1;
			qDebug() << "readIOError" << readErrorTimes << endl;
			if (readErrorTimes >= 100)
			{
				qDebug() << "IOCardRead_stopped_after_consecutive_errors" << endl;
				readEnable.store(false);
				emit fatalReadError();
			}
			continue;
		}
		readErrorTimes = 0;
		enable = bufferForReading[1] & 0x01;//当前触发状态
		if (last_enable)//已在触发程序中
		{
			if (enable)//已在触发程序中，触发，无操作
			{
				continue;
			}
			else {//已在触发程序中，无触发，复位触发状态
				last_enable = false;
				continue;
			}
		}
		else {//未在触发状态

			if (enable)//未在触发状态，触发，置位触发状态，编号执行更新
			{
				////QThread::usleep(20);//20us
				//if (instantDiCtrl->Read(0, 2, bufferForReading))//延时后重新读入，消抖
				//{
				//	//如果失败，不置位上次使能last_enable重新读
				//	continue;
				//}
				last_enable = true;
				//sidPicture = bufferForReading[0];
				//mainDlg->nowPictureNumber= sidPicture;//更新编号
				//uchar temp = 255 - bufferForReading[0];
				const Automation::BDaq::uint8 tempChar = reverseBits(bufferForReading[0]);
				if (tempChar > 99)
				{
					qDebug() << "invalid picture number" << tempChar << endl;
					continue;
				}
				//组件2_1烟支编号
				Automation::BDaq::uint8 temp2_1Char;
				int zu2ToZu1Steps = 0;//组件2到组件1距离工位数
				if (runComponent2ToReject > runComponent1ToReject)
				{
					zu2ToZu1Steps = runComponent2ToReject - runComponent1ToReject;
				}
				else
				{
					qDebug() << "invalid component reject positions" << endl;
					continue;
				}
				temp2_1Char = wrapPictureNumber(static_cast<int>(tempChar) + zu2ToZu1Steps);
				//组件2_2烟支编号
				Automation::BDaq::uint8 temp2_2Char =
					wrapPictureNumber(static_cast<int>(tempChar) + zu2ToZu1Steps - 1);

				//组件2_1烟支IO编号
				const Automation::BDaq::uint8 tempZu2_1IO = reverseBits(temp2_1Char);
				const Automation::BDaq::uint8 tempZu2_2IO = reverseBits(temp2_2Char);

				machineStateStruct::PictureNumberSnapshot snapshot;
				snapshot.nowShowPicReadIO1 = bufferForReading[0];
				snapshot.nowShowPicReadIO2_1 = tempZu2_1IO;
				snapshot.nowShowPicReadIO2_2 = tempZu2_2IO;
				snapshot.nowPictureNumber1 = tempChar;
				snapshot.nowPictureNumber2_1 = temp2_1Char;
				snapshot.nowPictureNumber2_2 = temp2_2Char;
				{
					QMutexLocker locker(&mainDlg->machineState.mutexPictureNumbers);
					mainDlg->machineState.pictureNumbers = snapshot;
				}

				//qDebug() << "  picNumber" << tempChar << endl;
				//qDebug() << "  nowShowPicReadIO2_1" << tempZu2_1IO << endl;



				continue;
			}
			else {//未在触发状态，无触发，无操作
				continue;
			}
		}

	}

	readEnable.store(false);
	//while (readEnable)
	//{
	//	QThread::usleep(10);//100us
	//	if (!USB5841_GetDeviceDI_PA(artCard, byte_PA))
	//	{
	//		QMessageBox::warning(NULL, "", "读取失败", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//		return;
	//	}
	//	if (!USB5841_GetDeviceDI_PB(artCard, byte_PB))
	//	{
	//		QMessageBox::warning(NULL, "", "读取失败", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//		return;
	//	}
	//	sidPicture = byte_PA[0]; sidPicture += byte_PA[1] * 2; sidPicture += byte_PA[2] * 4; sidPicture += byte_PA[3] * 8;
	//	sidPicture += byte_PA[4] * 16; sidPicture += byte_PA[5] * 32; sidPicture += byte_PA[6] * 64; sidPicture += byte_PA[7] * 128;
	//	number_Camera = byte_PB[0]; number_Camera += byte_PB[1] * 2;
	//	//if (sidPicture != last_sidPicture || number_Camera != last_number_Camera)
	//	if (sidPicture != last_sidPicture)
	//	{
	//		last_sidPicture = sidPicture;
	//		if (sidPicture == 100)
	//		{
	//			clock_t t2 = clock();
	//			double t21 = 1000 * (t2 - t1) / (double)CLOCKS_PER_SEC;
	//			t1 = t2;
	//			QString tempString(QString::fromLocal8Bit("每百支烟时间间隔:"));
	//			tempString.append(QString::number(t21));
	//			//mainDlg->ui.listWidget__information->clear();
	//			mainDlg->ui.listWidget__information->addItem(tempString);
	//			//writeEnable = 0;//发一次
	//		}
	//		if (sidPicture > 0 && sidPicture < 101)
	//		{
	//			mainDlg->pool.start(new testWrite(mainDlg, sidPicture));
	//		}
	//		//last_number_Camera = number_Camera;
	//
	//		/*QString tempString(QString::fromLocal8Bit("camera:"));
	//		tempString.append(QString::number(number_Camera));
	//		tempString.append(QString::fromLocal8Bit(" picture:"));
	//		tempString.append(QString::number(sidPicture));*/
	//		//informationList.append(tempString);
	//		/*if (number_Camera == 0)
	//		{
	//			mainDlg->camera_1_number = QString::number(sidPicture);
	//		}
	//		if (number_Camera == 1)
	//		{
	//			mainDlg->camera_2_number = QString::number(sidPicture);
	//		}
	//		if (number_Camera == 2)
	//		{
	//			mainDlg->camera_3_number = QString::number(sidPicture);
	//		}
	//		if (number_Camera == 3)
	//		{
	//			mainDlg->camera_4_number = QString::number(sidPicture);
	//		}*/
	//	}
	//}
}
LONGLONG readIOTask::initTime() {
	LONGLONG Qpart1;
	//LARGE_INTEGER litmp;
	//double dfFreq;//CPU频率
	QueryPerformanceFrequency(&litmp);
	dfFreq = (double)litmp.QuadPart;
	QueryPerformanceCounter(&litmp);
	Qpart1 = litmp.QuadPart;//开始计时
	return Qpart1;
}
LONGLONG readIOTask::getExpendTime(LONGLONG startQpart)
{
	LONGLONG expendTime, Qpart2;//毫秒ms
	double dfMins, dfTime;
	QueryPerformanceCounter(&litmp);
	Qpart2 = litmp.QuadPart;//结束计时
	dfMins = (double)(Qpart2 - startQpart);
	dfTime = dfMins / dfFreq;
	expendTime = dfTime * 1000000;//us秒
	return expendTime;
}
