#pragma once

// Qt Widgets
#include <QWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolButton>

#include <windows.h>
#include <shellapi.h>

// Qt Core
#include <QString>
#include <QObject>
#include <QSettings>
#include <QDir>

// Custom Widgets
#include "myTabWidget.h"
#include "CusTabBar.h"
#include "HalconGraphicsView.h"
#include "paramStructs.h" 

#ifndef IMAGEPROCESS_EXPORTS
#define IMAGEPROCESS_API __declspec(dllexport)
// 错误代码定义
enum class ErrorCode {
	Success = 0,
	Failed = -1,
	InvalidImage = -2,
	// 可以添加更多错误码
};
// 当前处理步骤
enum class process_step {
	Loc_Top = 0,
    Loc_Side = 2,
	Loc_stick = 4,
	Loc_filter = 6,
	Loc_joint = 8,
    Stick_dark = 10,
    Filter_dark = 12,
    Filter_white = 14,
    Cig_Out = 16,
    Joint = 18,
    Deep = 20,
    
};
#endif

typedef ErrorCode(*FindLeftPoint)(HObject ho_image, int rect_leftX, int rect_leftY,
	int rect_rightX, int rect_rightY,int sigma_code, int line_strength_start,
	int line_strength_step_length, int& left_pointX, int& left_pointY);

typedef ErrorCode(*FindSidePoint)(HObject ho_image, int left_pointX, int left_pointY,
    int rects_1st_distance, int rects_distance, int rects_height, int rects_width, int sigma,
    int min_threshold, int model_int_rects3_array[3][3], bool set_new_model, int& side_pointY1, int& side_pointY2,
    int& side_width, int(&model_out_rects3_array)[3][3]);

// 定义参数类
class CigVisionParams : public QWidget
{
    Q_OBJECT

public:
    explicit CigVisionParams(QWidget* parent = nullptr);
    ~CigVisionParams();

signals:
    void systemParaWidgetQuit();
    void selectNewBrand();

private slots:
	void onProcessTableWidgetItemClicked(QTableWidgetItem* Item);
	void onOperatorTableWidgetItemClicked(QTableWidgetItem* Item);
	// 添加新的槽函数
	void onBrandComboBoxChanged(const QString& brandName);
	void onNewBrandButtonClicked();
	void createNewBrand(const QString& brandName);
	void onModelPicButtonClicked();
public:
	//系统参数
	 // 系统参数结构体
	struct SystemParams {
		int pulseCount;              // P1：移位链脉冲数 (0-9)
		int component1ToReject;      // P2：1组件到剔除口工位数 (10-30)
		int component2ToReject;      // P3：2组件到剔除口工位数 (20-40)
		int rejectStart;             // P4：剔除开启(MCP) (0-5)
		int rejectEnd;               // P5：剔除关闭(MCP) (5-9)
		bool rejectEnabled;          // P6：剔除功能开关
		bool positionDisplayEnabled; // P7：定位效果显示开关
		QString comPort;             // P8：串口号 (COM1-COM3)
	};

	// 相机参数结构体
	struct CameraParams {
		// 一组件控制参数
		QString camera1SerialNum;    // P21：1组件相机序列号
		int lightTriggerDelay1;      // P22：光源触发延时(us) (0-20)
		int lightExposureTime1;      // P23：光源曝光时间(us) (0-200)
		int cameraTriggerStart1;     // P24：相机触发开始(MCP) (1-9)

		// 二组件控制参数
		QString camera2InnerSerialNum; // P30：2组件1相机(内)序列号
		QString camera2OuterSerialNum; // P31：2组件2相机(外)序列号
		int lightTriggerDelay2;      // P32：光源触发延时(us) (0-20)
		int lightExposureTime2;      // P33：光源曝光时间(us) (0-200)
		int cameraTriggerStart2;     // P34：相机触发开始(MCP) (1-9)
	};

    
    
public:
    bool single_step(process_step step,QString modelPic);//单步执行
    ErrorCode local_top(QString modelPic,FindLeftPoint dllpro,int &up_left_pointX,int &up_left_pointY,int &down_left_pointX,int &down_left_pointY);//单步执行-端部定位
    ErrorCode local_side(QString modelPic,  FindSidePoint FindSidePoint_dllpro, int in_side_array[2][3], int in_model_side_array[2][3][3], 
        bool set_new_model, int(&out_side_array)[2][3], int(&out_model_side_array)[2][3][3]);//单步执行-边界定位
    ErrorCode local_stick(int in_top_points_array[2][3], int in_side_array[2][3]);//单步执行-烟棒定位
    ErrorCode local_filter(int in_top_points_array[2][3], int in_side_array[2][3]);//单步执行-嘴棒定位
    ErrorCode local_joint(int in_top_points_array[2][3], int in_side_array[2][3]);//单步执行-拼接定位
	// 初始化系统参数界面
	void initSysParamsWidgets();
	void initSystemParamsGroup();    // 初始化系统参数组
	void initCameraParamsGroup();    // 初始化相机参数组

