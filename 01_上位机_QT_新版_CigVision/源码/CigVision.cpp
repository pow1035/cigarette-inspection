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
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QVariantList>
#include <QVariantMap>
#include <QVBoxLayout>
#include "adapters/qt/QtOfflineInspection.h"
#include <cmath>
#include <string>
#include <vector>



namespace
{
constexpr int kMaxPendingFrames = 10;

std::string utf8String(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

cigvision::TimestampMicros currentTimestampMicros()
{
    return static_cast<cigvision::TimestampMicros>(
        QDateTime::currentMSecsSinceEpoch()) * 1000;
}

QString decisionDisplayName(cigvision::InspectionDecision decision)
{
    switch (decision) {
    case cigvision::InspectionDecision::Ok: return QStringLiteral("OK");
    case cigvision::InspectionDecision::Ng: return QStringLiteral("NG");
    case cigvision::InspectionDecision::Error: return QStringLiteral("ERROR");
    default: return QStringLiteral("UNKNOWN");
    }
}

QString reviewOutcomeName(cigvision::ProductReviewOutcome outcome)
{
    switch (outcome) {
    case cigvision::ProductReviewOutcome::Confirmed: return QStringLiteral("CONFIRMED");
    case cigvision::ProductReviewOutcome::Corrected: return QStringLiteral("CORRECTED");
    case cigvision::ProductReviewOutcome::Dismissed: return QStringLiteral("DISMISSED");
    default: return QStringLiteral("UNREVIEWED");
    }
}

QString diagnosticSeverityName(cigvision::ProductDiagnosticSeverity severity)
{
    switch (severity) {
    case cigvision::ProductDiagnosticSeverity::Warning: return QStringLiteral("WARNING");
    case cigvision::ProductDiagnosticSeverity::Error: return QStringLiteral("ERROR");
    default: return QStringLiteral("INFORMATION");
    }
}

bool buildLegacyDeepLearningProfile(const DeepLearningParams& parameters,
    cigvision::ProductParameterProfile& profile, QString& errorMessage)
{
    profile = cigvision::ProductParameterProfile();
    profile.kind = cigvision::ProductParameterProfileKind::LegacyDeepLearningPage;
    profile.profileId = "legacy-deep-learning-page";
    profile.parameterVersion = "legacy-deep-learning-page-v1";
    const char* names[] = {
        "dakoucuoya", "feiyan", "jiamo", "lvzuizhezhou", "quezui",
        "yanbangposun", "yanbangzangwu", "wuzi", "jietou"
    };
    const double thresholds[] = {
        parameters.jointRollThreshold,
        parameters.flyingTobaccoThreshold,
        parameters.tobaccoClipsThreshold,
        parameters.filterWrinkleThreshold,
        parameters.missingFilterThreshold,
        parameters.rodDamageThreshold,
        parameters.rodStainThreshold
    };
    for (std::int32_t classId = 0; classId < 9; ++classId) {
        cigvision::ProductClassParameter rule;
        rule.classId = classId;
        rule.className = names[classId];
        if (classId < 7) {
            const double threshold = thresholds[classId];
            if (!std::isfinite(threshold) || threshold < 0.0 || threshold > 1.0) {
                errorMessage = QStringLiteral("深度学习类别 %1 阈值必须位于 [0,1]")
                    .arg(classId);
                return false;
            }
            rule.confidenceThreshold = static_cast<float>(threshold);
            rule.enabled = true;
        } else {
            rule.confidenceThreshold = 1.0F;
            rule.enabled = false;
        }
        profile.classes.push_back(rule);
    }
    std::string profileError;
    if (!profile.validate(&profileError)) {
        errorMessage = QString::fromUtf8(profileError.c_str());
        return false;
    }
    errorMessage.clear();
    return true;
}

cigvision::ProductParameterProfile fixtureParameterProfile()
{
    cigvision::ProductParameterProfile profile;
    profile.kind = cigvision::ProductParameterProfileKind::DeterministicFixture;
    profile.profileId = "offline-fixture";
    profile.parameterVersion = "offline-fixture-v1";
    profile.detectorVersion = "deterministic-fixture-v1";
    return profile;
}

QJsonObject parameterProfileJson(const cigvision::ProductParameterProfile& profile)
{
    QJsonObject value;
    value.insert(QStringLiteral("schemaVersion"),
        QString::fromUtf8(profile.schemaVersion.c_str()));
    value.insert(QStringLiteral("kind"), QString::fromLatin1(
        cigvision::productParameterProfileKindName(profile.kind)));
    value.insert(QStringLiteral("profileId"), QString::fromUtf8(profile.profileId.c_str()));
    value.insert(QStringLiteral("parameterVersion"),
        QString::fromUtf8(profile.parameterVersion.c_str()));
    value.insert(QStringLiteral("detectorVersion"),
        QString::fromUtf8(profile.detectorVersion.c_str()));
    value.insert(QStringLiteral("modelSha256"),
        QString::fromLatin1(profile.modelSha256.c_str()));
    value.insert(QStringLiteral("inputTensorName"),
        QString::fromUtf8(profile.inputTensorName.c_str()));
    value.insert(QStringLiteral("outputTensorName"),
        QString::fromUtf8(profile.outputTensorName.c_str()));
    value.insert(QStringLiteral("inputWidth"), static_cast<qint64>(profile.inputWidth));
    value.insert(QStringLiteral("inputHeight"), static_cast<qint64>(profile.inputHeight));
    value.insert(QStringLiteral("preprocessMode"),
        QString::fromUtf8(profile.preprocessMode.c_str()));
    value.insert(QStringLiteral("sha256"), QString::fromLatin1(profile.sha256().c_str()));
    QJsonArray classes;
    for (const cigvision::ProductClassParameter& rule : profile.classes) {
        QJsonObject item;
        item.insert(QStringLiteral("classId"), rule.classId);
        item.insert(QStringLiteral("className"), QString::fromUtf8(rule.className.c_str()));
        item.insert(QStringLiteral("confidenceThreshold"), rule.confidenceThreshold);
        item.insert(QStringLiteral("enabled"), rule.enabled);
        classes.append(item);
    }
    value.insert(QStringLiteral("classes"), classes);
    return value;
}

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

CigVision::CigVision(QWidget *parent, bool offlineOnly)
    : QWidget(parent), offlineOnlyMode(offlineOnly)
{
    ui.setupUi(this);
    //初始化参数类
    //CigVisionParams params;
    //运行界面
    initRunView();
    if (!offlineOnlyMode) {
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
    } else {
        ui.btn_run->setEnabled(false);
        ui.btn_run->setToolTip(QStringLiteral("离线模式不初始化相机或 IO"));
        qDebug() << "offline-only mode: camera and IO initialization skipped";
    }
    //参数设置界面
    initParaView();
    initReviewView();
    initInsightsViews();
    ui.label_brandName->setText(params.getCurrentBrand());
    //统计查询界面

    //系统设置 界面

    //ui.stackedWidget->setCurrentIndex(1);//调试用

    // 连接系统参数窗口退出信号
    connect(&params, &CigVisionParams::systemParaWidgetQuit, this, &CigVision::onSystemParaWidgetQuit);
    connect(&params, &CigVisionParams::selectNewBrand, this, &CigVision::onBrandComboBoxChanged);
    ui.btn_switch->setText(QStringLiteral("离线检测"));
    ui.btn_switch->setToolTip(QStringLiteral("使用链路测试检测器，不连接相机、IO 或 TensorRT"));
    connect(ui.btn_switch, &QToolButton::clicked, this, &CigVision::onOfflineButtonClicked);
}

CigVision::~CigVision()
{
    stopOfflineInspection();
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
    offlineImageLabel = v1_zu1_label;
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
    offlineDefectLabel = v1_bug_label;
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
    offlineStatusLabel = new QLabel(QStringLiteral("离线链路测试：未运行"), run_page);
    offlineStatusLabel->setStyleSheet("color: white; font-size: 16px;");
    offlineStatusLabel->setWordWrap(true);
    v1_layout2->addWidget(offlineStatusLabel);

    //运行图像——右侧统计表
    QHBoxLayout* v1_hlayout2 = new QHBoxLayout(run_page);
    QTableView* v1_zu1TreeTable = new QTableView(run_page);
    QTableView* v1_zu2TreeTable = new QTableView(run_page);

    QStringList NG_class_stringlist;
    NG_class_stringlist << QStringLiteral("定位失败(外形缺陷)") << QStringLiteral("烟棒缺陷") << QStringLiteral("滤嘴缺陷") << QStringLiteral("拼接缺陷") << QStringLiteral("搭口错牙(DL)") << QStringLiteral("滤嘴破损(DL)") << QStringLiteral("滤嘴皱褶(DL)") << QStringLiteral("缺滤嘴(DL)") << QStringLiteral("烟棒破损(DL)") << QStringLiteral("烟棒脏污(DL)");

    // 创建QStandardItemModel对象并设置行数和列数
    QStandardItemModel* model1 = new QStandardItemModel(NG_class_stringlist.size(), 2, run_page);
    offlineStatsModel = model1;
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
        model1->setItem(i, 1, new QStandardItem(QStringLiteral("0")));
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

void CigVision::initReviewView()
{
    QWidget* reviewPage = new QWidget(ui.stackedWidget);
    QVBoxLayout* layout = new QVBoxLayout(reviewPage);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->setSpacing(12);

    QLabel* title = new QLabel(QStringLiteral("缺陷结果复核"), reviewPage);
    title->setStyleSheet(QStringLiteral(
        "color: white; font-size: 24px; font-weight: bold;"));
    layout->addWidget(title);

    reviewStatusLabel = new QLabel(QStringLiteral("尚无本地检测结果"), reviewPage);
    reviewStatusLabel->setStyleSheet(QStringLiteral(
        "color: #C8D1DA; font-size: 15px;"));
    layout->addWidget(reviewStatusLabel);

    reviewTable = new QTableView(reviewPage);
    reviewModel = new QStandardItemModel(0, 8, reviewPage);
    reviewModel->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("帧")
        << QStringLiteral("工位/相机")
        << QStringLiteral("烟支")
        << QStringLiteral("判定")
        << QStringLiteral("缺陷")
        << QStringLiteral("置信度")
        << QStringLiteral("复核")
        << QStringLiteral("复核人"));
    reviewTable->setModel(reviewModel);
    reviewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    reviewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    reviewTable->setSelectionMode(QAbstractItemView::SingleSelection);
    reviewTable->setAlternatingRowColors(true);
    reviewTable->verticalHeader()->setVisible(false);
    reviewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    reviewTable->setStyleSheet(QStringLiteral(
        "QTableView { color: white; background: #333333; alternate-background-color: #414141;"
        " gridline-color: #666666; border: 1px solid #777777; font-size: 14px; }"
        "QTableView::item:selected { background: #1769AA; }"
        "QHeaderView::section { color: white; background: #252525; padding: 8px;"
        " border: 1px solid #555555; font-weight: bold; }"));
    layout->addWidget(reviewTable, 1);

    QHBoxLayout* editor = new QHBoxLayout();
    QLabel* operatorLabel = new QLabel(QStringLiteral("复核人"), reviewPage);
    QLabel* noteLabel = new QLabel(QStringLiteral("备注"), reviewPage);
    operatorLabel->setStyleSheet(QStringLiteral("color: white; font-size: 14px;"));
    noteLabel->setStyleSheet(QStringLiteral("color: white; font-size: 14px;"));
    reviewOperatorEdit = new QLineEdit(reviewPage);
    reviewOperatorEdit->setPlaceholderText(QStringLiteral("必填"));
    reviewOperatorEdit->setMaximumWidth(180);
    reviewNoteEdit = new QLineEdit(reviewPage);
    reviewNoteEdit->setPlaceholderText(QStringLiteral("可填写判断依据或修正说明"));
    editor->addWidget(operatorLabel);
    editor->addWidget(reviewOperatorEdit);
    editor->addWidget(noteLabel);
    editor->addWidget(reviewNoteEdit, 1);

    QPushButton* confirmButton = new QPushButton(QStringLiteral("确认结果"), reviewPage);
    QPushButton* correctButton = new QPushButton(QStringLiteral("标记需修正"), reviewPage);
    QPushButton* dismissButton = new QPushButton(QStringLiteral("标记误报"), reviewPage);
    const QString buttonStyle = QStringLiteral(
        "QPushButton { color: white; background: #1769AA; border: 1px solid #4A90C2;"
        " border-radius: 3px; padding: 8px 14px; font-size: 14px; }"
        "QPushButton:hover { background: #2185D0; }"
        "QPushButton:pressed { background: #0F4F82; }");
    confirmButton->setStyleSheet(buttonStyle);
    correctButton->setStyleSheet(buttonStyle);
    dismissButton->setStyleSheet(buttonStyle);
    editor->addWidget(confirmButton);
    editor->addWidget(correctButton);
    editor->addWidget(dismissButton);
    layout->addLayout(editor);

    connect(confirmButton, &QPushButton::clicked, this, [this] {
        applySelectedReview(cigvision::ProductReviewOutcome::Confirmed);
    });
    connect(correctButton, &QPushButton::clicked, this, [this] {
        applySelectedReview(cigvision::ProductReviewOutcome::Corrected);
    });
    connect(dismissButton, &QPushButton::clicked, this, [this] {
        applySelectedReview(cigvision::ProductReviewOutcome::Dismissed);
    });

    ui.stackedWidget->addWidget(reviewPage);
}

void CigVision::refreshReviewView()
{
    if (reviewModel == nullptr || reviewStatusLabel == nullptr) {
        return;
    }
    const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
    reviewModel->removeRows(0, reviewModel->rowCount());
    int unreviewedRows = 0;
    for (const cigvision::ProductRecentResult& recent : snapshot.recentResults) {
        if (recent.frame.decision == cigvision::InspectionDecision::Ok) {
            continue;
        }
        QList<QStandardItem*> row;
        QStandardItem* frameItem = new QStandardItem(QString::number(
            static_cast<qulonglong>(recent.frame.frameId)));
        frameItem->setData(static_cast<qulonglong>(recent.frame.frameId), Qt::UserRole);
        row << frameItem;
        row << new QStandardItem(QStringLiteral("%1/%2")
            .arg(QString::fromUtf8(recent.frame.stationId.c_str()))
            .arg(QString::fromUtf8(recent.frame.cameraId.c_str())));
        row << new QStandardItem(QString::number(recent.frame.cigaretteNumber));
        row << new QStandardItem(recent.frame.decision == cigvision::InspectionDecision::Ng
            ? QStringLiteral("NG") : QStringLiteral("错误"));

        QStringList defectNames;
        QStringList confidences;
        for (const cigvision::ProductDefectSummary& defect : recent.frame.defects) {
            defectNames << QString::fromUtf8(defect.className.c_str());
            confidences << QString::number(defect.confidence, 'f', 3);
        }
        row << new QStandardItem(defectNames.isEmpty()
            ? QString::fromUtf8(recent.frame.errorCode.c_str())
            : defectNames.join(QStringLiteral(", ")));
        row << new QStandardItem(confidences.join(QStringLiteral(", ")));

        QString reviewText = QStringLiteral("未复核");
        switch (recent.review.outcome) {
        case cigvision::ProductReviewOutcome::Confirmed:
            reviewText = QStringLiteral("已确认");
            break;
        case cigvision::ProductReviewOutcome::Corrected:
            reviewText = QStringLiteral("需修正");
            break;
        case cigvision::ProductReviewOutcome::Dismissed:
            reviewText = QStringLiteral("误报");
            break;
        default:
            ++unreviewedRows;
            break;
        }
        row << new QStandardItem(reviewText);
        row << new QStandardItem(QString::fromUtf8(recent.review.reviewer.c_str()));
        reviewModel->appendRow(row);
    }
    reviewStatusLabel->setText(
        QStringLiteral("运行 %1 | 保留 %2 条，待复核 %3 条 | 已复核 %4 条")
            .arg(QString::fromUtf8(snapshot.configuration.runId.c_str()))
            .arg(static_cast<qulonglong>(snapshot.recentResults.size()))
            .arg(unreviewedRows)
            .arg(static_cast<qulonglong>(snapshot.statistics.reviewed)));
}

void CigVision::applySelectedReview(cigvision::ProductReviewOutcome outcome)
{
    if (reviewTable == nullptr || reviewModel == nullptr ||
        reviewTable->selectionModel() == nullptr) {
        return;
    }
    const QModelIndexList selected = reviewTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        reviewStatusLabel->setText(QStringLiteral("请先选择一条结果"));
        return;
    }
    const QModelIndex frameIndex = reviewModel->index(selected.first().row(), 0);
    const std::uint64_t frameId = frameIndex.data(Qt::UserRole).toULongLong();
    const std::string reviewer = utf8String(reviewOperatorEdit->text().trimmed());
    const std::string note = utf8String(reviewNoteEdit->text().trimmed());
    std::string errorMessage;
    if (!productRuntimeState.reviewResult(frameId, outcome, reviewer, note,
            currentTimestampMicros(), errorMessage)) {
        reviewStatusLabel->setText(QStringLiteral("复核保存失败：%1")
            .arg(QString::fromUtf8(errorMessage.c_str())));
        return;
    }
    QString persistenceError;
    if (!persistProductState(persistenceError)) {
        reviewStatusLabel->setText(QStringLiteral("复核已更新，但落盘失败：%1")
            .arg(persistenceError));
        return;
    }
    refreshReviewView();
}

void CigVision::initInsightsViews()
{
    const QString tableStyle = QStringLiteral(
        "QTableView { color: white; background: #333333; alternate-background-color: #414141;"
        " gridline-color: #666666; border: 1px solid #777777; font-size: 14px; }"
        "QHeaderView::section { color: white; background: #252525; padding: 8px;"
        " border: 1px solid #555555; font-weight: bold; }");

    QWidget* statisticsPage = new QWidget(ui.stackedWidget);
    QVBoxLayout* statisticsLayout = new QVBoxLayout(statisticsPage);
    statisticsLayout->setContentsMargins(24, 18, 24, 18);
    statisticsLayout->setSpacing(12);
    QLabel* statisticsTitle = new QLabel(QStringLiteral("检测统计"), statisticsPage);
    statisticsTitle->setStyleSheet(QStringLiteral(
        "color: white; font-size: 24px; font-weight: bold;"));
    statisticsLayout->addWidget(statisticsTitle);
    statisticsIdentityLabel = new QLabel(statisticsPage);
    statisticsSummaryLabel = new QLabel(statisticsPage);
    statisticsIdentityLabel->setStyleSheet(QStringLiteral(
        "color: #9CCBFF; font-size: 15px;"));
    statisticsSummaryLabel->setStyleSheet(QStringLiteral(
        "color: white; font-size: 18px; font-weight: bold; padding: 8px;"));
    statisticsLayout->addWidget(statisticsIdentityLabel);
    statisticsLayout->addWidget(statisticsSummaryLabel);

    QHBoxLayout* statisticsTables = new QHBoxLayout();
    QTableView* cameraTable = new QTableView(statisticsPage);
    statisticsCameraModel = new QStandardItemModel(0, 5, statisticsPage);
    statisticsCameraModel->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("工位/相机") << QStringLiteral("处理")
        << QStringLiteral("OK") << QStringLiteral("NG") << QStringLiteral("错误"));
    cameraTable->setModel(statisticsCameraModel);
    cameraTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cameraTable->setAlternatingRowColors(true);
    cameraTable->verticalHeader()->setVisible(false);
    cameraTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cameraTable->setStyleSheet(tableStyle);
    statisticsTables->addWidget(cameraTable, 3);

