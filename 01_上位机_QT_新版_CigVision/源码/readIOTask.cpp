#include "readIOTask.h"
#include <qthread.h>
readIOTask::readIOTask(CigVision* pUser)
{
	mainDlg = pUser;
	sidPicture=0;
	number_Camera=0;
	
	readEnable = 1;
}
void readIOTask::run()
{
	int last_sidPicture = 0;
	bool last_enable = false;
	bool enable = false;
	//clock_t t1 = clock();//开始时间
	QStartCount =initTime();
	instantDiCtrl = Automation::BDaq::InstantDiCtrl::Create();
	Automation::BDaq::DeviceInformation devInfo(deviceDescription);

	cardRet = instantDiCtrl->setSelectedDevice(devInfo);
	if (cardRet != Automation::BDaq::Success)
	{
		qDebug() << "IOCardRead_setSelectedDevice_fail"<< endl;
	}
	//LONGLONG expendTime = getExpendTime(QStartCount);
	//qDebug() << "task readIOTaskInitExpend expend" << expendTime << endl;
	while (readEnable)
	{
		//QThread::usleep(20);//20us

		if (instantDiCtrl->Read(0, 2, bufferForReading))
		{
			//如果失败
			readErrorTimes = readErrorTimes + 1;
			qDebug() << "readIOError" << readErrorTimes << endl;
			continue;
		}
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
				mainDlg->machineState.nowShowPicReadIO1 = bufferForReading[0];//组件I/O编号（IO直发）
				//组件1烟支编号
				Automation::BDaq::uint8 tempChar=0;
				//int i = 8;
				Automation::BDaq::uint8 moveRight = 0x80;
				Automation::BDaq::uint8 moveLeft = 0x01;
				while (moveRight)
				{
					if (moveRight & bufferForReading[0])
					{
						tempChar = tempChar | moveLeft;
					}
					moveRight = moveRight >> 1;
					moveLeft = moveLeft << 1;
				}
				mainDlg->machineState.nowPictureNumber1 = tempChar;//首尾颠倒后编号
				//组件2_1烟支编号
				Automation::BDaq::uint8 temp2_1Char;
				int zu2ToZu1Steps = 0;//组件2到组件1距离工位数
				if (mainDlg->params.systemParams.component2ToReject > mainDlg->params.systemParams.component1ToReject)
				{
					zu2ToZu1Steps = mainDlg->params.systemParams.component2ToReject - mainDlg->params.systemParams.component1ToReject;
				}

				if (tempChar + zu2ToZu1Steps > 99)
				{
					temp2_1Char = tempChar + zu2ToZu1Steps - 100;
				}
				else {
					temp2_1Char = tempChar + zu2ToZu1Steps;
				}
				//组件2_2烟支编号
				Automation::BDaq::uint8 temp2_2Char;
				if (tempChar + zu2ToZu1Steps -1 > 99)
				{
					temp2_2Char = tempChar + zu2ToZu1Steps - 100 - 1;
				}
				else {
					temp2_2Char = tempChar + zu2ToZu1Steps - 1;
				}

				mainDlg->machineState.nowPictureNumber2_1 = temp2_1Char;//2_1组件编号
				mainDlg->machineState.nowPictureNumber2_2 = temp2_2Char;//2_2组件编号
				//组件2_1烟支IO编号
				Automation::BDaq::uint8 tempZu2_1IO=0;//2_1输出
				Automation::BDaq::uint8 tempZu2_2IO=0;//2_2输出
				moveLeft = 0x01;
				moveRight = 0x80;
				while (moveRight)
				{
					if (moveRight & temp2_1Char)
					{
						tempZu2_1IO = tempZu2_1IO | moveLeft;
					}
					if (moveRight & temp2_2Char)
					{
						tempZu2_2IO = tempZu2_2IO | moveLeft;
					}
					moveRight = moveRight >> 1;
					moveLeft = moveLeft << 1;
				}
				mainDlg->machineState.nowShowPicReadIO2_1 = tempZu2_1IO;//组件2_1 I/O编号（IO直发）
				mainDlg->machineState.nowShowPicReadIO2_2 = tempZu2_2IO;//组件2_2 I/O编号（IO直发）

				//qDebug() << "  picNumber" << tempChar << endl;
				//qDebug() << "  nowShowPicReadIO2_1" << tempZu2_1IO << endl;



				continue;
			}
			else {//未在触发状态，无触发，无操作
				continue;
			}
		}

	}
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