	// 系统参数相关成员变量
	SystemParams systemParams;
	CameraParams cameraParams;

	// 系统参数界面控件
	QGroupBox* systemParamsGroup;    // 系统参数组
	QGroupBox* cameraParamsGroup;    // 相机参数组

	//1级界面
	QTabWidget* paramConfigTabWidget = new QTabWidget();//切换组件页面标签栏

	QTableWidget* zu1_processTableWidget = new QTableWidget(10, 3);//处理过程选择
	QTableWidget* zu1_operatorTableWidget = new QTableWidget(7, 3);//算法流程选择
	//参数修改窗自定义体
	QWidget* paramsWidget = new QWidget();
	myTabWidget* paramsTabWidget = new myTabWidget();
	QWidget* buttonsWidget = new QWidget();
	//界面设计
	QWidget* upCigTopPosParaSet_widget = new QWidget();//上烟端定位参数设置
	QWidget* downCigTopPosParaSet_widget = new QWidget();//下烟端定位参数设置

	QWidget* upCigBoundryPosParaSet_widget = new QWidget();//上烟边定位参数设置
	QWidget* downCigBoundryPosParaSet_widget = new QWidget();//下烟边定位参数设置

    QWidget* cigStickROISet_widget = new QWidget();//烟支区域参数设置
    QWidget* cigFilterROISet_widget = new QWidget();//滤嘴区域参数设置
    QWidget* cigJointerROISet_widget = new QWidget();//拼接区域参数设置

    QWidget* stickNGSet_widget = new QWidget();//烟棒缺陷参数设置
    QWidget* filgerDarkNGSet_widget = new QWidget();//滤嘴暗缺陷参数设置
    QWidget* filgerWhiteNGSet_widget = new QWidget();//滤嘴亮缺陷参数设置

    QWidget* cigOutNGSet_widget = new QWidget();//烟支轮廓缺陷参数设置
    QWidget* jointNGSet_widget = new QWidget();//拼接缺陷参数设置
    //QWidget* downJointNGSet_widget = new QWidget();//拼接缺陷参数设置
    QWidget* deepNGSet_widget = new QWidget();//深度学习缺陷参数设置


	// 添加品牌选择界面
	QWidget* brandSelectWidget = new QWidget();
	// 添加系统参数界面
	QWidget* sysParamsWidget = new QWidget();

    QLabel* currentBrandLabel;     // 当前品牌显示标签

    // 获取各部分参数
    UpCigTopPositionParams getUpCigTopPositionParams() const { return upCigTopPositionParams; }
    DownCigTopPositionParams getDownCigTopPositionParams() const { return downCigTopPositionParams; }

    UpCigTopModelPositionParams getUpCigTopModelPositionParams() const { return upCigTopModelPositionParams; }
    DownCigTopModelPositionParams getDownCigTopModelPositionParams() const { return downCigTopModelPositionParams; }

    UpBoxPositionParams getUpBoxPositionParams() const { return upBoxPositionParams; }
    DownBoxPositionParams getDownBoxPositionParams() const { return downBoxPositionParams; }

	UpBoxModelPositionParams getUpBoxModelPositionParams() const { return upBoxModelPositionParams; }
	DownBoxModelPositionParams getDownBoxModelPositionParams() const { return downBoxModelPositionParams; }

    UpCigBodyPositionParams getUpCigBodyPositionParams() const { return upCigBodyPositionParams; }
    DownCigBodyPositionParams getDownCigBodyPositionParams() const { return downCigBodyPositionParams; }