    QTableView* classTable = new QTableView(statisticsPage);
    statisticsClassModel = new QStandardItemModel(0, 2, statisticsPage);
    statisticsClassModel->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("缺陷类别 ID") << QStringLiteral("数量"));
    classTable->setModel(statisticsClassModel);
    classTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    classTable->setAlternatingRowColors(true);
    classTable->verticalHeader()->setVisible(false);
    classTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    classTable->setStyleSheet(tableStyle);
    statisticsTables->addWidget(classTable, 2);
    statisticsLayout->addLayout(statisticsTables, 1);
    ui.stackedWidget->addWidget(statisticsPage);

    QWidget* diagnosticsPage = new QWidget(ui.stackedWidget);
    QVBoxLayout* diagnosticsLayout = new QVBoxLayout(diagnosticsPage);
    diagnosticsLayout->setContentsMargins(24, 18, 24, 18);
    diagnosticsLayout->setSpacing(12);
    QLabel* diagnosticsTitle = new QLabel(QStringLiteral("运行日志与诊断"), diagnosticsPage);
    diagnosticsTitle->setStyleSheet(QStringLiteral(
        "color: white; font-size: 24px; font-weight: bold;"));
    diagnosticsLayout->addWidget(diagnosticsTitle);
    diagnosticsSummaryLabel = new QLabel(QStringLiteral("尚无诊断事件"), diagnosticsPage);
    diagnosticsSummaryLabel->setStyleSheet(QStringLiteral(
        "color: #C8D1DA; font-size: 15px;"));
    diagnosticsLayout->addWidget(diagnosticsSummaryLabel);
    QTableView* diagnosticsTable = new QTableView(diagnosticsPage);
    diagnosticsModel = new QStandardItemModel(0, 6, diagnosticsPage);
    diagnosticsModel->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("时间(µs)") << QStringLiteral("级别")
        << QStringLiteral("组件") << QStringLiteral("代码")
        << QStringLiteral("帧") << QStringLiteral("消息"));
    diagnosticsTable->setModel(diagnosticsModel);
    diagnosticsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    diagnosticsTable->setAlternatingRowColors(true);
    diagnosticsTable->verticalHeader()->setVisible(false);
    diagnosticsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    diagnosticsTable->setStyleSheet(tableStyle);
    diagnosticsLayout->addWidget(diagnosticsTable, 1);
    ui.stackedWidget->addWidget(diagnosticsPage);
}

