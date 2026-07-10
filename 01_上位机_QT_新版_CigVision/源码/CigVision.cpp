#include "CigVision.h"
#include <QTableView>
#include "readIOTask.h"
#include <QStandardItemModel>
#include<qdebug.h>
#include <QTableWidget>
#include <QPushButton>
#include <QToolButton>
#include<qtabbar.h>
#include <QLineEdit>
#include <QMutexLocker>
#include <QSet>
#include <vector>



namespace
{
constexpr int kMaxPendingFrames = 10;

class CameraCallbackContext
{
public:
    void attach(CigVision* owner)
    {
        QMutexLocker locker(&mutex_);
        owner_ = owner;
    }

    CigVision* acquire()
    {
        QMutexLocker locker(&mutex_);
        if (owner_ == nullptr)
        {
            return nullptr;
        }
        ++activeCallbacks_;
        return owner_;
    }

    void release()
    {
        QMutexLocker locker(&mutex_);
        --activeCallbacks_;
        if (activeCallbacks_ == 0)
        {
            idle_.wakeAll();
        }
    }

    void detach(CigVision* owner)
    {
        QMutexLocker locker(&mutex_);
        if (owner_ == owner)
        {
            owner_ = nullptr;
        }
        while (activeCallbacks_ > 0)
        {
            idle_.wait(&mutex_);
        }
    }

private:
    QMutex mutex_;
    QWaitCondition idle_;
    CigVision* owner_ = nullptr;
    int activeCallbacks_ = 0;
};

// MVS owns the callback user pointer. Keep these tiny guards alive until process exit
// so a late SDK callback can only observe a detached owner, never freed memory.
CameraCallbackContext* const callbackContext1_1 = new CameraCallbackContext;
CameraCallbackContext* const callbackContext2_1 = new CameraCallbackContext;
CameraCallbackContext* const callbackContext2_2 = new CameraCallbackContext;

CameraCallbackContext* callbackContextForKey(const QString& cameraKey)
{
    if (cameraKey == "1-1")
    {
        return callbackContext1_1;
    }
    if (cameraKey == "2-1")
    {
        return callbackContext2_1;
    }
    if (cameraKey == "2-2")
    {
        return callbackContext2_2;
    }
    return nullptr;
}

class CameraCallbackScope
{
public:
    explicit CameraCallbackScope(void* userContext)
        : context_(static_cast<CameraCallbackContext*>(userContext))
    {
        owner_ = context_ == nullptr ? nullptr : context_->acquire();
        if (owner_ != nullptr && !owner_->beginCameraCallback())
        {
            context_->release();
            owner_ = nullptr;
        }
    }

    ~CameraCallbackScope()
    {
        if (owner_ != nullptr)
        {
            owner_->endCameraCallback();
            context_->release();
        }
    }

    CigVision* owner() const { return owner_; }

private:
    CameraCallbackContext* context_ = nullptr;
    CigVision* owner_ = nullptr;
};

bool enqueueGrayFrame(unsigned char* pData,
    MV_FRAME_OUT_INFO* pFrameInfo,
    QQueue<picStruct>& queue,
    QMutex& mutex,
    uchar pictureNumber,
    uchar pictureIo)
{
    if (pData == nullptr || pFrameInfo == nullptr || pFrameInfo->nWidth == 0 || pFrameInfo->nHeight == 0)
    {
        return false;
    }

    const size_t width = static_cast<size_t>(pFrameInfo->nWidth);
    const size_t height = static_cast<size_t>(pFrameInfo->nHeight);
    const size_t pixelCount = width * height;
    const size_t expectedFrameLength = pixelCount * 3;
    if (pixelCount == 0 || pFrameInfo->nFrameLen < expectedFrameLength)
    {
        return false;
    }

    try
    {
        std::vector<unsigned char> dataGray(pixelCount);
        for (size_t i = 0; i < pixelCount; ++i)
        {
            dataGray[i] = static_cast<unsigned char>(
                pData[3 * i] * 0.299 + pData[3 * i + 1] * 0.587 + pData[3 * i + 2] * 0.114);
        }

        HObject temporaryImage;
        HObject grayImage;
        GenImage1(&temporaryImage, "byte", pFrameInfo->nWidth, pFrameInfo->nHeight,
            reinterpret_cast<Hlong>(dataGray.data()));
        CopyImage(temporaryImage, &grayImage);

        picStruct frame;
        frame.ho_Cam_Image = grayImage;
        frame.mv_frame = *pFrameInfo;
        frame.uchar_pic_number = pictureNumber;
        frame.uchar_pic_IO = pictureIo;

        QMutexLocker locker(&mutex);
        while (queue.size() >= kMaxPendingFrames)
        {
            queue.dequeue();
        }
        queue.enqueue(frame);
        return true;
    }
    catch (...)
    {
        qDebug() << "camera callback frame conversion failed";
        return false;
    }
}
}