    UpFilterPositionParams getUpFilterPositionParams() const { return upFilterPositionParams; }
    DownFilterPositionParams getDownFilterPositionParams() const { return downFilterPositionParams; }

    UpJointPositionParams getUpJointPositionParams() const { return upJointPositionParams; }
    DownJointPositionParams getDownJointPositionParams() const { return downJointPositionParams; }

    UpCigStickDarkDefectParams getUpCigStickDarkDefectParams() const { return upCigStickDarkDefectParams; }
    DownCigStickDarkDefectParams getDownCigStickDarkDefectParams() const { return downCigStickDarkDefectParams; }

    UpCigFilterDarkDefectParams getUpCigFilterDarkDefectParams() const { return upCigFilterDarkDefectParams; }
    DownCigFilterDarkDefectParams getDownCigFilterDarkDefectParams() const { return downCigFilterDarkDefectParams; }

    UpJointDefectParams getUpJointDefectParams() const { return upJointDefectParams; };
    DownJointDefectParams getDownJointDefectParams() const { return downJointDefectParams; };

    DeepLearningParams getDeepLearningParams() const { return deepLearningParams; };

    SystemParams getSystemParams() const { return systemParams; }
    CameraParams getCameraParams() const { return cameraParams; }

    QString getDefaultIniFile() const;
	// 获取当前品牌名称
	QString getCurrentBrand() const { return currentBrand; }
	QString getConfigPath() const { return configPath; }
	QString getBrandsBasePath() const { return brandsBasePath; }
    QString getCurrentBrandModelPicturesPath() const { return currentBrandModelPicturesPath; }
    HalconGraphicsView* getView() const { return view; }

    // 修改设置参数的方法，添加自动保存功能
    void setUpCigTopPositionParams(const UpCigTopPositionParams& params);
    void setDownCigTopPositionParams(const DownCigTopPositionParams& params);
    
    void setUpCigTopModelPositionParams(const UpCigTopModelPositionParams& params);
    void setDownCigTopModelPositionParams(const DownCigTopModelPositionParams& params);

    void setUpBoxPositionParams(const UpBoxPositionParams& params);
    void setDownBoxPositionParams(const DownBoxPositionParams& params);

    void setUpBoxModelPositionParams(const UpBoxModelPositionParams& params);
    void setDownBoxModelPositionParams(const DownBoxModelPositionParams& params);

    void setUpCigBodyPositionParams(const UpCigBodyPositionParams& params);
    void setDownCigBodyPositionParams(const DownCigBodyPositionParams& params);

    void setUpFilterPositionParams(const UpFilterPositionParams& params);
    void setDownFilterPositionParams(const DownFilterPositionParams& params);

    void setUpJointPositionParams(const UpJointPositionParams& params);
    void setDownJointPositionParams(const DownJointPositionParams& params);

    void setUpCigStickDarkDefectParams(const UpCigStickDarkDefectParams& params);
    void setDownCigStickDarkDefectParams(const DownCigStickDarkDefectParams& params);

    void setUpCigFilterDarkDefectParams(const UpCigFilterDarkDefectParams& params);
    void setDownCigFilterDarkDefectParams(const DownCigFilterDarkDefectParams& params);
    void setUpCigFilterWhiteDefectParams(const UpCigFilterWhiteDefectParams& params);
    void setDownCigFilterWhiteDefectParams(const DownCigFilterWhiteDefectParams& params);

    void setUpJointDefectParams(const UpJointDefectParams& params);
    void setDownJointDefectParams(const DownJointDefectParams& params);

    void setDeepLearningParams(const DeepLearningParams& params);

	void setSystemParams(const SystemParams& params);
	void setCameraParams(const CameraParams& params);

	// 设置默认的INI文件路径
	void setDefaultIniFile(const QString& filename);

	// 设置当前品牌
	bool setCurrentBrand(const QString& brandName);

    // INI文件操作方法
    bool loadFromIni(const QString& filename = "config.ini");
    bool saveToIni(const QString& filename = "config.ini") const;

    //界面初始化
    void init_1st_TabWidgets();//初始化1级界面布局

    void initParamsWidgets();//主参数界面布局初始化

