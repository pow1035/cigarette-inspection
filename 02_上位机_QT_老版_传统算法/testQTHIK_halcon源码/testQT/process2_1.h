#pragma once
#include <QObject>
#include <QRunnable>
#include "testQT.h"
#include <Windows.h>//计算用时
#include <vector>
#include<qdatetime.h>
#include <QThreadPool>//线程池
#include<HalconCpp.h>
#include<qdir.h>
#include<qqueue.h>
#include"C:/Advantech/DAQNavi/Inc/bdaqctrl.h"

using namespace Automation::BDaq;
using namespace HalconCpp;
#define Err_NotFindCig 0x01000000 //未找到烟支
#define Err_FindMoreThan2Cig 0x02000000//超过两根烟
#define Err_SameSide2Cig 0x04000000//上侧或者下侧侧出现两支烟
//计划使用0x0000F000三个字节做位置缺陷同位置校验，排除飞沫干扰，精度为烟支长度的1/10
struct picStruct;
struct picSaveStruct;
struct picCheckAgainStruct
{
    uint result;
    uint8 numberIO;
    HTuple hv_NGPara;//缺陷参数，仅用于烟棒黑点，其他缺陷需要传参时，此处应改为参数数组
};
struct ParaStructProcess
{
    //待检烟支区域
    int cig1_region_leftX = 180;
    int cig1_region_leftY = 340;
    int cig1_region_rightX = 1140;
    int cig1_region_rightY = 560;
    //全局二值化
    int all_gray_thr_min = 50;//全局二值化低限
    int all_gray_thr_max = 255;//全局二值化高限
    //开运算
    int open_width = 5;
    int open_height = 5;
    //闭运算
    int close_width = 10;
    int close_height = 10;
    //烟棒面积最小阈值
    int cig_area_min = 35000;

    bool checkCig2 = false;//是否进行第二支烟的检验

    int cig1_thr = 50;//烟棒1内部缺陷二值化阈值
    int smallestCheckArea = 15;//最小检查面积
    double smallestDrawR = 10;//最小缺陷绘画半径
    //水纸搭接不齐
    //int cig1_div_thr = 100;//烟与嘴棒分割二值化
    int divding_x1 = 570;//分割区域左边界
    int divding_x2 = 600;//分割区域右边界
    int div_thr = 128;//分割二值化
    int div_diff_area = 150;//分割外界矩形差值最小面积
    //滤嘴缺陷
    int filter_thr = 120;//滤嘴二值化阈值
    int filter_check_width = 270;//滤嘴检测区域宽度，略小于滤嘴长度
    int filter_check_height = 60;//滤嘴检测区域高度，略小于滤嘴宽度
    int filter_except_min_offsetX = 0;//滤嘴例外区域X轴最小偏移量
    int filter_except_max_offsetX = 150;//滤嘴例外区域X轴最小偏移量，便宜区域为例外
    int filter_min_area = 50;//最小缺陷面积


};
struct ParaStructProcess1
{
    int cig_area_min = 35000;
};
class process2_1 : public QObject, public QRunnable
{
    Q_OBJECT
public:
    process2_1(testQT* pDlg,int camera_number, uchar photo_number);
    uint whiteCigDarkLightCircleFilterProcess(picStruct* ps);//处理过程：硬红
    uint whiteCigDarkLightCircleFilterProcess(picStruct* ps, uint8* checkAgainRejectNumber, int* rejectType);//处理过程：硬红
    
    /*
    picStruct：队列中图片的信息；
    Bool_cig1RejectEnable：烟支1剔除使能；
    Bool_cig2RejectEnable：烟支2剔除使能；
    uint_cig1Result：烟支1缺陷分类结果按位。共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷,上烟。每支烟：1位圆度不合格，2位烟支内部缺陷，3位搭扣缺陷，4位滤嘴缺陷
    uint_cig2Result：烟支1缺陷分类结果按位。同上
    ho_cig1DrawNGSingle：烟支1单支缺陷图
    ho_cig2DrawNGSingle：烟支2单支缺陷图
    uint8_cig2RejectNumberIO：烟支2剔除发下位机编号
    hv_cig1Param：烟支1复检参数。参数含义与缺陷类型对应：烟支内部缺陷对应缺陷中心点X坐标，搭口缺陷对应面积值，滤嘴缺陷对应缺陷中心点X坐标
    ho_showCheckRegion：用以标识检测基本设定情况，显示用
    */
    
    bool whiteCigDarkLightCircleFilterProcess(picStruct& ps, bool* Bool_cig1RejectEnable, bool* Bool_cig2RejectEnable, uint* uint_cig1Result, uint* uint_cig2Result, HObject* ho_cig1DrawNGSingle, HObject* ho_cig2DrawNGSingle, uint8* uint8_cig2RejectNumberIO,HTuple *hv_cig1Param, HObject *ho_showCheckRegion);//处理过程：硬红
    bool whiteCigLightCircleFilterProcess(picStruct& ps, bool* Bool_cig1RejectEnable, bool* Bool_cig2RejectEnable, uint* uint_cig1Result, uint* uint_cig2Result, HObject* ho_cig1DrawNGSingle, HObject* ho_cig2DrawNGSingle, uint8* uint8_cig2RejectNumberIO, HTuple* hv_cig1Param, HObject* ho_showCheckRegion);//处理过程：迎宾
    ~process2_1();

    LONGLONG initTime();
    LONGLONG getExpendTime(LONGLONG startQpart);

    testQT* testqt;
    ParaStructProcess para1;
    LARGE_INTEGER litmp;
    LONGLONG QStartCount, expendTime;//毫秒ms
    double dfFreq;//CPU频率
    //QQueue<picCheckAgainStruct> checkAgainList;//复检队列
    QHash<uchar, picCheckAgainStruct> checkAgainHash;//复检hash
    uint needCheckAgainSigns = 0x00000FFE;//复检标志位过滤器，除了缺陷位1对应的外形缺陷，都需要复检
    bool showCheckRegion = false;//画框显示

protected:
    void run();
    
};