bool addQQueue1_1(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, CigVision* pDlg)//图像需要是RGB
{
    machineStateStruct::PictureNumberSnapshot snapshot;
    {
        QMutexLocker locker(&pDlg->machineState.mutexPictureNumbers);
        snapshot = pDlg->machineState.pictureNumbers;
    }
    return enqueueGrayFrame(pData, pFrameInfo, pDlg->machineState.grayPicQueList1,
        pDlg->machineState.mutexGrayPicQueList1, snapshot.nowPictureNumber1,
        snapshot.nowShowPicReadIO1);
}

bool addQQueue2_1(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, CigVision* pDlg)//图像需要是RGB
{
    machineStateStruct::PictureNumberSnapshot snapshot;
    {
        QMutexLocker locker(&pDlg->machineState.mutexPictureNumbers);
        snapshot = pDlg->machineState.pictureNumbers;
    }
    return enqueueGrayFrame(pData, pFrameInfo, pDlg->machineState.grayPicQueList2_1,
        pDlg->machineState.mutexGrayPicQueList2_1, snapshot.nowPictureNumber2_1,
        snapshot.nowShowPicReadIO2_1);
}

bool addQQueue2_2(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, CigVision* pDlg)//图像需要是RGB
{
    machineStateStruct::PictureNumberSnapshot snapshot;
    {
        QMutexLocker locker(&pDlg->machineState.mutexPictureNumbers);
        snapshot = pDlg->machineState.pictureNumbers;
    }
    return enqueueGrayFrame(pData, pFrameInfo, pDlg->machineState.grayPicQueList2_2,
        pDlg->machineState.mutexGrayPicQueList2_2, snapshot.nowPictureNumber2_2,
        snapshot.nowShowPicReadIO2_2);
}
void __stdcall workProcedure1_1(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, void* pUser)
{
    if (NULL == pData || NULL == pFrameInfo)
    {
        return;
    }
    CameraCallbackScope callbackScope(pUser);
    CigVision* pCam = callbackScope.owner();
    if (pCam == nullptr)
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
    if (NULL == pData || NULL == pFrameInfo)
    {
        return;
    }
    CameraCallbackScope callbackScope(pUser);
    CigVision* pCam = callbackScope.owner();
    if (pCam == nullptr)
    {
        return;
    }
    addQQueue2_1(pData, pFrameInfo, pCam);
    //pCam->pool.start(new readIOTask(pUser));

    return;
}
void __stdcall workProcedure2_2(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, void* pUser)
{
    if (NULL == pData || NULL == pFrameInfo)
    {
        return;
    }
    CameraCallbackScope callbackScope(pUser);
    CigVision* pCam = callbackScope.owner();
    if (pCam == nullptr)
    {
        return;
    }
    addQQueue2_2(pData, pFrameInfo, pCam);
    //pCam->pool.start(new task(pData, pFrameInfo, pCam));

    return;
}

CigVision::CigVision(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    //初始化参数类
    //CigVisionParams params;
    //运行界面
    initRunView();
    if (initCamera())//相机初始化
    {
        QString tempString(QString::fromLocal8Bit("成功：相机初始化"));
        qDebug() << tempString;
    }
    else
    {
        qDebug() << QString::fromLocal8Bit("错误：相机初始化未完成，运行按钮将保持安全停止");
    }
    if (initIOCard())//IO板卡初始化
    {
        QString tempString(QString::fromLocal8Bit("成功：IO板卡初始化"));
        //ui.listWidget__information->addItem(tempString);
    }
    //参数设置界面
    initParaView();
    ui.label_brandName->setText(params.getCurrentBrand());
    //统计查询界面

    //系统设置 界面

    //ui.stackedWidget->setCurrentIndex(1);//调试用

    // 连接系统参数窗口退出信号
    connect(&params, &CigVisionParams::systemParaWidgetQuit, this, &CigVision::onSystemParaWidgetQuit);
    connect(&params, &CigVisionParams::selectNewBrand, this, &CigVision::onBrandComboBoxChanged);
}

CigVision::~CigVision()
{
    QMutexLocker runtimeLock(&runtimeMutex);
    machineState.systemRun.store(false);
    stopCameras();
    detachCameraCallbacks();
    waitForCameraCallbacks();
    stopIOReading();
    clearFrameQueues();

    delete ioTask;
    ioTask = nullptr;
    shutdownCameras();
}

