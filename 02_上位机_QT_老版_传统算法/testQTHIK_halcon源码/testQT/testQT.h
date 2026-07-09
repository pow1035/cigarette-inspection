#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_testQT.h"
#include "configForm.h"
#include "search.h"

#include <QCloseEvent>//头文件记得添加
#include <qmessagebox.h>
#include <Qsettings.h>//读取配置文件
#include <QMutex>//内存锁
#include <QThreadPool>//线程池
#include<qfiledialog.h>

#include <Windows.h>//计算用时
#include <vector>

//#include <opencv2/opencv.hpp>
//串口通信
#include<QtSerialPort/qserialport.h>
#include<QtSerialPort/qserialportinfo.h>

#include<MyCamera.h>
#include "MultipleCameraDefine.h"
//#include<task.h>
#include<readIOTask.h>
#include<testWrite.h>
#include<getNumberTask.h>
//#include"USB5841.h"
#include<HalconCpp.h>
#include<qdebug.h>
#include<qtimer.h>
#include<qqueue.h>
#include<process.h>
#include<process2_1.h>
#include<reject.h>
#include<saveImage.h>
using namespace HalconCpp;

struct picStruct
{
	HObject ho_Cam_Image;
	MV_FRAME_OUT_INFO* mv_frame;
	uchar uchar_pic_IO;//用于硬件读写
	uchar uchar_pic_number;//用于图片编号计数
};
struct picSaveStruct
{
	HObject ho_double_gray_image;//原始图
	HObject ho_single_gray_image;//单烟支图
	HObject ho_drawNG_image;//有标记彩色单烟支标记图
	int rejectType;//缺陷分类
	int zuNumber;//组件号
};
//研华PCIE-1730
#include"C:/Advantech/DAQNavi/Inc/bdaqctrl.h"
using namespace Automation::BDaq;
#define  deviceDescription  L"PCIE-1730,BID#0"
#ifndef NGTypeNumber
#define NGTypeNumber 16 //缺陷总类型数
#endif // !NGTypeNumber

//#include<qsqldatabase.h>
//#include<qsqlquery.h>
class testQT : public QMainWindow

{
	Q_OBJECT

public:
	testQT(QWidget *parent = Q_NULLPTR);
	~testQT();
	

private slots:
	void on_btnManualTriggle_clicked();
	void on_btnReject_clicked();
	void on_btnConfig_clicked();
	void on_btnSearch_clicked();
	void on_btnInitControlCard_clicked();
	void on_btnShowCheckRegions_clicked();
	//void itemDoubleClicked(QListWidgetItem *item);
	//void doubleclicked(QListWidgetItem* item);
	void updateConfigSlot();
	void timerSlot();
public:
	Ui::testQTClass ui;
	//QImage img1;
	configForm* config;
	search* searchForm;

	//当前相机图号
	QString camera_1_number, camera_2_number, camera_3_number, camera_4_number;
	
	//QString rectX, rectY, rectWidth, rectHeight;
	//截取矩形区域
	int pic1RectX, pic1RectY, pic1RectHeight, pic1RectWidth;
	int pic2RectX, pic2RectY, pic2RectHeight, pic2RectWidth;
	int pic3RectX, pic3RectY, pic3RectHeight, pic3RectWidth;
	int pic4RectX, pic4RectY, pic4RectHeight, pic4RectWidth;
	
	int pic1DividingX, pic1DividingY, pic1DividingHeight, pic1DividingWidth, pic1DividingThreshold, filterCigDividingVerticalRelative;//滤嘴烟支分界区域
	int filterDividingRectX, filterDividingRectY, filterDividingRectHeight, filterDividingRectWidth, filterDividingRectThreshold, filterVerticalRelative;//滤嘴端分界区域
	int cigDividingRectX, cigDividingRectY, cigDividingRectHeight, cigDividingRectWidth, cigDividingRectThreshold, cigVerticalRelative;//卷烟端分界区域
	int cigUpDividingRectX, cigUpDividingRectY, cigUpDividingRectHeight, cigUpDividingRectWidth, cigUpDividingRectThreshold;//烟支上部分界区域
	int cigDownDividingRectX, cigDownDividingRectY, cigDownDividingRectHeight, cigDownDividingRectWidth, cigDownDividingRectThreshold;//烟支下部分界区域
	int filterWidthRectX, filterWidthRectY, filterWidthRectHeight, filterWidthRectWidth, filterWidthRectThreshold;//滤棒端宽度分界区域
	int cigNearFilterWidthRectX, cigNearFilterWidthRectY, cigNearFilterWidthRectHeight, cigNearFilterWidthRectWidth, cigNearFilterWidthRectThreshold;//烟支近滤棒端宽度分界区域
	int cigTopWidthRectX, cigTopWidthRectY, cigTopWidthRectHeight, cigTopWidthRectWidth, cigTopWidthRectThreshold;//烟支端部宽度分界区域
	