void CigVision::refreshStatisticsView()
{
    if (statisticsSummaryLabel == nullptr || statisticsIdentityLabel == nullptr ||
        statisticsCameraModel == nullptr || statisticsClassModel == nullptr) {
        return;
    }
    const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
    const cigvision::ProductRuntimeStatistics& totals = snapshot.statistics;
    statisticsIdentityLabel->setText(
        QStringLiteral("运行 %1 | 品牌 %2 | 模式 %3 | 检测器 %4 | 参数 %5 | 应用哈希 %6 | 状态 %7")
            .arg(QString::fromUtf8(snapshot.configuration.runId.c_str()))
            .arg(QString::fromUtf8(snapshot.configuration.brandName.c_str()))
            .arg(QString::fromLatin1(cigvision::productRunModeName(
                snapshot.configuration.mode)))
            .arg(QString::fromUtf8(snapshot.configuration.detectorVersion.c_str()))
            .arg(QString::fromUtf8(snapshot.configuration.parameterVersion.c_str()))
            .arg(QString::fromLatin1(
                snapshot.configuration.appliedParameters.sha256().substr(0, 12).c_str()))
            .arg(QString::fromLatin1(cigvision::productRuntimeStatusName(snapshot.status))));
    statisticsSummaryLabel->setText(
        QStringLiteral("处理 %1 | OK %2 | NG %3 | 错误 %4 | 已复核 %5 | 最大耗时 %6 µs | 最大队列 %7")
            .arg(static_cast<qulonglong>(totals.processed))
            .arg(static_cast<qulonglong>(totals.ok))
            .arg(static_cast<qulonglong>(totals.ng))
            .arg(static_cast<qulonglong>(totals.error))
            .arg(static_cast<qulonglong>(totals.reviewed))
            .arg(static_cast<qulonglong>(totals.maximumElapsedMicros))
            .arg(static_cast<qulonglong>(totals.maximumQueueDepth)));

    statisticsCameraModel->removeRows(0, statisticsCameraModel->rowCount());
    for (const auto& entry : totals.cameras) {
        QList<QStandardItem*> row;
        row << new QStandardItem(QStringLiteral("%1/%2")
                .arg(QString::fromUtf8(entry.first.stationId.c_str()))
                .arg(QString::fromUtf8(entry.first.cameraId.c_str())))
            << new QStandardItem(QString::number(
                static_cast<qulonglong>(entry.second.processed)))
            << new QStandardItem(QString::number(
                static_cast<qulonglong>(entry.second.ok)))
            << new QStandardItem(QString::number(
                static_cast<qulonglong>(entry.second.ng)))
            << new QStandardItem(QString::number(
                static_cast<qulonglong>(entry.second.error)));
        statisticsCameraModel->appendRow(row);
    }
    statisticsClassModel->removeRows(0, statisticsClassModel->rowCount());
    for (const auto& entry : totals.defectsByClass) {
        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(entry.first))
            << new QStandardItem(QString::number(
                static_cast<qulonglong>(entry.second)));
        statisticsClassModel->appendRow(row);
    }
}