//运行界面
void CigVision::initRunView()
{

    //参数
    int stackedWidgetWidth = ui.stackedWidget->size().width();//窗体宽度
    int stackedWidgetHeight = ui.stackedWidget->size().height();//窗体高度
    int v1_titleLabelWidth = (int)(stackedWidgetWidth * 0.2);
    int v1_viewLabelWidth = (int)(stackedWidgetWidth * 0.48);
    int v1_title_labelHeight = (int)(stackedWidgetHeight * 0.025);//左侧画面标题高度
    int v1_view_labelHeight = (int)(stackedWidgetHeight * 0.25);//左侧画面图像高度
    int bugClassCount = 6;//缺陷图像分类
    QWidget* run_page = new QWidget;
    QHBoxLayout* v1_hlayout = new QHBoxLayout(run_page);
    QVBoxLayout* v1_layout1 = new QVBoxLayout(run_page);
    QVBoxLayout* v1_layout2 = new QVBoxLayout(run_page);
    QFont title_font;
    title_font.setFamily("Arial");
    title_font.setPointSize(16);
    title_font.setBold(true);
    QPalette title_palette;
    title_palette.setColor(QPalette::WindowText, Qt::green);

    //运行画面——左侧显示实时图
    QLabel* v1_zu1_label = new QLabel(run_page);
    QLabel* v1_zu1_title_label = new QLabel(run_page);
    v1_zu1_title_label->setFixedSize(v1_titleLabelWidth, v1_title_labelHeight);
    v1_zu1_title_label->setText(QStringLiteral("组件1（检测轮）实时显示"));
    v1_zu1_title_label->setFont(title_font);
    v1_zu1_title_label->setPalette(title_palette);
    v1_zu1_label->setFixedSize(v1_viewLabelWidth, v1_view_labelHeight);
    v1_zu1_label->setStyleSheet("QLabel { border: 3px solid green; }");

    QLabel* v1_zu2w_label = new QLabel(run_page);
    QLabel* v1_zu2w_title_label = new QLabel(run_page);
    v1_zu2w_title_label->setFixedSize(v1_titleLabelWidth, v1_title_labelHeight);
    v1_zu2w_title_label->setFont(title_font);
    v1_zu2w_title_label->setPalette(title_palette);
    v1_zu2w_title_label->setText(QStringLiteral("组件2外侧（调头轮）实时显示"));
    v1_zu2w_label->setFixedSize(v1_viewLabelWidth, v1_view_labelHeight);
    v1_zu2w_label->setStyleSheet("QLabel { border: 3px solid green; }");

    QLabel* v1_zu2n_label = new QLabel(run_page);
    QLabel* v1_zu2n_title_label = new QLabel(run_page);
    v1_zu2n_title_label->setFixedSize(v1_titleLabelWidth, v1_title_labelHeight);
    v1_zu2n_title_label->setFont(title_font);
    v1_zu2n_title_label->setPalette(title_palette);
    v1_zu2n_title_label->setText(QStringLiteral("组件2内侧（调头轮）实时显示"));
    v1_zu2n_label->setFixedSize(v1_viewLabelWidth, v1_view_labelHeight);
    v1_zu2n_label->setStyleSheet("QLabel { border: 3px solid green; }");

    v1_layout1->addWidget(v1_zu1_title_label);
    v1_layout1->addWidget(v1_zu1_label);
    v1_layout1->addWidget(v1_zu2w_title_label);
    v1_layout1->addWidget(v1_zu2w_label);
    v1_layout1->addWidget(v1_zu2n_title_label);
    v1_layout1->addWidget(v1_zu2n_label);

    QLabel* v1_bug_label = new QLabel(run_page);
    QLabel* v1_bug_title_label = new QLabel(run_page);
    v1_bug_title_label->setFixedSize(v1_titleLabelWidth, v1_title_labelHeight);
    v1_bug_title_label->setFont(title_font);
    v1_bug_title_label->setPalette(title_palette);
    v1_bug_title_label->setText(QStringLiteral("缺陷实时显示"));
    v1_bug_title_label->setStyleSheet("color:red;");
    v1_bug_label->setFixedSize(v1_viewLabelWidth, v1_view_labelHeight);
    v1_bug_label->setStyleSheet("QLabel { border: 3px solid red; }");

    v1_layout2->addWidget(v1_bug_title_label);
    v1_layout2->addWidget(v1_bug_label);

    //运行图像——右侧统计表
    QHBoxLayout* v1_hlayout2 = new QHBoxLayout(run_page);
    QTableView* v1_zu1TreeTable = new QTableView(run_page);
    QTableView* v1_zu2TreeTable = new QTableView(run_page);

    QStringList NG_class_stringlist;
    NG_class_stringlist << QStringLiteral("定位失败(外形缺陷)") << QStringLiteral("烟棒缺陷") << QStringLiteral("滤嘴缺陷") << QStringLiteral("拼接缺陷") << QStringLiteral("搭口错牙(DL)") << QStringLiteral("滤嘴破损(DL)") << QStringLiteral("滤嘴皱褶(DL)") << QStringLiteral("缺滤嘴(DL)") << QStringLiteral("烟棒破损(DL)") << QStringLiteral("烟棒脏污(DL)");

    // 创建QStandardItemModel对象并设置行数和列数
    QStandardItemModel* model1 = new QStandardItemModel(6, 2);
    QStringList v1_zu1_bug_show_h_heads, v1_zu1_bug_show_v_heads;
    v1_zu1_bug_show_h_heads << QStringLiteral("流程") << QStringLiteral("组1统计");//行

    model1->setHorizontalHeaderLabels(v1_zu1_bug_show_h_heads);
    v1_zu1_bug_show_v_heads << "1" << "2" << "3" << "4" << "5" << "6";//列
    model1->setVerticalHeaderLabels(v1_zu1_bug_show_v_heads);

    QStandardItemModel* model2 = new QStandardItemModel(6, 3);
    QStringList v1_zu2_bug_show_h_heads, v1_zu2_bug_show_v_heads;
    v1_zu2_bug_show_h_heads << QStringLiteral("流程") << QStringLiteral("组2外统计") << QStringLiteral("组2内统计");

    model2->setHorizontalHeaderLabels(v1_zu2_bug_show_h_heads);
    v1_zu2_bug_show_v_heads << "1" << "2" << "3" << "4" << "5" << "6";
    model2->setVerticalHeaderLabels(v1_zu2_bug_show_v_heads);

    // 为模型设置数据
    for (int i = 0; i != NG_class_stringlist.size(); i++)
    {
        QStandardItem* item1 = new QStandardItem(QString(NG_class_stringlist[i]));
        QStandardItem* item2 = new QStandardItem(QString(NG_class_stringlist[i]));
        model1->setItem(i, 0, item1);
        model2->setItem(i, 0, item2);
        model1->setData(model1->index(i, 0), Qt::AlignCenter, Qt::TextAlignmentRole);
        model2->setData(model2->index(i, 0), Qt::AlignCenter, Qt::TextAlignmentRole);
    }

    // 设置QTableView的数据模型
    v1_zu1TreeTable->setModel(model1);
    v1_zu2TreeTable->setModel(model2);
    v1_hlayout2->addWidget(v1_zu1TreeTable, 2);
    v1_hlayout2->addWidget(v1_zu2TreeTable, 3);
    v1_layout2->addLayout(v1_hlayout2);
    v1_hlayout->addLayout(v1_layout1);
    v1_hlayout->addLayout(v1_layout2);

    //样式调整
    // 设置表头字体
    QFont font;
    font.setBold(true); // 设置为粗体
    font.setPointSize(16); // 设置字体大小为10
    v1_zu1TreeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v1_zu1TreeTable->setShowGrid(true);
    v1_zu1TreeTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    v1_zu1TreeTable->verticalHeader()->setFont(font);
    v1_zu1TreeTable->verticalHeader()->setStyleSheet("QHeaderView::section { background-color:  rgb(59, 59, 59); color: white; }");
    v1_zu1TreeTable->verticalHeader()->setSectionsClickable(false);
    v1_zu1TreeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    v1_zu1TreeTable->horizontalHeader()->setFont(font);
    v1_zu1TreeTable->horizontalHeader()->setSectionsClickable(false);
    v1_zu1TreeTable->horizontalHeader()->setStyleSheet("QHeaderView::section { background-color:  rgb(59, 59, 59); color: white; }");
    v1_zu1TreeTable->setStyleSheet("QTableView QTableCornerButton::section { background-color: rgb(59, 59, 59);}"
        "QTableView{font-size:14px; color: white;gridline-color: white;border-style: solid;border-width: 2px;border-color: #C0C0C0; } ");
    v1_zu1TreeTable->setSelectionMode(QAbstractItemView::NoSelection);
    v1_zu1TreeTable->setColumnWidth(0, 150);
    v1_zu1TreeTable->setColumnWidth(1, 150);

    v1_zu2TreeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v1_zu2TreeTable->setShowGrid(true);
    v1_zu2TreeTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    v1_zu2TreeTable->verticalHeader()->setFont(font);
    v1_zu2TreeTable->verticalHeader()->setStyleSheet("QHeaderView::section { background-color:  rgb(59, 59, 59); color: white; }");
    v1_zu2TreeTable->verticalHeader()->setSectionsClickable(false);
    v1_zu2TreeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    v1_zu2TreeTable->horizontalHeader()->setFont(font);
    v1_zu2TreeTable->horizontalHeader()->setSectionsClickable(false);
    v1_zu2TreeTable->horizontalHeader()->setStyleSheet("QHeaderView::section { background-color:  rgb(59, 59, 59); color: white; }");
    v1_zu2TreeTable->setStyleSheet("QTableView QTableCornerButton::section { background-color: rgb(59, 59, 59);}"
        "QTableView{font-size:14px; color: white;gridline-color: white;border-style: solid;border-width: 2px;border-color: #C0C0C0; } ");
    v1_zu2TreeTable->setSelectionMode(QAbstractItemView::NoSelection);
    v1_zu2TreeTable->setColumnWidth(0, 150);
    v1_zu2TreeTable->setColumnWidth(1, 150);
    v1_zu2TreeTable->setColumnWidth(2, 150);
    //v1_zu1TreeTable->setStyleSheet("QTableView{font-size:14px; color: white;}");


    ui.stackedWidget->addWidget(run_page);
}

