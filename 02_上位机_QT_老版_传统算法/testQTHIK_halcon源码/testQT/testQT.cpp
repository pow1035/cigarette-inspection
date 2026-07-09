#include "testQT.h"
//#include "configForm.h"
//#include "search.h"
//#include <opencv2/opencv.hpp>
//#include <QPainter>
#include <QDebug>
#include <iostream>


//using namespace cv;
int photo_number = 0;
bool addQQueue1_1(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, testQT* pDlg)//图像需要是RGB
{
	if (pDlg->picQueList1_1.size() > 10)
	{
		pDlg->picQueList1_1.dequeue();
	}
	int picHeght = pFrameInfo->nHeight;
	int picWidth = pFrameInfo->nWidth;
	HObject ho_image;
	picStruct picstruct1;
	unsigned char* dataGray = new unsigned char[picWidth * picHeght];

	/*unsigned char* data = new unsigned char[picWidth * picHeght * 3];

	memcpy(data, in_pData, picWidth * picHeght * 3);*/

	for (int i = 0; i < picWidth * picHeght; i++)
	{
		dataGray[i] = (pData[3 * i]) * 0.299 + pData[3 * i + 1] * 0.587 + pData[3 * i + 2] * 0.114;

	}
	GenImage1(&ho_image, "byte", picWidth, picHeght, (Hlong)(dataGray));
	picstruct1.ho_Cam_Image = ho_image;
	picstruct1.mv_frame = pFrameInfo;
	picstruct1.uchar_pic_number = pDlg->nowPictureNumber;
	picstruct1.uchar_pic_IO = pDlg->nowShowPicReadIO;
	if (pDlg->picQueList1_1.size() != 0)
	{
		pDlg->picQueList1_1.enqueue(picstruct1);
	}
	else//为了避免最后一张未插完就处理的BUG
	{
		pDlg->mutexPicQueList1.lock();
		pDlg->picQueList1_1.enqueue(picstruct1);
		pDlg->mutexPicQueList1.unlock();
	}
	
	//内存释放
	//delete data;
	delete dataGray;
	return true;
}
bool addQQueue2_1(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, testQT* pDlg)//图像需要是RGB
{
	if (pDlg->picQueList2_1.size() > 10)
	{
		pDlg->picQueList2_1.dequeue();
	}
	int picHeght = pFrameInfo->nHeight;
	int picWidth = pFrameInfo->nWidth;
	HObject ho_image;
	picStruct picstruct1;
	unsigned char* dataGray = new unsigned char[picWidth * picHeght];

	/*unsigned char* data = new unsigned char[picWidth * picHeght * 3];

	memcpy(data, in_pData, picWidth * picHeght * 3);*/

	for (int i = 0; i < picWidth * picHeght; i++)
	{
		dataGray[i] = (pData[3 * i]) * 0.299 + pData[3 * i + 1] * 0.587 + pData[3 * i + 2] * 0.114;

	}
	GenImage1(&ho_image, "byte", picWidth, picHeght, (Hlong)(dataGray));
	picstruct1.ho_Cam_Image = ho_image;
	picstruct1.mv_frame = pFrameInfo;
	picstruct1.uchar_pic_number = pDlg->nowPictureNumber;
	picstruct1.uchar_pic_IO = pDlg->nowShowPicReadIO;
	//pDlg->picQueList2_1.enqueue(picstruct1);
	if (pDlg->picQueList2_1.size() != 0)
	{
		pDlg->picQueList2_1.enqueue(picstruct1);
	}
	else//为了避免最后一张未插完就处理的BUG
	{
		pDlg->mutexPicQueList2_1.lock();
		pDlg->picQueList2_1.enqueue(picstruct1);
		pDlg->mutexPicQueList2_1.unlock();
	}
	//内存释放
	//delete data;
	delete dataGray;
	return true;
}
bool addQQueue2_2(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, testQT* pDlg)//图像需要是RGB
{
	if (pDlg->picQueList2_2.size() > 10)
	{
		pDlg->picQueList2_2.dequeue();
	}
	int picHeght = pFrameInfo->nHeight;
	int picWidth = pFrameInfo->nWidth;
	HObject ho_image;
	picStruct picstruct1;
	unsigned char* dataGray = new unsigned char[picWidth * picHeght];

	/*unsigned char* data = new unsigned char[picWidth * picHeght * 3];

	memcpy(data, in_pData, picWidth * picHeght * 3);*/

	for (int i = 0; i < picWidth * picHeght; i++)
	{
		dataGray[i] = (pData[3 * i]) * 0.299 + pData[3 * i + 1] * 0.587 + pData[3 * i + 2] * 0.114;

	}
	GenImage1(&ho_image, "byte", picWidth, picHeght, (Hlong)(dataGray));
	picstruct1.ho_Cam_Image = ho_image;
	picstruct1.mv_frame = pFrameInfo;
	picstruct1.uchar_pic_number = pDlg->nowPictureNumber;
	picstruct1.uchar_pic_IO = pDlg->nowShowPicReadIO;
	pDlg->picQueList2_2.enqueue(picstruct1);
	//内存释放
	//delete data;
	delete dataGray;
	return true;
}
void __stdcall workProcedure1_1(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, void* pUser)
{

	testQT* pCam = (testQT*)pUser;//testQT类
	
	if (NULL == pCam)
	{
		return;
	}
	if (NULL == pData)
	{
		return;
	}
	addQQueue1_1(pData, pFrameInfo, pCam);

	//pCam->pool.start(new task(pData, pFrameInfo,pCam,0, pCam->nowPictureNumber));

	return;

	//MV_CC_GetOneFrame(pCam->m_stDevList[0])
	//int camera_number = 1;
	//模拟照片编号
	//photo_number = photo_number++;
	/*if (photo_number >= 1000)
	{
		photo_number = 1;
	}*/
	//读取编号
	//Mat src =Mat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3, (uchar*)pData);
	/*int temp = pFrameInfo->nHeight * pFrameInfo->nWidth;
	uchar ad[100];
	memset(ad, 0, sizeof(ad));
	memcpy(ad, pData, temp);*/
	/*Mat a;
	src.copyTo(a);
	namedWindow("input image", WINDOW_AUTOSIZE);
	imshow("input image", a);
	waitKey(0);*/
	//Mat originalReadRGBMat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3);//数据读入

	//originalReadRGBMat = Mat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3, (uchar*)pData);
	//Mat readRGBMatClone = originalReadRGBMat.clone();
	//int ii = 0;
}
void __stdcall workProcedure2_1(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, void* pUser)
{

	testQT* pCam = (testQT*)pUser;//testQT类
	if (NULL == pCam)
	{
		return;
	}
	if (NULL == pData)
	{
		return;
	}
	addQQueue2_1(pData, pFrameInfo, pCam);
	//pCam->pool.start(new readIOTask(pUser));

	return;
}
void __stdcall workProcedure2_2(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, void* pUser)
{

	testQT* pCam = (testQT*)pUser;//testQT类
	if (NULL == pCam)
	{
		return;
	}
	if (NULL == pData)
	{
		return;
	}
	addQQueue2_2(pData, pFrameInfo, pCam);
	//pCam->pool.start(new task(pData, pFrameInfo, pCam));

	return;
}
void __stdcall workProcedure4(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, void* pUser)
{

	testQT* pCam = (testQT*)pUser;//testQT类
	if (NULL == pCam)
	{
		return;
	}
	if (NULL == pData)
	{
		return;
	}
	//pCam->pool.start(new task(pData, pFrameInfo, pCam));

	return;
}

