#pragma once
#include <QObject>
#include <QRunnable>
#include "testQT.h"
//#include <opencv2/opencv.hpp>
//#include <opencv2/core.hpp>
#include <Windows.h>//计算用时
#include <vector>
#include<qdatetime.h>
#include <QThreadPool>//线程池
#include<HalconCpp.h>
#include<qdir.h>
//MySQL数据库相关
//#include<qsqldatabase.h>
//#include<qsqlquery.h>
using namespace HalconCpp;
#define Err_NotFindCig 0x01000000 //未找到烟支
#define Err_FindMoreThan2Cig 0x02000000//超过两根烟
#define Err_SameSide2Cig 0x04000000//上侧或者下侧侧出现两支烟

struct ParaStruct
{
    //全局二值化
    int all_gray_thr_min = 100;//全局二值化低限
    int all_gray_thr_max = 255;//全局二值化高限
    //开运算
    int open_width=5;
    int open_height = 5;
    //烟棒面积最小阈值
    int cig_area_min = 25000;

    bool checkCig2 = false;//是否进行第二支烟的检验

    int cig1_thr = 50;//烟棒1内部缺陷二值化阈值
    int smallestCheckArea = 5;//最小检查面积
    double smallestDrawR = 10;//最小缺陷绘画半径
    //水纸搭接不齐
    //int cig1_div_thr = 100;//烟与嘴棒分割二值化
    int divding_x1 = 640;//分割区域左边界
    int divding_x2 = 690;//分割区域右边界
    int div_thr = 128;//分割二值化
    int div_diff_area = 150;//分割外界矩形差值最小面积
};
struct Para1Struct
{
    int cig_area_min = 35000;
};

struct CigNGDetail
{
    //QString ng;
    QString noclassID="0";
    QString value1="0";
    QString value2="0";
    QString value3="0";
    QString value4="0";
};

class testQT;
class task : public QObject, public QRunnable
{
    Q_OBJECT

public:
    task(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, testQT* pUser);
    task(HTuple filePath, testQT* pCam);
    task(QString filename, testQT* pCam);
    task(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, testQT* pCam, int camera_number,uchar photo_number);
    //task(int camera_number, uchar photo_number);
    uint whiteCigDarkLightCircleFilterProcess();//处理过程：硬红
    uint whiteCigWhiteFilterProcess();//处理过程：迎宾

    bool initHalconWindowHandle();//初始化Halcon窗口
    bool saveNGImage(QString dir, QString name);//缺陷图片保存
    bool saveNGImageByClass(QString dir, QString name,uint result);//缺陷图片保存
    bool showImage();//显示图像
    bool addMasterMySQL(QString dir, QString name,QString checkTime,QString banciID);//数据库添加记录
    //bool addDetailMySQL(QString classID, QString value1, QString value2, QString value3,QString value4);//数据库添加记录
    bool addDetailMySQL();
    bool showEffect(HObject image);//显示过程图像
    bool sendResultToIOCard();//向IO卡发送处理结果
   
    //以下单流程算法未完成20220729
    bool getRGBHImageFromRGBData(unsigned char* image_pData, HObject& Out_image);//得到彩色图像
    bool getOneOfRGBHImageFromRGBData(unsigned char* image_pData,int RGB_Number, HObject& Out_image);//得到单通道图像;RGB_Number=1,2,3;对应R,G,B
    bool getGrayHImageFromRGBData(unsigned char* image_pData, HObject& Out_image);//得到灰色图像
    //bool getCigaretteNumberAndPositon(HObject& in_img, HTuple& out_number, HTuple& out_row, HTuple& out_col, HTuple& out_Phi, HTuple& out_width, HTuple& out_height);//获得烟支的定位信息
    bool getCigaretteNumberAndPositon(HObject& in_img, int gray_thr_min, int gray_thr_max, int close_width, int close_height, int area_min, HObject& ho_SelectedRegions);//获得烟支的定位信息

    int getXCigaretteGrayImage(HObject& in_img,int in_cigNumber, HTuple& out_row, HTuple& out_col, HTuple& out_Phi, HTuple& out_length1, HTuple& out_length2, HObject& out_img);//获得第in_cigNumber支烟棒的图片
    int getCigConAndRectRightNumber(HObject& in_img, int gray_thr_min, int gray_thr_max, int close_width, int close_height);//返回图像中烟棒的突度和矩形度满足要求的数量，用于检测烟棒外形
    //bool checkCigWidthAndHeight(HTuple& in_number, HTuple& in_row, HTuple& out_col, HTuple& out_Phi, HTuple& out_length1, HTuple& out_length2,int in_cigNumber,int in_width_threshold,int in_height_threshold);//检查宽度和高度
    int checkCigWidthAndHeight(HTuple& in_width, HTuple& in_height, int in_cigNumber, int in_width_threshold, int in_height_threshold,int& out_width,int& out_height);//检查宽度和高度
    //bool checkDarkPointsOnCigarette(HObject& region, HTuple& in_darkNGOnCigOriRow, HTuple& in_darkNGOnCigOriCol, HTuple& in_darkNGOnCigCorRow, HTuple& in_darkNGOnCigCorCol, int smallestCheckArea);//检查暗点
    //bool checkDarkPointsOnCigarette(HObject& region, HTuple& out_row, HTuple& out_col, HTuple& out_Phi, HTuple& out_width, HTuple& out_height, HTuple& in_darkNGOnCigOriRow, HTuple& in_darkNGOnCigOriCol, HTuple& in_darkNGOnCigCorRow, HTuple& in_darkNGOnCigCorCol, int smallestCheckArea);//检查暗点
    bool checkDarkPointsOnCigarette(HObject& ho_SelectedRegions, HTuple& out_row, HTuple& out_col, HTuple& out_Phi, HTuple& out_width, HTuple& out_height, HTuple& in_darkNGOnCigOriRow, HTuple& in_darkNGOnCigOriCol, HTuple& in_darkNGOnCigCorRow, HTuple& in_darkNGOnCigCorCol, int smallestCheckArea);//检查暗点
    void drawPictureOnQlabel();//画图
    LONGLONG initTime();
    LONGLONG getExpendTime(LONGLONG startTime);
    bool createFileDir(QString dir);//创建图片保存路径
    bool QStringCNFileNameSaveImage(QString str);
    bool QStringCNFileNameReadImage(QString str, HObject &ho_image);
    bool writeIOCard();//写IO板卡
    ~task();
protected:
    void run();
    void one_camera_hardware_save_image_task();

public:
    unsigned char* pDataTask;//图像数据指针，构造函数中初始化

    MV_FRAME_OUT_INFO* pFrameInfoTask;
    testQT* pDlg;

    int camera_number;//相机编号
    uchar photo_number;//图片编号
    HObject global_gray_image;//全局灰色图像原图

    ParaStruct para;//处理过程参数变量结构体
    Para1Struct para1;//处理过程参数变量结构体
    
    bool testPic = 0;//以图片测试时为1
    //HObject global_gray_image;//全局灰色图像
    //bool getAllMask(InputArray _originalMat, OutputArray _allMask);
//signals:
    //QSqlDatabase db;//数据库
    //QSqlQuery* query;

    QString timestamp;//时间戳也是烟支编号
    QList<CigNGDetail> ngList;
    //void mySignal();
    LARGE_INTEGER litmp;
    LONGLONG QStartCount,expendTime;//毫秒ms
    double dfFreq;//CPU频率

    bool BoolInitHalconWindow=false;
};