//参数设置界面
void CigVision::initParaView() {
    /// <summary>
    /// 组件画面构造
    /// </summary>
    /////////////////////////////////3个画面公共部分///////////////////////////////
   // int containerWidgetWidth = ui.stackedWidget->size().width();//窗体宽度
    int containerWidgetHeight = ui.stackedWidget->size().height();//窗体高度
    int leftWidth = (int)(containerWidgetHeight * 0.48);
    int rightWidth = (int)(containerWidgetHeight * 0.48);
    ui.stackedWidget->addWidget(params.paramConfigTabWidget);
    //ui.stackedWidget->addWidget(params.sysParamsWidget);
    ui.stackedWidget->addWidget(params.brandSelectWidget);
    ui.stackedWidget->addWidget(params.sysParamsWidget);
}

void CigVision::on_btn_run_clicked()
{
    QMutexLocker runtimeLock(&runtimeMutex);
    qDebug() << "run clicked";

    if (!machineState.systemRun.load())
    {
        if (ioTask == nullptr)
        {
            qDebug() << QString::fromLocal8Bit("错误：IO板卡未就绪，系统未进入运行状态");
            btnColorUpdate();
            return;
        }
        if (!startCameras())
        {
            qDebug() << QString::fromLocal8Bit("错误：相机未全部就绪，系统未进入运行状态");
            machineState.systemRun.store(false);
            btnColorUpdate();
            return;
        }
        if (!ioTask->prepareStart())
        {
            qDebug() << QString::fromLocal8Bit("错误：IO读取任务无法启动，正在回滚相机");
            stopCameras();
            detachCameraCallbacks();
            waitForCameraCallbacks();
            btnColorUpdate();
            return;
        }
        machineState.systemRun.store(true);
        pool.start(ioTask);
    }
    else
    {
        machineState.systemRun.store(false);
        stopCameras();
        detachCameraCallbacks();
        waitForCameraCallbacks();
        stopIOReading();
        clearFrameQueues();
    }
    btnColorUpdate();
}
void CigVision::on_btn_edit_clicked()
{
    qDebug() << "edit clicked";
    if (ui.stackedWidget->currentIndex() != current_stackedwidget::edit)
    {
        ui.stackedWidget->setCurrentIndex(current_stackedwidget::edit);
    }
    else
    {
        ui.stackedWidget->setCurrentIndex(current_stackedwidget::run);
    }
    btnColorUpdate();
}
void CigVision::on_btn_log_clicked()
{
    qDebug() << "log clicked";

}
void CigVision::on_btn_alarm_clicked()
{
    qDebug() << "alarm clicked";
}
void CigVision::on_btn_change_clicked()
{
    qDebug() << "change_brand clicked";
	if (ui.stackedWidget->currentIndex() != current_stackedwidget::change_brand)
	{
		ui.stackedWidget->setCurrentIndex(current_stackedwidget::change_brand);
	}
	else
	{
		ui.stackedWidget->setCurrentIndex(current_stackedwidget::run);
	}
	btnColorUpdate();
}
void CigVision::on_btn_login_clicked()
{
    qDebug() << "login clicked";
}
void CigVision::on_btn_quit_clicked()
{
    qDebug() << "quit clicked";
}
void CigVision::on_btn_system_clicked()
{
    qDebug() << "system clicked";
    if (ui.stackedWidget->currentIndex() != current_stackedwidget::system_set)
    {
        ui.stackedWidget->setCurrentIndex(current_stackedwidget::system_set);
    }
    else
    {
        ui.stackedWidget->setCurrentIndex(current_stackedwidget::run);
    }
    btnColorUpdate();
}
void CigVision::on_btn_count_clicked()
{
    qDebug() << "count clicked";
}


