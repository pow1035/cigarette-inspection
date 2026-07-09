#include "testWrite.h"
testWrite::testWrite(testQT* pUser)//测试用构造函数
{
	//this->instantDiCtrl = pUser->instantDiCtrl;
	mainDlg = pUser;
	//mainDlg = pUser;
	//byte_PA[8]=NULL;
	//byte_PB[8]=NULL;
	sidPicture = 0;
	byte_PC0[0] = 0; byte_PC0[1] = 0; byte_PC0[2] = 0; byte_PC0[3] = 0; byte_PC0[4] = 0; byte_PC0[5] = 0; byte_PC0[6] = 0; byte_PC0[7] = 0;
	byte_PD0[0] = 0; byte_PD0[1] = 0;
	writeEnable = 1;
	//number_Camera = 0;
	//readEnable = 1;
}
testWrite::testWrite(testQT* pUser, int sidPicture)//传图像参数构造函数
{
	//this->instantDiCtrl = pUser->instantDiCtrl;
	mainDlg = pUser;
	//mainDlg = pUser;
	//byte_PA[8]=NULL;
	//byte_PB[8]=NULL;
	this->sidPicture = sidPicture;
	byte_PC0[0] = 0; byte_PC0[1] = 0; byte_PC0[2] = 0; byte_PC0[3] = 0; byte_PC0[4] = 0; byte_PC0[5] = 0; byte_PC0[6] = 0; byte_PC0[7] = 0;
	byte_PD0[0] = 0; byte_PD0[1] = 0;
	writeEnable = 1;
	//number_Camera = 0;
	//readEnable = 1;
}

void testWrite::run()
{
	//int last_sidPicture = 0;
	//int last_number_Camera = 0;
	////byte_PC[0] = 0; byte_PC[1] = 0; byte_PC[2] = 0; byte_PC[3] = 0; byte_PC[4] = 0; byte_PC[5] = 0; byte_PC[6] = 0; byte_PC[7] = 0;
	////byte_PD[0] = 0; byte_PD[1] = 0;
	//memset(byte_PC, 0, sizeof(byte_PC));
	//memset(byte_PD, 0, sizeof(byte_PD));
	//while (writeEnable)
	//{
	//	
	//	QThread::msleep(6);//间隔时间
	//	//sidPicture++;//从1开始
	//	//if (sidPicture > 100)
	//	//{
	//	//	sidPicture = 1;
	//	//}
	//	int temp = sidPicture;
	//	int i = 0;
	//	//byte_PC0[0] = 0; byte_PC0[1] = 0; byte_PC0[2] = 0; byte_PC0[3] = 0; byte_PC0[4] = 0; byte_PC0[5] = 0; byte_PC0[6] = 0; byte_PC0[7] = 0;byte_PD0[0] = 0; byte_PD0[1] = 0;
	//	memset(byte_PC, 0, sizeof(byte_PC));
	//	memset(byte_PD, 0, sizeof(byte_PD));
	//	while (temp>0)
	//	{
	//		//十进制变二进制

	//		if (temp % 2==1)
	//		{
	//			byte_PC[i] = 1;
	//			i++;
	//		}
	//		else
	//		{
	//			byte_PC[i] = 0;
	//			i++;
	//		}
	//		temp=temp >> 1;
	//	}
	//	//byte_PD[0] = 1;//是否合格
	//	byte_PD[1] = 1;//中断信号  
	//	if (sidPicture % 20 == 0)
	//	{
	//		byte_PD[0] = 1;//不合格
	//	}
	//	else {
	//		byte_PD[0] = 0;//是否合格
	//	}
	//	if (!USB5841_SetDeviceDO_PC(artCard, byte_PC))
	//	{
	//		QMessageBox::warning(NULL, "", "写入失败", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//		return;
	//	}
	//	if (!USB5841_SetDeviceDO_PD(artCard, byte_PD))
	//	{
	//		QMessageBox::warning(NULL, "", "写入失败", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//		return;
	//	}
	//	QThread::msleep(1);//100us
	//	if (!USB5841_SetDeviceDO_PC(artCard, byte_PC0))
	//	{
	//		QMessageBox::warning(NULL, "", "写入失败", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//		return;
	//	}
	//	if (!USB5841_SetDeviceDO_PD(artCard, byte_PD0))
	//	{
	//		QMessageBox::warning(NULL, "", "写入失败", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//		return;
	//	}
	//	
	//	/*QString tempString(QString::fromLocal8Bit("图片编号:"));
	//	tempString.append(QString::number(sidPicture));
	//	mainDlg->ui.listWidget__information->clear();
	//	mainDlg->ui.listWidget__information->addItem(tempString);*/
	//	writeEnable = 0;//发一次
	//}
}