testQT::testQT(QWidget *parent)
	: QMainWindow(parent)
{
	
	ui.setupUi(this);
	
	//组件相机对应初始化
	QString cameraTempS;
	//1组件
	cameraTempS = "00J18974152";
	cameraMatchMap.insert("1-1", cameraTempS);
	//2组件1相机(内)
	cameraTempS = "00G12535816";
	cameraMatchMap.insert("2-1", cameraTempS);
	//2组件2相机(外)
	cameraTempS = "00L25932827";
	cameraMatchMap.insert("2-2", cameraTempS);

	if (initCamera())//相机初始化
	{
		QString tempString(QString::fromLocal8Bit("成功：相机初始化"));
		ui.listWidget__information->addItem(tempString);
	};
	if (initIOCard())//IO板卡初始化
	{
		QString tempString(QString::fromLocal8Bit("成功：IO板卡初始化"));
		ui.listWidget__information->addItem(tempString);
	}
	/*if (initMySQL())
	{
		QString tempString(QString::fromLocal8Bit("成功：数据库链接"));
		ui.listWidget__information->addItem(tempString);
	}*/
	
	updateConfigSlot();
	//initHalconShowWindow();//静态初始化Halcon显示用窗口
	//pool.setMaxThreadCount(16);//线程池数量
	config = new configForm();
	searchForm = new search();
	nowBrandName = QStringLiteral("钻石(硬红)");
	//connect(ui.listWidget_camera, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(doubleclicked(QListWidgetItem*)));//双击选择相机
	connect(ui.btnManual, SIGNAL(clicked()), this, SLOT(on_btnManualTriggle_clicked()));
	connect(config, SIGNAL(updateConfig()), this, SLOT(updateConfigSlot()));
	
	halconWindowHandleInit = false;
	//connect(this,SIGNAL(OnStartGrabbing()),this, SLOT(OnGrabStart(Pylon::CInstantCamera & camera)));
	//connect(this, SIGNAL(OnImageGrabbed()), this, SLOT(OnImageGrabbed(Pylon::CInstantCamera & camera, const Pylon::CGrabResultPtr & grabResult)));
}
testQT::~testQT()
{
	systemRun = false;
}