	int cigBrokenX, cigBrokenY, cigBrokenRectHeight, cigBrokenRectWidth, cigBrokenRectThreshold, cigBrokenRelative;//烟支刺破检测区域

	double line_rho, line_theta, line_minLineLength, line_maxLineGap;//霍夫直线查找参数
	double approxPolyDPSet;//拟合多边形精度，参数越小精度越高建议0.8-1.2
	int minDisOfTwoPoints;//参数设置，两点间最小距离
	int line_threshold;
	QMutex m_mutexLock;
	QThreadPool pool;
	bool cameraTriggerAuto=false;
	uchar nowPictureNumber=0;//图像编号：1-100

	//int picDataValue[4] = {0};//四个相机的帧数据大小

	void closeEvent(QCloseEvent* event);//头文件中声明
	bool initCamera();//相机初s始化
	bool initIOCard();
	void initHalconShowWindow();
	bool receiveAndCheckSerialData();//接收串口数据
	//void sendInitHalconShowWindow();
	//bool initMySQL();

	//bool addMasterMySQL(QString timestamp, QString dir, QString name, QString checkTime, QString banciID);//数据库添加记录
	//void OnImageGrabbed(Pylon::CInstantCamera& camera, const Pylon::CGrabResultPtr& grabResult);
	QString m_strDeviceFullName;
	QSettings* configIniWrite;

	LONGLONG getUseTimeStart();//计算程序执行时间开始，返回当前时间
	LONGLONG getUseTime(LONGLONG startTime);//计算程序执行时间结束，返回用时ms

	MV_CC_DEVICE_INFO_LIST m_stDevList;             // ch:设备信息列表结构体变量，用来存储设备列表
	
	MyCamera* m_pcMyCamera[MAX_DEVICE_NUM];      // ch:MyCamera封装了常用接口 | en:CMyCamera packed normal used interface
	//HANDLE *cameraHandle[MAX_DEVICE_NUM];
	// ch:标志设备 | en:
	//HWND  m_hwndDisplay[MAX_DEVICE_NUM];            // ch:显示句柄 | en:Display window

	HTuple hv_WindowHandle1, hv_WindowHandle1NG;
	HObject showPic1;//显示实时刷新图片
	HObject showNGPic1; //显示缺陷图片

	HObject showPic2_1;//显示实时刷新图片
	HObject showNGPic2_1; //显示缺陷图片

	HObject showPic2_2;//显示实时刷新图片
	HObject showNGPic2_2; //显示缺陷图片

	bool halconWindowHandleInit=false;//halcon显示初始化标志
	int picShowInterval = 20;
	int nowShowPicNumber = 0;//烟支编号用于复检
	int nowShowPicReadIO = 0;//烟支编号用于IO
	uint8 maxCigNumber = 100;//需要和单片机程序对应
	//数据统计用
	uint checkCigNumber = 0;//烟支数量
	uint ngCigNumber = 0;//坏烟数量
	float qualifiedRate = 0.0;//合格率
	QTimer* timer;//定时刷新数据

	bool systemRun=false;
	//QSqlDatabase db1;//数据库
	//QSqlQuery* query1;

	//队列QQueue
	QQueue<picStruct> picQueList1_1;//图像缓存队列
	QQueue<picStruct> picQueList2_1;//图像缓存队列
	QQueue<picStruct> picQueList2_2;//图像缓存队列
	//输出缺陷信息队列
	QQueue<uint8> NGNumberList;//缺陷图片编号队列
	//存图队列
	QQueue<picSaveStruct> picSaveList;//缺陷图像存储队列
	//错误分类计数
	uint rejectType[NGTypeNumber] = { 0 };
	//测试时间
	int processTime=0;
	QSerialPort* serial;

	QByteArray controlCardSet;

	//测试剔除
	bool rejectTest = false;
	//显示检测范围
	bool showCheckRegion = false;

	QString nowBrandName = "";
	//组件相机对应关系
	QMap<QString, QString> cameraMatchMap;

	//互斥量
	QMutex mutex;//互斥量
	QMutex mutexPicQueList1;//1组队列互斥量
	QMutex mutexPicQueList2_1;//2组1队列互斥量
	QMutex mutexPicQueList2_2;//2组2队列互斥量
};