    void initUpCigTopParamsWidgets();//初始化上烟端设置界面
    void initDownCigTopParamsWidgets();//初始化下烟端设置界面

    void initUpCigBoundaryParamsWidgets();//初始化上烟边设置界面
    void initDownCigBoundaryParamsWidgets();//初始化上烟边设置界面

    void initCigStickROIWidgets();//初始化烟棒感兴趣参数设置界面
    void initCigFilterROIWidgets();//初始化嘴棒感兴趣参数设置界面
    void initCigJointROIWidgets();//初始化拼接感兴趣参数设置界面
    void initStickNGWidgets();
    void initFilterDarkNGWidgets();
    void initFilterWhiteNGWidgets();
    void initOutNGWidgets();
    void initJointNGWidgets();
    //void initDownJointNGWidgets();
    
    //界面显示
    void showParamsWidgets(int processID,int operatorID);//显示具体画面，添加tabWidgets

    static CigVisionParams& getInstance();

    // 加载参数
    bool loadParams();
    // 保存参数
    bool saveParams();

    // 系统参数的保存和加载
    bool saveSystemParams();
    bool loadSystemParams();

    // 添加新的方法声明
    bool loadFromConfigFile();
    void setDefaultParams();

    
private:
    
	// 添加新的成员变量
	QComboBox* brandComboBox;      // 品牌选择下拉框
	QPushButton* newBrandButton;   // 新建品牌按钮
	// 添加默认INI文件路径
	QString defaultIniFile;

	int select_processRow = 0;//处理过程选择行
	int select_operatorRow = 0;//算法选择行
	process_step current_process_step = process_step::Loc_Top;//当前处理步骤
	QString currentBrand;           // 当前品牌名称
	QString configPath;             // 项目配置文件路径
	QString brandsBasePath;         // 品牌参数文件夹基础路径

    QString currentBrandPath; // 当前品牌基础路径
    QString currentBrandModelPicturesPath; // 当前品牌测试图存放路径
    // 初始化默认参数
    void initDefaultParams();
    // 保存参数到指定的INI文件
    bool saveParamsToIni(QSettings& settings) const;
    // 从指定的INI文件加载参数
    bool loadParamsFromIni(QSettings& settings);
    // 检查并创建参数文件
    bool ensureParamFileExists(const QString& filename);

    // 加载品牌特定参数
    bool loadBrandParams(const QString& brandName);
    // 保存品牌特定参数
    bool saveBrandParams(const QString& brandName);

    // 添加品牌选择函数
    void initBrandSelectWidget();
    
    //显示Halcon图像
    HalconGraphicsView* view = new HalconGraphicsView();

    UpCigTopPositionParams upCigTopPositionParams;
    DownCigTopPositionParams downCigTopPositionParams;
	UpCigTopModelPositionParams upCigTopModelPositionParams;
	DownCigTopModelPositionParams downCigTopModelPositionParams;

    UpBoxPositionParams upBoxPositionParams;
    DownBoxPositionParams downBoxPositionParams;
    UpBoxModelPositionParams upBoxModelPositionParams;
    DownBoxModelPositionParams downBoxModelPositionParams;
  
    UpCigBodyPositionParams upCigBodyPositionParams;
    DownCigBodyPositionParams downCigBodyPositionParams;

    UpFilterPositionParams upFilterPositionParams;
    DownFilterPositionParams downFilterPositionParams;

    UpJointPositionParams upJointPositionParams;
    DownJointPositionParams downJointPositionParams;

    UpCigStickDarkDefectParams upCigStickDarkDefectParams;
    DownCigStickDarkDefectParams downCigStickDarkDefectParams;

    UpCigFilterDarkDefectParams upCigFilterDarkDefectParams;
    DownCigFilterDarkDefectParams downCigFilterDarkDefectParams;
    UpCigFilterWhiteDefectParams upCigFilterWhiteDefectParams;
    DownCigFilterWhiteDefectParams downCigFilterWhiteDefectParams;

    UpJointDefectParams upJointDefectParams;
    DownJointDefectParams downJointDefectParams;

    UpOutDefectParams upOutDefectParams;
    DownOutDefectParams downOutDefectParams;
    DeepLearningParams deepLearningParams;

};