void testQT::on_btnManualTriggle_clicked()
{
	SetHcppInterfaceStringEncodingIsUtf8(false);
	Hlong winId1 = (Hlong)ui.picture1->winId();
	int labHeight1 = this->ui.picture1->height();// (Hlong)ui.picture1->height();
	
	int labWidth1 = this->ui.picture1->width();//(Hlong)ui.picture1->width();
	OpenWindow(0, 0, (Hlong)labWidth1, (Hlong)labHeight1, winId1, "visible", "", &hv_WindowHandle1);
	Hlong winId1NG = (Hlong)ui.picture1_NG->winId();
	int labHeight1_NG = this->ui.picture1_NG->height();// (Hlong)ui.picture1->height();
	int labWidth1_NG = this->ui.picture1_NG->width();//(Hlong)ui.picture1->width();
	
	OpenWindow(0, 0, (Hlong)labWidth1_NG, (Hlong)labHeight1_NG, winId1NG, "visible", "", &hv_WindowHandle1NG);
	halconWindowHandleInit = true;//已初始化

	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(timerSlot()));
	timer->setInterval(1000);
	timer->start();
	systemRun = true;//开启运行
	pool.start(new process(this, 0, nowPictureNumber));//处理线程
	pool.start(new process2_1(this, 0, nowPictureNumber));//处理线程
	pool.start(new reject(this));//剔除线程
	pool.start(new saveImage(this));//保存图像线程
	rejectTest = true;//开剔除
	return;


	QString QSFileName = QStringLiteral("D:/test1/搭口/1_1_20221011105151504_4.jpg");

	//pool.start(new task(QSFileName,this));
	//ui.picture1->setScaledContents(true);
	return;
	
	HObject ho_Image;
	HTuple hv_Width, hv_Height;
	ReadImage(&ho_Image, HTuple("D:/test_pic/0_0_0726080843409.jpg"));
	//RotateImage(ho_Image, &ho_rotateImage, 90, "constant");
	GetImageSize(ho_Image, &hv_Width, &hv_Height);
	//Hlong winId = (Hlong)ui.picture1->winId();
	//int labHeight = (Hlong)ui.picture1->height();
	//int labWidth = (Hlong)ui.picture1->width();
	//OpenWindow(0, 0, (Hlong)labWidth, (Hlong)labHeight, winId, "visible", "", &hv_WindowHandle);
	SetColor(hv_WindowHandle1, "red");
	SetDraw(hv_WindowHandle1, "margin");
	SetLineWidth(hv_WindowHandle1, 3);
	SetPart(hv_WindowHandle1, 0, 0, hv_Height, hv_Width);
	DispObj(ho_Image, hv_WindowHandle1);

	//int nRet = MV_OK;
	//int i = 0;

	//for (i = 0; i < m_stDevList.nDeviceNum; i++)
	//{
	//	nRet = m_pcMyCamera[i]->CommandExecute("TriggerSoftware");//单次触发
	//}

	return ;
}
void testQT::on_btnReject_clicked()
{
	QString tempString;
	if (rejectTest)
	{
		rejectTest = false;
		tempString = QString::fromLocal8Bit("剔除功能：关闭");
	}
	else
	{
		rejectTest = true;
		tempString = QString::fromLocal8Bit("剔除功能：打开");
	}
	ui.listWidget__information->addItem(tempString);

}
void testQT::on_btnShowCheckRegions_clicked()
{
	QString tempString;
	if (showCheckRegion)
	{
		showCheckRegion = false;
		tempString = QString::fromLocal8Bit("检测设置实时观察功能：关闭");
	}
	else
	{
		showCheckRegion = true;
		tempString = QString::fromLocal8Bit("检测设置实时观察功能：打开");
	}
	ui.listWidget__information->addItem(tempString);

}
void testQT::on_btnSearch_clicked() {
	searchForm->show();
}
void testQT::on_btnConfig_clicked()
{
	config->show();
}
void testQT::on_btnInitControlCard_clicked() {

	
	//serial = new QSerialPort;
	controlCardSet.resize(20);
	controlCardSet[0] = 0x15; //检测烟支距离剔除口步数
	controlCardSet[1] = 0x02;//给上位机发送编号的MCP
	controlCardSet[2] = 0x02;//组件2触发MCP
	controlCardSet[3] = 0x03;//组件1触发MCP
	controlCardSet[4] = 0x00;//常剔除使能
	//controlCardSet[5] = 0x01;//相机光源触发延时
	controlCardSet[5] = 0x08;//相机光源触发延时
	controlCardSet[6] = 0x00; //开始剔除MCP
	controlCardSet[7] = 0x09; //关闭剔除MCP
	controlCardSet[8] = 0x00;
	controlCardSet[9] = 0x00;
	controlCardSet[10] = 0x00;
	controlCardSet[11] = 0x00;
	controlCardSet[12] = 0x00;
	controlCardSet[13] = 0x00;
	controlCardSet[14] = 0x00;
	controlCardSet[15] = 0x00;
	controlCardSet[16] = 0x00;
	controlCardSet[17] = 0x00;
	controlCardSet[18] = 0x00;
	controlCardSet[19] = 0x00;
	
	//foreach (const QSerialPortInfo &info,QSerialPortInfo::availablePorts())
	//{
	//	serial->setPort(info);//设置串口
	//	if (serial->open(QIODevice::ReadWrite))
	//	{
	//		serial->close();
	//		serial->setBaudRate(QSerialPort::Baud4800);//波特率4800
	//		serial->setParity(QSerialPort::NoParity);//校验 无
	//		serial->setDataBits(QSerialPort::Data8);//数据位8
	//		serial->setStopBits(QSerialPort::OneStop);//停止位 1
	//		serial->setFlowControl(QSerialPort::NoFlowControl);//控制流 无
	//		serial->write(controlCardSet);
	//		break;
	//	}
	//	else
	//	{
	//		QString tempString(QString::fromLocal8Bit("错误：串口打开失败！"));
	//		ui.listWidget__information->addItem(tempString);
	//	}
	//}

	serial = new QSerialPort("COM1");
	
	serial->close();
	//serial->setBaudRate(QSerialPort::Baud1200);//波特率4800
	serial->setBaudRate(QSerialPort::Baud9600);//波特率4800
	serial->setParity(QSerialPort::NoParity);//校验 无
	serial->setDataBits(QSerialPort::Data8);//数据位8
	serial->setStopBits(QSerialPort::OneStop);//停止位 1
	serial->setFlowControl(QSerialPort::NoFlowControl);//控制流 无
	if (!serial->open(QIODevice::ReadWrite))
	{
		return;
	}
	serial->write(controlCardSet);
	
	QString tempString = QString::fromLocal8Bit("参数初始化");
	ui.listWidget__information->addItem(tempString);
	return;


	SetHcppInterfaceStringEncodingIsUtf8(false);
	if (!halconWindowHandleInit)
	{
		Hlong winId = (Hlong)ui.picture1->winId();
		int labHeight = this->ui.picture1->height();// (Hlong)ui.picture1->height();
		int labWidth = this->ui.picture1->width();//(Hlong)ui.picture1->width();
		OpenWindow(0, 0, (Hlong)labWidth, (Hlong)labHeight, winId, "visible", "", &hv_WindowHandle1);
		halconWindowHandleInit = true;//已初始化
	}
	
	QFileDialog* fd = new QFileDialog(this);
	QString filename = QFileDialog::getOpenFileName(this, QStringLiteral("选择处理的图片"), "d:", "");
	//pool.start(new task(filename, this));
	return;

}
void testQT::closeEvent(QCloseEvent* event)
{
	// QMessageBox::StandardButton button;
	int button;
	
	button = QMessageBox::question(this, QString::fromLocal8Bit("退出程序"),
		QString::fromLocal8Bit("确认退出程序?"),
		QMessageBox::Yes | QMessageBox::No);
	if (button == QMessageBox::No) {
		event->ignore();  //忽略退出信号，程序继续运行
	}
	else if (button == QMessageBox::Yes) {

		int nRet = MV_OK;

		for (int i = 0; i < m_stDevList.nDeviceNum; i++)
		{
			nRet = m_pcMyCamera[i]->Close();
		}
	
		event->accept();  //接受退出信号，程序退出
	}
}
bool testQT::initCamera() {
	
	int nRet = -1;
	void* m_handle = NULL;
	//int nCanOpenDeviceNum = 0;
	memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
	//枚举子网内指定的传输协议对应的所有设备
	nRet = MyCamera::EnumDevices(&m_stDevList);
	if (MV_OK != nRet)
	{
		QString tempString(QString::fromLocal8Bit("错误：枚举相机失败"));
		ui.listWidget__information->addItem(tempString);
		return false;
	}
	//
	for (unsigned int i = 0, j = 0; j < m_stDevList.nDeviceNum; j++, i++)
	{

		unsigned char deviceGUID[INFO_MAX_BUFFER_SIZE] = {0};
		m_pcMyCamera[i] = new MyCamera;
		m_pcMyCamera[i]->m_pBufForDriver = NULL;
		m_pcMyCamera[i]->m_pBufForSaveImage = NULL;
		m_pcMyCamera[i]->m_nBufSizeForDriver = 0;
		m_pcMyCamera[i]->m_nBufSizeForSaveImage = 0;
		m_pcMyCamera[i]->m_nTLayerType = m_stDevList.pDeviceInfo[j]->nTLayerType;

		nRet = m_pcMyCamera[i]->Open(m_stDevList.pDeviceInfo[j]);
		if (MV_OK != nRet)
		{
			delete(m_pcMyCamera[i]);
			m_pcMyCamera[i] = NULL;
			i--;
			continue;
		}
		else
		{
			memcpy(deviceGUID, m_stDevList.pDeviceInfo[j]->SpecialInfo.stUsb3VInfo.chSerialNumber, INFO_MAX_BUFFER_SIZE);
			QString deviceGUIDString = QString::fromLocal8Bit((char*)deviceGUID);
		
			QMap<QString, QString>::const_iterator it = cameraMatchMap.constBegin();
			while (it != cameraMatchMap.constEnd())//配置map遍历，找到对应相机的设置
			{
				QString cameraKey;//当前设置相机的Key
				if (!QString::compare(it.value(), deviceGUIDString))//相机ID匹配到设置中ID
				{
					cameraKey = it.key();//设置Key
					if (cameraKey == "1-1")//1组件相机1
					{
						nRet = m_pcMyCamera[i]->SetEnumValue("TriggerMode", MV_TRIGGER_MODE_ON);//上升沿
						nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_LINE0);//触发源Line0

						nRet = m_pcMyCamera[i]->SetEnumValue("TriggerActivation", 0);//激活方式0:RisingEdge 1:FallingEdge2.LevelHigh3.LevelLow
						nRet = m_pcMyCamera[i]->SetFloatValue("ExposureTime", 50.0);//曝光时间us
						nRet = m_pcMyCamera[i]->SetIntValue("Width", 992);//画面宽度
						nRet = m_pcMyCamera[i]->SetIntValue("Height", 300);//画面高度
						nRet = m_pcMyCamera[i]->SetIntValue("OffsetX", 176);//画面水平偏移
						nRet = m_pcMyCamera[i]->SetIntValue("OffsetY", 332);//画面垂直偏移，运动时400，手动盘340
						nRet = m_pcMyCamera[i]->SetFloatValue("ExposureTime", 50);//曝光时间设置
						nRet = m_pcMyCamera[i]->SetFloatValue("Gain", 0.0);//增益值设置
						//nRet = m_pcMyCamera[i]->CommandExecute("TriggerSoftware");//单次软触发
						//nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", SOFTWAREMODE);//软件触发
						nRet = m_pcMyCamera[i]->SetEnumValue("PixelFormat", PixelType_Gvsp_RGB8_Packed);//0x02180014 RGB8Packed

						nRet = m_pcMyCamera[i]->SetFloatValue("AcquisitionFrameRate", 200);//相机帧率
						//unsigned int enValue = PixelType_Gvsp_RGB8_Packed;
						//MV_CC_SetPixelFormat(m_pcMyCamera[i]->m_hDevHandle, enValue);//设置图片格式RGB
						nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure1_1, this);
					}
					if (cameraKey == "2-1")//2组件相机1
					{
						nRet = m_pcMyCamera[i]->SetEnumValue("TriggerMode", MV_TRIGGER_MODE_ON);//上升沿
						nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_LINE0);//触发源Line0

						nRet = m_pcMyCamera[i]->SetEnumValue("TriggerActivation", 0);//激活方式0:RisingEdge 1:FallingEdge2.LevelHigh3.LevelLow
						nRet = m_pcMyCamera[i]->SetFloatValue("ExposureTime", 100.0);//曝光时间us
						nRet = m_pcMyCamera[i]->SetIntValue("Width", 1200);//画面宽度
						nRet = m_pcMyCamera[i]->SetIntValue("Height", 600);//画面高度
						nRet = m_pcMyCamera[i]->SetIntValue("OffsetX", 96);//画面水平偏移
						nRet = m_pcMyCamera[i]->SetIntValue("OffsetY", 160);//画面垂直偏移，运动时400，手动盘340
						nRet = m_pcMyCamera[i]->SetFloatValue("Gain", 0.0);//增益值设置
						//nRet = m_pcMyCamera[i]->CommandExecute("TriggerSoftware");//单次软触发
						//nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", SOFTWAREMODE);//软件触发
						nRet = m_pcMyCamera[i]->SetEnumValue("PixelFormat", PixelType_Gvsp_RGB8_Packed);//0x02180014 RGB8Packed

						nRet = m_pcMyCamera[i]->SetFloatValue("AcquisitionFrameRate", 200);//相机帧率
						//unsigned int enValue = PixelType_Gvsp_RGB8_Packed;
						//MV_CC_SetPixelFormat(m_pcMyCamera[i]->m_hDevHandle, enValue);//设置图片格式RGB
						nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure2_1, this);
					}
					if (cameraKey == "2-2")//2组件相机1
					{
						nRet = m_pcMyCamera[i]->SetEnumValue("TriggerMode", MV_TRIGGER_MODE_ON);//上升沿
						nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_LINE0);//触发源Line0

						nRet = m_pcMyCamera[i]->SetEnumValue("TriggerActivation", 0);//激活方式0:RisingEdge 1:FallingEdge2.LevelHigh3.LevelLow
						nRet = m_pcMyCamera[i]->SetFloatValue("ExposureTime", 100);//曝光时间us
						nRet = m_pcMyCamera[i]->SetIntValue("Width", 1200);//画面宽度
						nRet = m_pcMyCamera[i]->SetIntValue("Height", 600);//画面高度
						nRet = m_pcMyCamera[i]->SetIntValue("OffsetX", 96);//画面水平偏移
						nRet = m_pcMyCamera[i]->SetIntValue("OffsetY", 168);//画面垂直偏移
						nRet = m_pcMyCamera[i]->SetFloatValue("Gain", 0.0);//增益值设置
						//nRet = m_pcMyCamera[i]->CommandExecute("TriggerSoftware");//单次软触发
						//nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", SOFTWAREMODE);//软件触发
						nRet = m_pcMyCamera[i]->SetEnumValue("PixelFormat", PixelType_Gvsp_RGB8_Packed);//0x02180014 RGB8Packed

						nRet = m_pcMyCamera[i]->SetFloatValue("AcquisitionFrameRate", 200);//相机帧率
						//unsigned int enValue = PixelType_Gvsp_RGB8_Packed;
						//MV_CC_SetPixelFormat(m_pcMyCamera[i]->m_hDevHandle, enValue);//设置图片格式RGB
						nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure2_2, this);
						
					}
					QString tempString(QString::fromLocal8Bit("找到相机"));
					tempString.append(cameraKey);
					tempString.append(QString::fromLocal8Bit("编号："));
					tempString.append(deviceGUIDString);
					ui.listWidget__information->addItem(tempString);
					if (MV_OK != nRet)
					{
						QString tempString(QString::fromLocal8Bit("相机"));
						tempString.append(cameraKey);
						tempString.append(QString::fromLocal8Bit(" 回调函数设置失败"));
						ui.listWidget__information->addItem(tempString);
						return false;
					}
					
					nRet = m_pcMyCamera[i]->StartGrabbing();
					if (MV_OK != nRet)
					{
						QString tempString(QString::fromLocal8Bit("错误：第 "));
						tempString.append(QString::number(i + 1));
						tempString.append(QString::fromLocal8Bit(" 个相机启动抓拍失败"));

						ui.listWidget__information->addItem(tempString);
						return false;
					}
					//获取帧数据大小
					//MVCC_INTVALUE stIntvalue = { 0 };
					//int nRet = MV_CC_GetIntValue(m_pcMyCamera[i]->m_hDevHandle, "PayloadSize", &stIntvalue);
					//if (nRet != MV_OK)
					//{
					//	printf("Get PayloadSize failed! nRet [%x]\n", nRet);

					//}
					//picDataValue[i] = stIntvalue.nCurValue; //一帧数据大小
					
				}
				it++;
			}

			/*QString tempString(QString::fromLocal8Bit("初始化第"));
			tempString.append(QString::number(i + 1));
			tempString.append(QString::fromLocal8Bit("个相机，全名为："));
			tempString.append(QString::number((int)m_stDevList.pDeviceInfo[j]->SpecialInfo.stUsb3VInfo.chSerialNumber));
			ui.listWidget__information->addItem(tempString);
			nCanOpenDeviceNum++;*/
		}
	

		/*
		0x01080001:Mono8
		0x01100003:Mono10
		0x010C0004:Mono10Packed
		0x01100005:Mono12
		0x010C0006:Mono12Packed
		0x01100007:Mono16
		0x02180014:RGB8Packed
		0x02100032:YUV422_8
		0x0210001F:YUV422_8_UYVY
		0x02180020:YUV8_UYV
		0x020C001E:YUV411_8_UYYVYY
		0x01080008:BayerGR8
		0x01080009:BayerRG8
		0x0108000A:BayerGB8
		0x0108000B:BayerBG8
		0x0110000c:BayerGR10
		0x0110000d:BayerRG10
		0x0110000e:BayerGB10
		0x0110000f:BayerBG10
		0x010C0029:BayerBG10Packed
		0x010C0026:BayerGR10Packed
		0x010C0027:BayerRG10Packed
		0x010C0028:BayerGB10Packed
		0x01100010:BayerGR12
		0x01100011:BayerRG12
		0x01100012:BayerGB12
		0x01100013:BayerBG12
		0x010C002D:BayerBG12Packed
		0x010C002A:BayerGR12Packed
		0x010C002B:BayerRG12Packed
		0x010C002C:BayerGB12Packed
		0x0110002E:BayerGR16
		0x0110002F:BayerRG16
		0x01100030:BayerGB16
		0x01100031:BayerBG16
		*/
	}

	//相机对应返回函数
	//for (int i = 0; i < m_stDevList.nDeviceNum; i++)
	//{
	//	if (i == 0) {
	//		nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure1, this);
	//		//MV_CC_CreateHandle(cameraHandle[0], m_stDevList.pDeviceInfo[0]);
	//	}
	//	else if (i == 1) {
	//		nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure2, this);
	//		//MV_CC_CreateHandle(cameraHandle[1], m_stDevList.pDeviceInfo[1]);
	//	}
	//	else if (i == 2) {
	//		nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure3, this);
	//		//MV_CC_CreateHandle(cameraHandle[2], m_stDevList.pDeviceInfo[2]);
	//	}
	//	//else if (i == 3) {
	//	//	nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure4, this);
	//	//	//MV_CC_CreateHandle(cameraHandle[3], m_stDevList.pDeviceInfo[3]);
	//	//}

	//	if (MV_OK != nRet)
	//	{
	//		QString tempString(QString::fromLocal8Bit("相机"));
	//		tempString.append(cameraKey);
	//		tempString.append(QString::fromLocal8Bit(" 个相机回调函数设置失败"));

	//		ui.listWidget__information->addItem(tempString);
	//		return false;
	//	}
	//	else
	//	{
	//		QString tempString(QString::fromLocal8Bit("第 "));
	//		tempString.append(QString::number(i + 1));
	//		tempString.append(QString::fromLocal8Bit(" 个相机回调函数设置成功"));

	//		ui.listWidget__information->addItem(tempString);
	//		ui.btnManual->setEnabled(true);
	//		ui.btnReject->setEnabled(true);
	//		cameraTriggerAuto = false;
	//	}

	//	nRet = m_pcMyCamera[i]->StartGrabbing();
	//	if (MV_OK != nRet)
	//	{
	//		QString tempString(QString::fromLocal8Bit("错误：第 "));
	//		tempString.append(QString::number(i + 1));
	//		tempString.append(QString::fromLocal8Bit(" 个相机启动抓拍失败"));

	//		ui.listWidget__information->addItem(tempString);
	//		return false;
	//	}
	//	QString tempString(QString::fromLocal8Bit("第 "));
	//	tempString.append(QString::number(i + 1));
	//	tempString.append(QString::fromLocal8Bit(" 个相机启动"));

	//	ui.listWidget__information->addItem(tempString);
	//	//获取帧数据大小
	//	MVCC_INTVALUE stIntvalue = { 0 };
	//	int nRet = MV_CC_GetIntValue(m_pcMyCamera[i]->m_hDevHandle, "PayloadSize", &stIntvalue);
	//	if (nRet != MV_OK)
	//	{
	//		printf("Get PayloadSize failed! nRet [%x]\n", nRet);
	//		
	//	}
	//	picDataValue[i] = stIntvalue.nCurValue; //一帧数据大小
	//}


	
	return TRUE;
}
bool testQT::initIOCard()
{

	pool.start(new readIOTask(this));//开始读IO线程
	//pool.start(new testWrite(this));
	//artCard = USB5841_CreateDevice(0);//创建IO板卡句柄
	//if (artCard == INVALID_HANDLE_VALUE)
	//{
	//	QMessageBox::warning(NULL, "", "初始化失败!", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//	return 0;
	//}
	//
	//if (USB5841_EnableStsDIO(artCard, TRUE, 0) != TRUE)//设置DI0-7为输入
	//{
	//	QMessageBox::warning(NULL, "", "DI0-7设置输入失败!", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//	return 0;
	//}
	//if (USB5841_EnableStsDIO(artCard, TRUE, 1) != TRUE)//设置DI15-8为输入
	//{
	//	QMessageBox::warning(NULL, "", "DI15-8设置输入失败!", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//	return 0;
	//}
	//if (USB5841_EnableStsDIO(artCard, FALSE, 2) != TRUE)//设置DO16-23为输出
	//{
	//	QMessageBox::warning(NULL, "", "DO16-23设置输入失败!", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//	return 0;
	//}
	//if (USB5841_EnableStsDIO(artCard, FALSE, 3) != TRUE)//设置DO24-31为输出
	//{
	//	QMessageBox::warning(NULL, "", "DO24-31设置输入失败!", QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	//	return 0;
	//}
	//pool.start(new readIOTask(this));//开始读IO线程
	//pool.start(new testWrite(this));
	return 1;
}
//bool testQT::initMySQL()
//{
//	if (QSqlDatabase::contains("qt_sql_default_connection"))
//		db1 = QSqlDatabase::database("qt_sql_default_connection");
//	else
//		db1 = QSqlDatabase::addDatabase("QODBC");
//	db1.setHostName("127.0.0.1");
//	db1.setPort(3306);
//	db1.setDatabaseName("mysql");
//	db1.setUserName("root");
//	db1.setPassword("root");
//	query1=new QSqlQuery(db1);
//	bool ok = db1.open();
//	if (!ok )
//	{
//		QString tempString(QString::fromLocal8Bit("错误：数据库链接失败"));
//		ui.listWidget__information->addItem(tempString);
//		return false;
//	}
//	/*QString connection;
//	connection = db1.connectionName();
//	db1.close();
//	db1 = QSqlDatabase();
//	db1.removeDatabase(connection);*/
//	/*QString sql = "insert into ngcigmaster (picFileName,picAddriess) values ('1','2') ";
//	QSqlQuery que(db);
//	bool result= que.prepare(sql);
//	bool result1 = que.exec();*/
//	return ok;
//}
//void testQT::doubleclicked(QListWidgetItem* item) {
//	//QMessageBox::information(NULL, "Camera", item->text());
//	
//	int nRet = MV_OK;
//	int i = 0;
//
//	for (i = 0; i < m_stDevList.nDeviceNum; i++)
//	{
//		if (i == 0) {
//			nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure1, this);
//			//MV_CC_CreateHandle(cameraHandle[0], m_stDevList.pDeviceInfo[0]);
//		}
//		else if (i == 1) {
//			nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure2, this);
//			//MV_CC_CreateHandle(cameraHandle[1], m_stDevList.pDeviceInfo[1]);
//		}
//		else if (i == 2) {
//			nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure3, this);
//			//MV_CC_CreateHandle(cameraHandle[2], m_stDevList.pDeviceInfo[2]);
//		}
//		else if (i == 3) {
//			nRet = m_pcMyCamera[i]->RegisterImageCallBack(workProcedure4, this);
//			//MV_CC_CreateHandle(cameraHandle[3], m_stDevList.pDeviceInfo[3]);
//		}
//		
//		if (MV_OK != nRet)
//		{
//			QString tempString(QString::fromLocal8Bit("错误：第 "));
//			tempString.append(QString::number(i + 1));
//			tempString.append(QString::fromLocal8Bit(" 个相机回调函数设置失败"));
//
//			ui.listWidget__information->addItem(tempString);
//			return;
//		}
//		else
//		{
//			QString tempString(QString::fromLocal8Bit("第 "));
//			tempString.append(QString::number(i + 1));
//			tempString.append(QString::fromLocal8Bit(" 个相机回调函数设置成功"));
//
//			ui.listWidget__information->addItem(tempString);
//			ui.btnManual->setEnabled(true);
//			ui.btnReject->setEnabled(true);
//			cameraTriggerAuto = false;
//		}
//
//		nRet = m_pcMyCamera[i]->StartGrabbing();
//		if (MV_OK != nRet)
//		{
//			QString tempString(QString::fromLocal8Bit("错误：第 "));
//			tempString.append(QString::number(i + 1));
//			tempString.append(QString::fromLocal8Bit(" 个相机启动抓拍失败"));
//
//			ui.listWidget__information->addItem(tempString);
//			return;
//		}
//		QString tempString(QString::fromLocal8Bit("第 "));
//		tempString.append(QString::number(i + 1));
//		tempString.append(QString::fromLocal8Bit(" 个相机启动"));
//
//		ui.listWidget__information->addItem(tempString);
//
//		// ch:开始采集之后才创建workthread线程 | en:Create workthread after start grabbing
//		
//		
//	}
//
//
//	return;
//}
void testQT::initHalconShowWindow() {
	Hlong winId = (Hlong)ui.picture1->winId();
	int labHeight = this->ui.picture1->height();// (Hlong)ui.picture1->height();
	int labWidth = this->ui.picture1->width();//(Hlong)ui.picture1->width();
	OpenWindow(0, 0, (Hlong)labWidth, (Hlong)labHeight, winId, "visible", "", &hv_WindowHandle1);
	halconWindowHandleInit = true;//已初始化
}

