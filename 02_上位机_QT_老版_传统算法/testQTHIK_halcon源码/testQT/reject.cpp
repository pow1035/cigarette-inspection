#include "reject.h"
reject::reject(testQT* pdlg)
{
	this->pdlg = pdlg;
}
reject::~reject()
{

}
void reject::run()
{
	while (pdlg->systemRun)
	{
		if (!pdlg->NGNumberList.isEmpty())
		{
			if (pdlg->NGNumberList.size() == 1)//最后一张
			{
				Sleep(1);
			}
			//输出剔除信号
			ErrorCode        ret = Success;
			// Step 1: Create a instantDoCtrl for DO function.
			InstantDoCtrl* instantDoCtrl = InstantDoCtrl::Create();
			DeviceInformation devInfo(deviceDescription);
			ret = instantDoCtrl->setSelectedDevice(devInfo);
			if (ret != Success)
			{
				qDebug() << "IOCardSelectFail" << endl;
			}
			uint8  bufferForWriting[64] = { 0 };
			uint8  startWriteSingle[] = { 0x02 };//输出使能
			uint8  endWriteSingle[] = { 0x00 };//输出使能
			//uint8 reject[] = { 0x01 };//使能剔除
			//if (pdlg->rejectTest == true)
			//{
			//	startWriteSingle[0] = startWriteSingle[0] | reject[0];
			//}
			bufferForWriting[0] = pdlg->NGNumberList.dequeue();
			if (pdlg->rejectTest == false)//关闭剔除
			{
				continue;
			}
			ret = instantDoCtrl->Write(1, 1, startWriteSingle);
			ret = instantDoCtrl->Write(0, 1, bufferForWriting);
			Sleep(1);//延时1毫秒建立稳定电压
			ret = instantDoCtrl->Write(1, 1, endWriteSingle);
			if (ret != Success)
			{
				qDebug() << "IOCardWriteFail" << endl;
			}
			instantDoCtrl->Dispose();
			instantDoCtrl = NULL;
		}
		Sleep(0);
	}
	
}