void CigVision::on_btn_search_clicked()
{
    qDebug() << "search clicked";
}

void CigVision::btnColorUpdate()//按钮颜色更新
{
    ui.btn_edit->setIcon(QPixmap(QStringLiteral("icons/use/edit.png")));
    ui.btn_search->setIcon(QPixmap(QStringLiteral("icons/use/picture-filling (1).png")));
    ui.btn_change->setIcon(QPixmap(QStringLiteral("icons/use/copy.png")));
    ui.btn_system->setIcon(QPixmap(QStringLiteral("icons/use/settings.png")));
    //ui.b
    switch (ui.stackedWidget->currentIndex())
    {
    case current_stackedwidget::run:

        break;
    case current_stackedwidget::edit:
        ui.btn_edit->setIcon(QPixmap(QStringLiteral("icons/use/edit (blue).png")));
        break;
	case current_stackedwidget::search:
		ui.btn_search->setIcon(QPixmap(QStringLiteral("icons/use/picture-filling (blue).png")));
		break;
	case current_stackedwidget::change_brand:
		ui.btn_change->setIcon(QPixmap(QStringLiteral("icons/use/copy (blue).png")));
		break;
    case current_stackedwidget::system_set:
        ui.btn_system->setIcon(QPixmap(QStringLiteral("icons/use/settings (blue).png")));

    default:
        break;
    }
    if (machineState.systemRun.load())
    {
        ui.btn_run->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-filling (green).png")));
    }
    else
    {
        ui.btn_run->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-filling.png")));
    }
}