void CigVision::refreshDiagnosticsView()
{
    if (diagnosticsSummaryLabel == nullptr || diagnosticsModel == nullptr) {
        return;
    }
    const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
    diagnosticsModel->removeRows(0, diagnosticsModel->rowCount());
    for (const cigvision::ProductDiagnosticEvent& event : snapshot.diagnostics) {
        QString severity = QStringLiteral("信息");
        if (event.severity == cigvision::ProductDiagnosticSeverity::Warning) {
            severity = QStringLiteral("警告");
        } else if (event.severity == cigvision::ProductDiagnosticSeverity::Error) {
            severity = QStringLiteral("错误");
        }
        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(
                static_cast<qlonglong>(event.occurredAtMicros)))
            << new QStandardItem(severity)
            << new QStandardItem(QString::fromUtf8(event.component.c_str()))
            << new QStandardItem(QString::fromUtf8(event.code.c_str()))
            << new QStandardItem(event.frameId == 0 ? QStringLiteral("-") :
                QString::number(static_cast<qulonglong>(event.frameId)))
            << new QStandardItem(QString::fromUtf8(event.message.c_str()));
        diagnosticsModel->appendRow(row);
    }
    diagnosticsSummaryLabel->setText(
        QStringLiteral("运行 %1 | 状态 %2 | 保留诊断 %3 条（容量 %4）")
            .arg(QString::fromUtf8(snapshot.configuration.runId.c_str()))
            .arg(QString::fromLatin1(cigvision::productRuntimeStatusName(snapshot.status)))
            .arg(diagnosticsModel->rowCount())
            .arg(static_cast<qulonglong>(snapshot.configuration.diagnosticCapacity)));
}

