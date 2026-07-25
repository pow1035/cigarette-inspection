#pragma once

#include <QtWidgets/QWidget>
#include "ui_CigVision.h"
#include <QStackedWidget>
#include<QTableWidgetItem>
#include<CusTabBar.h>
#include<myTabWidget.h>
#include<CigVisionParams.h>
#include <atomic>
#include <QMutex>//内存锁
#include <QWaitCondition>
#include <QVariant>
#include <QPointer>
#include <QThreadPool>//线程池
#include<qqueue.h>
#include "MvCameraControl.h"
#include<MyCamera.h>
#include"MultipleCameraDefine.h"
#include "core/ProductRuntimeState.h"
#include<qdebug.h>

class QLabel;
class QCloseEvent;
class QLineEdit;
class QStandardItemModel;
class QTableView;
class QThread;
class QImage;
namespace cigvision { class OfflineInspectionWorker; }

using namespace HalconCpp;
class readIOTask;

enum current_stackedwidget { run, edit, change_brand, system_set, search, count, logTxt, alarm, login };//stackedWidget画面枚举类型
struct picStruct
{
    HObject ho_Cam_Image;
    MV_FRAME_OUT_INFO mv_frame = {};
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
struct machineStateStruct
{
    //组件相机对应关系
    QMap<QString, QString> cameraMatchMap;
    MV_CC_DEVICE_INFO_LIST m_stDevList;             // ch:设备信息列表结构体变量，用来存储设备列表
    MyCamera* m_pcMyCamera[MAX_DEVICE_NUM] = {}; // ch:MyCamera封装了常用接口 | en:CMyCamera packed normal used interface
    int cameraCount = 0;
    //系统运行状态
    std::atomic_bool systemRun{ false };

    struct PictureNumberSnapshot
    {
        uchar nowShowPicReadIO1 = 0;
        uchar nowShowPicReadIO2_1 = 0;
        uchar nowShowPicReadIO2_2 = 0;
        uchar nowPictureNumber1 = 0;
        uchar nowPictureNumber2_1 = 0;
        uchar nowPictureNumber2_2 = 0;
    } pictureNumbers;
    //灰度图队列QQueue
    QQueue<picStruct> grayPicQueList1;//图像缓存队列
    QQueue<picStruct> grayPicQueList2_1;//图像缓存队列
    QQueue<picStruct> grayPicQueList2_2;//图像缓存队列
    //互斥量
    QMutex mutex;//互斥量
    QMutex mutexPictureNumbers;//同一触发周期的编号必须整体读写
    QMutex mutexGrayPicQueList1;//1组队列互斥量
    QMutex mutexGrayPicQueList2_1;//2组1队列互斥量
    QMutex mutexGrayPicQueList2_2;//2组2队列互斥量
    //彩色队列QQueue
    QQueue<picStruct> rgbPicQueList1_1;//图像缓存队列
    QQueue<picStruct> rgbPicQueList2_1;//图像缓存队列
    QQueue<picStruct> rgbPicQueList2_2;//图像缓存队列


};

class CigVision : public QWidget
{
    Q_OBJECT

public:
    explicit CigVision(QWidget *parent = nullptr, bool offlineOnly = true);
    ~CigVision();
protected:
    void closeEvent(QCloseEvent* event) override;
private slots:
    void on_btn_run_clicked();
    void on_btn_alarm_clicked();
    void on_btn_change_clicked();
    void on_btn_login_clicked();
    void on_btn_quit_clicked();
    void on_btn_system_clicked();
    void on_btn_count_clicked();
    void on_btn_edit_clicked();
    void on_btn_log_clicked();
    void on_btn_search_clicked();

    // 添加系统参数窗口退出响应槽
    void onSystemParaWidgetQuit();
    void onBrandComboBoxChanged();
    void onIOReadFailure();
    void onOfflineButtonClicked();
    void onOfflineFrameProcessed(const QImage& image, const QString& resultText,
        const QVariantMap& statistics);
    void onOfflineFinished(const QString& message, bool success);


public:
    Ui::CigVisionClass ui;
    //参数调整画面
    /*QTableWidget* zu1_processTableWidget = new QTableWidget(10, 3);
    QTableWidget* zu1_operatorTableWidget = new QTableWidget(7, 3);*/

    CusTabBar* myTabBar = new CusTabBar();
    myTabWidget* zu1_para_widget = new myTabWidget(myTabBar);
    CigVisionParams params;

    machineStateStruct machineState;//用于记录机车状态
    QThreadPool pool;//线程池

    void initRunView();//初始化运行界面
    void initParaView();//初始化参数调整界面
    void initReviewView();
    void initInsightsViews();
    void refreshReviewView();
    void refreshStatisticsView();
    void refreshDiagnosticsView();
    bool persistProductState(QString& errorMessage) const;
    void applySelectedReview(cigvision::ProductReviewOutcome outcome);
    void btnColorUpdate();//按钮颜色更新

    bool initCamera();//初始化相机
    bool initIOCard();//初始化IO板卡
    bool beginCameraCallback();
    void endCameraCallback();

private:
    bool configureCamera(MyCamera* camera, const QString& cameraKey);
    bool startCameras();
    bool stopCameras();
    void attachCameraCallbacks();
    void detachCameraCallbacks();
    void stopIOReading();
    void waitForCameraCallbacks();
    void shutdownCameras();
    void clearFrameQueues();
    void stopOfflineInspection();

    readIOTask* ioTask = nullptr;
    QMutex runtimeMutex;
    QMutex callbackMutex;
    QWaitCondition callbackIdle;
    int activeCameraCallbacks = 0;
    bool cameraLifecycleFault = false;
    bool offlineOnlyMode = false;
    QThread* offlineThread = nullptr;
    QPointer<cigvision::OfflineInspectionWorker> offlineWorker;
    QLabel* offlineImageLabel = nullptr;
    QLabel* offlineDefectLabel = nullptr;
    QLabel* offlineStatusLabel = nullptr;
    QStandardItemModel* offlineStatsModel = nullptr;
    cigvision::ProductRuntimeState productRuntimeState;
    QTableView* reviewTable = nullptr;
    QStandardItemModel* reviewModel = nullptr;
    QLabel* reviewStatusLabel = nullptr;
    QLineEdit* reviewOperatorEdit = nullptr;
    QLineEdit* reviewNoteEdit = nullptr;
    QLabel* statisticsSummaryLabel = nullptr;
    QLabel* statisticsIdentityLabel = nullptr;
    QStandardItemModel* statisticsCameraModel = nullptr;
    QStandardItemModel* statisticsClassModel = nullptr;
    QLabel* diagnosticsSummaryLabel = nullptr;
    QStandardItemModel* diagnosticsModel = nullptr;
    bool closeWhenOfflineStops = false;
    QString activeProductOutputDirectory;

};