void CigVision::onSystemParaWidgetQuit()
{
    // 实现系统参数窗口退出后的处理逻辑
    qDebug() << "System parameter widget quit";
    ui.stackedWidget->setCurrentIndex(current_stackedwidget::run);
    btnColorUpdate();
}
void CigVision::onBrandComboBoxChanged() {
    qDebug() << "select new brand";
    ui.label_brandName->setText(params.currentBrandLabel->text());
}

bool CigVision::configureCamera(MyCamera* camera, const QString& cameraKey)
{
    if (camera == nullptr)
    {
        return false;
    }

    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int offsetX = 0;
    unsigned int offsetY = 0;
    float exposureTime = 0.0f;

    if (cameraKey == "1-1")
    {
        width = 992;
        height = 300;
        offsetX = 176;
        offsetY = 332;
        exposureTime = 50.0f;
    }
    else if (cameraKey == "2-1" || cameraKey == "2-2")
    {
        width = 1200;
        height = 600;
        offsetX = 96;
        offsetY = (cameraKey == "2-1") ? 160 : 168;
        exposureTime = 100.0f;
    }
    else
    {
        return false;
    }

    auto applySetting = [&cameraKey](int result, const char* settingName)
    {
        if (result != MV_OK)
        {
            qDebug() << QString::fromLocal8Bit("错误：相机参数设置失败")
                << cameraKey << settingName << result;
            return false;
        }
        return true;
    };

    if (!applySetting(camera->SetEnumValue("TriggerMode", MV_TRIGGER_MODE_ON), "TriggerMode") ||
        !applySetting(camera->SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_LINE0), "TriggerSource") ||
        !applySetting(camera->SetEnumValue("TriggerActivation", 0), "TriggerActivation") ||
        !applySetting(camera->SetFloatValue("ExposureTime", exposureTime), "ExposureTime") ||
        !applySetting(camera->SetIntValue("Width", width), "Width") ||
        !applySetting(camera->SetIntValue("Height", height), "Height") ||
        !applySetting(camera->SetIntValue("OffsetX", offsetX), "OffsetX") ||
        !applySetting(camera->SetIntValue("OffsetY", offsetY), "OffsetY") ||
        !applySetting(camera->SetFloatValue("Gain", 0.0f), "Gain") ||
        !applySetting(camera->SetEnumValue("PixelFormat", PixelType_Gvsp_RGB8_Packed), "PixelFormat") ||
        !applySetting(camera->SetFloatValue("AcquisitionFrameRate", 200.0f), "AcquisitionFrameRate"))
    {
        return false;
    }

    CameraCallbackContext* callbackContext = callbackContextForKey(cameraKey);
    if (callbackContext == nullptr)
    {
        return false;
    }
    if (cameraKey == "1-1")
    {
        return applySetting(camera->RegisterImageCallBack(workProcedure1_1, callbackContext), "RegisterImageCallBack");
    }
    if (cameraKey == "2-1")
    {
        return applySetting(camera->RegisterImageCallBack(workProcedure2_1, callbackContext), "RegisterImageCallBack");
    }
    return applySetting(camera->RegisterImageCallBack(workProcedure2_2, callbackContext), "RegisterImageCallBack");
}

bool CigVision::startCameras()
{
    if (cameraLifecycleFault || machineState.cameraCount != machineState.cameraMatchMap.size())
    {
        return false;
    }

    attachCameraCallbacks();

    for (int i = 0; i < machineState.cameraCount; ++i)
    {
        MyCamera* camera = machineState.m_pcMyCamera[i];
        if (camera == nullptr || camera->StartGrabbing() != MV_OK)
        {
            for (int started = 0; started < i; ++started)
            {
                const int stopRet = machineState.m_pcMyCamera[started]->StopGrabbing();
                if (stopRet != MV_OK)
                {
                    cameraLifecycleFault = true;
                    qDebug() << QString::fromLocal8Bit("错误：相机启动回滚时停止失败")
                        << started << stopRet;
                }
            }
            detachCameraCallbacks();
            waitForCameraCallbacks();
            return false;
        }
    }
    return true;
}