bool CigVision::persistProductState(QString& errorMessage) const
{
    if (activeProductOutputDirectory.isEmpty()) {
        errorMessage = QStringLiteral("产品运行输出目录尚未建立");
        return false;
    }
    const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"),
        QStringLiteral("cigvision-product-session-v1"));
    root.insert(QStringLiteral("revision"), static_cast<qint64>(snapshot.revision));
    root.insert(QStringLiteral("status"), QString::fromLatin1(
        cigvision::productRuntimeStatusName(snapshot.status)));
    root.insert(QStringLiteral("startedAtMicros"),
        static_cast<qint64>(snapshot.startedAtMicros));
    root.insert(QStringLiteral("stoppedAtMicros"),
        static_cast<qint64>(snapshot.stoppedAtMicros));

    QJsonObject configuration;
    configuration.insert(QStringLiteral("runId"),
        QString::fromUtf8(snapshot.configuration.runId.c_str()));
    configuration.insert(QStringLiteral("brandName"),
        QString::fromUtf8(snapshot.configuration.brandName.c_str()));
    configuration.insert(QStringLiteral("mode"), QString::fromLatin1(
        cigvision::productRunModeName(snapshot.configuration.mode)));
    configuration.insert(QStringLiteral("detectorVersion"),
        QString::fromUtf8(snapshot.configuration.detectorVersion.c_str()));
    configuration.insert(QStringLiteral("parameterVersion"),
        QString::fromUtf8(snapshot.configuration.parameterVersion.c_str()));
    configuration.insert(QStringLiteral("modelSha256"),
        QString::fromLatin1(snapshot.configuration.modelSha256.c_str()));
    configuration.insert(QStringLiteral("queueCapacity"),
        static_cast<qint64>(snapshot.configuration.queueCapacity));
    configuration.insert(QStringLiteral("recentResultCapacity"),
        static_cast<qint64>(snapshot.configuration.recentResultCapacity));
    configuration.insert(QStringLiteral("diagnosticCapacity"),
        static_cast<qint64>(snapshot.configuration.diagnosticCapacity));
    configuration.insert(QStringLiteral("realIoEnabled"),
        snapshot.configuration.realIoEnabled);
    configuration.insert(QStringLiteral("configuredParametersApplied"),
        snapshot.configuration.configuredParametersApplied);
    configuration.insert(QStringLiteral("configuredParameters"),
        parameterProfileJson(snapshot.configuration.configuredParameters));
    configuration.insert(QStringLiteral("appliedParameters"),
        parameterProfileJson(snapshot.configuration.appliedParameters));
    root.insert(QStringLiteral("configuration"), configuration);

    const cigvision::ProductRuntimeStatistics& totals = snapshot.statistics;
    QJsonObject statistics;
    statistics.insert(QStringLiteral("processed"), static_cast<qint64>(totals.processed));
    statistics.insert(QStringLiteral("ok"), static_cast<qint64>(totals.ok));
    statistics.insert(QStringLiteral("ng"), static_cast<qint64>(totals.ng));
    statistics.insert(QStringLiteral("error"), static_cast<qint64>(totals.error));
    statistics.insert(QStringLiteral("reviewed"), static_cast<qint64>(totals.reviewed));
    statistics.insert(QStringLiteral("confirmed"), static_cast<qint64>(totals.confirmed));
    statistics.insert(QStringLiteral("corrected"), static_cast<qint64>(totals.corrected));
    statistics.insert(QStringLiteral("dismissed"), static_cast<qint64>(totals.dismissed));
    statistics.insert(QStringLiteral("totalElapsedMicros"),
        static_cast<qint64>(totals.totalElapsedMicros));
    statistics.insert(QStringLiteral("maximumElapsedMicros"),
        static_cast<qint64>(totals.maximumElapsedMicros));
    statistics.insert(QStringLiteral("maximumQueueDepth"),
        static_cast<qint64>(totals.maximumQueueDepth));
    QJsonObject classCounts;
    for (const auto& entry : totals.defectsByClass) {
        classCounts.insert(QString::number(entry.first), static_cast<qint64>(entry.second));
    }
    statistics.insert(QStringLiteral("defectsByClass"), classCounts);
    QJsonArray cameras;
    for (const auto& entry : totals.cameras) {
        QJsonObject camera;
        camera.insert(QStringLiteral("stationId"),
            QString::fromUtf8(entry.first.stationId.c_str()));
        camera.insert(QStringLiteral("cameraId"),
            QString::fromUtf8(entry.first.cameraId.c_str()));
        camera.insert(QStringLiteral("processed"),
            static_cast<qint64>(entry.second.processed));
        camera.insert(QStringLiteral("ok"), static_cast<qint64>(entry.second.ok));
        camera.insert(QStringLiteral("ng"), static_cast<qint64>(entry.second.ng));
        camera.insert(QStringLiteral("error"), static_cast<qint64>(entry.second.error));
        cameras.append(camera);
    }
    statistics.insert(QStringLiteral("cameras"), cameras);
    root.insert(QStringLiteral("statistics"), statistics);

    QJsonArray recentResults;
    for (const cigvision::ProductRecentResult& recent : snapshot.recentResults) {
        QJsonObject item;
        item.insert(QStringLiteral("frameId"),
            static_cast<qint64>(recent.frame.frameId));
        item.insert(QStringLiteral("stationId"),
            QString::fromUtf8(recent.frame.stationId.c_str()));
        item.insert(QStringLiteral("cameraId"),
            QString::fromUtf8(recent.frame.cameraId.c_str()));
        item.insert(QStringLiteral("cigaretteNumber"),
            static_cast<qint64>(recent.frame.cigaretteNumber));
        item.insert(QStringLiteral("capturedAtMicros"),
            static_cast<qint64>(recent.frame.capturedAtMicros));
        item.insert(QStringLiteral("completedAtMicros"),
            static_cast<qint64>(recent.frame.completedAtMicros));
        item.insert(QStringLiteral("decision"), decisionDisplayName(recent.frame.decision));
        item.insert(QStringLiteral("elapsedMicros"),
            static_cast<qint64>(recent.frame.elapsedMicros));
        item.insert(QStringLiteral("parameterVersion"),
            QString::fromUtf8(recent.frame.parameterVersion.c_str()));
        item.insert(QStringLiteral("parameterSha256"),
            QString::fromLatin1(recent.frame.parameterSha256.c_str()));
        item.insert(QStringLiteral("errorCode"),
            QString::fromUtf8(recent.frame.errorCode.c_str()));
        item.insert(QStringLiteral("errorMessage"),
            QString::fromUtf8(recent.frame.errorMessage.c_str()));
        QJsonArray defects;
        for (const cigvision::ProductDefectSummary& defect : recent.frame.defects) {
            QJsonObject defectValue;
            defectValue.insert(QStringLiteral("classId"), defect.classId);
            defectValue.insert(QStringLiteral("className"),
                QString::fromUtf8(defect.className.c_str()));
            defectValue.insert(QStringLiteral("confidence"), defect.confidence);
            defects.append(defectValue);
        }
        item.insert(QStringLiteral("defects"), defects);
        QJsonObject review;
        review.insert(QStringLiteral("outcome"), reviewOutcomeName(recent.review.outcome));
        review.insert(QStringLiteral("reviewer"),
            QString::fromUtf8(recent.review.reviewer.c_str()));
        review.insert(QStringLiteral("note"),
            QString::fromUtf8(recent.review.note.c_str()));
        review.insert(QStringLiteral("reviewedAtMicros"),
            static_cast<qint64>(recent.review.reviewedAtMicros));
        item.insert(QStringLiteral("review"), review);
        recentResults.append(item);
    }
    root.insert(QStringLiteral("recentResults"), recentResults);

    QJsonArray diagnostics;
    for (const cigvision::ProductDiagnosticEvent& event : snapshot.diagnostics) {
        QJsonObject item;
        item.insert(QStringLiteral("occurredAtMicros"),
            static_cast<qint64>(event.occurredAtMicros));
        item.insert(QStringLiteral("severity"), diagnosticSeverityName(event.severity));
        item.insert(QStringLiteral("component"),
            QString::fromUtf8(event.component.c_str()));
        item.insert(QStringLiteral("code"), QString::fromUtf8(event.code.c_str()));
        item.insert(QStringLiteral("message"), QString::fromUtf8(event.message.c_str()));
        item.insert(QStringLiteral("frameId"), static_cast<qint64>(event.frameId));
        diagnostics.append(item);
    }
    root.insert(QStringLiteral("diagnostics"), diagnostics);

    QSaveFile file(QDir(activeProductOutputDirectory).filePath(
        QStringLiteral("product-session.json")));
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 ||
        !file.commit()) {
        errorMessage = file.errorString().isEmpty()
            ? QStringLiteral("product session write failed") : file.errorString();
        return false;
    }
    errorMessage.clear();
    return true;
}