LONGLONG testQT::getUseTimeStart()
{
	LARGE_INTEGER litmp;
	QueryPerformanceCounter(&litmp);//取得高精度运行计数器的数值  
	return litmp.QuadPart; //开始时刻
}
LONGLONG testQT::getUseTime(LONGLONG startTime)
{
	LARGE_INTEGER litmp;
	LONGLONG endTime,usetime;
	double dfFreq,dfTime;
	//获得CPU计时器的时钟频率  
	QueryPerformanceFrequency(&litmp);//取得高精度运行计数器的频率f,单位是每秒多少次（n/s），  
	dfFreq = (double)litmp.QuadPart;

	QueryPerformanceCounter(&litmp);//取得高精度运行计数器的数值  
	endTime = litmp.QuadPart; //终止计时 
	dfTime = (double)(endTime - startTime);//计算计数器值  
	dfTime = dfTime / dfFreq;//获得对应时间，单位为秒,可以乘1000000精确到微秒级（us）  
	usetime = dfTime * 1000000;
	return usetime;
}
void testQT::updateConfigSlot()
{
	configIniWrite = new QSettings("config.ini", QSettings::IniFormat);//配置文件
	//设置编码，使支持中文
	configIniWrite->setIniCodec("UTF-8");
	pic1RectX = configIniWrite->value("rect1/x").toInt();
	pic1RectY = configIniWrite->value("rect1/y").toInt();
	pic1RectWidth = configIniWrite->value("rect1/width").toInt();
	pic1RectHeight = configIniWrite->value("rect1/height").toInt();

	pic1DividingX = configIniWrite->value("rect1Dividing/x").toInt();
	pic1DividingY = configIniWrite->value("rect1Dividing/y").toInt();
	pic1DividingHeight = configIniWrite->value("rect1Dividing/height").toInt();
	pic1DividingWidth = configIniWrite->value("rect1Dividing/width").toInt();
	pic1DividingThreshold= configIniWrite->value("rect1Dividing/threshold").toInt();
	if (configIniWrite->value("rect1Dividing/verticalRelative").toInt() == 1)
	{
		filterCigDividingVerticalRelative = 1;
	}
	else
	{
		filterCigDividingVerticalRelative = 0;
	}

	filterDividingRectX = configIniWrite->value("filterDividingRect/x").toInt();
	filterDividingRectY = configIniWrite->value("filterDividingRect/y").toInt();
	filterDividingRectHeight = configIniWrite->value("filterDividingRect/height").toInt();
	filterDividingRectWidth = configIniWrite->value("filterDividingRect/width").toInt();
	filterDividingRectThreshold = configIniWrite->value("filterDividingRect/threshold").toInt();
	if (configIniWrite->value("filterDividingRect/verticalRelative").toInt() == 1)
	{
		filterVerticalRelative = 1;
	}
	else
	{
		filterVerticalRelative = 0;
	}

	cigDividingRectX = configIniWrite->value("cigDividingRect/x").toInt();
	cigDividingRectY = configIniWrite->value("cigDividingRect/y").toInt();
	cigDividingRectHeight = configIniWrite->value("cigDividingRect/height").toInt();
	cigDividingRectWidth = configIniWrite->value("cigDividingRect/width").toInt();
	cigDividingRectThreshold = configIniWrite->value("cigDividingRect/threshold").toInt();
	if (configIniWrite->value("cigDividingRect/verticalRelative").toInt() == 1)
	{
		cigVerticalRelative = 1;
	}
	else
	{
		cigVerticalRelative = 0;
	}

	cigUpDividingRectX = configIniWrite->value("cigUpDividingRect/x").toInt();
	cigUpDividingRectY = configIniWrite->value("cigUpDividingRect/y").toInt();
	cigUpDividingRectHeight = configIniWrite->value("cigUpDividingRect/height").toInt();
	cigUpDividingRectWidth = configIniWrite->value("cigUpDividingRect/width").toInt();
	cigUpDividingRectThreshold = configIniWrite->value("cigUpDividingRect/threshold").toInt();

	cigDownDividingRectX = configIniWrite->value("cigDownDividingRect/x").toInt();
	cigDownDividingRectY = configIniWrite->value("cigDownDividingRect/y").toInt();
	cigDownDividingRectHeight = configIniWrite->value("cigDownDividingRect/height").toInt();
	cigDownDividingRectWidth = configIniWrite->value("cigDownDividingRect/width").toInt();
	cigDownDividingRectThreshold = configIniWrite->value("cigDownDividingRect/threshold").toInt();

	filterWidthRectX = configIniWrite->value("filterWidthRect/x").toInt();
	filterWidthRectY = configIniWrite->value("filterWidthRect/y").toInt();
	filterWidthRectHeight = configIniWrite->value("filterWidthRect/height").toInt();
	filterWidthRectWidth = configIniWrite->value("filterWidthRect/width").toInt();
	filterWidthRectThreshold= configIniWrite->value("filterWidthRect/threshold").toInt();

	cigNearFilterWidthRectX = configIniWrite->value("cigNearFilterWidthRect/x").toInt();
	cigNearFilterWidthRectY = configIniWrite->value("cigNearFilterWidthRect/y").toInt();
	cigNearFilterWidthRectHeight = configIniWrite->value("cigNearFilterWidthRect/height").toInt();
	cigNearFilterWidthRectWidth = configIniWrite->value("cigNearFilterWidthRect/width").toInt();

	cigTopWidthRectX = configIniWrite->value("cigTopWidthRect/x").toInt();
	cigTopWidthRectY = configIniWrite->value("cigTopWidthRect/y").toInt();
	cigTopWidthRectHeight = configIniWrite->value("cigTopWidthRect/height").toInt();
	cigTopWidthRectWidth = configIniWrite->value("cigTopWidthRect/width").toInt();

	line_rho = configIniWrite->value("HoughLinesP/rho").toFloat();
	line_theta = configIniWrite->value("HoughLinesP/theta").toFloat();
	line_minLineLength = configIniWrite->value("HoughLinesP/minLineLength").toFloat();
	line_maxLineGap = configIniWrite->value("HoughLinesP/maxLineGap").toFloat();
	line_threshold = configIniWrite->value("HoughLinesP/threshold").toInt();

	approxPolyDPSet = configIniWrite->value("approxPolyDP/approxPolyDPSet").toDouble();

	minDisOfTwoPoints= configIniWrite->value("dingwei/minDisOfTwoPoints").toInt();

	cigBrokenX = configIniWrite->value("cigBroken/x").toInt();
	cigBrokenY = configIniWrite->value("cigBroken/y").toInt();
	cigBrokenRectHeight = configIniWrite->value("cigBroken/height").toInt();
	cigBrokenRectWidth = configIniWrite->value("cigBroken/width").toInt();
	cigBrokenRectThreshold = configIniWrite->value("cigBroken/threshold").toInt();
	if (configIniWrite->value("cigBroken/relative").toInt() == 1)
	{
		cigBrokenRelative = 1;
	}
	else
	{
		cigBrokenRelative = 0;
	}
}
void testQT::timerSlot() {
	ui.textEdit_check->setText(QString::number(checkCigNumber));
	ui.textEdit_NG->setText(QString::number(ngCigNumber));
	if (checkCigNumber!=0)//烟支不为0，算合格率
	{
		float rate = checkCigNumber * 100.0 / (float)(checkCigNumber + ngCigNumber);
		rate = ((float)((int)((rate + 0.005) * 100))) / 100;
		ui.textEdit_rate->setText(QString::number(rate));
	}
	
	ui.textEdit_zu1Pic->setText(QString::number(picQueList1_1.size()));
	//ui.textEdit_zu1Reject->setText(QString::number(NGNumberList.size()));
	ui.textEdit_zu1NGType1->setText(QString::number(rejectType[1]));
	ui.textEdit_zu1NGType2->setText(QString::number(rejectType[2]));
	ui.textEdit_zu1NGType3->setText(QString::number(rejectType[3]));
	ui.textEdit_zu1NGType4->setText(QString::number(rejectType[4]));
	ui.textEdit_Brand->setText(nowBrandName);
	//ui.textEdit_rate->setText(ui.textEdit_NG->*100.0/);
	// 
	//ui.textEdit_show->setText(QString::number(picQueList.size()));
	
	if (showPic1.IsInitialized())
	{
		HTuple hv_width, hv_height;
		GetImageSize(showPic1, &hv_width, &hv_height);
		SetPart(hv_WindowHandle1, 0, 0, hv_height, hv_width);
		
		DispObj(showPic1, hv_WindowHandle1);
	}
	if (showNGPic1.IsInitialized())
	{

		HTuple hv_width, hv_height;
		
		GetImageSize(showNGPic1, &hv_width, &hv_height);
		if (hv_width.I())
		{
			SetPart(hv_WindowHandle1NG, 0, 0, hv_height, hv_width);
			ClearWindow(hv_WindowHandle1NG);
			DispObj(showNGPic1, hv_WindowHandle1NG);
		}
		
	}
	
}
//BOOL InitUSB5841()
//bool testQT::addMasterMySQL(QString timestamp, QString dir, QString name, QString checkTime, QString banciID) {
//	//用主线程的定义可能出现竞争关系
//
//	////QSqlQuery que(pDlg->db);
//	//bool result = pDlg->query->prepare(sql);
//	//bool result1 = pDlg->query->exec();
//	//事务
//
//	dir = dir.replace("\\", "\\\\");
//	QString sql1 = QString("insert into ngcigmaster (ngCigTimeStamp,picFileName,picAddress,checkTime,belowBanciID) values ('%1','%2','%3','%4','%5') ").arg(timestamp).arg(name).arg(dir).arg(checkTime).arg(banciID);
//	//QString sql2 = "insert into ngcigmaster (picFileName,picAddriess) values ('2','3') ";
//	bool result1 = query1->exec(sql1);
//	if (!result1)//主记录已写入
//	{
//		return 0;//失败
//	}
//	return 1;
//
//
//	/*if (db.transaction())
//	{
//		query = new QSqlQuery();
//		query->exec(sql1);
//		query->exec(sql2);
//		if (!db.commit())
//		{
//			db.rollback();
//			return 0;
//		}
//	}*/
//	//return 1;
//}