bool CigVision::stopCameras()
{
    bool allStopped = true;
    for (int i = 0; i < machineState.cameraCount; ++i)
    {
        if (machineState.m_pcMyCamera[i] != nullptr)
        {
            const int stopRet = machineState.m_pcMyCamera[i]->StopGrabbing();
            if (stopRet != MV_OK)
            {
                allStopped = false;
                cameraLifecycleFault = true;
                qDebug() << QString::fromLocal8Bit("错误：相机停止失败") << i << stopRet;
            }
        }
    }
    return allStopped;
}

void CigVision::attachCameraCallbacks()
{
    callbackContext1_1->attach(this);
    callbackContext2_1->attach(this);
    callbackContext2_2->attach(this);
}

void CigVision::detachCameraCallbacks()
{
    callbackContext1_1->detach(this);
    callbackContext2_1->detach(this);
    callbackContext2_2->detach(this);
}

bool CigVision::beginCameraCallback()
{
    QMutexLocker locker(&callbackMutex);
    if (!machineState.systemRun.load())
    {
        return false;
    }
    ++activeCameraCallbacks;
    return true;
}

void CigVision::endCameraCallback()
{
    QMutexLocker locker(&callbackMutex);
    --activeCameraCallbacks;
    if (activeCameraCallbacks == 0)
    {
        callbackIdle.wakeAll();
    }
}

void CigVision::waitForCameraCallbacks()
{
    QMutexLocker locker(&callbackMutex);
    while (activeCameraCallbacks > 0)
    {
        callbackIdle.wait(&callbackMutex);
    }
}

void CigVision::stopIOReading()
{
    if (ioTask == nullptr)
    {
        return;
    }
    ioTask->requestStop();
    if (!pool.waitForDone(2000))
    {
        qDebug() << QString::fromLocal8Bit("错误：IO读取线程未在2秒内退出，继续等待以避免释放竞态");
        pool.waitForDone();
    }
}

void CigVision::shutdownCameras()
{
    const bool allStopped = stopCameras();
    detachCameraCallbacks();
    waitForCameraCallbacks();
    for (int i = 0; i < machineState.cameraCount; ++i)
    {
        delete machineState.m_pcMyCamera[i];
        machineState.m_pcMyCamera[i] = nullptr;
    }
    machineState.cameraCount = 0;
    cameraLifecycleFault = !allStopped;
}

void CigVision::clearFrameQueues()
{
    QMutexLocker lock1(&machineState.mutexGrayPicQueList1);
    QMutexLocker lock2(&machineState.mutexGrayPicQueList2_1);
    QMutexLocker lock3(&machineState.mutexGrayPicQueList2_2);
    machineState.grayPicQueList1.clear();
    machineState.grayPicQueList2_1.clear();
    machineState.grayPicQueList2_2.clear();
}