void CigVision::on_btn_run_clicked()
{
    QMutexLocker runtimeLock(&runtimeMutex);
    qDebug() << "run clicked";

    if (!machineState.systemRun.load())
    {
        const cigvision::ProductRuntimeStatus productStatus =
            productRuntimeState.snapshot().status;
        if (productStatus == cigvision::ProductRuntimeStatus::Running ||
            productStatus == cigvision::ProductRuntimeStatus::Stopping) {
            offlineStatusLabel->setText(QStringLiteral(
                "请先停止离线任务，再启动在线运行"));
            return;
        }
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
        const bool camerasStopped = stopCameras();
        cameraLifecycleFault = !camerasStopped;
        detachCameraCallbacks();
        waitForCameraCallbacks();
        stopIOReading();
        clearFrameQueues();
        if (!camerasStopped) {
            offlineStatusLabel->setText(QStringLiteral(
                "相机停止失败，运行已锁定且禁止退出"));
        }
    }
    btnColorUpdate();
}
void CigVision::on_btn_edit_clicked()
{
    qDebug() << "edit clicked";
    const cigvision::ProductRuntimeStatus status = productRuntimeState.snapshot().status;
    if (status == cigvision::ProductRuntimeStatus::Running ||
        status == cigvision::ProductRuntimeStatus::Stopping) {
        offlineStatusLabel->setText(QStringLiteral("运行中参数已冻结，停止后再编辑"));
        return;
    }
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
    if (ui.stackedWidget->currentIndex() != current_stackedwidget::logTxt)
    {
        refreshDiagnosticsView();
        ui.stackedWidget->setCurrentIndex(current_stackedwidget::logTxt);
    }
    else
    {
        ui.stackedWidget->setCurrentIndex(current_stackedwidget::run);
    }
    btnColorUpdate();
}
void CigVision::on_btn_alarm_clicked()
{
    qDebug() << "alarm clicked";
}
void CigVision::on_btn_change_clicked()
{
    qDebug() << "change_brand clicked";
    const cigvision::ProductRuntimeStatus status = productRuntimeState.snapshot().status;
    if (status == cigvision::ProductRuntimeStatus::Running ||
        status == cigvision::ProductRuntimeStatus::Stopping) {
        offlineStatusLabel->setText(QStringLiteral("运行中品牌已冻结，停止后再切换"));
        return;
    }
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
    close();
}

void CigVision::closeEvent(QCloseEvent* event)
{
    if (offlineWorker != nullptr) {
        closeWhenOfflineStops = true;
        std::string stateError;
        const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
        if (snapshot.status == cigvision::ProductRuntimeStatus::Running) {
            (void)productRuntimeState.requestStop(currentTimestampMicros(), stateError);
        }
        offlineWorker->requestStop();
        ui.btn_quit->setEnabled(false);
        ui.btn_switch->setEnabled(false);
        offlineStatusLabel->setText(QStringLiteral("正在安全停止离线任务后退出"));
        event->ignore();
        return;
    }
    if (machineState.systemRun.load()) {
        on_btn_run_clicked();
    }
    if (machineState.systemRun.load() || cameraLifecycleFault) {
        offlineStatusLabel->setText(QStringLiteral(
            "在线任务未能安全停止，已取消退出"));
        event->ignore();
        return;
    }
    if (!activeProductOutputDirectory.isEmpty()) {
        QString persistenceError;
        if (!persistProductState(persistenceError)) {
            offlineStatusLabel->setText(QStringLiteral(
                "最终会话证据落盘失败，已取消退出：%1").arg(persistenceError));
            ui.btn_quit->setEnabled(true);
            event->ignore();
            return;
        }
    }
    event->accept();
}
void CigVision::on_btn_system_clicked()
{
    qDebug() << "system clicked";
    const cigvision::ProductRuntimeStatus status = productRuntimeState.snapshot().status;
    if (status == cigvision::ProductRuntimeStatus::Running ||
        status == cigvision::ProductRuntimeStatus::Stopping) {
        offlineStatusLabel->setText(QStringLiteral("运行中系统配置已冻结，停止后再修改"));
        return;
    }
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
    if (ui.stackedWidget->currentIndex() != current_stackedwidget::count)
    {
        refreshStatisticsView();
        ui.stackedWidget->setCurrentIndex(current_stackedwidget::count);
    }
    else
    {
        ui.stackedWidget->setCurrentIndex(current_stackedwidget::run);
    }
    btnColorUpdate();
}


void CigVision::on_btn_search_clicked()
{
    qDebug() << "search clicked";
    if (ui.stackedWidget->currentIndex() != current_stackedwidget::search)
    {
        refreshReviewView();
        ui.stackedWidget->setCurrentIndex(current_stackedwidget::search);
    }
    else
    {
        ui.stackedWidget->setCurrentIndex(current_stackedwidget::run);
    }
    btnColorUpdate();
}

