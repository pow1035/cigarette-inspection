#include "CigVision.h"
#include <QTableView>
#include <QStandardItemModel>
#include<qdebug.h>
#include <QTableWidget>
#include <QPushButton>
#include <QToolButton>
#include<qtabbar.h>
#include <QLineEdit>



bool addQQueue1_1(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, CigVision* pDlg)//图像需要是RGB
{
    if (pDlg->machineState.grayPicQueList1.size() > 10)
    {
        pDlg->machineState.grayPicQueList1.dequeue();
    }
    /*if (pDlg->machineState->rgbPicQueList1_1.size() > 10)
    {
        pDlg->machineState->rgbPicQueList1_1.dequeue();
    }*/
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
    picstruct1.uchar_pic_number = pDlg->machineState.nowPictureNumber1;
    picstruct1.uchar_pic_IO = pDlg->machineState.nowPicReadIO;
    if (pDlg->machineState.grayPicQueList1.size() != 0)
    {
        pDlg->machineState.grayPicQueList1.enqueue(picstruct1);
    }
    else//为了避免最后一张未插完就处理的BUG
    {
        pDlg->machineState.mutexGrayPicQueList1.lock();
        pDlg->machineState.grayPicQueList1.enqueue(picstruct1);
        pDlg->machineState.mutexGrayPicQueList1.unlock();
    }

    //内存释放
    //delete data;
    delete dataGray;
    return true;
}
bool addQQueue2_1(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, CigVision* pDlg)//图像需要是RGB
{
    if (pDlg->machineState.grayPicQueList2_1.size() > 10)
    {
        pDlg->machineState.grayPicQueList2_1.dequeue();
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
    picstruct1.uchar_pic_number = pDlg->machineState.nowPictureNumber2_1;
    picstruct1.uchar_pic_IO = pDlg->machineState.nowPicReadIO;
    //pDlg->picQueList2_1.enqueue(picstruct1);
    if (pDlg->machineState.grayPicQueList2_1.size() != 0)
    {
        pDlg->machineState.grayPicQueList2_1.enqueue(picstruct1);
    }
    else//为了避免最后一张未插完就处理的BUG
    {
        pDlg->machineState.mutexGrayPicQueList2_1.lock();
        pDlg->machineState.grayPicQueList2_1.enqueue(picstruct1);
        pDlg->machineState.mutexGrayPicQueList2_1.unlock();
    }
    //内存释放
    //delete data;
    delete dataGray;
    return true;
}
bool addQQueue2_2(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, CigVision* pDlg)//图像需要是RGB
{
    if (pDlg->machineState.grayPicQueList2_2.size() > 10)
    {
        pDlg->machineState.grayPicQueList2_2.dequeue();
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
    picstruct1.uchar_pic_number = pDlg->machineState.nowPictureNumber2_2;
    picstruct1.uchar_pic_IO = pDlg->machineState.nowPicReadIO;
    pDlg->machineState.grayPicQueList2_2.enqueue(picstruct1);
    //内存释放
    //delete data;
    delete dataGray;
    return true;
}
void __stdcall workProcedure1_1(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, void* pUser)
{

    CigVision* pCam = (CigVision*)pUser;//CigVison类

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

    CigVision* pCam = (CigVision*)pUser;//testQT类
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

    CigVision* pCam = (CigVision*)pUser;//testQT类
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
    };
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
{}

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
    qDebug() << "run clicked";
  
    if (!machineState.systemRun)
    {
        machineState.systemRun = true;
    }
    else
    {
        machineState.systemRun = false;
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
    if (machineState.systemRun)
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

bool CigVision::initCamera()
{
    int nRet = -1;
    void* m_handle = NULL;


    //组件相机对应初始化
    QString cameraTempS;
    //1组件
    machineState.cameraMatchMap.insert("1-1", params.cameraParams.camera1SerialNum);
    //2组件1相机(内)
    machineState.cameraMatchMap.insert("2-1", params.cameraParams.camera2OuterSerialNum);
    //2组件2相机(外)
    machineState.cameraMatchMap.insert("2-1", params.cameraParams.camera2InnerSerialNum);

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
    for (unsigned int i = 0, j = 0; j < machineState.m_stDevList.nDeviceNum; j++, i++)
    {
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
            i--;
            continue;
        }
        else
        {
            memcpy(deviceGUID, machineState.m_stDevList.pDeviceInfo[j]->SpecialInfo.stUsb3VInfo.chSerialNumber, INFO_MAX_BUFFER_SIZE);
            QString deviceGUIDString = QString::fromLocal8Bit((char*)deviceGUID);

            QMap<QString, QString>::const_iterator it = machineState.cameraMatchMap.constBegin();
            while (it != machineState.cameraMatchMap.constEnd())//配置map遍历，找到对应相机的设置
            {
                QString cameraKey;//当前设置相机的Key
                if (!QString::compare(it.value(), deviceGUIDString))//相机ID匹配到设置中ID
                {
                    cameraKey = it.key();//设置Key
                    if (cameraKey == "1-1")//1组件相机1
                    {
                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("TriggerMode", MV_TRIGGER_MODE_ON);//上升沿
                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_LINE0);//触发源Line0

                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("TriggerActivation", 0);//激活方式0:RisingEdge 1:FallingEdge2.LevelHigh3.LevelLow
                        nRet = machineState.m_pcMyCamera[i]->SetFloatValue("ExposureTime", 50.0);//曝光时间us
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("Width", 992);//画面宽度
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("Height", 300);//画面高度
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("OffsetX", 176);//画面水平偏移
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("OffsetY", 332);//画面垂直偏移，运动时400，手动盘340
                        nRet = machineState.m_pcMyCamera[i]->SetFloatValue("ExposureTime", 50);//曝光时间设置
                        nRet = machineState.m_pcMyCamera[i]->SetFloatValue("Gain", 0.0);//增益值设置
                        //nRet = m_pcMyCamera[i]->CommandExecute("TriggerSoftware");//单次软触发
                        //nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", SOFTWAREMODE);//软件触发
                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("PixelFormat", PixelType_Gvsp_RGB8_Packed);//0x02180014 RGB8Packed

                        nRet = machineState.m_pcMyCamera[i]->SetFloatValue("AcquisitionFrameRate", 200);//相机帧率
                        //unsigned int enValue = PixelType_Gvsp_RGB8_Packed;
                        //MV_CC_SetPixelFormat(m_pcMyCamera[i]->m_hDevHandle, enValue);//设置图片格式RGB
                        nRet = machineState.m_pcMyCamera[i]->RegisterImageCallBack(workProcedure1_1, this);
                    }
                    if (cameraKey == "2-1")//2组件相机1
                    {
                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("TriggerMode", MV_TRIGGER_MODE_ON);//上升沿
                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_LINE0);//触发源Line0

                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("TriggerActivation", 0);//激活方式0:RisingEdge 1:FallingEdge2.LevelHigh3.LevelLow
                        nRet = machineState.m_pcMyCamera[i]->SetFloatValue("ExposureTime", 100.0);//曝光时间us
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("Width", 1200);//画面宽度
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("Height", 600);//画面高度
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("OffsetX", 96);//画面水平偏移
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("OffsetY", 160);//画面垂直偏移，运动时400，手动盘340
                        nRet = machineState.m_pcMyCamera[i]->SetFloatValue("Gain", 0.0);//增益值设置
                        //nRet = m_pcMyCamera[i]->CommandExecute("TriggerSoftware");//单次软触发
                        //nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", SOFTWAREMODE);//软件触发
                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("PixelFormat", PixelType_Gvsp_RGB8_Packed);//0x02180014 RGB8Packed

                        nRet = machineState.m_pcMyCamera[i]->SetFloatValue("AcquisitionFrameRate", 200);//相机帧率
                        //unsigned int enValue = PixelType_Gvsp_RGB8_Packed;
                        //MV_CC_SetPixelFormat(m_pcMyCamera[i]->m_hDevHandle, enValue);//设置图片格式RGB
                        nRet = machineState.m_pcMyCamera[i]->RegisterImageCallBack(workProcedure2_1, this);
                    }
                    if (cameraKey == "2-2")//2组件相机1
                    {
                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("TriggerMode", MV_TRIGGER_MODE_ON);//上升沿
                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_LINE0);//触发源Line0

                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("TriggerActivation", 0);//激活方式0:RisingEdge 1:FallingEdge2.LevelHigh3.LevelLow
                        nRet = machineState.m_pcMyCamera[i]->SetFloatValue("ExposureTime", 100);//曝光时间us
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("Width", 1200);//画面宽度
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("Height", 600);//画面高度
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("OffsetX", 96);//画面水平偏移
                        nRet = machineState.m_pcMyCamera[i]->SetIntValue("OffsetY", 168);//画面垂直偏移
                        nRet = machineState.m_pcMyCamera[i]->SetFloatValue("Gain", 0.0);//增益值设置
                        //nRet = m_pcMyCamera[i]->CommandExecute("TriggerSoftware");//单次软触发
                        //nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", SOFTWAREMODE);//软件触发
                        nRet = machineState.m_pcMyCamera[i]->SetEnumValue("PixelFormat", PixelType_Gvsp_RGB8_Packed);//0x02180014 RGB8Packed

                        nRet = machineState.m_pcMyCamera[i]->SetFloatValue("AcquisitionFrameRate", 200);//相机帧率
                        //unsigned int enValue = PixelType_Gvsp_RGB8_Packed;
                        //MV_CC_SetPixelFormat(m_pcMyCamera[i]->m_hDevHandle, enValue);//设置图片格式RGB
                        nRet = machineState.m_pcMyCamera[i]->RegisterImageCallBack(workProcedure2_2, this);
                    }
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
                        tempString.append(QString::fromLocal8Bit(" 回调函数设置失败"));
                        qDebug() << tempString;
                        //ui.listWidget__information->addItem(tempString);
                        return false;
                    }

                    nRet = machineState.m_pcMyCamera[i]->StartGrabbing();
                    if (MV_OK != nRet)
                    {
                        QString tempString(QString::fromLocal8Bit("错误：第 "));
                        tempString.append(QString::number(i + 1));
                        tempString.append(QString::fromLocal8Bit(" 个相机启动抓拍失败"));
                        qDebug() << tempString;
                        //ui.listWidget__information->addItem(tempString);
                        return false;
                    }
                }
                it++;
            }
        }
    }
    return TRUE;
}

bool CigVision::initIOCard()
{
    pool.start(new readIOTask(this));//开始读IO线程

    return 1;
}