bool CigVision::initCamera()
{
    int nRet = -1;

    //组件相机对应初始化
    shutdownCameras();
    if (cameraLifecycleFault)
    {
        qDebug() << QString::fromLocal8Bit("错误：相机未安全关闭，拒绝重新初始化");
        return false;
    }
    machineState.cameraMatchMap.clear();
    //1组件
    machineState.cameraMatchMap.insert("1-1", params.cameraParams.camera1SerialNum);
    //2组件1相机(内)
    machineState.cameraMatchMap.insert("2-1", params.cameraParams.camera2OuterSerialNum);
    //2组件2相机(外)
    machineState.cameraMatchMap.insert("2-2", params.cameraParams.camera2InnerSerialNum);

    QSet<QString> expectedSerialNumbers;
    for (auto it = machineState.cameraMatchMap.constBegin(); it != machineState.cameraMatchMap.constEnd(); ++it)
    {
        const QString serialNumber = it.value().trimmed();
        if (serialNumber.isEmpty() || expectedSerialNumbers.contains(serialNumber))
        {
            qDebug() << QString::fromLocal8Bit("错误：相机序列号为空或重复") << it.key();
            return false;
        }
        expectedSerialNumbers.insert(serialNumber);
    }
    QSet<QString> matchedCameraKeys;

    //int nCanOpenDeviceNum = 0;
    memset(&machineState.m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    //枚举子网内指定的传输协议对应的所有设备
    nRet = MyCamera::EnumDevices(&machineState.m_stDevList);
    if (MV_OK != nRet)
    {
        QString tempString(QString::fromLocal8Bit("错误：枚举相机失败"));
        //ui.listWidget__information->addItem(tempString);
        return false;
    }
    //
    int cameraIndex = 0;
    for (unsigned int j = 0;
        j < machineState.m_stDevList.nDeviceNum && cameraIndex < MAX_DEVICE_NUM;
        ++j)
    {
        const int i = cameraIndex;
        unsigned char deviceGUID[INFO_MAX_BUFFER_SIZE] = { 0 };
        machineState.m_pcMyCamera[i] = new MyCamera;
        machineState.m_pcMyCamera[i]->m_pBufForDriver = NULL;
        machineState.m_pcMyCamera[i]->m_pBufForSaveImage = NULL;
        machineState.m_pcMyCamera[i]->m_nBufSizeForDriver = 0;
        machineState.m_pcMyCamera[i]->m_nBufSizeForSaveImage = 0;
        machineState.m_pcMyCamera[i]->m_nTLayerType = machineState.m_stDevList.pDeviceInfo[j]->nTLayerType;

        nRet = machineState.m_pcMyCamera[i]->Open(machineState.m_stDevList.pDeviceInfo[j]);
        if (MV_OK != nRet)
        {
            delete(machineState.m_pcMyCamera[i]);
            machineState.m_pcMyCamera[i] = NULL;
            continue;
        }
        else
        {
            machineState.cameraCount = cameraIndex + 1;
            memcpy(deviceGUID, machineState.m_stDevList.pDeviceInfo[j]->SpecialInfo.stUsb3VInfo.chSerialNumber, INFO_MAX_BUFFER_SIZE);
            QString deviceGUIDString = QString::fromLocal8Bit((char*)deviceGUID);

            bool matchedCurrentCamera = false;
            QMap<QString, QString>::const_iterator it = machineState.cameraMatchMap.constBegin();
            while (it != machineState.cameraMatchMap.constEnd())//配置map遍历，找到对应相机的设置
            {
                QString cameraKey;//当前设置相机的Key
                if (!QString::compare(it.value(), deviceGUIDString))//相机ID匹配到设置中ID
                {
                    cameraKey = it.key();//设置Key
                    if (matchedCameraKeys.contains(cameraKey))
                    {
                        qDebug() << QString::fromLocal8Bit("错误：同一相机配置被重复匹配") << cameraKey;
                        shutdownCameras();
                        return false;
                    }
                    matchedCurrentCamera = true;
                    matchedCameraKeys.insert(cameraKey);
                    nRet = configureCamera(machineState.m_pcMyCamera[i], cameraKey) ? MV_OK : MV_E_PARAMETER;
                    QString tempString(QString::fromLocal8Bit("找到相机"));
                    tempString.append(cameraKey);
                    tempString.append(QString::fromLocal8Bit("编号："));
                    tempString.append(deviceGUIDString);
                    qDebug() << tempString;
                    //ui.listWidget__information->addItem(tempString);
                    if (MV_OK != nRet)
                    {
                        QString tempString(QString::fromLocal8Bit("相机"));
                        tempString.append(cameraKey);
                        tempString.append(QString::fromLocal8Bit(" 参数或回调设置失败"));
                        qDebug() << tempString;
                        //ui.listWidget__information->addItem(tempString);
                        shutdownCameras();
                        return false;
                    }

                }
                it++;
            }

            if (!matchedCurrentCamera)
            {
                machineState.m_pcMyCamera[i]->Close();
                delete machineState.m_pcMyCamera[i];
                machineState.m_pcMyCamera[i] = nullptr;
                machineState.cameraCount = cameraIndex;
            }
            else
            {
                ++cameraIndex;
                machineState.cameraCount = cameraIndex;
            }
        }
    }

    if (matchedCameraKeys.size() != machineState.cameraMatchMap.size())
    {
        qDebug() << QString::fromLocal8Bit("错误：未找到全部配置相机")
            << matchedCameraKeys << machineState.cameraMatchMap.keys();
        shutdownCameras();
        return false;
    }

    return true;
}

bool CigVision::initIOCard()
{
    if (ioTask != nullptr)
    {
        return true;
    }

    ioTask = new readIOTask(this);
    ioTask->setAutoDelete(false);
    connect(ioTask, &readIOTask::fatalReadError, this, &CigVision::onIOReadFailure,
        Qt::QueuedConnection);
    if (!ioTask->initialize())
    {
        delete ioTask;
        ioTask = nullptr;
        return false;
    }
    return true;
}

void CigVision::onIOReadFailure()
{
    QMutexLocker runtimeLock(&runtimeMutex);
    if (!machineState.systemRun.load())
    {
        return;
    }

    qDebug() << QString::fromLocal8Bit("错误：IO连续读取失败，系统进入安全停止状态");
    machineState.systemRun.store(false);
    stopCameras();
    detachCameraCallbacks();
    waitForCameraCallbacks();
    stopIOReading();
    clearFrameQueues();
    btnColorUpdate();
}