void CigVision::onOfflineButtonClicked()
{
    if (offlineWorker != nullptr) {
        std::string stateError;
        (void)productRuntimeState.requestStop(currentTimestampMicros(), stateError);
        offlineWorker->requestStop();
        ui.btn_switch->setText(QStringLiteral("正在停止"));
        ui.btn_switch->setEnabled(false);
        return;
    }
    if (machineState.systemRun.load()) {
        offlineStatusLabel->setText(QStringLiteral("请先停止在线运行，再启动离线检测"));
        return;
    }

    const QStringList files = QFileDialog::getOpenFileNames(this,
        QStringLiteral("选择离线图片"), QString(),
        QStringLiteral("图像 (*.jpg *.jpeg *.png *.bmp)"));
    if (files.isEmpty()) {
        return;
    }
    QString outputRoot = QFileDialog::getExistingDirectory(this,
        QStringLiteral("选择结果保存目录"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    if (outputRoot.isEmpty()) {
        return;
    }
    outputRoot = QDir(outputRoot).filePath(QStringLiteral("CigVisionOffline-%1")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
    if (!QDir().mkpath(outputRoot)) {
        offlineStatusLabel->setText(QStringLiteral("无法创建结果目录"));
        return;
    }

    cigvision::ProductRunConfiguration runConfiguration;
    runConfiguration.runId = utf8String(QFileInfo(outputRoot).fileName());
    runConfiguration.brandName = utf8String(params.getCurrentBrand());
    runConfiguration.mode = cigvision::ProductRunMode::OfflineFixture;
    runConfiguration.detectorVersion = "deterministic-fixture-v1";
    runConfiguration.parameterVersion = "offline-fixture-v1";
    runConfiguration.queueCapacity = 4;
    runConfiguration.realIoEnabled = false;
    QString parameterProfileError;
    if (!buildLegacyDeepLearningProfile(params.getDeepLearningParams(),
            runConfiguration.configuredParameters, parameterProfileError)) {
        offlineStatusLabel->setText(QStringLiteral("参数快照校验失败：%1")
            .arg(parameterProfileError));
        return;
    }
    runConfiguration.appliedParameters = fixtureParameterProfile();
    runConfiguration.configuredParametersApplied = false;
    std::string stateError;
    if (!productRuntimeState.start(runConfiguration, currentTimestampMicros(), stateError)) {
        offlineStatusLabel->setText(QStringLiteral("运行看板初始化失败：%1")
            .arg(QString::fromUtf8(stateError.c_str())));
        return;
    }
    activeProductOutputDirectory = outputRoot;
    cigvision::ProductDiagnosticEvent startEvent;
    startEvent.occurredAtMicros = currentTimestampMicros();
    startEvent.component = "runtime";
    startEvent.code = "RUN_STARTED";
    startEvent.message = "offline fixture workflow started";
    (void)productRuntimeState.appendDiagnostic(startEvent, stateError);
    QString persistenceError;
    if (!persistProductState(persistenceError)) {
        (void)productRuntimeState.fail("PRODUCT_SESSION_WRITE_FAILED",
            utf8String(persistenceError), currentTimestampMicros(), stateError);
        offlineStatusLabel->setText(QStringLiteral("产品运行状态初始化落盘失败：%1")
            .arg(persistenceError));
        return;
    }

    offlineThread = new QThread(this);
    offlineWorker = new cigvision::OfflineInspectionWorker(
        files, outputRoot, runConfiguration.parameterVersion,
        runConfiguration.appliedParameters.sha256());
    offlineWorker->moveToThread(offlineThread);
    connect(offlineThread, &QThread::started, offlineWorker,
        &cigvision::OfflineInspectionWorker::run);
    connect(offlineWorker, &cigvision::OfflineInspectionWorker::frameProcessed, this,
        &CigVision::onOfflineFrameProcessed);
    connect(offlineWorker, &cigvision::OfflineInspectionWorker::finished, this,
        &CigVision::onOfflineFinished);
    connect(offlineWorker, &cigvision::OfflineInspectionWorker::finished,
        offlineThread, &QThread::quit, Qt::DirectConnection);
    connect(offlineThread, &QThread::finished, offlineWorker, &QObject::deleteLater);
    connect(offlineThread, &QThread::finished, this, [this] {
        offlineWorker = nullptr;
        QThread* finishedThread = offlineThread;
        offlineThread = nullptr;
        finishedThread->deleteLater();
        ui.btn_switch->setText(QStringLiteral("离线检测"));
        ui.btn_switch->setEnabled(true);
        if (closeWhenOfflineStops) {
            closeWhenOfflineStops = false;
            std::string stateError;
            const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
            if (snapshot.status == cigvision::ProductRuntimeStatus::Running ||
                snapshot.status == cigvision::ProductRuntimeStatus::Stopping) {
                (void)productRuntimeState.completeStop(
                    currentTimestampMicros(), stateError);
            }
            QString persistenceError;
            (void)persistProductState(persistenceError);
            ui.btn_quit->setEnabled(true);
            close();
            return;
        }
        ui.btn_quit->setEnabled(true);
    });
    ui.btn_switch->setText(QStringLiteral("停止离线"));
    offlineStatusLabel->setText(QStringLiteral("离线链路测试运行中（非生产算法）"));
    offlineThread->start();
}

void CigVision::onOfflineFrameProcessed(const QImage& image, const QString& resultText,
    const QVariantMap& statistics)
{
    const QPixmap preview = QPixmap::fromImage(image).scaled(offlineImageLabel->size(),
        Qt::KeepAspectRatio, Qt::SmoothTransformation);
    offlineImageLabel->setPixmap(preview);
    if (resultText == QStringLiteral("NG")) {
        offlineDefectLabel->setPixmap(QPixmap::fromImage(image).scaled(
            offlineDefectLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        offlineDefectLabel->clear();
    }
    cigvision::ProductFrameResult frameResult;
    frameResult.frameId = statistics.value(QStringLiteral("frameId")).toULongLong();
    frameResult.stationId = utf8String(statistics.value(QStringLiteral("stationId")).toString());
    frameResult.cameraId = utf8String(statistics.value(QStringLiteral("cameraId")).toString());
    frameResult.cigaretteNumber = statistics.value(
        QStringLiteral("cigaretteNumber")).toUInt();
    frameResult.capturedAtMicros = statistics.value(
        QStringLiteral("capturedAtMicros")).toLongLong();
    frameResult.completedAtMicros = statistics.value(
        QStringLiteral("completedAtMicros")).toLongLong();
    frameResult.elapsedMicros = statistics.value(
        QStringLiteral("elapsedMicros")).toULongLong();
    frameResult.parameterVersion = utf8String(statistics.value(
        QStringLiteral("parameterVersion")).toString());
    frameResult.parameterSha256 = utf8String(statistics.value(
        QStringLiteral("parameterSha256")).toString());
    frameResult.errorCode = utf8String(statistics.value(
        QStringLiteral("errorCode")).toString());
    frameResult.errorMessage = utf8String(statistics.value(
        QStringLiteral("errorMessage")).toString());
    if (resultText == QStringLiteral("OK")) {
        frameResult.decision = cigvision::InspectionDecision::Ok;
    } else if (resultText == QStringLiteral("NG")) {
        frameResult.decision = cigvision::InspectionDecision::Ng;
    } else {
        frameResult.decision = cigvision::InspectionDecision::Error;
    }
    const QVariantList defectValues = statistics.value(QStringLiteral("defects")).toList();
    for (const QVariant& defectValue : defectValues) {
        const QVariantMap item = defectValue.toMap();
        cigvision::ProductDefectSummary defect;
        defect.classId = item.value(QStringLiteral("classId")).toInt();
        defect.className = utf8String(item.value(QStringLiteral("className")).toString());
        defect.confidence = item.value(QStringLiteral("confidence")).toFloat();
        frameResult.defects.push_back(defect);
    }

    std::string stateError;
    if (!productRuntimeState.recordResult(frameResult, 0, stateError)) {
        offlineStatusLabel->setText(QStringLiteral("运行看板拒绝帧：%1")
            .arg(QString::fromUtf8(stateError.c_str())));
        return;
    }
    if (frameResult.decision == cigvision::InspectionDecision::Error) {
        cigvision::ProductDiagnosticEvent event;
        event.occurredAtMicros = frameResult.completedAtMicros;
        event.severity = cigvision::ProductDiagnosticSeverity::Error;
        event.component = "detector";
        event.code = frameResult.errorCode;
        event.message = frameResult.errorMessage.empty()
            ? "inspection result error" : frameResult.errorMessage;
        event.frameId = frameResult.frameId;
        (void)productRuntimeState.appendDiagnostic(event, stateError);
    }
    const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
    const cigvision::ProductRuntimeStatistics& totals = snapshot.statistics;
    ui.label_19->setText(QStringLiteral("合格：%1")
        .arg(static_cast<qulonglong>(totals.ok)));
    ui.label_20->setText(QStringLiteral("缺陷：%1 错误：%2")
        .arg(static_cast<qulonglong>(totals.ng))
        .arg(static_cast<qulonglong>(totals.error)));
    ui.label_14->setText(totals.processed == 0 ? QStringLiteral("0.0%") :
        QStringLiteral("%1%").arg(100.0 * static_cast<double>(totals.ng) /
            static_cast<double>(totals.processed), 0, 'f', 1));
    if (offlineStatsModel != nullptr) {
        for (int row = 0; row < offlineStatsModel->rowCount(); ++row) {
            const auto found = totals.defectsByClass.find(row);
            offlineStatsModel->setData(offlineStatsModel->index(row, 1),
                found == totals.defectsByClass.end() ? qulonglong(0) :
                    static_cast<qulonglong>(found->second));
        }
    }
    offlineStatusLabel->setText(
        QStringLiteral("%1 | %2/%3 | 烟支 %4 | 帧 %5：%6（链路测试检测器）")
            .arg(QString::fromLatin1(cigvision::productRunModeName(
                snapshot.configuration.mode)))
            .arg(QString::fromUtf8(frameResult.stationId.c_str()))
            .arg(QString::fromUtf8(frameResult.cameraId.c_str()))
            .arg(frameResult.cigaretteNumber)
            .arg(static_cast<qulonglong>(totals.processed))
            .arg(resultText));
}

void CigVision::onOfflineFinished(const QString& message, bool success)
{
    std::string stateError;
    const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
    cigvision::ProductDiagnosticEvent event;
    event.occurredAtMicros = currentTimestampMicros();
    event.severity = success ? cigvision::ProductDiagnosticSeverity::Information :
        (snapshot.status == cigvision::ProductRuntimeStatus::Stopping
            ? cigvision::ProductDiagnosticSeverity::Information
            : cigvision::ProductDiagnosticSeverity::Error);
    event.component = "runtime";
    event.code = success ? "RUN_COMPLETED" :
        (snapshot.status == cigvision::ProductRuntimeStatus::Stopping
            ? "RUN_STOPPED" : "RUN_FAILED");
    event.message = utf8String(message);
    (void)productRuntimeState.appendDiagnostic(event, stateError);
    if (success || snapshot.status == cigvision::ProductRuntimeStatus::Stopping) {
        (void)productRuntimeState.completeStop(currentTimestampMicros(), stateError);
    } else if (snapshot.status == cigvision::ProductRuntimeStatus::Running) {
        (void)productRuntimeState.fail("OFFLINE_WORKER_FAILED", utf8String(message),
            currentTimestampMicros(), stateError);
    }
    QString persistenceError;
    const bool persisted = persistProductState(persistenceError);
    offlineStatusLabel->setText(persisted ? message :
        QStringLiteral("%1；最终会话证据落盘失败：%2")
            .arg(message).arg(persistenceError));
    const bool stoppedByRequest =
        snapshot.status == cigvision::ProductRuntimeStatus::Stopping;
    offlineStatusLabel->setStyleSheet((success || stoppedByRequest)
        ? "color: #7CFC90; font-size: 16px;" : "color: #FF6B6B; font-size: 16px;");
}

void CigVision::stopOfflineInspection()
{
    if (offlineWorker != nullptr) {
        std::string stateError;
        const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
        if (snapshot.status == cigvision::ProductRuntimeStatus::Running) {
            (void)productRuntimeState.requestStop(currentTimestampMicros(), stateError);
        }
        offlineWorker->requestStop();
    }
    if (offlineThread != nullptr) {
        offlineThread->quit();
        offlineThread->wait();
    }
    std::string stateError;
    const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
    if (snapshot.status == cigvision::ProductRuntimeStatus::Running ||
        snapshot.status == cigvision::ProductRuntimeStatus::Stopping) {
        (void)productRuntimeState.completeStop(currentTimestampMicros(), stateError);
        QString persistenceError;
        if (!persistProductState(persistenceError)) {
            qCritical() << "final product session persistence failed:" << persistenceError;
        }
    }
}

void CigVision::btnColorUpdate()//按钮颜色更新
{
    ui.btn_edit->setIcon(QPixmap(QStringLiteral(":/CigVision/icons/use/edit.png")));
    ui.btn_search->setIcon(QPixmap(QStringLiteral(":/CigVision/icons/use/picture-filling.png")));
    ui.btn_change->setIcon(QPixmap(QStringLiteral(":/CigVision/icons/use/copy.png")));
    ui.btn_system->setIcon(QPixmap(QStringLiteral(":/CigVision/icons/use/settings.png")));
    //ui.b
    switch (ui.stackedWidget->currentIndex())
    {
    case current_stackedwidget::run:

        break;
    case current_stackedwidget::edit:
        ui.btn_edit->setIcon(QPixmap(QStringLiteral(":/CigVision/icons/use/edit.png")));
        break;
	case current_stackedwidget::search:
		ui.btn_search->setIcon(QPixmap(QStringLiteral(":/CigVision/icons/use/picture-filling.png")));
		break;
	case current_stackedwidget::change_brand:
		ui.btn_change->setIcon(QPixmap(QStringLiteral(":/CigVision/icons/use/copy.png")));
		break;
    case current_stackedwidget::system_set:
        ui.btn_system->setIcon(QPixmap(QStringLiteral(":/CigVision/icons/use/settings.png")));

    default:
        break;
    }
    if (machineState.systemRun.load())
    {
        ui.btn_run->setIcon(QPixmap(QStringLiteral(":/CigVision/icons/use/arrow-right-filling.png")));
    }
    else
    {
        ui.btn_run->setIcon(QPixmap(QStringLiteral(":/CigVision/icons/use/arrow-right-filling.png")));
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
    const cigvision::ProductRuntimeSnapshot snapshot = productRuntimeState.snapshot();
    if (snapshot.status == cigvision::ProductRuntimeStatus::Running ||
        snapshot.status == cigvision::ProductRuntimeStatus::Stopping) {
        ui.label_brandName->setText(QString::fromUtf8(
            snapshot.configuration.brandName.c_str()));
        offlineStatusLabel->setText(QStringLiteral(
            "运行中品牌已冻结；停止后再切换品牌"));
        return;
    }
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
