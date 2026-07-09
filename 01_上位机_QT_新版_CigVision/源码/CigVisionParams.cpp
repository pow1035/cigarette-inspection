#include "CigVisionParams.h"
#include <QFile>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <qlayout.h>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidgetItem>
#include<qdebug.h>
#include <QGroupBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>
#include <fstream>
#include <string>


CigVisionParams::CigVisionParams(QWidget* parent)
	: QWidget(parent)
{
	// 设置配置文件路径 - 使用绝对路径
	configPath = QDir::currentPath() + "/config.ini";
	brandsBasePath = QDir::currentPath() + QStringLiteral("/品牌设置");
	
	qDebug() << "Config file path:" << configPath;
	qDebug() << "Brands base path:" << brandsBasePath;

	// 检查并创建品牌参数目录
	QDir brandDir(brandsBasePath);
	if (!brandDir.exists()) {
		if (brandDir.mkpath(".")) {
			qDebug() << "Created brands directory:" << brandsBasePath;
		}
		else {
			qDebug() << "Failed to create brands directory:" << brandsBasePath;
		}
	}

	// 检查配置文件是否存在
	if (!QFile::exists(configPath)) {
		qDebug() << "Config file does not exist, creating new one:" << configPath;

		// 获取所有品牌文件夹
		QStringList brands = brandDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

		// 创建新的配置文件，使用UTF-8编码
		QSettings settings(configPath, QSettings::IniFormat);
		settings.setIniCodec("UTF-8");  // 修改这里：使用 UTF-8 编码

		if (!brands.isEmpty()) {
			// 按名称排序并获取第一个品牌名称
			brands.sort();
			currentBrand = brands.first();
            settings.setValue("General/CurrentBrand", currentBrand);
		}
		else {
			// 如果没有品牌文件夹，创建一个默认品牌
			currentBrand = QStringLiteral("默认品牌");
			QString defaultBrandPath = brandsBasePath + "/" + currentBrand;
			QDir().mkpath(defaultBrandPath);
			settings.setValue("General/CurrentBrand", currentBrand);
			qDebug() << "Created default brand directory:" << defaultBrandPath;
		}
		// 创建并保存新的配置文件
		{
			QSettings settings(configPath, QSettings::IniFormat);
			settings.setIniCodec("UTF-8");  // 修改这里：使用 UTF-8 编码
			settings.setValue("General/CurrentBrand", currentBrand);

			// 初始化并保存系统参数
			settings.beginGroup("SystemParams");
			settings.setValue("pulseCount", 0);                    // 最小值 0
			settings.setValue("component1ToReject", 10);          // 最小值 10
			settings.setValue("component2ToReject", 20);          // 最小值 20
			settings.setValue("rejectStart", 0);                  // 最小值 0
			settings.setValue("rejectEnd", 5);                    // 最小值 5
			settings.setValue("rejectEnabled", false);            // 默认关闭
			settings.setValue("positionDisplayEnabled", false);   // 默认关闭
			settings.setValue("comPort", "COM1");                 // 默认COM1
			settings.endGroup();

			// 初始化并保存相机参数
			settings.beginGroup("CameraParams");
			// 一组件参数
			settings.setValue("camera1SerialNum", "1");           // 默认空
			settings.setValue("lightTriggerDelay1", 0);         // 最小值 0
			settings.setValue("lightExposureTime1", 0);         // 最小值 0
			settings.setValue("cameraTriggerStart1", 1);        // 最小值 1
			// 二组件参数
			settings.setValue("camera2InnerSerialNum", "2_2");     // 默认空
			settings.setValue("camera2OuterSerialNum", "2_1");     // 默认空
			settings.setValue("lightTriggerDelay2", 0);         // 最小值 0
			settings.setValue("lightExposureTime2", 0);         // 最小值 0
			settings.setValue("cameraTriggerStart2", 1);        // 最小值 1
			settings.endGroup();

			settings.sync();
		}
		// 强制写入文件
		settings.sync();

		// 验证文件是否创建成功
		if (QFile::exists(configPath)) {
			qDebug() << "Successfully created config file";
		}
		else {
			qDebug() << "Failed to create config file";
		}
	}

	//初始化系统参数
	loadSystemParams();
	//初始化牌号参数
	loadParams();
	// 初始化界面
	initParamsWidgets();
	init_1st_TabWidgets();
	//初始化品牌切换
	initBrandSelectWidget();
	// 初始化系统设置界面
	initSysParamsWidgets();
    //初始化烟棒检测调参界面
    initStickNGWidgets();
    //初始化嘴棒检测调参界面
    initFilterDarkNGWidgets();
    initFilterWhiteNGWidgets();
    //初始化烟棒轮廓检测调参界面
    initOutNGWidgets();
    //初始化拼接检测调参界面
    initJointNGWidgets();
    
}
CigVisionParams::~CigVisionParams()
{
    
}
CigVisionParams& CigVisionParams::getInstance()
{
	static CigVisionParams* instance = nullptr;
	if (instance == nullptr) {
		instance = new CigVisionParams(nullptr);
		// 确保在程序退出时删除实例
		static struct Cleanup {
			~Cleanup() { delete instance; }
		} cleanup;
	}
	return *instance;
}
bool CigVisionParams::loadParams()
{
    // 检查配置文件是否存在
    if (!QFile::exists(configPath)) {
        qDebug() << "Config file does not exist, creating new one:" << configPath;
        return false;
	
    } else {
		// 检查品牌参数文件夹是否存在
		QDir brandDir(brandsBasePath);
		if (!brandDir.exists()) {
			qDebug() << "Brands directory does not exist:" << brandsBasePath;
			return false;
		}
       
    }

    if (currentBrand.isEmpty()) {
        qDebug() << "No brand specified in config.ini";
        return false;
    }

    // 加载当前品牌的具体参数
    return loadBrandParams(currentBrand);
}
void CigVisionParams::initDefaultParams()
{
	// 初始化上烟支定位参数
	upCigTopPositionParams = {
		0, 0, 0, 0,  // x1, y1, x2, y2
		0,                    // sigmaCode端西格玛值
		0,                 // edgeGradientStartThreshold端边界起始梯度阈值
		0                  // stepGradientThreshold端边界梯度阈值(步进值)
	};
	downCigTopPositionParams = {
		0, 0, 0, 0,  // x1, y1, x2, y2
		0,                    // sigmaCode端西格玛值
		0,                 // edgeGradientStartThreshold端边界起始梯度阈值
		0                  // stepGradientThreshold端边界梯度阈值(步进值)
	};

	// 初始化模版计算参数
	upCigTopModelPositionParams = {
		0,    // modelPositionX
		0     // modelPositionY
	};
	downCigTopModelPositionParams = {
		0,    // modelPositionX
		0     // modelPositionY
	};

	// 初始化框定位参数
	upBoxPositionParams = {
		0,    // frameStartDistance
		0,    // frameToHorizontalDistance
		0,    // frameHeight
		0,    // frameWidth
		0,      // frameWestEastCode
		0     // frameEdgeGradientThreshold
	};
	downBoxPositionParams = {
		0,    // frameStartDistance
		0,    // frameToHorizontalDistance
		0,    // frameHeight
		0,    // frameWidth
		0,      // frameWestEastCode
		0     // frameEdgeGradientThreshold
	};
	// 初始化模版计算参数
	upBoxModelPositionParams = {
		0,    // modelPositionX
		0,    // modelPositionY
		0    // modelWidth
	};
	downBoxModelPositionParams = {
		0,    // modelPositionX
		0,    // modelPositionY
		0    // modelWidth
	};

	// 初始化烟体定位参数
    upCigBodyPositionParams.bodyStartDistance = 0;
    upCigBodyPositionParams.bodyLength = 0;
    upCigBodyPositionParams.innerEdgeThreshold = 0;

    downCigBodyPositionParams.bodyStartDistance = 0;
    downCigBodyPositionParams.bodyLength = 0;
    downCigBodyPositionParams.innerEdgeThreshold = 0;


	// 初始化嘴棒定位参数
    upFilterPositionParams.filterStartDistance = 0;
    upFilterPositionParams.filterLength = 0;
    upFilterPositionParams.innerEdgeThreshold = 0;
    downFilterPositionParams.filterStartDistance = 0;
    downFilterPositionParams.filterLength = 0;
    downFilterPositionParams.innerEdgeThreshold = 0;


	// 初始化拼接定位参数
    upJointPositionParams.jointStartDistance = 0;
    upJointPositionParams.jointLength = 0;
    upJointPositionParams.isLinear = false;
    upJointPositionParams.isTopDown = false;
    upJointPositionParams.isDeepLearning = false;

    downJointPositionParams.jointStartDistance = 0;
    downJointPositionParams.jointLength = 0;
    downJointPositionParams.isLinear = false;
    downJointPositionParams.isTopDown = false;
    downJointPositionParams.isDeepLearning = false;

	// 初始化烟体缺陷参数
	upCigStickDarkDefectParams.darkPointAreasValue = 0;
	upCigStickDarkDefectParams.darkPointGrayValue = 0;
	downCigStickDarkDefectParams.darkPointAreasValue = 0;
	downCigStickDarkDefectParams.darkPointGrayValue = 0;

	// 初始化嘴棒缺陷参数
	upCigFilterDarkDefectParams.darkPointAreasValue = 0;
	upCigFilterDarkDefectParams.darkPointGrayValue = 0;
	downCigFilterDarkDefectParams.darkPointAreasValue = 0;
	downCigFilterDarkDefectParams.darkPointGrayValue = 0;

	// 初始化嘴棒亮点检测参数
	upCigFilterWhiteDefectParams.brightPointAreasValue = 0;
	upCigFilterWhiteDefectParams.brightPointGrayValue = 0;
	downCigFilterWhiteDefectParams.brightPointAreasValue = 0;
	downCigFilterWhiteDefectParams.brightPointGrayValue = 0;

	// 初始化拼接搓牙检测参数参数
    upJointDefectParams.defectArea = 0;
    downJointDefectParams.defectArea = 0;
	// 初始化烟支外形检测参数
    upOutDefectParams.outPix = 0;
    upOutDefectParams.convexity = 0.0;
    upOutDefectParams.rectangularity = 0.0;
    downOutDefectParams.outPix = 0;
    downOutDefectParams.convexity = 0.0;
    downOutDefectParams.rectangularity = 0.0;

	// 初始化深度学习参数默认值
	deepLearningParams.jointRollThreshold = 0.5;      // 搭口搓牙默认阈值
	deepLearningParams.flyingTobaccoThreshold = 0.5;  // 飞烟默认阈值
	deepLearningParams.tobaccoClipsThreshold = 0.5;   // 夹末默认阈值
	deepLearningParams.filterWrinkleThreshold = 0.5;  // 滤嘴皱褶默认阈值
	deepLearningParams.missingFilterThreshold = 0.5;  // 缺嘴默认阈值
	deepLearningParams.rodDamageThreshold = 0.5;      // 烟棒破损默认阈值
	deepLearningParams.rodStainThreshold = 0.5;       // 烟棒脏污默认阈值
}
bool CigVisionParams::saveParams()
{
    // 保存当前品牌名称到config.ini
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");  // 修改这里：使用 UTF-8 编码
    settings.setValue("General/CurrentBrand", currentBrand);
    
    // 保存当前品牌的具体参数
    return saveBrandParams(currentBrand);
}

bool CigVisionParams::setCurrentBrand(const QString& brandName)
{
    // 检查品牌参数文件夹是否存在
    currentBrandPath = brandsBasePath + "/" + brandName;
    if (!QDir(currentBrandPath).exists()) {
        qDebug() << "Brand directory does not exist:" << currentBrandPath;
        return false;
    }
    currentBrandModelPicturesPath = currentBrandPath + "/" + QStringLiteral("模版图片");
	if (!QDir(currentBrandModelPicturesPath).exists()) {
		qDebug() << "Brand pictures directory does not exist:" << currentBrandModelPicturesPath;
		return false;
	}
    // 保存更改到config.ini
    {
        QSettings settings(configPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.setValue("General/CurrentBrand", brandName);
        settings.sync();  // 确保立即写入
    }

    // 更新当前品牌
    currentBrand = brandName;
    
    // 加载新品牌的参数
    if (!loadParams()) {
        qDebug() << "Failed to load parameters for brand:" << brandName;
        return false;
    }
    
    qDebug() << "Successfully switched to brand:" << brandName;
    return true;
}

void CigVisionParams::setUpCigTopPositionParams(const UpCigTopPositionParams& params)
{
	upCigTopPositionParams = params;
	saveToIni();
}
void CigVisionParams::setDownCigTopPositionParams(const DownCigTopPositionParams& params)
{
	downCigTopPositionParams = params;
	saveToIni();
}
void CigVisionParams::setUpCigTopModelPositionParams(const UpCigTopModelPositionParams& params)
{
	upCigTopModelPositionParams = params;
	saveToIni();
}
void CigVisionParams::setDownCigTopModelPositionParams(const DownCigTopModelPositionParams& params)
{
	downCigTopModelPositionParams = params;
	saveToIni();
}

void CigVisionParams::setUpBoxPositionParams(const UpBoxPositionParams& params)
{
	upBoxPositionParams = params;
	saveToIni();
}
void CigVisionParams::setDownBoxPositionParams(const DownBoxPositionParams& params)
{
	downBoxPositionParams = params;
	saveToIni();
}
void CigVisionParams::setUpBoxModelPositionParams(const UpBoxModelPositionParams& params)
{
	upBoxModelPositionParams = params;
	saveToIni();
}
void CigVisionParams::setDownBoxModelPositionParams(const DownBoxModelPositionParams& params)
{
	downBoxModelPositionParams = params;
	saveToIni();
}

void CigVisionParams::setUpCigBodyPositionParams(const UpCigBodyPositionParams& params)
{
    upCigBodyPositionParams = params;
    saveToIni();
}
void CigVisionParams::setDownCigBodyPositionParams(const DownCigBodyPositionParams& params)
{
    downCigBodyPositionParams = params;
    saveToIni();
}

void CigVisionParams::setUpFilterPositionParams(const UpFilterPositionParams& params)
{
    upFilterPositionParams = params;
    saveToIni();
}
void CigVisionParams::setDownFilterPositionParams(const DownFilterPositionParams& params)
{
    downFilterPositionParams = params;
    saveToIni();
}

void CigVisionParams::setUpJointPositionParams(const UpJointPositionParams& params)
{
    upJointPositionParams = params;
    saveToIni();
}
void CigVisionParams::setDownJointPositionParams(const DownJointPositionParams& params)
{
    downJointPositionParams = params;
    saveToIni();
}

void CigVisionParams::setUpCigStickDarkDefectParams(const UpCigStickDarkDefectParams& params)
{
    upCigStickDarkDefectParams = params;
    saveToIni();
}
void CigVisionParams::setDownCigStickDarkDefectParams(const DownCigStickDarkDefectParams& params)
{
    downCigStickDarkDefectParams = params;
    saveToIni();
}

void CigVisionParams::setUpCigFilterDarkDefectParams(const UpCigFilterDarkDefectParams& params)
{
    upCigFilterDarkDefectParams = params;
    saveToIni();
}
void CigVisionParams::setDownCigFilterDarkDefectParams(const DownCigFilterDarkDefectParams& params)
{
    downCigFilterDarkDefectParams = params;
    saveToIni();
}
void CigVisionParams::setUpCigFilterWhiteDefectParams(const UpCigFilterWhiteDefectParams& params)
{
    upCigFilterWhiteDefectParams = params;
    saveToIni();
}
void CigVisionParams::setDownCigFilterWhiteDefectParams(const DownCigFilterWhiteDefectParams& params)
{
    downCigFilterWhiteDefectParams = params;
    saveToIni();
}

void CigVisionParams::setUpJointDefectParams(const UpJointDefectParams& params)
{
    upJointDefectParams = params;
    saveToIni();
}
void CigVisionParams::setDownJointDefectParams(const DownJointDefectParams& params)
{
    downJointDefectParams = params;
    saveToIni();
}

void CigVisionParams::setDefaultIniFile(const QString& filename)
{
    defaultIniFile = filename;
}

QString CigVisionParams::getDefaultIniFile() const
{
    return defaultIniFile;
}

bool CigVisionParams::ensureParamFileExists(const QString& filename)
{
    QFileInfo fileInfo(filename);

    // 检查目录是否存在，如果不存在则创建
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning("无法创建参数文件目录：%s", qUtf8Printable(dir.path()));
            return false;
        }
    }

    // 检查文件是否存在
    if (!fileInfo.exists()) {
        // 文件不存在，保存默认参数
        if (!saveToIni(filename)) {
            qWarning("无法创建参数文件：%s", qUtf8Printable(filename));
            return false;
        }
        qInfo("已创建默认参数文件：%s", qUtf8Printable(filename));
    }

    return true;
}

bool CigVisionParams::saveToIni(const QString& filename) const
{
    // 确保目录存在
    QFileInfo fileInfo(filename);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning("无法创建参数文件目录：%s", qUtf8Printable(dir.path()));
            return false;
        }
    }

    QSettings settings(filename, QSettings::IniFormat);
    return saveParamsToIni(settings);
}

bool CigVisionParams::saveParamsToIni(QSettings& settings) const
{
    settings.setIniCodec("UTF-8");  // 修改这里：使用 UTF-8 编码
    // 保存烟支定位参数
	settings.beginGroup("UpCigTopPosition");
	settings.setValue("centerX", upCigTopPositionParams.centerX);
	settings.setValue("centerY", upCigTopPositionParams.centerY);
	settings.setValue("width", upCigTopPositionParams.width);
	settings.setValue("height", upCigTopPositionParams.height);
	settings.setValue("sigmaCode", upCigTopPositionParams.sigmaCode);
	settings.setValue("edgeGradientStartThreshold", upCigTopPositionParams.edgeGradientStartThreshold);
	settings.setValue("stepGradientThreshold", upCigTopPositionParams.stepGradientThreshold);
	settings.endGroup();

	settings.beginGroup("DownCigTopPosition");
	settings.setValue("centerX", downCigTopPositionParams.centerX);
	settings.setValue("centerY", downCigTopPositionParams.centerY);
	settings.setValue("width", downCigTopPositionParams.width);
	settings.setValue("height", downCigTopPositionParams.height);
	settings.setValue("sigmaCode", downCigTopPositionParams.sigmaCode);
	settings.setValue("edgeGradientStartThreshold", downCigTopPositionParams.edgeGradientStartThreshold);
	settings.setValue("stepGradientThreshold", downCigTopPositionParams.stepGradientThreshold);
	settings.endGroup();

	// 保存模版计算参数
	settings.beginGroup("UpCigModelPosition");
	settings.setValue("modelPositionX", upCigTopModelPositionParams.modelPositionX);
	settings.setValue("modelPositionY", upCigTopModelPositionParams.modelPositionY);
	settings.endGroup();
	settings.beginGroup("DownCigModelPosition");
	settings.setValue("modelPositionX", downCigTopModelPositionParams.modelPositionX);
	settings.setValue("modelPositionY", downCigTopModelPositionParams.modelPositionY);
	settings.endGroup();

    // 保存框定位参数
	settings.beginGroup("UpBoxPosition");
	settings.setValue("frameStartDistance", upBoxPositionParams.frameStartDistance);
	settings.setValue("framesHorizontalDistance", upBoxPositionParams.framesHorizontalDistance);
	settings.setValue("frameHeight", upBoxPositionParams.frameHeight);
	settings.setValue("frameWidth", upBoxPositionParams.frameWidth);
	settings.setValue("frameSigmaCode", upBoxPositionParams.frameSigmaCode);
	settings.setValue("frameEdgeGradientThreshold", upBoxPositionParams.frameEdgeGradientThreshold);
	settings.endGroup();
	settings.beginGroup("DownBoxPosition");
	settings.setValue("frameStartDistance", downBoxPositionParams.frameStartDistance);
	settings.setValue("frameToHorizontalDistance", downBoxPositionParams.framesHorizontalDistance);
	settings.setValue("frameHeight", downBoxPositionParams.frameHeight);
	settings.setValue("frameWidth", downBoxPositionParams.frameWidth);
	settings.setValue("frameSigmaCode", downBoxPositionParams.frameSigmaCode);
	settings.setValue("frameEdgeGradientThreshold", downBoxPositionParams.frameEdgeGradientThreshold);
	settings.endGroup();
    //保存框定位模版参数
	settings.beginGroup("UpBoxModelPosition");
	settings.setValue("modelPositionX", upBoxModelPositionParams.model1PositionX);
	settings.setValue("modelPositionY", upBoxModelPositionParams.model1PositionY);
	settings.setValue("modelWidth", upBoxModelPositionParams.model1Width);
    settings.endGroup();
	settings.beginGroup("DownBoxModelPosition");
	settings.setValue("modelPositionX", downBoxModelPositionParams.model1PositionX);
	settings.setValue("modelPositionY", downBoxModelPositionParams.model1PositionY);
	settings.setValue("modelWidth", downBoxModelPositionParams.model1Width);
    settings.endGroup();
    // 保存烟体定位参数
    settings.beginGroup("UpCigBodyPosition");
    settings.setValue("bodyStartDistance", upCigBodyPositionParams.bodyStartDistance);
    settings.setValue("bodyLength", upCigBodyPositionParams.bodyLength);
    settings.setValue("innerEdgeThreshold", upCigBodyPositionParams.innerEdgeThreshold);
    settings.endGroup();
    settings.beginGroup("DownCigBodyPosition");
    settings.setValue("bodyStartDistance", downCigBodyPositionParams.bodyStartDistance);
    settings.setValue("bodyLength", downCigBodyPositionParams.bodyLength);
    settings.setValue("innerEdgeThreshold", downCigBodyPositionParams.innerEdgeThreshold);
    settings.endGroup();

    // 保存烟体缺陷参数
    settings.beginGroup("UpCigStickDarkDefect");
    settings.setValue("darkPointAreasValue", upCigStickDarkDefectParams.darkPointAreasValue);
    settings.setValue("darkPointGrayValue", upCigStickDarkDefectParams.darkPointGrayValue);
    settings.endGroup();

    // 保存嘴棒定位参数
    settings.beginGroup("UpFilterPosition");
    settings.setValue("filterStartDistance", upFilterPositionParams.filterStartDistance);
    settings.setValue("filterLength", upFilterPositionParams.filterLength);
    settings.setValue("innerEdgeThreshold", upFilterPositionParams.innerEdgeThreshold);
    settings.endGroup();
    settings.beginGroup("DownFilterPosition");
    settings.setValue("filterStartDistance", downFilterPositionParams.filterStartDistance);
    settings.setValue("filterLength", downFilterPositionParams.filterLength);
    settings.setValue("innerEdgeThreshold", downFilterPositionParams.innerEdgeThreshold);
    settings.endGroup();

    // 保存嘴棒缺陷参数
    settings.beginGroup("UpFilterDarkDefect");
    settings.setValue("darkPointAreasValue", upCigFilterDarkDefectParams.darkPointAreasValue);
    settings.setValue("darkPointGrayValue", upCigFilterDarkDefectParams.darkPointGrayValue);
    settings.endGroup();

    settings.beginGroup("DownFilterDarkDefect");
    settings.setValue("darkPointAreasValue", downCigFilterDarkDefectParams.darkPointAreasValue);
    settings.setValue("darkPointGrayValue", downCigFilterDarkDefectParams.darkPointGrayValue);
    settings.endGroup();

    settings.beginGroup("UpFilterWhiteDefect");
    settings.setValue("darkPointAreasValue", upCigFilterWhiteDefectParams.brightPointAreasValue);
    settings.setValue("darkPointGrayValue", upCigFilterWhiteDefectParams.brightPointGrayValue);
    settings.endGroup();

    settings.beginGroup("DownFilterWhiteDefect");
    settings.setValue("darkPointAreasValue", downCigFilterWhiteDefectParams.brightPointAreasValue);
    settings.setValue("darkPointGrayValue", downCigFilterWhiteDefectParams.brightPointGrayValue);
    settings.endGroup();

    // 保存拼接定位参数
    settings.beginGroup("UpJointPosition");
    settings.setValue("jointStartDistance", upJointPositionParams.jointStartDistance);
    settings.setValue("jointLength", upJointPositionParams.jointLength);
    settings.setValue("isLinear", upJointPositionParams.isLinear);
    settings.setValue("isTopDown", upJointPositionParams.isTopDown);
    settings.setValue("isDeepLearning", upJointPositionParams.isDeepLearning);
    settings.endGroup();
    settings.beginGroup("DownJointPosition");
    settings.setValue("jointStartDistance", downJointPositionParams.jointStartDistance);
    settings.setValue("jointLength", downJointPositionParams.jointLength);
    settings.setValue("isLinear", downJointPositionParams.isLinear);
    settings.setValue("isTopDown", downJointPositionParams.isTopDown);
    settings.setValue("isDeepLearning", downJointPositionParams.isDeepLearning);
    settings.endGroup();

    // 保存拼接搓牙参数
    settings.beginGroup("UpJointDefect");
    settings.setValue("defectArea", upJointDefectParams.defectArea);
    settings.endGroup();
    settings.beginGroup("DownJointDefect");
    settings.setValue("defectArea", upJointDefectParams.defectArea);
    settings.endGroup();

    // 保存深度学习参数
    settings.beginGroup("DeepLearningParams");
    settings.setValue("jointRollThreshold", deepLearningParams.jointRollThreshold);
    settings.setValue("flyingTobaccoThreshold", deepLearningParams.flyingTobaccoThreshold);
    settings.setValue("tobaccoClipsThreshold", deepLearningParams.tobaccoClipsThreshold);
    settings.setValue("filterWrinkleThreshold", deepLearningParams.filterWrinkleThreshold);
    settings.setValue("missingFilterThreshold", deepLearningParams.missingFilterThreshold);
    settings.setValue("rodDamageThreshold", deepLearningParams.rodDamageThreshold);
    settings.setValue("rodStainThreshold", deepLearningParams.rodStainThreshold);
    settings.endGroup();

    settings.sync();
    return true;
}

//bool CigVisionParams::loadParamsFromIni(QSettings& settings)
//{
//    settings.setIniCodec("UTF-8");  // 修改这里：使用 UTF-8 编码
//    // 加载烟支定位参数
//	settings.beginGroup("UpTopCigPosition");
//	upCigTopPositionParams.centerX = settings.value("centerX", 0.0).toInt();
//	upCigTopPositionParams.centerY = settings.value("centerY", 0.0).toInt();
//	upCigTopPositionParams.width = settings.value("width", 0.0).toInt();
//	upCigTopPositionParams.height = settings.value("height", 0.0).toInt();
//	upCigTopPositionParams.sigmaCode = settings.value("westEastCode", 0).toInt();
//	upCigTopPositionParams.edgeGradientStartThreshold = settings.value("edgeGradientThreshold", 0.0).toDouble();
//	upCigTopPositionParams.stepGradientThreshold = settings.value("stepGradientThreshold", 0.0).toDouble();
//	settings.endGroup();
//	settings.beginGroup("DownTopCigPosition");
//	downCigTopPositionParams.centerX = settings.value("centerX", 0.0).toInt();
//    downCigTopPositionParams.centerY = settings.value("centerY", 0.0).toInt();
//    downCigTopPositionParams.width = settings.value("width", 0.0).toInt();
//    downCigTopPositionParams.height = settings.value("height", 0.0).toInt();
//    downCigTopPositionParams.sigmaCode = settings.value("westEastCode", 0).toInt();
//    downCigTopPositionParams.edgeGradientStartThreshold = settings.value("edgeGradientThreshold", 0.0).toInt();
//    downCigTopPositionParams.stepGradientThreshold = settings.value("stepGradientThreshold", 0.0).toInt();
//	settings.endGroup();
//	// 加载模版计算参数
//	settings.beginGroup("UpTopCigModelPosition");
//	upCigTopModelPositionParams.modelPositionX = settings.value("modelPositionX", 0.0).toInt();
//	upCigTopModelPositionParams.modelPositionY = settings.value("modelPositionY", 0.0).toInt();
//	settings.endGroup();
//	settings.beginGroup("DownTopCigModelPosition");
//	downCigTopModelPositionParams.modelPositionX = settings.value("modelPositionX", 0.0).toInt();
//	downCigTopModelPositionParams.modelPositionY = settings.value("modelPositionY", 0.0).toInt();
//	settings.endGroup();
//
//    // 加载框定位参数
//	settings.beginGroup("UpBoxPosition");
//	upBoxPositionParams.frameStartDistance = settings.value("frameStartDistance", 0.0).toInt();
//	/*settings.setValue("frameHorizontalDistance", upBoxPositionParams.framesHorizontalDistance);
//	settings.setValue("frameHeight", upBoxPositionParams.frameHeight);
//	settings.setValue("frameWidth", upBoxPositionParams.frameWidth);
//	settings.setValue("frameSigmaCode", upBoxPositionParams.frameSigmaCode);
//	settings.setValue("frameEdgeGradientThreshold", upBoxPositionParams.frameEdgeGradientThreshold);*/
//	settings.endGroup();
//	settings.beginGroup("DownBoxPosition");
//	downBoxPositionParams.frameStartDistance = settings.value("frameStartDistance", 0).toInt();
//    downBoxPositionParams.framesHorizontalDistance = settings.value("frameToHorizontalDistance", 0.0).toInt();
//    downBoxPositionParams.frameHeight = settings.value("frameHeight", 0).toInt();
//    downBoxPositionParams.frameWidth = settings.value("frameWidth", 0).toInt();
//    downBoxPositionParams.frameSigmaCode = settings.value("frameWestEastCode", 0).toInt();
//    downBoxPositionParams.frameEdgeGradientThreshold = settings.value("frameEdgeGradientThreshold", 0).toInt();
//	settings.endGroup();
//
//    // 加载模版计算参数
//	settings.beginGroup("UpBoxModelPosition");
//	upBoxModelPositionParams.model1PositionX = settings.value("model1PositionX", 0).toInt();
//	upBoxModelPositionParams.model1PositionY = settings.value("model1PositionY", 0).toInt();
//	upBoxModelPositionParams.model1Width = settings.value("model1Width", 0).toInt();
//	upBoxModelPositionParams.model2PositionX = settings.value("model2PositionX", 0).toInt();
//	upBoxModelPositionParams.model2PositionY = settings.value("model2PositionY", 0).toInt();
//	upBoxModelPositionParams.model2Width = settings.value("model2Width", 0).toInt();
//	upBoxModelPositionParams.model3PositionX = settings.value("model3PositionX", 0).toInt();
//	upBoxModelPositionParams.model3PositionY = settings.value("model3PositionY", 0).toInt();
//	upBoxModelPositionParams.model3Width = settings.value("model3Width", 0).toInt();
//	/*settings.setValue("model1PositionX", upBoxModelPositionParams.model1PositionX);
//	settings.setValue("model1PositionY", upBoxModelPositionParams.model1PositionY);
//	settings.setValue("model1Width", upBoxModelPositionParams.model1Width);
//	settings.setValue("model2PositionX", upBoxModelPositionParams.model2PositionX);
//	settings.setValue("model2PositionY", upBoxModelPositionParams.model2PositionY);
//	settings.setValue("model2Width", upBoxModelPositionParams.model2Width);
//	settings.setValue("model3PositionX", upBoxModelPositionParams.model3PositionX);
//	settings.setValue("model3PositionY", upBoxModelPositionParams.model3PositionY);
//	settings.setValue("model3Width", upBoxModelPositionParams.model3Width);*/
//	settings.endGroup();
//
//	settings.beginGroup("DownBoxModelPosition");
//	downBoxModelPositionParams.model1PositionX = settings.value("model1PositionX", 0.0).toInt();
//	downBoxModelPositionParams.model1PositionY = settings.value("model1PositionY", 0.0).toInt();
//	downBoxModelPositionParams.model1Width = settings.value("model1Width", 0).toInt();
//	downBoxModelPositionParams.model2PositionX = settings.value("model2PositionX", 0.0).toInt();
//	downBoxModelPositionParams.model2PositionY = settings.value("model2PositionY", 0.0).toInt();
//	downBoxModelPositionParams.model2Width = settings.value("model2Width", 0).toInt();
//	downBoxModelPositionParams.model3PositionX = settings.value("model3PositionX", 0.0).toInt();
//	downBoxModelPositionParams.model3PositionY = settings.value("model3PositionY", 0.0).toInt();
//	downBoxModelPositionParams.model3Width = settings.value("model3Width", 0).toInt();
//	settings.endGroup();
//
//
//    // 加载烟体定位参数
//    settings.beginGroup("CigBodyPosition");
//    cigBodyPositionParams.bodyStartDistance = settings.value("bodyStartDistance", 0.0).toDouble();
//    cigBodyPositionParams.bodyLength = settings.value("bodyLength", 0.0).toDouble();
//    cigBodyPositionParams.innerEdgeThreshold = settings.value("innerEdgeThreshold", 0.0).toDouble();
//    cigBodyPositionParams.darkPointAreasValue = settings.value("darkPointAreasValue", 0.0).toDouble();
//    cigBodyPositionParams.darkPointGrayValue = settings.value("darkPointGrayValue", 0.0).toDouble();
//    settings.endGroup();
//
//    // 加载烟体缺陷参数
//    settings.beginGroup("CigBodyDefect");
//    cigBodyDefectParams.darkPointAreasValue = settings.value("darkPointProtectValue", 0.0).toDouble();
//    cigBodyDefectParams.darkPointGrayValue = settings.value("darkPointGrayValue", 0.0).toDouble();
//    settings.endGroup();
//
//    // 加载嘴棒定位参数
//    settings.beginGroup("FilterPosition");
//    filterPositionParams.filterStartDistance = settings.value("filterStartDistance", 0.0).toDouble();
//    filterPositionParams.filterLength = settings.value("filterLength", 0.0).toDouble();
//    filterPositionParams.innerEdgeThreshold = settings.value("innerEdgeThreshold", 0.0).toDouble();
//    filterPositionParams.darkPointAreasValue = settings.value("darkPointAreasValue", 0.0).toDouble();
//    filterPositionParams.darkPointGrayValue = settings.value("darkPointGrayValue", 0.0).toDouble();
//    settings.endGroup();
//
//    // 加载嘴棒缺陷参数
//    settings.beginGroup("FilterDefect");
//    filterDefectParams.darkPointAreasValue = settings.value("darkPointProtectValue", 0.0).toDouble();
//    filterDefectParams.darkPointGrayValue = settings.value("darkPointGrayValue", 0.0).toDouble();
//    settings.endGroup();
//
//    // 加载嘴棒亮点检测参数
//    settings.beginGroup("FilterBrightPoint");
//    filterBrightPointParams.brightPointAreasValue = settings.value("brightPointProtectValue", 0.0).toDouble();
//    filterBrightPointParams.brightPointGrayValue = settings.value("brightPointGrayValue", 0.0).toDouble();
//    settings.endGroup();
//
//    // 加载拼接定位参数
//    settings.beginGroup("JointPosition");
//    jointPositionParams.jointStartDistance = settings.value("jointStartDistance", 0.0).toDouble();
//    jointPositionParams.jointLength = settings.value("jointLength", 0.0).toDouble();
//    jointPositionParams.isLinear = settings.value("isLinear", false).toBool();
//    jointPositionParams.isTopDown = settings.value("isTopDown", false).toBool();
//    jointPositionParams.isDeepLearning = settings.value("isDeepLearning", false).toBool();
//    settings.endGroup();
//
//    // 加载拼接搓牙参数
//    settings.beginGroup("JointRoll");
//    jointRollParams.defectArea = settings.value("defectArea", 0).toInt();
//    settings.endGroup();
//
//    // 加载水纸长短参数
//    settings.beginGroup("WaterPaperLength");
//    waterPaperLengthParams.startDistance = settings.value("startDistance", 0).toInt();
//    waterPaperLengthParams.length = settings.value("length", 0).toInt();
//    settings.endGroup();
//
//    // 加载深度学习参数
//    settings.beginGroup("DeepLearningParams");
//    deepLearningParams.jointRollThreshold = settings.value("jointRollThreshold", 0.5).toDouble();
//    deepLearningParams.flyingTobaccoThreshold = settings.value("flyingTobaccoThreshold", 0.5).toDouble();
//    deepLearningParams.tobaccoClipsThreshold = settings.value("tobaccoClipsThreshold", 0.5).toDouble();
//    deepLearningParams.filterWrinkleThreshold = settings.value("filterWrinkleThreshold", 0.5).toDouble();
//    deepLearningParams.missingFilterThreshold = settings.value("missingFilterThreshold", 0.5).toDouble();
//    deepLearningParams.rodDamageThreshold = settings.value("rodDamageThreshold", 0.5).toDouble();
//    deepLearningParams.rodStainThreshold = settings.value("rodStainThreshold", 0.5).toDouble();
//    settings.endGroup();
//
//    return true;
//}

void CigVisionParams::init_1st_TabWidgets()//主参数界面布局初始化
{
    paramConfigTabWidget->setStyleSheet(
		"QTabBar::tab {"
		"background-color: rgb(59,59,59);"  // 标签页背景颜色
		"color: white;"               // 标签页文字颜色
		"padding: 5px;"                // 标签页内边距
		"margin-right: 5px;"           // 标签页之间的间距
		"font:14px;"
		"min-width:117px;"
		"max-width:117px;"
		"min-height:26px;"
		"max-height:26px;"
		"}"
		"QTabBar::tab:selected {"
		"background-color: #4CAF50;"  // 选中标签页的背景颜色
		"font-weight: bold;"
		"}"
	);
	//组件界面
	QWidget* zu1_para_page = new QWidget;
	QWidget* zu2w_para_page = new QWidget;
	QWidget* zu2n_para_page = new QWidget;
	paramConfigTabWidget->addTab(zu1_para_page, QStringLiteral("组件1(检测轮)"));
	paramConfigTabWidget->addTab(zu2w_para_page, QStringLiteral("组件2外(调头轮)"));
	paramConfigTabWidget->addTab(zu2n_para_page, QStringLiteral("组件2内(调头轮)"));

	QHBoxLayout* zu1_paraHBoxLayout = new QHBoxLayout(zu1_para_page);//组件1整体布局
	QHBoxLayout* zu2w_paraHBoxLayout = new QHBoxLayout(zu2w_para_page);//组件2外整体布局
	QHBoxLayout* zu2n_paraHBoxLayout = new QHBoxLayout(zu2n_para_page);//组件2内整体布局

	QVBoxLayout* zu1_paraVBoxLayout1 = new QVBoxLayout(zu1_para_page);//组1左画面
	QVBoxLayout* zu1_paraVBoxLayout2 = new QVBoxLayout(zu1_para_page);//组1右画面
	QVBoxLayout* zu2w_paraVBoxLayout1 = new QVBoxLayout(zu1_para_page);//组2外左画面
	QVBoxLayout* zu2w_paraVBoxLayout2 = new QVBoxLayout(zu1_para_page);//组2外右画面
	QVBoxLayout* zu2n_paraVBoxLayout1 = new QVBoxLayout(zu1_para_page);//组2内左画面
	QVBoxLayout* zu2n_paraVBoxLayout2 = new QVBoxLayout(zu1_para_page);//组2内右画面

	//组件布局 比例1:3
	zu1_paraHBoxLayout->addLayout(zu1_paraVBoxLayout1, 1);
	zu1_paraHBoxLayout->addLayout(zu1_paraVBoxLayout2, 3);
	zu2w_paraHBoxLayout->addLayout(zu2w_paraVBoxLayout1, 1);
	zu2w_paraHBoxLayout->addLayout(zu2w_paraVBoxLayout2, 3);
	zu2n_paraHBoxLayout->addLayout(zu2n_paraVBoxLayout1, 1);
	zu2n_paraHBoxLayout->addLayout(zu2n_paraVBoxLayout2, 3);

	QStringList process_class_stringlist;
	process_class_stringlist << QStringLiteral("编号") << QStringLiteral("流  程") << QStringLiteral("作  用");
	QFont font;
	font.setBold(true); // 设置为粗体
	font.setPointSize(10); // 设置字体大小为10

	QLabel* address = new QLabel(QStringLiteral("当前图像路径："));
	QLabel* testCount = new QLabel(QStringLiteral("测试数量："));
	QLabel* NGCount = new QLabel(QStringLiteral("缺陷数量："));

	QSpacerItem* spacer1 = new QSpacerItem(500, 30);//填充弹簧
	QSpacerItem* spacer2 = new QSpacerItem(500, 30);
	QSpacerItem* spacer3 = new QSpacerItem(500, 30);
	QSpacerItem* spacer4 = new QSpacerItem(500, 30);

	QLabel* pic_state = new QLabel();//用于显示图像处理状态
	//QLabel* pic_show = new QLabel();//用于显示当前效果
	//pic_show->setStyleSheet("background-color: black;");

	QWidget* actions_widget = new QWidget(zu1_para_page);//用于跑图的画面
	actions_widget->setStyleSheet("background-color: rgb(200,200,200);");

	//构造右侧第4行，动作按钮
	QHBoxLayout* actions_h_layout = new QHBoxLayout(actions_widget);

	QPushButton* start_caitu_button = new QPushButton(actions_widget);
	QPushButton* stop_caitu_button = new QPushButton(actions_widget);
	QPushButton* single_check_button = new QPushButton(actions_widget);
	QPushButton* all_check_button = new QPushButton(actions_widget);
	QSpacerItem* actions_spacer = new QSpacerItem(300, 50);

	QToolButton* right_1step_button = new QToolButton(actions_widget);
	QToolButton* left_1step_button = new QToolButton(actions_widget);
	QToolButton* right_next20_button = new QToolButton(actions_widget);
	QToolButton* left_next20_button = new QToolButton(actions_widget);
	QToolButton* right_NG_button = new QToolButton(actions_widget);
	QToolButton* left_NG_button = new QToolButton(actions_widget);
	QToolButton* left_end_button = new QToolButton(actions_widget);
	QToolButton* right_end_button = new QToolButton(actions_widget);
	start_caitu_button->setText(QStringLiteral("开始采图"));
	stop_caitu_button->setText(QStringLiteral("停止采图"));
	single_check_button->setText(QStringLiteral("单步测试"));
	all_check_button->setText(QStringLiteral("全部测试"));

	left_1step_button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);//设置文字在图标下方
	left_1step_button->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
	left_1step_button->setText(QStringLiteral("左1步"));
	left_1step_button->setMaximumSize(80, 50);

	right_1step_button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);//设置文字在图标下方
	right_1step_button->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
	right_1step_button->setText(QStringLiteral("右1步"));
	right_1step_button->setMaximumSize(80, 50);

	left_next20_button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);//设置文字在图标下方
	left_next20_button->setIcon(QPixmap(QStringLiteral("icons/use/arrow-double-left (green).png")));
	left_next20_button->setText(QStringLiteral("左20步"));
	left_next20_button->setMaximumSize(80, 50);

	right_next20_button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);//设置文字在图标下方
	right_next20_button->setIcon(QPixmap(QStringLiteral("icons/use/arrow-double-right (green).png")));
	right_next20_button->setText(QStringLiteral("右20步"));
	right_next20_button->setMaximumSize(80, 50);

	left_NG_button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);//设置文字在图标下方
	left_NG_button->setIcon(QPixmap(QStringLiteral("icons/use/arrowleft (red).png")));
	left_NG_button->setText(QStringLiteral("查左缺陷图"));
	left_NG_button->setMaximumSize(80, 50);

	right_NG_button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);//设置文字在图标下方
	right_NG_button->setIcon(QPixmap(QStringLiteral("icons/use/arrowright (red).png")));
	right_NG_button->setText(QStringLiteral("查右缺陷图"));
	right_NG_button->setMaximumSize(QSize(80, 50));

	left_end_button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);//设置文字在图标下方
	left_end_button->setIcon(QPixmap(QStringLiteral("icons/use/page_first (green).png")));
	left_end_button->setText(QStringLiteral("最左端"));
	left_end_button->setMaximumSize(QSize(80, 50));

	right_end_button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);//设置文字在图标下方
	right_end_button->setIcon(QPixmap(QStringLiteral("icons/use/page_last (green).png")));
	right_end_button->setText(QStringLiteral("最右端"));
	right_end_button->setMaximumSize(QSize(80, 50));

	QString BtnNormalStyleSheet = "QPushButton {"
		"    background-color: #808080;"   // Default blue background
		"    color: black;"                // White text
		"    border-radius: 5px;"         // Rounded corners
		"    width:50px;"
		"    height:50px;"
		"    font-weight: bold;"
		"    font-size: 14px;"
		"    padding: 0px 0px;"          // Adjust padding
		"}"
		"QPushButton:hover {"
		"    background-color: qlineargradient("
		"        spread:pad, x1:0.6, y1:0, x2:0, y2:0.8, "  // Top to bottom gradient
		"        stop:0 #707070, stop:1 #606060"        // Lighter blue to default blue
		"    );"
		"}"
		"QPushButton:pressed {"
		"    background-color: #505050;"   // Darker blue when pressed
		"}";
	QString BtnModelslStyleSheet = "QPushButton {"
		"    background-color: #A0A0A0;"   // Default blue background
		"    color: white;"                // White text
		"    border-radius: 5px;"         // Rounded corners
		"    width:40px;"
		"    height:40px;"
		"    font-weight: bold;"
		"    font-size: 25px;"
		"    padding: 0px 0px;"          // Adjust padding
		"}"
		"QPushButton:hover {"
		"    background-color: qlineargradient("
		"        spread:pad, x1:0.6, y1:0, x2:0, y2:0.8, "  // Top to bottom gradient
		"        stop:0 #707070, stop:1 #606060"        // Lighter blue to default blue
		"    );"
		"}"
		"QPushButton:pressed {"
		"    background-color: #505050;"   // Darker blue when pressed
		"}";
	QString ToolBtnNormalStyleSheet = "QToolButton {"
		"    background-color: #A0A0A0;"   // Default blue background
		"    color: black;"                // White text
		"    border-radius: 5px;"         // Rounded corners
		"    width:50px;"
		"    height:50px;"
		"    font-weight: bold;"
		"    font-size: 14px;"
		"    padding: 0px 0px;"          // Adjust padding
		"}"
		"QToolButton:hover {"
		"    background-color: qlineargradient("
		"        spread:pad, x1:0.6, y1:0, x2:0, y2:0.8, "  // Top to bottom gradient
		"        stop:0 #707070, stop:1 #606060"        // Lighter blue to default blue
		"    );"
		"}"
		"QToolButton:pressed {"
		"    background-color: #505050;"   // Darker blue when pressed
		"}";
	actions_h_layout->addWidget(start_caitu_button);
	actions_h_layout->addWidget(stop_caitu_button);
	actions_h_layout->addWidget(single_check_button);
	actions_h_layout->addWidget(all_check_button);
	actions_h_layout->addItem(actions_spacer);
	actions_h_layout->addWidget(left_end_button);
	actions_h_layout->addWidget(left_next20_button);
	actions_h_layout->addWidget(left_1step_button);
	actions_h_layout->addWidget(right_1step_button);
	actions_h_layout->addWidget(right_next20_button);
	actions_h_layout->addWidget(right_end_button);
	actions_h_layout->addWidget(left_NG_button);
	actions_h_layout->addWidget(right_NG_button);

	start_caitu_button->setStyleSheet(BtnNormalStyleSheet);
	stop_caitu_button->setStyleSheet(BtnNormalStyleSheet);
	single_check_button->setStyleSheet(BtnNormalStyleSheet);
	all_check_button->setStyleSheet(BtnNormalStyleSheet);
	left_1step_button->setStyleSheet(ToolBtnNormalStyleSheet);
	right_1step_button->setStyleSheet(ToolBtnNormalStyleSheet);
	left_next20_button->setStyleSheet(ToolBtnNormalStyleSheet);
	right_next20_button->setStyleSheet(ToolBtnNormalStyleSheet);
	left_NG_button->setStyleSheet(ToolBtnNormalStyleSheet);
	right_NG_button->setStyleSheet(ToolBtnNormalStyleSheet);
	left_end_button->setStyleSheet(ToolBtnNormalStyleSheet);
	right_end_button->setStyleSheet(ToolBtnNormalStyleSheet);
	//////////////////////////////////////组1构造画面/////////////////////////////////////
		//QTableWidget* zu1_processTableWidget = new QTableWidget(4,3);
		//QTableWidget* zu1_operatorTableWidget = new QTableWidget(4,3);
	zu1_paraVBoxLayout1->addWidget(zu1_processTableWidget);
	zu1_paraVBoxLayout1->addWidget(zu1_operatorTableWidget);
	zu1_processTableWidget->setFixedWidth(350);
	zu1_operatorTableWidget->setFixedWidth(350);
	zu1_processTableWidget->setHorizontalHeaderLabels(process_class_stringlist);
	zu1_processTableWidget->setRowCount(6);
	zu1_processTableWidget->setItem(0, 0, new QTableWidgetItem("1"));
	zu1_processTableWidget->setItem(1, 0, new QTableWidgetItem("2"));
	zu1_processTableWidget->setItem(2, 0, new QTableWidgetItem("3"));
	zu1_processTableWidget->setItem(3, 0, new QTableWidgetItem("4"));
    zu1_processTableWidget->setItem(4, 0, new QTableWidgetItem("5"));
    zu1_processTableWidget->setItem(5, 0, new QTableWidgetItem("6"));
	zu1_processTableWidget->setItem(0, 1, new QTableWidgetItem(QStringLiteral("烟支定位")));
	zu1_processTableWidget->setItem(1, 1, new QTableWidgetItem(QStringLiteral("烟棒缺陷")));
	zu1_processTableWidget->setItem(2, 1, new QTableWidgetItem(QStringLiteral("滤嘴缺陷")));
	zu1_processTableWidget->setItem(3, 1, new QTableWidgetItem(QStringLiteral("外形缺陷")));
    zu1_processTableWidget->setItem(4, 1, new QTableWidgetItem(QStringLiteral("搭口缺陷")));
    zu1_processTableWidget->setItem(5, 1, new QTableWidgetItem(QStringLiteral("深度学习")));
	zu1_processTableWidget->setItem(0, 2, new QTableWidgetItem(QStringLiteral("准确定位")));
	zu1_processTableWidget->setItem(1, 2, new QTableWidgetItem(QStringLiteral("严重外形缺陷")));
	zu1_processTableWidget->setItem(2, 2, new QTableWidgetItem(QStringLiteral("烟棒缺陷")));
	zu1_processTableWidget->setItem(3, 2, new QTableWidgetItem(QStringLiteral("滤嘴缺陷")));
    zu1_processTableWidget->setItem(4, 2, new QTableWidgetItem(QStringLiteral("搭口缺陷")));
    zu1_processTableWidget->setItem(5, 2, new QTableWidgetItem(QStringLiteral("置信度")));

	//样式调整
	zu1_processTableWidget->verticalHeader()->hide();
	zu1_processTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	zu1_processTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

	zu1_processTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);//不可编辑
	zu1_processTableWidget->setShowGrid(false);//不显示网格
	zu1_processTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);//拉伸宽度
	zu1_processTableWidget->horizontalHeader()->setFont(font);
	zu1_processTableWidget->setColumnWidth(0, 50);//序号列，宽度
	zu1_processTableWidget->setColumnWidth(1, 150);//序号列，宽度
	zu1_processTableWidget->setColumnWidth(2, 150);//序号列，宽度
	zu1_processTableWidget->horizontalHeader()->setSectionsClickable(false);//不可点表头
	zu1_processTableWidget->setStyleSheet("QTableWidget { gridline-color: transparent; }");
	// 遍历内容，样式设置
	for (int row = 0; row < zu1_processTableWidget->rowCount(); ++row) {
		for (int col = 0; col < zu1_processTableWidget->columnCount(); ++col)
		{
			QTableWidgetItem* item = zu1_processTableWidget->item(row, col);
			if (item) {
				item->setBackground(QBrush(QColor(Qt::white)));
				item->setTextAlignment(Qt::AlignCenter);
			}
		}
	}
	zu1_operatorTableWidget->verticalHeader()->hide();
	zu1_operatorTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	zu1_operatorTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	zu1_operatorTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);//不可编辑
	zu1_operatorTableWidget->setShowGrid(false);//不显示网格
	zu1_operatorTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);//固定宽
	zu1_operatorTableWidget->horizontalHeader()->setFont(font);
	zu1_operatorTableWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	zu1_operatorTableWidget->setColumnWidth(0, 50);//序号列，宽度
	zu1_operatorTableWidget->setColumnWidth(1, 150);//序号列，宽度
	zu1_operatorTableWidget->setColumnWidth(2, 150);//序号列，宽度

	zu1_operatorTableWidget->horizontalHeader()->setSectionsClickable(false);//不可点表头
	zu1_operatorTableWidget->setStyleSheet("QTableWidget { gridline-color: transparent; }");

	zu1_para_page->setLayout(zu1_paraHBoxLayout);

	//////////////////////////右侧窗口///////////////////////////////
	//1行
	QWidget* zu1_para_right_H1_container_widget = new QWidget(zu1_para_page);//背景容器
	zu1_paraVBoxLayout2->addWidget(zu1_para_right_H1_container_widget);//容器进布局

	QHBoxLayout* zu1_para_right_H_1_Layout = new QHBoxLayout(zu1_para_right_H1_container_widget);//图路径，结果
	QHBoxLayout* zu1_para_right_H_2_Layout = new QHBoxLayout(zu1_para_right_H1_container_widget);//图
    
	QHBoxLayout* zu1_para_right_H_3_Layout = new QHBoxLayout(zu1_para_right_H1_container_widget);//图标签
	QHBoxLayout* zu1_para_right_H_4_Layout = new QHBoxLayout(zu1_para_right_H1_container_widget);//取样、跑算法等功能
	QHBoxLayout* zu1_para_right_H_5_Layout = new QHBoxLayout(zu1_para_right_H1_container_widget);//参数设置框
	zu1_paraVBoxLayout2->addLayout(zu1_para_right_H_1_Layout, 1);
	zu1_paraVBoxLayout2->addLayout(zu1_para_right_H_2_Layout, 14);
	zu1_paraVBoxLayout2->addLayout(zu1_para_right_H_3_Layout, 1);
	zu1_paraVBoxLayout2->addLayout(zu1_para_right_H_4_Layout, 1);
	zu1_paraVBoxLayout2->addLayout(zu1_para_right_H_5_Layout, 7);

	zu1_para_right_H_1_Layout->addWidget(address);
	zu1_para_right_H_1_Layout->addItem(spacer1);
	zu1_para_right_H_1_Layout->addWidget(testCount);
	zu1_para_right_H_1_Layout->addItem(spacer2);
	zu1_para_right_H_1_Layout->addWidget(NGCount);
	zu1_para_right_H_1_Layout->addItem(spacer3);
	zu1_para_right_H_1_Layout->addWidget(pic_state);

	zu1_para_right_H1_container_widget->setStyleSheet("background-color: rgb(200,200,200);");//样式
	//2行
	zu1_para_right_H_2_Layout->addWidget(view);

	//3行
	QWidget* zu1_pic_select_widget = new QWidget(zu1_para_page);
    //QLabel* zu1_pic_selects[20];
    QPushButton* zu1_pic_selects[20];
	QHBoxLayout* zu1_pic_select_h_layout = new QHBoxLayout(zu1_pic_select_widget);
	for (int i = 0;i < 20;i++)
	{
		zu1_pic_selects[i] = new QPushButton();
		zu1_pic_selects[i]->setText(QStringLiteral("%1").arg(i + 1));
		zu1_pic_selects[i]->setStyleSheet("QLabel{background-color: rgb(200,200,200);font-size: 16pt;}");
        QString objectName = QStringLiteral("modelPic%1").arg(i + 1);
        zu1_pic_selects[i]->setObjectName(objectName);
        zu1_pic_selects[i]->setStyleSheet(BtnModelslStyleSheet);
		//zu1_pic_selects[i]->setAlignment(Qt::AlignCenter);
		zu1_pic_select_h_layout->addWidget(zu1_pic_selects[i]);

		// 添加信号连接
		connect(zu1_pic_selects[i], &QPushButton::clicked, this, &CigVisionParams::onModelPicButtonClicked);
	}
	zu1_para_right_H_3_Layout->addWidget(zu1_pic_select_widget);
	zu1_pic_select_widget->setStyleSheet("background-color: rgb(100,100,100);");//样式

	zu1_para_right_H_4_Layout->addWidget(actions_widget);

	//5行
	//zu1_para_right_H_5_Layout->addWidget(zu1_para_widget);
	zu1_para_right_H_5_Layout->addWidget(paramsWidget);//将参数类放到第五行
	//链接
	connect(zu1_processTableWidget, &QTableWidget::itemClicked, this, &CigVisionParams::onProcessTableWidgetItemClicked);
	connect(zu1_operatorTableWidget, &QTableWidget::itemClicked, this, &CigVisionParams::onOperatorTableWidgetItemClicked);
}

void CigVisionParams::initParamsWidgets() {

    //参数设置小界面风格
    paramsWidget->setStyleSheet(
        "QPushButton {"
        "    background-color: #808080;"   // Default blue background
        "    color: black;"                // White text
        "    border-radius: 5px;"         // Rounded corners
        "    width:50px;"
        "    height:50px;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    padding: 0px 0px;"          // Adjust padding
        "}"
        "QPushButton:hover {"
        "    background-color: qlineargradient("
        "        spread:pad, x1:0.6, y1:0, x2:0, y2:0.8, "  // Top to bottom gradient
        "        stop:0 #707070, stop:1 #606060"        // Lighter blue to default blue
        "    );"
        "}"
        "QPushButton:pressed {"
        "    background-color: #505050;"   // Darker blue when pressed
        "}"
    );
    //样式设计
    QHBoxLayout* hLayout = new QHBoxLayout();
    paramsWidget->setLayout(hLayout);
    hLayout->addWidget(paramsTabWidget, 9);
    hLayout->addWidget(buttonsWidget, 1);
    QVBoxLayout* vLayout = new QVBoxLayout();
    QToolButton* save = new QToolButton();
    QToolButton* cancel = new QToolButton();
    buttonsWidget->setLayout(vLayout);
    vLayout->addWidget(save);
    vLayout->addWidget(cancel);
    
    save->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);//设置文字在图标下方
    save->setIcon(QPixmap(QStringLiteral("icons/use/selected(blue).png")));
    save->setText(QStringLiteral("保存"));
    //save->setMaximumSize(120, 120);
    save->setFixedSize(100, 50);
    //save->setIconSize(QSize(80,80));

    cancel->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);//设置文字在图标下方
    cancel->setIcon(QPixmap(QStringLiteral("icons/use/close (blue).png")));
    cancel->setText(QStringLiteral("恢复"));
    cancel->setFixedSize(100, 50);
    //cancel->setIconSize(QSize(80, 80));
    QString ToolBtnNormalStyleSheet = "QToolButton {"
        "    background-color: #A0A0A0;"   // Default blue background
        "    color: black;"                // White text
        "    border-radius: 5px;"         // Rounded corners
        "    width:50px;"
        "    height:50px;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    padding: 0px 0px;"          // Adjust padding
        "}"
        "QToolButton:hover {"
        "    background-color: qlineargradient("
        "        spread:pad, x1:0.6, y1:0, x2:0, y2:0.8, "  // Top to bottom gradient
        "        stop:0 #707070, stop:1 #606060"        // Lighter blue to default blue
        "    );"
        "}"
        "QToolButton:pressed {"
        "    background-color: #505050;"   // Darker blue when pressed
        "}";
    save->setStyleSheet(ToolBtnNormalStyleSheet);
    cancel->setStyleSheet(ToolBtnNormalStyleSheet);

    // 连接保存按钮和取消按钮的信号
    connect(save, &QToolButton::clicked, this, [this]() {
        saveBrandParams(currentBrand);
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("参数保存成功！"));
    });
    
    connect(cancel, &QToolButton::clicked, this, [this]() {
        loadBrandParams(currentBrand);
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("参数已恢复！"));
        // 刷新当前显示的参数界面
        int currentIndex = paramsTabWidget->currentIndex();
        if (currentIndex >= 0) {
            showParamsWidgets(select_processRow, select_operatorRow);
        }
    });

    paramsTabWidget->setTabPosition(QTabWidget::West);//左侧选项
    paramsTabWidget->clear();
    initUpCigTopParamsWidgets();
    initDownCigTopParamsWidgets();
    initUpCigBoundaryParamsWidgets();
    initDownCigBoundaryParamsWidgets();
    initCigStickROIWidgets();
    initCigFilterROIWidgets();
    initCigJointROIWidgets();
}
void CigVisionParams::initUpCigTopParamsWidgets()//初始化上烟设置界面
{
    QHBoxLayout* zu1_upCig_paraHBoxLayout = new QHBoxLayout();//整体布局
    upCigTopPosParaSet_widget->setLayout(zu1_upCig_paraHBoxLayout);
    //上烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_upCig_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_upCig_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_upCig_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_upCig_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
          //X
    QPushButton* btn_UPcenterX_add = new QPushButton();
    QPushButton* btn_UPcenterX_dec = new QPushButton();
    QLabel* lab_UPCenterX = new QLabel();
    QLineEdit* ledit_UPCenterX = new QLineEdit();
    // 删除默认值设置
    // ledit_UPCenterX->setText("0"); 
    
    //Y
    QPushButton* btn_UPcenterY_add = new QPushButton();
    QPushButton* btn_UPcenterY_dec = new QPushButton();
    QLabel* lab_UPCenterY = new QLabel();
    QLineEdit* ledit_UPCenterY = new QLineEdit();
    // ledit_UPCenterY->setText("0");
    
    //宽度
    QPushButton* btn_UPwidth_add = new QPushButton();
    QPushButton* btn_UPwidth_dec = new QPushButton();
    QLabel* lab_UPwidth = new QLabel();
    QLineEdit* ledit_UPwidth = new QLineEdit();
    // ledit_UPwidth->setText("0");
    
    //高度
    QPushButton* btn_UPheight_add = new QPushButton();
    QPushButton* btn_UPheight_dec = new QPushButton();
    QLabel* lab_UPheight = new QLabel();
    QLineEdit* ledit_UPheight = new QLineEdit();
    // ledit_UPheight->setText("0");
    
    //端西格玛值
    QPushButton* btn_UPsigma_add = new QPushButton();
    QPushButton* btn_UPsigma_dec = new QPushButton();
    QLabel* lab_UPsigma = new QLabel();
    QLineEdit* ledit_UPsigma = new QLineEdit();
    // ledit_UPsigma->setText("0");
    
    //端边界强度定位起始阈值
    QPushButton* btn_UPstartBS_add = new QPushButton();
    QPushButton* btn_UPstartBS_dec = new QPushButton();
    QLabel* lab_UPstartBS = new QLabel();
    QLineEdit* ledit_UPstartBS = new QLineEdit();
    // ledit_UPstartBS->setText("0");
    
    //端边界强度阈值步进值
    QPushButton* btn_UPstepBS_add = new QPushButton();
    QPushButton* btn_UPstepBS_dec = new QPushButton();
    QLabel* lab_UPstepBS = new QLabel();
    QLineEdit* ledit_UPstepBS = new QLineEdit();
    // ledit_UPstepBS->setText("0");

    //模版功能介绍
    QLabel* lab_UPintroduce_model = new QLabel();
    lab_UPintroduce_model->setText(QStringLiteral("模版是提供一个大致端点位置<br>超过范围报错"));
    lab_UPintroduce_model->setMaximumSize(200, 50);
    lab_UPintroduce_model->setAlignment(Qt::AlignCenter);
    lab_UPintroduce_model->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    //模版输出模版端点X值
    QPushButton* btn_UPmodelX_add = new QPushButton();
    QPushButton* btn_UPmodelX_dec = new QPushButton();
    QLabel* lab_UPmodelX = new QLabel();
    QLineEdit* ledit_UPmodelX = new QLineEdit();
    // ledit_UPmodelX->setText("0");

    //模版输出模版端点Y值
    QPushButton* btn_UPmodelY_add = new QPushButton();
    QPushButton* btn_UPmodelY_dec = new QPushButton();
    QLabel* lab_UPmodelY = new QLabel();
    QLineEdit* ledit_UPmodelY = new QLineEdit();
    // ledit_UPmodelY->setText("0");
    
    //模版输出模版宽度值
	QPushButton* btn_UPmodelWidth_add = new QPushButton();
	QPushButton* btn_UPmodelWidth_dec = new QPushButton();
	QLabel* lab_UPmodelWidth = new QLabel();
	QLineEdit* ledit_UPmodelWidth = new QLineEdit();
	// ledit_UPmodelWidth->setText("0");
    
    //模版值设定
    QPushButton* btn_UPmodelSet = new QPushButton();
    QPushButton* btn_UPmodelDel = new QPushButton();
    btn_UPmodelSet->setText(QStringLiteral("模版值设定 "));
    btn_UPmodelDel->setText(QStringLiteral("模版值清空 "));

    btn_UPmodelSet->setMinimumSize(150, 50);
    btn_UPmodelDel->setMinimumSize(150, 50);

    btn_UPcenterX_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPcenterX_add->setMaximumSize(50, 50);
    btn_UPcenterX_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPcenterX_dec->setMaximumSize(50, 50);

    lab_UPCenterX->setText(QStringLiteral("上烟中心点X"));
    lab_UPCenterX->setMaximumSize(100, 50);
    lab_UPCenterX->setAlignment(Qt::AlignCenter);
    lab_UPCenterX->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_UPCenterX->setObjectName("ledit_UPCenterX");
    ledit_UPCenterX->setMaximumSize(100, 50);
    ledit_UPCenterX->setAlignment(Qt::AlignCenter);
    ledit_UPCenterX->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPCenterX->setMaxLength(3);

    zu1_00_v1_h1->addWidget(btn_UPcenterX_dec);
    zu1_00_v1_h1->addWidget(lab_UPCenterX);
    zu1_00_v1_h1->addWidget(ledit_UPCenterX);
    zu1_00_v1_h1->addWidget(btn_UPcenterX_add);

    btn_UPcenterY_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPcenterY_add->setMaximumSize(50, 50);
    btn_UPcenterY_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPcenterY_dec->setMaximumSize(50, 50);

    lab_UPCenterY->setText(QStringLiteral("上烟中心点Y"));
    lab_UPCenterY->setMaximumSize(100, 50);
    lab_UPCenterY->setAlignment(Qt::AlignCenter);
    lab_UPCenterY->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_UPCenterY->setObjectName("ledit_UPCenterY");
    ledit_UPCenterY->setMaximumSize(100, 50);
    ledit_UPCenterY->setAlignment(Qt::AlignCenter);
    ledit_UPCenterY->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPCenterY->setMaxLength(3);

    zu1_00_v1_h2->addWidget(btn_UPcenterY_dec);
    zu1_00_v1_h2->addWidget(lab_UPCenterY);
    zu1_00_v1_h2->addWidget(ledit_UPCenterY);
    zu1_00_v1_h2->addWidget(btn_UPcenterY_add);

    btn_UPwidth_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPwidth_add->setMaximumSize(50, 50);
    btn_UPwidth_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPwidth_dec->setMaximumSize(50, 50);

    lab_UPwidth->setText(QStringLiteral("上烟宽度"));
    lab_UPwidth->setMaximumSize(100, 50);
    lab_UPwidth->setAlignment(Qt::AlignCenter);
    lab_UPwidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_UPwidth->setObjectName("ledit_UPwidth");
    ledit_UPwidth->setMaximumSize(100, 50);
    ledit_UPwidth->setAlignment(Qt::AlignCenter);
    ledit_UPwidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPwidth->setMaxLength(3);

    zu1_00_v1_h3->addWidget(btn_UPwidth_dec);
    zu1_00_v1_h3->addWidget(lab_UPwidth);
    zu1_00_v1_h3->addWidget(ledit_UPwidth);
    zu1_00_v1_h3->addWidget(btn_UPwidth_add);

    btn_UPheight_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPheight_add->setMaximumSize(50, 50);
    btn_UPheight_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPheight_dec->setMaximumSize(50, 50);

    lab_UPheight->setText(QStringLiteral("上烟高度"));
    lab_UPheight->setMaximumSize(100, 50);
    lab_UPheight->setAlignment(Qt::AlignCenter);
    lab_UPheight->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_UPheight->setObjectName("ledit_UPheight");
    ledit_UPheight->setMaximumSize(100, 50);
    ledit_UPheight->setAlignment(Qt::AlignCenter);
    ledit_UPheight->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPheight->setMaxLength(3);

    zu1_00_v1_h4->addWidget(btn_UPheight_dec);
    zu1_00_v1_h4->addWidget(lab_UPheight);
    zu1_00_v1_h4->addWidget(ledit_UPheight);
    zu1_00_v1_h4->addWidget(btn_UPheight_add);

    btn_UPsigma_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPsigma_add->setMaximumSize(50, 50);
    btn_UPsigma_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPsigma_dec->setMaximumSize(50, 50);
    lab_UPsigma->setText(QStringLiteral("西格玛值"));
    lab_UPsigma->setMaximumSize(100, 50);
    lab_UPsigma->setAlignment(Qt::AlignCenter);
    lab_UPsigma->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPsigma->setObjectName("ledit_UPsigma");
    ledit_UPsigma->setMaximumSize(100, 50);
    ledit_UPsigma->setAlignment(Qt::AlignCenter);
    ledit_UPsigma->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPsigma->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_UPsigma_dec);
    zu1_00_v2_h1->addWidget(lab_UPsigma);
    zu1_00_v2_h1->addWidget(ledit_UPsigma);
    zu1_00_v2_h1->addWidget(btn_UPsigma_add);

    btn_UPstartBS_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPstartBS_add->setMaximumSize(50, 50);
    btn_UPstartBS_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPstartBS_dec->setMaximumSize(50, 50);
    lab_UPstartBS->setText(QStringLiteral("边界强度起始阈值"));
    lab_UPstartBS->setMaximumSize(150, 50);
    lab_UPstartBS->setAlignment(Qt::AlignCenter);
    lab_UPstartBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPstartBS->setObjectName("ledit_UPstartBS");
    ledit_UPstartBS->setMaximumSize(100, 50);
    ledit_UPstartBS->setAlignment(Qt::AlignCenter);
    ledit_UPstartBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPstartBS->setMaxLength(3);

    zu1_00_v2_h2->addWidget(btn_UPstartBS_dec);
    zu1_00_v2_h2->addWidget(lab_UPstartBS);
    zu1_00_v2_h2->addWidget(ledit_UPstartBS);
    zu1_00_v2_h2->addWidget(btn_UPstartBS_add);

    btn_UPstepBS_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPstepBS_add->setMaximumSize(50, 50);
    btn_UPstepBS_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPstepBS_dec->setMaximumSize(50, 50);
    lab_UPstepBS->setText(QStringLiteral("边界强度阈值步进数"));
    lab_UPstepBS->setMaximumSize(150, 50);
    lab_UPstepBS->setAlignment(Qt::AlignCenter);
    lab_UPstepBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPstepBS->setObjectName("ledit_UPstepBS");
    ledit_UPstepBS->setMaximumSize(100, 50);
    ledit_UPstepBS->setAlignment(Qt::AlignCenter);
    ledit_UPstepBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPstepBS->setMaxLength(3);

    zu1_00_v2_h3->addWidget(btn_UPstepBS_dec);
    zu1_00_v2_h3->addWidget(lab_UPstepBS);
    zu1_00_v2_h3->addWidget(ledit_UPstepBS);
    zu1_00_v2_h3->addWidget(btn_UPstepBS_add);

    zu1_00_v3_h1->addWidget(lab_UPintroduce_model);

    btn_UPmodelX_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPmodelX_add->setMaximumSize(50, 50);
    btn_UPmodelX_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPmodelX_dec->setMaximumSize(50, 50);
    lab_UPmodelX->setText(QStringLiteral("模版输出X"));
    lab_UPmodelX->setMaximumSize(150, 50);
    lab_UPmodelX->setAlignment(Qt::AlignCenter);
    lab_UPmodelX->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPmodelX->setObjectName("ledit_UPmodelX");
    ledit_UPmodelX->setMaximumSize(100, 50);
    ledit_UPmodelX->setAlignment(Qt::AlignCenter);
    ledit_UPmodelX->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPmodelX->setMaxLength(3);

    zu1_00_v3_h2->addWidget(btn_UPmodelX_dec);
    zu1_00_v3_h2->addWidget(lab_UPmodelX);
    zu1_00_v3_h2->addWidget(ledit_UPmodelX);
    zu1_00_v3_h2->addWidget(btn_UPmodelX_add);

    btn_UPmodelY_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPmodelY_add->setMaximumSize(50, 50);
    btn_UPmodelY_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPmodelY_dec->setMaximumSize(50, 50);
    lab_UPmodelY->setText(QStringLiteral("模版输出Y"));
    lab_UPmodelY->setMaximumSize(150, 50);
    lab_UPmodelY->setAlignment(Qt::AlignCenter);
    lab_UPmodelY->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPmodelY->setObjectName("ledit_UPmodelY");
    ledit_UPmodelY->setMaximumSize(100, 50);
    ledit_UPmodelY->setAlignment(Qt::AlignCenter);
    ledit_UPmodelY->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPmodelY->setMaxLength(3);

    zu1_00_v3_h3->addWidget(btn_UPmodelY_dec);
    zu1_00_v3_h3->addWidget(lab_UPmodelY);
    zu1_00_v3_h3->addWidget(ledit_UPmodelY);
    zu1_00_v3_h3->addWidget(btn_UPmodelY_add);

    btn_UPmodelWidth_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPmodelWidth_add->setMaximumSize(50, 50);
    btn_UPmodelWidth_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPmodelWidth_dec->setMaximumSize(50, 50);
    lab_UPmodelWidth->setText(QStringLiteral("模版输出宽度"));
    lab_UPmodelWidth->setMaximumSize(150, 50);
    lab_UPmodelWidth->setAlignment(Qt::AlignCenter);
    lab_UPmodelWidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPmodelWidth->setMaximumSize(100, 50);
    ledit_UPmodelWidth->setAlignment(Qt::AlignCenter);
    ledit_UPmodelWidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPmodelWidth->setMaxLength(3);

    zu1_00_v3_h4->addWidget(btn_UPmodelWidth_dec);
    zu1_00_v3_h4->addWidget(lab_UPmodelWidth);
    zu1_00_v3_h4->addWidget(ledit_UPmodelWidth);
    zu1_00_v3_h4->addWidget(btn_UPmodelWidth_add);

    zu1_00_v4_h1->addWidget(btn_UPmodelSet);
    zu1_00_v4_h2->addWidget(btn_UPmodelDel);

    // 连接中心点X的编辑框
    connect(ledit_UPCenterX, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            upCigTopPositionParams.centerX = value;
        }
    });
    
    // 连接中心点Y的编辑框
    connect(ledit_UPCenterY, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            upCigTopPositionParams.centerY = value;
        }
    });
    
    // 连接宽度的编辑框
    connect(ledit_UPwidth, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            upCigTopPositionParams.width = value;
        }
    });
    
    // 连接高度的编辑框
    connect(ledit_UPheight, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            upCigTopPositionParams.height = value;
        }
    });
    
    // 连接西格玛值的编辑框
    connect(ledit_UPsigma, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            upCigTopPositionParams.sigmaCode = value;
        }
    });
    
    // 连接边界强度起始阈值的编辑框
    connect(ledit_UPstartBS, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            upCigTopPositionParams.edgeGradientStartThreshold = value;
        }
    });
    
    // 连接边界强度步进值的编辑框
    connect(ledit_UPstepBS, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            upCigTopPositionParams.stepGradientThreshold = value;
        }
    });
    
    // 连接模版X值的编辑框
    connect(ledit_UPmodelX, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            upCigTopModelPositionParams.modelPositionX = value;
        }
    });
    
    // 连接模版Y值的编辑框
    connect(ledit_UPmodelY, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            upCigTopModelPositionParams.modelPositionY = value;
        }
    });
    
    // 连接模版宽度值的编辑框
    connect(ledit_UPmodelWidth, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            upCigTopModelPositionParams.modelWidth = value;
        }
    });
    
    // 初始化编辑框的值
    ledit_UPCenterX->setText(QString::number(upCigTopPositionParams.centerX));
    ledit_UPCenterY->setText(QString::number(upCigTopPositionParams.centerY));
    ledit_UPwidth->setText(QString::number(upCigTopPositionParams.width));
    ledit_UPheight->setText(QString::number(upCigTopPositionParams.height));
    ledit_UPsigma->setText(QString::number(upCigTopPositionParams.sigmaCode));
    ledit_UPstartBS->setText(QString::number(upCigTopPositionParams.edgeGradientStartThreshold));
    ledit_UPstepBS->setText(QString::number(upCigTopPositionParams.stepGradientThreshold));
    ledit_UPmodelX->setText(QString::number(upCigTopModelPositionParams.modelPositionX));
    ledit_UPmodelY->setText(QString::number(upCigTopModelPositionParams.modelPositionY));
    ledit_UPmodelWidth->setText(QString::number(upCigTopModelPositionParams.modelWidth));
}
void CigVisionParams::initDownCigTopParamsWidgets()//初始化下烟设置界面
{
    QHBoxLayout* zu1_downCig_paraHBoxLayout = new QHBoxLayout();//整体布局
    downCigTopPosParaSet_widget->setLayout(zu1_downCig_paraHBoxLayout);
    //下烟
    QVBoxLayout* zu1_01_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_01_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_01_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_01_v4 = new QVBoxLayout();//行
    //QSpacerItem* spacer2 = new QSpacerItem(50, 30);//填充弹簧

    zu1_downCig_paraHBoxLayout->addLayout(zu1_01_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_downCig_paraHBoxLayout->addLayout(zu1_01_v2, 1);
    zu1_downCig_paraHBoxLayout->addLayout(zu1_01_v3, 1);
    zu1_downCig_paraHBoxLayout->addLayout(zu1_01_v4, 1);

    QHBoxLayout* zu1_01_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_01_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_01_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_01_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_01_v4_h4 = new QHBoxLayout();//列

    zu1_01_v1->addLayout(zu1_01_v1_h1);
    zu1_01_v1->addLayout(zu1_01_v1_h2);
    zu1_01_v1->addLayout(zu1_01_v1_h3);
    zu1_01_v1->addLayout(zu1_01_v1_h4);

    zu1_01_v2->addLayout(zu1_01_v2_h1);
    zu1_01_v2->addLayout(zu1_01_v2_h2);
    zu1_01_v2->addLayout(zu1_01_v2_h3);
    zu1_01_v2->addLayout(zu1_01_v2_h4);

    zu1_01_v3->addLayout(zu1_01_v3_h1);
    zu1_01_v3->addLayout(zu1_01_v3_h2);
    zu1_01_v3->addLayout(zu1_01_v3_h3);
    zu1_01_v3->addLayout(zu1_01_v3_h4);

    zu1_01_v4->addLayout(zu1_01_v4_h1);
    zu1_01_v4->addLayout(zu1_01_v4_h2);
    zu1_01_v4->addLayout(zu1_01_v4_h3);
    zu1_01_v4->addLayout(zu1_01_v4_h4);

    //下图
    //X
    QPushButton* btn_DowncenterX_add = new QPushButton();
    QPushButton* btn_DowncenterX_dec = new QPushButton();
    QLabel* lab_DownCenterX = new QLabel();
    QLineEdit* ledit_DownCenterX = new QLineEdit();
    ledit_DownCenterX->setText("0"); // 使用默认值0
    //Y
    QPushButton* btn_DowncenterY_add = new QPushButton();
    QPushButton* btn_DowncenterY_dec = new QPushButton();
    QLabel* lab_DownCenterY = new QLabel();
    QLineEdit* ledit_DownCenterY = new QLineEdit();
    ledit_DownCenterY->setText("0"); // 使用默认值0
    //宽度
    QPushButton* btn_Downwidth_add = new QPushButton();
    QPushButton* btn_Downwidth_dec = new QPushButton();
    QLabel* lab_Downwidth = new QLabel();
    QLineEdit* ledit_Downwidth = new QLineEdit();
    ledit_Downwidth->setText("0"); // 使用默认值0
    //高度
    QPushButton* btn_Downheight_add = new QPushButton();
    QPushButton* btn_Downheight_dec = new QPushButton();
    QLabel* lab_Downheight = new QLabel();
    QLineEdit* ledit_Downheight = new QLineEdit();
    ledit_Downheight->setText("0"); // 使用默认值0
    //端西格玛值
    QPushButton* btn_Downsigma_add = new QPushButton();
    QPushButton* btn_Downsigma_dec = new QPushButton();
    QLabel* lab_Downsigma = new QLabel();
    QLineEdit* ledit_Downsigma = new QLineEdit();
    ledit_Downsigma->setText("0"); // 使用默认值0
    //端边界强度定位起始阈值
    QPushButton* btn_DownstartBS_add = new QPushButton();
    QPushButton* btn_DownstartBS_dec = new QPushButton();
    QLabel* lab_DownstartBS = new QLabel();
    QLineEdit* ledit_DownstartBS = new QLineEdit();
    ledit_DownstartBS->setText("0"); // 使用默认值0
    //端边界强度阈值步进值
    QPushButton* btn_DownstepBS_add = new QPushButton();
    QPushButton* btn_DownstepBS_dec = new QPushButton();
    QLabel* lab_DownstepBS = new QLabel();
    QLineEdit* ledit_DownstepBS = new QLineEdit();
    ledit_DownstepBS->setText("0"); // 使用默认值0
    //模版功能介绍
    QLabel* lab_Downintroduce_model = new QLabel();
    lab_Downintroduce_model->setText(QStringLiteral("模版是提供一个大致端点位置<br>超过范围报错"));
    lab_Downintroduce_model->setMaximumSize(200, 50);
    lab_Downintroduce_model->setAlignment(Qt::AlignCenter);
    lab_Downintroduce_model->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    //模版输出模版端点X值
    QPushButton* btn_DownmodelX_add = new QPushButton();
    QPushButton* btn_DownmodelX_dec = new QPushButton();
    QLabel* lab_DownmodelX = new QLabel();
    QLineEdit* ledit_DownmodelX = new QLineEdit();
    ledit_DownmodelX->setText("0"); // 使用默认值0
    //模版输出模版端点Y值
    QPushButton* btn_DownmodelY_add = new QPushButton();
    QPushButton* btn_DownmodelY_dec = new QPushButton();
    QLabel* lab_DownmodelY = new QLabel();
    QLineEdit* ledit_DownmodelY = new QLineEdit();
    ledit_DownmodelY->setText("0"); // 使用默认值0
    
	//模版输出模版宽度值
	QPushButton* btn_DownmodelWidth_add = new QPushButton();
	QPushButton* btn_DownmodelWidth_dec = new QPushButton();
	QLabel* lab_DownmodelWidth = new QLabel();
	QLineEdit* ledit_DownmodelWidth = new QLineEdit();
	ledit_DownmodelWidth->setText("0");
	ledit_DownmodelWidth->setObjectName("ledit_DownmodelWidth");
	ledit_DownmodelWidth->setMaximumSize(100, 50);
	ledit_DownmodelWidth->setAlignment(Qt::AlignCenter);
    //模版值设定
    QPushButton* btn_DownmodelSet = new QPushButton();
    QPushButton* btn_DownmodelDel = new QPushButton();
    btn_DownmodelSet->setText(QStringLiteral("模版值设定 "));
    btn_DownmodelDel->setText(QStringLiteral("模版值清空 "));

    btn_DownmodelSet->setMinimumSize(150, 50);
    btn_DownmodelDel->setMinimumSize(150, 50);

    btn_DowncenterX_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DowncenterX_add->setMaximumSize(50, 50);
    btn_DowncenterX_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DowncenterX_dec->setMaximumSize(50, 50);

    lab_DownCenterX->setText(QStringLiteral("下烟中心点X"));
    lab_DownCenterX->setMaximumSize(100, 50);
    lab_DownCenterX->setAlignment(Qt::AlignCenter);
    lab_DownCenterX->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_DownCenterX->setObjectName("ledit_DownCenterX");
    ledit_DownCenterX->setMaximumSize(100, 50);
    ledit_DownCenterX->setAlignment(Qt::AlignCenter);
    ledit_DownCenterX->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCenterX->setMaxLength(3);

    zu1_01_v1_h1->addWidget(btn_DowncenterX_dec);
    zu1_01_v1_h1->addWidget(lab_DownCenterX);
    zu1_01_v1_h1->addWidget(ledit_DownCenterX);
    zu1_01_v1_h1->addWidget(btn_DowncenterX_add);

    btn_DowncenterY_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DowncenterY_add->setMaximumSize(50, 50);
    btn_DowncenterY_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DowncenterY_dec->setMaximumSize(50, 50);

    lab_DownCenterY->setText(QStringLiteral("下烟中心点Y"));
    lab_DownCenterY->setMaximumSize(100, 50);
    lab_DownCenterY->setAlignment(Qt::AlignCenter);
    lab_DownCenterY->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_DownCenterY->setObjectName("ledit_DownCenterY");
    ledit_DownCenterY->setMaximumSize(100, 50);
    ledit_DownCenterY->setAlignment(Qt::AlignCenter);
    ledit_DownCenterY->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCenterY->setMaxLength(3);

    zu1_01_v1_h2->addWidget(btn_DowncenterY_dec);
    zu1_01_v1_h2->addWidget(lab_DownCenterY);
    zu1_01_v1_h2->addWidget(ledit_DownCenterY);
    zu1_01_v1_h2->addWidget(btn_DowncenterY_add);

    btn_Downwidth_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_Downwidth_add->setMaximumSize(50, 50);
    btn_Downwidth_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_Downwidth_dec->setMaximumSize(50, 50);

    lab_Downwidth->setText(QStringLiteral("下烟宽度"));
    lab_Downwidth->setMaximumSize(100, 50);
    lab_Downwidth->setAlignment(Qt::AlignCenter);
    lab_Downwidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_Downwidth->setObjectName("ledit_Downwidth");
    ledit_Downwidth->setMaximumSize(100, 50);
    ledit_Downwidth->setAlignment(Qt::AlignCenter);
    ledit_Downwidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_Downwidth->setMaxLength(3);

    zu1_01_v1_h3->addWidget(btn_Downwidth_dec);
    zu1_01_v1_h3->addWidget(lab_Downwidth);
    zu1_01_v1_h3->addWidget(ledit_Downwidth);
    zu1_01_v1_h3->addWidget(btn_Downwidth_add);

    btn_Downheight_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_Downheight_add->setMaximumSize(50, 50);
    btn_Downheight_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_Downheight_dec->setMaximumSize(50, 50);

    lab_Downheight->setText(QStringLiteral("下烟高度"));
    lab_Downheight->setMaximumSize(100, 50);
    lab_Downheight->setAlignment(Qt::AlignCenter);
    lab_Downheight->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_Downheight->setObjectName("ledit_Downheight");
    ledit_Downheight->setMaximumSize(100, 50);
    ledit_Downheight->setAlignment(Qt::AlignCenter);
    ledit_Downheight->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_Downheight->setMaxLength(3);

    zu1_01_v1_h4->addWidget(btn_Downheight_dec);
    zu1_01_v1_h4->addWidget(lab_Downheight);
    zu1_01_v1_h4->addWidget(ledit_Downheight);
    zu1_01_v1_h4->addWidget(btn_Downheight_add);

    btn_Downsigma_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_Downsigma_add->setMaximumSize(50, 50);
    btn_Downsigma_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_Downsigma_dec->setMaximumSize(50, 50);
    lab_Downsigma->setText(QStringLiteral("西格玛值"));
    lab_Downsigma->setMaximumSize(100, 50);
    lab_Downsigma->setAlignment(Qt::AlignCenter);
    lab_Downsigma->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_Downsigma->setObjectName("ledit_Downsigma");
    ledit_Downsigma->setMaximumSize(100, 50);
    ledit_Downsigma->setAlignment(Qt::AlignCenter);
    ledit_Downsigma->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_Downsigma->setMaxLength(3);

    zu1_01_v2_h1->addWidget(btn_Downsigma_dec);
    zu1_01_v2_h1->addWidget(lab_Downsigma);
    zu1_01_v2_h1->addWidget(ledit_Downsigma);
    zu1_01_v2_h1->addWidget(btn_Downsigma_add);

    btn_DownstartBS_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownstartBS_add->setMaximumSize(50, 50);
    btn_DownstartBS_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownstartBS_dec->setMaximumSize(50, 50);
    lab_DownstartBS->setText(QStringLiteral("边界强度起始阈值"));
    lab_DownstartBS->setMaximumSize(150, 50);
    lab_DownstartBS->setAlignment(Qt::AlignCenter);
    lab_DownstartBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownstartBS->setObjectName("ledit_DownstartBS");
    ledit_DownstartBS->setMaximumSize(100, 50);
    ledit_DownstartBS->setAlignment(Qt::AlignCenter);
    ledit_DownstartBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownstartBS->setMaxLength(3);

    zu1_01_v2_h2->addWidget(btn_DownstartBS_dec);
    zu1_01_v2_h2->addWidget(lab_DownstartBS);
    zu1_01_v2_h2->addWidget(ledit_DownstartBS);
    zu1_01_v2_h2->addWidget(btn_DownstartBS_add);

    btn_DownstepBS_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownstepBS_add->setMaximumSize(50, 50);
    btn_DownstepBS_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownstepBS_dec->setMaximumSize(50, 50);
    lab_DownstepBS->setText(QStringLiteral("边界强度阈值步进数"));
    lab_DownstepBS->setMaximumSize(150, 50);
    lab_DownstepBS->setAlignment(Qt::AlignCenter);
    lab_DownstepBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownstepBS->setObjectName("ledit_DownstepBS");
    ledit_DownstepBS->setMaximumSize(100, 50);
    ledit_DownstepBS->setAlignment(Qt::AlignCenter);
    ledit_DownstepBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownstepBS->setMaxLength(3);

    zu1_01_v2_h3->addWidget(btn_DownstepBS_dec);
    zu1_01_v2_h3->addWidget(lab_DownstepBS);
    zu1_01_v2_h3->addWidget(ledit_DownstepBS);
    zu1_01_v2_h3->addWidget(btn_DownstepBS_add);

    zu1_01_v3_h1->addWidget(lab_Downintroduce_model);
    btn_DownmodelX_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownmodelX_add->setMaximumSize(50, 50);
    btn_DownmodelX_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownmodelX_dec->setMaximumSize(50, 50);
    lab_DownmodelX->setText(QStringLiteral("模版输出X"));
    lab_DownmodelX->setMaximumSize(150, 50);
    lab_DownmodelX->setAlignment(Qt::AlignCenter);
    lab_DownmodelX->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownmodelX->setObjectName("ledit_DownmodelX");
    ledit_DownmodelX->setMaximumSize(100, 50);
    ledit_DownmodelX->setAlignment(Qt::AlignCenter);
    ledit_DownmodelX->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownmodelX->setMaxLength(3);

    zu1_01_v3_h2->addWidget(btn_DownmodelX_dec);
    zu1_01_v3_h2->addWidget(lab_DownmodelX);
    zu1_01_v3_h2->addWidget(ledit_DownmodelX);
    zu1_01_v3_h2->addWidget(btn_DownmodelX_add);

    btn_DownmodelY_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownmodelY_add->setMaximumSize(50, 50);
    btn_DownmodelY_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownmodelY_dec->setMaximumSize(50, 50);
    lab_DownmodelY->setText(QStringLiteral("模版输出Y"));
    lab_DownmodelY->setMaximumSize(150, 50);
    lab_DownmodelY->setAlignment(Qt::AlignCenter);
    lab_DownmodelY->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownmodelY->setObjectName("ledit_DownmodelY");
    ledit_DownmodelY->setMaximumSize(100, 50);
    ledit_DownmodelY->setAlignment(Qt::AlignCenter);
    ledit_DownmodelY->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownmodelY->setMaxLength(3);

    zu1_01_v3_h3->addWidget(btn_DownmodelY_dec);
    zu1_01_v3_h3->addWidget(lab_DownmodelY);
    zu1_01_v3_h3->addWidget(ledit_DownmodelY);
    zu1_01_v3_h3->addWidget(btn_DownmodelY_add);

	btn_DownmodelWidth_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
	btn_DownmodelWidth_add->setMaximumSize(50, 50);
	btn_DownmodelWidth_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
	btn_DownmodelWidth_dec->setMaximumSize(50, 50);
	lab_DownmodelWidth->setText(QStringLiteral("模版输出宽度"));
	lab_DownmodelWidth->setMaximumSize(150, 50);
	lab_DownmodelWidth->setAlignment(Qt::AlignCenter);
	lab_DownmodelWidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_DownmodelWidth->setMaximumSize(100, 50);
	ledit_DownmodelWidth->setAlignment(Qt::AlignCenter);
	ledit_DownmodelWidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_DownmodelWidth->setMaxLength(3);

	zu1_01_v3_h4->addWidget(btn_DownmodelWidth_dec);
	zu1_01_v3_h4->addWidget(lab_DownmodelWidth);
	zu1_01_v3_h4->addWidget(ledit_DownmodelWidth);
	zu1_01_v3_h4->addWidget(btn_DownmodelWidth_add);

    zu1_01_v4_h1->addWidget(btn_DownmodelSet);
    zu1_01_v4_h2->addWidget(btn_DownmodelDel);

     // 连接中心点X的编辑框
    connect(ledit_DownCenterX, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            downCigTopPositionParams.centerX = value;
        }
    });
    
    // 连接中心点Y的编辑框
    connect(ledit_DownCenterY, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            downCigTopPositionParams.centerY = value;
        }
    });
    
    // 连接宽度的编辑框
    connect(ledit_Downwidth, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            downCigTopPositionParams.width = value;
        }
    });
    
    // 连接高度的编辑框
    connect(ledit_Downheight, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            downCigTopPositionParams.height = value;
        }
    });
    
    // 连接西格玛值的编辑框
    connect(ledit_Downsigma, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            downCigTopPositionParams.sigmaCode = value;
        }
    });
    
    // 连接边界强度起始阈值的编辑框
    connect(ledit_DownstartBS, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            downCigTopPositionParams.edgeGradientStartThreshold = value;
        }
    });
    
    // 连接边界强度步进值的编辑框
    connect(ledit_DownstepBS, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            downCigTopPositionParams.stepGradientThreshold = value;
        }
    });
    
    // 连接模版X值的编辑框
    connect(ledit_DownmodelX, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            downCigTopModelPositionParams.modelPositionX = value;
        }
    });
    
    // 连接模版Y值的编辑框
    connect(ledit_DownmodelY, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            downCigTopModelPositionParams.modelPositionY = value;
        }
    });
    
    // 连接模版宽度值的编辑框
    connect(ledit_DownmodelWidth, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int value = text.toInt(&ok);
        if (ok) {
            downCigTopModelPositionParams.modelWidth = value;
        }
    });
    
    // 初始化编辑框的值
    ledit_DownCenterX->setText(QString::number(downCigTopPositionParams.centerX));
    ledit_DownCenterY->setText(QString::number(downCigTopPositionParams.centerY));
    ledit_Downwidth->setText(QString::number(downCigTopPositionParams.width));
    ledit_Downheight->setText(QString::number(downCigTopPositionParams.height));
    ledit_Downsigma->setText(QString::number(downCigTopPositionParams.sigmaCode));
    ledit_DownstartBS->setText(QString::number(downCigTopPositionParams.edgeGradientStartThreshold));
    ledit_DownstepBS->setText(QString::number(downCigTopPositionParams.stepGradientThreshold));
    ledit_DownmodelX->setText(QString::number(downCigTopModelPositionParams.modelPositionX));
    ledit_DownmodelY->setText(QString::number(downCigTopModelPositionParams.modelPositionY));
    ledit_DownmodelWidth->setText(QString::number(downCigTopModelPositionParams.modelWidth));
}

void CigVisionParams::initUpCigBoundaryParamsWidgets()//初始化上烟边设置界面
{
    QHBoxLayout* zu1_upCig_paraHBoxLayout = new QHBoxLayout();//整体布局
    upCigBoundryPosParaSet_widget->setLayout(zu1_upCig_paraHBoxLayout);
    //上烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_upCig_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_upCig_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_upCig_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_upCig_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
    //起始距离
    QPushButton* btn_UPStartDistance_add = new QPushButton();
    QPushButton* btn_UPStartDistance_dec = new QPushButton();
    QLabel* lab_UPStartDistance = new QLabel();
    QLineEdit* ledit_UPStartDistance = new QLineEdit();
    ledit_UPStartDistance->setText("0");

    //框到框水平距离
    QPushButton* btn_UPRectDistance_add = new QPushButton();
    QPushButton* btn_UPRectDistance_dec = new QPushButton();
    QLabel* lab_UPRectDistance = new QLabel();
    QLineEdit* ledit_UPRectDistance = new QLineEdit();
    ledit_UPRectDistance->setText("0");
    //宽度
    QPushButton* btn_UPwidth_add = new QPushButton();
    QPushButton* btn_UPwidth_dec = new QPushButton();
    QLabel* lab_UPwidth = new QLabel();
    QLineEdit* ledit_UPwidth = new QLineEdit();
    ledit_UPwidth->setText("0");
    //高度
    QPushButton* btn_UPheight_add = new QPushButton();
    QPushButton* btn_UPheight_dec = new QPushButton();
    QLabel* lab_UPheight = new QLabel();
    QLineEdit* ledit_UPheight = new QLineEdit();
    ledit_UPheight->setText("0");
    //边西格玛值
    QPushButton* btn_UPsigma_add = new QPushButton();
    QPushButton* btn_UPsigma_dec = new QPushButton();
    QLabel* lab_UPsigma = new QLabel();
    QLineEdit* ledit_UPsigma = new QLineEdit();
    ledit_UPsigma->setText("0");
    //边边界强度定位起始阈值
    QPushButton* btn_UPstartBS_add = new QPushButton();
    QPushButton* btn_UPstartBS_dec = new QPushButton();
    QLabel* lab_UPstartBS = new QLabel();
    QLineEdit* ledit_UPstartBS = new QLineEdit();
    ledit_UPstartBS->setText("0");
    //端边界强度阈值步进值
    QPushButton* btn_UPstepBS_add = new QPushButton();
    QPushButton* btn_UPstepBS_dec = new QPushButton();
    QLabel* lab_UPstepBS = new QLabel();
    QLineEdit* ledit_UPstepBS = new QLineEdit();
    ledit_UPstepBS->setText("0");

    //模版功能介绍
    QLabel* lab_UPintroduce_model = new QLabel();
    lab_UPintroduce_model->setText(QStringLiteral("模版提供边界参数<br>过大偏离报错"));
    lab_UPintroduce_model->setMaximumSize(200, 50);
    lab_UPintroduce_model->setAlignment(Qt::AlignCenter);
    lab_UPintroduce_model->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ////模版输出模版上边中心点X
    //QPushButton* btn_UPmodelX_add = new QPushButton();
    //QPushButton* btn_UPmodelX_dec = new QPushButton();
    QLabel* lab_UPmodel1 = new QLabel();
    QLabel* lab_UPmodel2 = new QLabel();
    QLabel* lab_UPmodel3 = new QLabel();
	QLineEdit* ledit_UPmodel1X = new QLineEdit();
	QLineEdit* ledit_UPmodel2X = new QLineEdit();
	QLineEdit* ledit_UPmodel3X = new QLineEdit();
	QLineEdit* ledit_UPmodel1Y = new QLineEdit();
	QLineEdit* ledit_UPmodel2Y = new QLineEdit();
	QLineEdit* ledit_UPmodel3Y = new QLineEdit();
	QLineEdit* ledit_UPmodel1Width = new QLineEdit();
	QLineEdit* ledit_UPmodel2Width = new QLineEdit();
	QLineEdit* ledit_UPmodel3Width = new QLineEdit();
    
    ledit_UPmodel1X->setText("0");
    ledit_UPmodel2X->setText("0");
    ledit_UPmodel3X->setText("0");
    ledit_UPmodel1Y->setText("0");
    ledit_UPmodel2Y->setText("0");
    ledit_UPmodel3Y->setText("0");
    ledit_UPmodel1Width->setText("0");
    ledit_UPmodel2Width->setText("0");
    ledit_UPmodel3Width->setText("0");
 
    //模版值设定
    QPushButton* btn_UPmodelSet = new QPushButton();
    QPushButton* btn_UPmodelDel = new QPushButton();
    btn_UPmodelSet->setText(QStringLiteral("模版值设定 "));
    btn_UPmodelDel->setText(QStringLiteral("模版值清空 "));

    btn_UPmodelSet->setMinimumSize(150, 50);
    btn_UPmodelDel->setMinimumSize(150, 50);

    btn_UPStartDistance_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPStartDistance_add->setMaximumSize(50, 50);
    btn_UPStartDistance_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPStartDistance_dec->setMaximumSize(50, 50);

    lab_UPStartDistance->setText(QStringLiteral("上烟定位框水平起始距离"));
    lab_UPStartDistance->setMaximumSize(150, 50);
    lab_UPStartDistance->setAlignment(Qt::AlignCenter);
    lab_UPStartDistance->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_UPStartDistance->setMaximumSize(50, 50);
    ledit_UPStartDistance->setAlignment(Qt::AlignCenter);
    ledit_UPStartDistance->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPStartDistance->setMaxLength(3);

    zu1_00_v1_h1->addWidget(btn_UPStartDistance_dec);
    zu1_00_v1_h1->addWidget(lab_UPStartDistance);
    zu1_00_v1_h1->addWidget(ledit_UPStartDistance);
    zu1_00_v1_h1->addWidget(btn_UPStartDistance_add);

    btn_UPRectDistance_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPRectDistance_add->setMaximumSize(50, 50);
    btn_UPRectDistance_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPRectDistance_dec->setMaximumSize(50, 50);

    lab_UPRectDistance->setText(QStringLiteral("上烟定位框水平间距"));
    lab_UPRectDistance->setMaximumSize(150, 50);
    lab_UPRectDistance->setAlignment(Qt::AlignCenter);
    lab_UPRectDistance->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_UPRectDistance->setMaximumSize(50, 50);
    ledit_UPRectDistance->setAlignment(Qt::AlignCenter);
    ledit_UPRectDistance->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPRectDistance->setMaxLength(3);

    zu1_00_v1_h2->addWidget(btn_UPRectDistance_dec);
    zu1_00_v1_h2->addWidget(lab_UPRectDistance);
    zu1_00_v1_h2->addWidget(ledit_UPRectDistance);
    zu1_00_v1_h2->addWidget(btn_UPRectDistance_add);

    btn_UPwidth_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPwidth_add->setMaximumSize(50, 50);
    btn_UPwidth_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPwidth_dec->setMaximumSize(50, 50);

    lab_UPwidth->setText(QStringLiteral("上烟框宽度"));
    lab_UPwidth->setMaximumSize(100, 50);
    lab_UPwidth->setAlignment(Qt::AlignCenter);
    lab_UPwidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_UPwidth->setMaximumSize(100, 50);
    ledit_UPwidth->setAlignment(Qt::AlignCenter);
    ledit_UPwidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPwidth->setMaxLength(3);

    zu1_00_v1_h3->addWidget(btn_UPwidth_dec);
    zu1_00_v1_h3->addWidget(lab_UPwidth);
    zu1_00_v1_h3->addWidget(ledit_UPwidth);
    zu1_00_v1_h3->addWidget(btn_UPwidth_add);

    btn_UPheight_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPheight_add->setMaximumSize(50, 50);
    btn_UPheight_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPheight_dec->setMaximumSize(50, 50);

    lab_UPheight->setText(QStringLiteral("上烟框高度"));
    lab_UPheight->setMaximumSize(100, 50);
    lab_UPheight->setAlignment(Qt::AlignCenter);
    lab_UPheight->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_UPheight->setMaximumSize(100, 50);
    ledit_UPheight->setAlignment(Qt::AlignCenter);
    ledit_UPheight->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPheight->setMaxLength(3);

    zu1_00_v1_h4->addWidget(btn_UPheight_dec);
    zu1_00_v1_h4->addWidget(lab_UPheight);
    zu1_00_v1_h4->addWidget(ledit_UPheight);
    zu1_00_v1_h4->addWidget(btn_UPheight_add);

    btn_UPsigma_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPsigma_add->setMaximumSize(50, 50);
    btn_UPsigma_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPsigma_dec->setMaximumSize(50, 50);
    lab_UPsigma->setText(QStringLiteral("西格玛值"));
    lab_UPsigma->setMaximumSize(100, 50);
    lab_UPsigma->setAlignment(Qt::AlignCenter);
    lab_UPsigma->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPsigma->setMaximumSize(100, 50);
    ledit_UPsigma->setAlignment(Qt::AlignCenter);
    ledit_UPsigma->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPsigma->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_UPsigma_dec);
    zu1_00_v2_h1->addWidget(lab_UPsigma);
    zu1_00_v2_h1->addWidget(ledit_UPsigma);
    zu1_00_v2_h1->addWidget(btn_UPsigma_add);

    btn_UPstartBS_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UPstartBS_add->setMaximumSize(50, 50);
    btn_UPstartBS_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UPstartBS_dec->setMaximumSize(50, 50);
    lab_UPstartBS->setText(QStringLiteral("边界强度起始阈值"));
    lab_UPstartBS->setMaximumSize(200, 50);
    lab_UPstartBS->setAlignment(Qt::AlignCenter);
    lab_UPstartBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPstartBS->setMaximumSize(50, 50);
    ledit_UPstartBS->setAlignment(Qt::AlignCenter);
    ledit_UPstartBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPstartBS->setMaxLength(3);

    zu1_00_v2_h2->addWidget(btn_UPstartBS_dec);
    zu1_00_v2_h2->addWidget(lab_UPstartBS);
    zu1_00_v2_h2->addWidget(ledit_UPstartBS);
    zu1_00_v2_h2->addWidget(btn_UPstartBS_add);

	/*btn_UPstepBS_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
	btn_UPstepBS_add->setMaximumSize(50, 50);
	btn_UPstepBS_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
	btn_UPstepBS_dec->setMaximumSize(50, 50);
	lab_UPstepBS->setText(QStringLiteral("边界强度阈值步进数"));
    lab_UPstepBS->setMaximumSize(150, 50);
    lab_UPstepBS->setAlignment(Qt::AlignCenter);
    lab_UPstepBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPstepBS->setMaximumSize(100, 50);
    ledit_UPstepBS->setAlignment(Qt::AlignCenter);
    ledit_UPstepBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPstepBS->setMaxLength(3);*/

   /* zu1_00_v2_h3->addWidget(btn_UPstepBS_dec);
    zu1_00_v2_h3->addWidget(lab_UPstepBS);
    zu1_00_v2_h3->addWidget(ledit_UPstepBS);
    zu1_00_v2_h3->addWidget(btn_UPstepBS_add);*/

    //zu1_00_v3_h1->addWidget(lab_UPintroduce_model);

    lab_UPmodel1->setText(QStringLiteral("模版1输出:X,Y,W"));
    lab_UPmodel2->setText(QStringLiteral("模版2输出:X,Y,W"));
    lab_UPmodel3->setText(QStringLiteral("模版3输出:X,Y,W"));
    lab_UPmodel1->setMaximumSize(150, 50);
    lab_UPmodel2->setMaximumSize(150, 50);
    lab_UPmodel3->setMaximumSize(150, 50);
	lab_UPmodel1->setAlignment(Qt::AlignCenter);
	lab_UPmodel1->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	lab_UPmodel2->setAlignment(Qt::AlignCenter);
	lab_UPmodel2->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	lab_UPmodel3->setAlignment(Qt::AlignCenter);
	lab_UPmodel3->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

	ledit_UPmodel1X->setObjectName("ledit_UPmodel1X");
	ledit_UPmodel1X->setMaximumSize(50, 50);
	ledit_UPmodel1X->setAlignment(Qt::AlignCenter);
	ledit_UPmodel1X->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPmodel1X->setMaxLength(3);
	ledit_UPmodel1Y->setObjectName("ledit_UPmodel1Y");
	ledit_UPmodel1Y->setMaximumSize(50, 50);
	ledit_UPmodel1Y->setAlignment(Qt::AlignCenter);
    ledit_UPmodel1Y->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPmodel1Y->setMaxLength(3);
    ledit_UPmodel1Width->setObjectName("ledit_UPmodel1Width");
    ledit_UPmodel1Width->setMaximumSize(50, 50);
    ledit_UPmodel1Width->setAlignment(Qt::AlignCenter);
    ledit_UPmodel1Width->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPmodel1Width->setMaxLength(3);

	ledit_UPmodel2X->setObjectName("ledit_UPmodel2X");
	ledit_UPmodel2X->setMaximumSize(50, 50);
	ledit_UPmodel2X->setAlignment(Qt::AlignCenter);
	ledit_UPmodel2X->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_UPmodel2X->setMaxLength(3);
	ledit_UPmodel2Y->setObjectName("ledit_UPmodel2Y");
	ledit_UPmodel2Y->setMaximumSize(50, 50);
	ledit_UPmodel2Y->setAlignment(Qt::AlignCenter);
	ledit_UPmodel2Y->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_UPmodel2Y->setMaxLength(3);
	ledit_UPmodel2Width->setObjectName("ledit_UPmodel2Width");
	ledit_UPmodel2Width->setMaximumSize(50, 50);
	ledit_UPmodel2Width->setAlignment(Qt::AlignCenter);
	ledit_UPmodel2Width->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_UPmodel2Width->setMaxLength(3);

	ledit_UPmodel3X->setObjectName("ledit_UPmodel3X");
	ledit_UPmodel3X->setMaximumSize(50, 50);
	ledit_UPmodel3X->setAlignment(Qt::AlignCenter);
	ledit_UPmodel3X->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_UPmodel3X->setMaxLength(3);
	ledit_UPmodel3Y->setObjectName("ledit_UPmodel3Y");
	ledit_UPmodel3Y->setMaximumSize(50, 50);
	ledit_UPmodel3Y->setAlignment(Qt::AlignCenter);
	ledit_UPmodel3Y->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_UPmodel3Y->setMaxLength(3);
	ledit_UPmodel3Width->setObjectName("ledit_UPmodel3Width");
	ledit_UPmodel3Width->setMaximumSize(50, 50);
	ledit_UPmodel3Width->setAlignment(Qt::AlignCenter);
	ledit_UPmodel3Width->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_UPmodel3Width->setMaxLength(3);

	zu1_00_v3_h1->addWidget(lab_UPmodel1);
	zu1_00_v3_h1->addWidget(ledit_UPmodel1X);
	zu1_00_v3_h1->addWidget(ledit_UPmodel1Y);
	zu1_00_v3_h1->addWidget(ledit_UPmodel1Width);
	zu1_00_v3_h2->addWidget(lab_UPmodel2);
	zu1_00_v3_h2->addWidget(ledit_UPmodel2X);
	zu1_00_v3_h2->addWidget(ledit_UPmodel2Y);
	zu1_00_v3_h2->addWidget(ledit_UPmodel2Width);
	zu1_00_v3_h3->addWidget(lab_UPmodel3);
	zu1_00_v3_h3->addWidget(ledit_UPmodel3X);
	zu1_00_v3_h3->addWidget(ledit_UPmodel3Y);
	zu1_00_v3_h3->addWidget(ledit_UPmodel3Width);

    zu1_00_v4_h1->addWidget(btn_UPmodelSet);
    zu1_00_v4_h2->addWidget(btn_UPmodelDel);

    // 连接信号槽，当编辑框内容改变时更新参数
    connect(ledit_UPStartDistance, &QLineEdit::textChanged, [=](const QString& text) {
        upBoxPositionParams.frameStartDistance = text.toInt();
    });
    
    connect(ledit_UPRectDistance, &QLineEdit::textChanged, [=](const QString& text) {
        upBoxPositionParams.framesHorizontalDistance = text.toInt();
    });
    
    connect(ledit_UPwidth, &QLineEdit::textChanged, [=](const QString& text) {
        upBoxPositionParams.frameWidth = text.toInt();
    });
    
    connect(ledit_UPheight, &QLineEdit::textChanged, [=](const QString& text) {
        upBoxPositionParams.frameHeight = text.toInt();
    });
    
    connect(ledit_UPsigma, &QLineEdit::textChanged, [=](const QString& text) {
        upBoxPositionParams.frameSigmaCode = text.toInt();
    });
    
    connect(ledit_UPstartBS, &QLineEdit::textChanged, [=](const QString& text) {
        upBoxPositionParams.frameEdgeGradientThreshold = text.toInt();
    });
    
    // 按钮增减连接
    connect(btn_UPStartDistance_add, &QPushButton::clicked, [=]() {
        int value = ledit_UPStartDistance->text().toInt() + 1;
        ledit_UPStartDistance->setText(QString::number(value));
    });
    
    connect(btn_UPStartDistance_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UPStartDistance->text().toInt() - 1;
        if (value >= 0) ledit_UPStartDistance->setText(QString::number(value));
    });
    
    connect(btn_UPRectDistance_add, &QPushButton::clicked, [=]() {
        int value = ledit_UPRectDistance->text().toInt() + 1;
        ledit_UPRectDistance->setText(QString::number(value));
    });
    
    connect(btn_UPRectDistance_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UPRectDistance->text().toInt() - 1;
        if (value >= 0) ledit_UPRectDistance->setText(QString::number(value));
    });
    
    connect(btn_UPwidth_add, &QPushButton::clicked, [=]() {
        int value = ledit_UPwidth->text().toInt() + 1;
        ledit_UPwidth->setText(QString::number(value));
    });
    
    connect(btn_UPwidth_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UPwidth->text().toInt() - 1;
        if (value >= 0) ledit_UPwidth->setText(QString::number(value));
    });
    
    connect(btn_UPheight_add, &QPushButton::clicked, [=]() {
        int value = ledit_UPheight->text().toInt() + 1;
        ledit_UPheight->setText(QString::number(value));
    });
    
    connect(btn_UPheight_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UPheight->text().toInt() - 1;
        if (value >= 0) ledit_UPheight->setText(QString::number(value));
    });
    
    connect(btn_UPsigma_add, &QPushButton::clicked, [=]() {
        int value = ledit_UPsigma->text().toInt() + 1;
        ledit_UPsigma->setText(QString::number(value));
    });
    
    connect(btn_UPsigma_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UPsigma->text().toInt() - 1;
        if (value >= 0) ledit_UPsigma->setText(QString::number(value));
    });
    
    connect(btn_UPstartBS_add, &QPushButton::clicked, [=]() {
        int value = ledit_UPstartBS->text().toInt() + 1;
        ledit_UPstartBS->setText(QString::number(value));
    });
    
    connect(btn_UPstartBS_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UPstartBS->text().toInt() - 1;
        if (value >= 0) ledit_UPstartBS->setText(QString::number(value));
    });

    // 初始化编辑框的值
    ledit_UPStartDistance->setText(QString::number(upBoxPositionParams.frameStartDistance));
    ledit_UPRectDistance->setText(QString::number(upBoxPositionParams.framesHorizontalDistance));
    ledit_UPwidth->setText(QString::number(upBoxPositionParams.frameWidth));
    ledit_UPheight->setText(QString::number(upBoxPositionParams.frameHeight));
    ledit_UPsigma->setText(QString::number(upBoxPositionParams.frameSigmaCode));
    ledit_UPstartBS->setText(QString::number(upBoxPositionParams.frameEdgeGradientThreshold));
}

void CigVisionParams::initDownCigBoundaryParamsWidgets()//初始化下烟边设置界面
{
    QHBoxLayout* zu1_downCig_paraHBoxLayout = new QHBoxLayout();//整体布局
    downCigBoundryPosParaSet_widget->setLayout(zu1_downCig_paraHBoxLayout);
        //下烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_downCig_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_downCig_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_downCig_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_downCig_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
    //起始距离
    QPushButton* btn_DOWNStartDistance_add = new QPushButton();
    QPushButton* btn_DOWNStartDistance_dec = new QPushButton();
    QLabel* lab_DOWNStartDistance = new QLabel();
    QLineEdit* ledit_DOWNStartDistance = new QLineEdit();
    ledit_DOWNStartDistance->setText("0");

    //框到框水平距离
    QPushButton* btn_DOWNRectDistance_add = new QPushButton();
    QPushButton* btn_DOWNRectDistance_dec = new QPushButton();
    QLabel* lab_DOWNRectDistance = new QLabel();
    QLineEdit* ledit_DOWNRectDistance = new QLineEdit();
    ledit_DOWNRectDistance->setText("0");
    //宽度
    QPushButton* btn_DOWNwidth_add = new QPushButton();
    QPushButton* btn_DOWNwidth_dec = new QPushButton();
    QLabel* lab_DOWNwidth = new QLabel();
    QLineEdit* ledit_DOWNwidth = new QLineEdit();
    ledit_DOWNwidth->setText("0");
    //高度
    QPushButton* btn_DOWNheight_add = new QPushButton();
    QPushButton* btn_DOWNheight_dec = new QPushButton();
    QLabel* lab_DOWNheight = new QLabel();
    QLineEdit* ledit_DOWNheight = new QLineEdit();
    ledit_DOWNheight->setText("0");
    //边西格玛值
    QPushButton* btn_DOWNsigma_add = new QPushButton();
    QPushButton* btn_DOWNsigma_dec = new QPushButton();
    QLabel* lab_DOWNsigma = new QLabel();
    QLineEdit* ledit_DOWNsigma = new QLineEdit();
    ledit_DOWNsigma->setText("0");
    //边边界强度定位起始阈值
    QPushButton* btn_DOWNstartBS_add = new QPushButton();
    QPushButton* btn_DOWNstartBS_dec = new QPushButton();
    QLabel* lab_DOWNstartBS = new QLabel();
    QLineEdit* ledit_DOWNstartBS = new QLineEdit();
    ledit_DOWNstartBS->setText("0");
    //端边界强度阈值步进值
    QPushButton* btn_DOWNstepBS_add = new QPushButton();
    QPushButton* btn_DOWNstepBS_dec = new QPushButton();
    QLabel* lab_DOWNstepBS = new QLabel();
    QLineEdit* ledit_DOWNstepBS = new QLineEdit();
    ledit_DOWNstepBS->setText("0");

    //模版功能介绍
    QLabel* lab_DOWNintroduce_model = new QLabel();
    lab_DOWNintroduce_model->setText(QStringLiteral("模版提供边界参数<br>过大偏离报错"));
    lab_DOWNintroduce_model->setMaximumSize(200, 50);
    lab_DOWNintroduce_model->setAlignment(Qt::AlignCenter);
    lab_DOWNintroduce_model->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");


    ////模版输出模版上边中心点X
    //QPushButton* btn_UPmodelX_add = new QPushButton();
    //QPushButton* btn_UPmodelX_dec = new QPushButton();
    QLabel* lab_DOWNmodel1 = new QLabel();
    QLabel* lab_DOWNmodel2 = new QLabel();
    QLabel* lab_DOWNmodel3 = new QLabel();
	QLineEdit* ledit_DOWNmodel1X = new QLineEdit();
	QLineEdit* ledit_DOWNmodel2X = new QLineEdit();
	QLineEdit* ledit_DOWNmodel3X = new QLineEdit();
	QLineEdit* ledit_DOWNmodel1Y = new QLineEdit();
	QLineEdit* ledit_DOWNmodel2Y = new QLineEdit();
	QLineEdit* ledit_DOWNmodel3Y = new QLineEdit();
	QLineEdit* ledit_DOWNmodel1Width = new QLineEdit();
	QLineEdit* ledit_DOWNmodel2Width = new QLineEdit();
	QLineEdit* ledit_DOWNmodel3Width = new QLineEdit();
    ledit_DOWNmodel1X->setText("0");
    ledit_DOWNmodel2X->setText("0");
    ledit_DOWNmodel3X->setText("0");
    ledit_DOWNmodel1Y->setText("0");
    ledit_DOWNmodel2Y->setText("0");
    ledit_DOWNmodel3Y->setText("0");
    ledit_DOWNmodel1Width->setText("0");
    ledit_DOWNmodel2Width->setText("0");
    ledit_DOWNmodel3Width->setText("0");
    
    //模版值设定
    QPushButton* btn_DOWNmodelSet = new QPushButton();
    QPushButton* btn_DOWNmodelDel = new QPushButton();
    btn_DOWNmodelSet->setText(QStringLiteral("模版值设定 "));
    btn_DOWNmodelDel->setText(QStringLiteral("模版值清空 "));

    btn_DOWNmodelSet->setMinimumSize(150, 50);
    btn_DOWNmodelDel->setMinimumSize(150, 50);

    btn_DOWNStartDistance_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DOWNStartDistance_add->setMaximumSize(50, 50);
    btn_DOWNStartDistance_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DOWNStartDistance_dec->setMaximumSize(50, 50);

    lab_DOWNStartDistance->setText(QStringLiteral("下烟定位框水平起始距离"));
    lab_DOWNStartDistance->setMaximumSize(150, 50);
    lab_DOWNStartDistance->setAlignment(Qt::AlignCenter);
    lab_DOWNStartDistance->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_DOWNStartDistance->setMaximumSize(50, 50);
    ledit_DOWNStartDistance->setAlignment(Qt::AlignCenter);
    ledit_DOWNStartDistance->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNStartDistance->setMaxLength(3);

    zu1_00_v1_h1->addWidget(btn_DOWNStartDistance_dec);
    zu1_00_v1_h1->addWidget(lab_DOWNStartDistance);
    zu1_00_v1_h1->addWidget(ledit_DOWNStartDistance);
    zu1_00_v1_h1->addWidget(btn_DOWNStartDistance_add);

    btn_DOWNRectDistance_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DOWNRectDistance_add->setMaximumSize(50, 50);
    btn_DOWNRectDistance_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DOWNRectDistance_dec->setMaximumSize(50, 50);

    lab_DOWNRectDistance->setText(QStringLiteral("下烟定位框水平间距"));
    lab_DOWNRectDistance->setMaximumSize(150, 50);
    lab_DOWNRectDistance->setAlignment(Qt::AlignCenter);
    lab_DOWNRectDistance->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_DOWNRectDistance->setMaximumSize(50, 50);
    ledit_DOWNRectDistance->setAlignment(Qt::AlignCenter);
    ledit_DOWNRectDistance->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNRectDistance->setMaxLength(3);

    zu1_00_v1_h2->addWidget(btn_DOWNRectDistance_dec);
    zu1_00_v1_h2->addWidget(lab_DOWNRectDistance);
    zu1_00_v1_h2->addWidget(ledit_DOWNRectDistance);
    zu1_00_v1_h2->addWidget(btn_DOWNRectDistance_add);

    btn_DOWNwidth_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DOWNwidth_add->setMaximumSize(50, 50);
    btn_DOWNwidth_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DOWNwidth_dec->setMaximumSize(50, 50);

    lab_DOWNwidth->setText(QStringLiteral("下烟框宽度"));
    lab_DOWNwidth->setMaximumSize(100, 50);
    lab_DOWNwidth->setAlignment(Qt::AlignCenter);
    lab_DOWNwidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_DOWNwidth->setMaximumSize(100, 50);
    ledit_DOWNwidth->setAlignment(Qt::AlignCenter);
    ledit_DOWNwidth->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNwidth->setMaxLength(3);

    zu1_00_v1_h3->addWidget(btn_DOWNwidth_dec);
    zu1_00_v1_h3->addWidget(lab_DOWNwidth);
    zu1_00_v1_h3->addWidget(ledit_DOWNwidth);
    zu1_00_v1_h3->addWidget(btn_DOWNwidth_add);

    btn_DOWNheight_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DOWNheight_add->setMaximumSize(50, 50);
    btn_DOWNheight_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DOWNheight_dec->setMaximumSize(50, 50);

    lab_DOWNheight->setText(QStringLiteral("下烟框高度"));
    lab_DOWNheight->setMaximumSize(100, 50);
    lab_DOWNheight->setAlignment(Qt::AlignCenter);
    lab_DOWNheight->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

    ledit_DOWNheight->setMaximumSize(100, 50);
    ledit_DOWNheight->setAlignment(Qt::AlignCenter);
    ledit_DOWNheight->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNheight->setMaxLength(3);

    zu1_00_v1_h4->addWidget(btn_DOWNheight_dec);
    zu1_00_v1_h4->addWidget(lab_DOWNheight);
    zu1_00_v1_h4->addWidget(ledit_DOWNheight);
    zu1_00_v1_h4->addWidget(btn_DOWNheight_add);

    btn_DOWNsigma_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DOWNsigma_add->setMaximumSize(50, 50);
    btn_DOWNsigma_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DOWNsigma_dec->setMaximumSize(50, 50);
    lab_DOWNsigma->setText(QStringLiteral("西格玛值"));
    lab_DOWNsigma->setMaximumSize(100, 50);
    lab_DOWNsigma->setAlignment(Qt::AlignCenter);
    lab_DOWNsigma->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNsigma->setMaximumSize(100, 50);
    ledit_DOWNsigma->setAlignment(Qt::AlignCenter);
    ledit_DOWNsigma->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNsigma->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_DOWNsigma_dec);
    zu1_00_v2_h1->addWidget(lab_DOWNsigma);
    zu1_00_v2_h1->addWidget(ledit_DOWNsigma);
    zu1_00_v2_h1->addWidget(btn_DOWNsigma_add);

    btn_DOWNstartBS_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DOWNstartBS_add->setMaximumSize(50, 50);
    btn_DOWNstartBS_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DOWNstartBS_dec->setMaximumSize(50, 50);
    lab_DOWNstartBS->setText(QStringLiteral("边界强度起始阈值"));
    lab_DOWNstartBS->setMaximumSize(200, 50);
    lab_DOWNstartBS->setAlignment(Qt::AlignCenter);
    lab_DOWNstartBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNstartBS->setMaximumSize(50, 50);
    ledit_DOWNstartBS->setAlignment(Qt::AlignCenter);
    ledit_DOWNstartBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNstartBS->setMaxLength(3);

    zu1_00_v2_h2->addWidget(btn_DOWNstartBS_dec);
    zu1_00_v2_h2->addWidget(lab_DOWNstartBS);
    zu1_00_v2_h2->addWidget(ledit_DOWNstartBS);
    zu1_00_v2_h2->addWidget(btn_DOWNstartBS_add);

	/*btn_UPstepBS_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
	btn_UPstepBS_add->setMaximumSize(50, 50);
	btn_UPstepBS_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
	btn_UPstepBS_dec->setMaximumSize(50, 50);
	lab_UPstepBS->setText(QStringLiteral("边界强度阈值步进数"));
    lab_UPstepBS->setMaximumSize(150, 50);
    lab_UPstepBS->setAlignment(Qt::AlignCenter);
    lab_UPstepBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPstepBS->setMaximumSize(100, 50);
    ledit_UPstepBS->setAlignment(Qt::AlignCenter);
    ledit_UPstepBS->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UPstepBS->setMaxLength(3);*/

   /* zu1_00_v2_h3->addWidget(btn_UPstepBS_dec);
    zu1_00_v2_h3->addWidget(lab_UPstepBS);
    zu1_00_v2_h3->addWidget(ledit_UPstepBS);
    zu1_00_v2_h3->addWidget(btn_UPstepBS_add);*/

    //zu1_00_v3_h1->addWidget(lab_UPintroduce_model);

    lab_DOWNmodel1->setText(QStringLiteral("模版1输出:X,Y,W"));
    lab_DOWNmodel2->setText(QStringLiteral("模版2输出:X,Y,W"));
    lab_DOWNmodel3->setText(QStringLiteral("模版3输出:X,Y,W"));
    lab_DOWNmodel1->setMaximumSize(150, 50);
    lab_DOWNmodel2->setMaximumSize(150, 50);
    lab_DOWNmodel3->setMaximumSize(150, 50);
	lab_DOWNmodel1->setAlignment(Qt::AlignCenter);
	lab_DOWNmodel1->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	lab_DOWNmodel2->setAlignment(Qt::AlignCenter);
	lab_DOWNmodel2->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	lab_DOWNmodel3->setAlignment(Qt::AlignCenter);
	lab_DOWNmodel3->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");

	ledit_DOWNmodel1X->setObjectName("ledit_DOWNmodel1X");
	ledit_DOWNmodel1X->setMaximumSize(50, 50);
	ledit_DOWNmodel1X->setAlignment(Qt::AlignCenter);
	ledit_DOWNmodel1X->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNmodel1X->setMaxLength(3);
	ledit_DOWNmodel1Y->setObjectName("ledit_DOWNmodel1Y");
	ledit_DOWNmodel1Y->setMaximumSize(50, 50);
	ledit_DOWNmodel1Y->setAlignment(Qt::AlignCenter);
    ledit_DOWNmodel1Y->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNmodel1Y->setMaxLength(3);
    ledit_DOWNmodel1Width->setObjectName("ledit_DOWNmodel1Width");
    ledit_DOWNmodel1Width->setMaximumSize(50, 50);
    ledit_DOWNmodel1Width->setAlignment(Qt::AlignCenter);
    ledit_DOWNmodel1Width->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DOWNmodel1Width->setMaxLength(3);

	ledit_DOWNmodel2X->setObjectName("ledit_DOWNmodel2X");
	ledit_DOWNmodel2X->setMaximumSize(50, 50);
	ledit_DOWNmodel2X->setAlignment(Qt::AlignCenter);
	ledit_DOWNmodel2X->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_DOWNmodel2X->setMaxLength(3);
	ledit_DOWNmodel2Y->setObjectName("ledit_DOWNmodel2Y");
	ledit_DOWNmodel2Y->setMaximumSize(50, 50);
	ledit_DOWNmodel2Y->setAlignment(Qt::AlignCenter);
	ledit_DOWNmodel2Y->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_DOWNmodel2Y->setMaxLength(3);
	ledit_DOWNmodel2Width->setObjectName("ledit_DOWNmodel2Width");
	ledit_DOWNmodel2Width->setMaximumSize(50, 50);
	ledit_DOWNmodel2Width->setAlignment(Qt::AlignCenter);
	ledit_DOWNmodel2Width->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_DOWNmodel2Width->setMaxLength(3);

	ledit_DOWNmodel3X->setObjectName("ledit_DOWNmodel3X");
	ledit_DOWNmodel3X->setMaximumSize(50, 50);
	ledit_DOWNmodel3X->setAlignment(Qt::AlignCenter);
	ledit_DOWNmodel3X->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_DOWNmodel3X->setMaxLength(3);
	ledit_DOWNmodel3Y->setObjectName("ledit_DOWNmodel3Y");
	ledit_DOWNmodel3Y->setMaximumSize(50, 50);
	ledit_DOWNmodel3Y->setAlignment(Qt::AlignCenter);
	ledit_DOWNmodel3Y->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_DOWNmodel3Y->setMaxLength(3);
	ledit_DOWNmodel3Width->setObjectName("ledit_DOWNmodel3Width");
	ledit_DOWNmodel3Width->setMaximumSize(50, 50);
	ledit_DOWNmodel3Width->setAlignment(Qt::AlignCenter);
	ledit_DOWNmodel3Width->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
	ledit_DOWNmodel3Width->setMaxLength(3);

	zu1_00_v3_h1->addWidget(lab_DOWNmodel1);
	zu1_00_v3_h1->addWidget(ledit_DOWNmodel1X);
	zu1_00_v3_h1->addWidget(ledit_DOWNmodel1Y);
	zu1_00_v3_h1->addWidget(ledit_DOWNmodel1Width);
	zu1_00_v3_h2->addWidget(lab_DOWNmodel2);
	zu1_00_v3_h2->addWidget(ledit_DOWNmodel2X);
	zu1_00_v3_h2->addWidget(ledit_DOWNmodel2Y);
	zu1_00_v3_h2->addWidget(ledit_DOWNmodel2Width);
	zu1_00_v3_h3->addWidget(lab_DOWNmodel3);
	zu1_00_v3_h3->addWidget(ledit_DOWNmodel3X);
	zu1_00_v3_h3->addWidget(ledit_DOWNmodel3Y);
	zu1_00_v3_h3->addWidget(ledit_DOWNmodel3Width);

    zu1_00_v4_h1->addWidget(btn_DOWNmodelSet);
    zu1_00_v4_h2->addWidget(btn_DOWNmodelDel);

    // 连接信号槽，当编辑框内容改变时更新参数
    connect(ledit_DOWNStartDistance, &QLineEdit::textChanged, [=](const QString& text) {
        downBoxPositionParams.frameStartDistance = text.toInt();
    });
    
    connect(ledit_DOWNRectDistance, &QLineEdit::textChanged, [=](const QString& text) {
        downBoxPositionParams.framesHorizontalDistance = text.toInt();
    });
    
    connect(ledit_DOWNwidth, &QLineEdit::textChanged, [=](const QString& text) {
        downBoxPositionParams.frameWidth = text.toInt();
    });
    
    connect(ledit_DOWNheight, &QLineEdit::textChanged, [=](const QString& text) {
        downBoxPositionParams.frameHeight = text.toInt();
    });
    
    connect(ledit_DOWNsigma, &QLineEdit::textChanged, [=](const QString& text) {
        downBoxPositionParams.frameSigmaCode = text.toInt();
    });
    
    connect(ledit_DOWNstartBS, &QLineEdit::textChanged, [=](const QString& text) {
        downBoxPositionParams.frameEdgeGradientThreshold = text.toInt();
    });
    
    // 按钮增减连接
    connect(btn_DOWNStartDistance_add, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNStartDistance->text().toInt() + 1;
        ledit_DOWNStartDistance->setText(QString::number(value));
    });
    
    connect(btn_DOWNStartDistance_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNStartDistance->text().toInt() - 1;
        if (value >= 0) ledit_DOWNStartDistance->setText(QString::number(value));
    });
    
    connect(btn_DOWNRectDistance_add, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNRectDistance->text().toInt() + 1;
        ledit_DOWNRectDistance->setText(QString::number(value));
    });
    
    connect(btn_DOWNRectDistance_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNRectDistance->text().toInt() - 1;
        if (value >= 0) ledit_DOWNRectDistance->setText(QString::number(value));
    });
    
    connect(btn_DOWNwidth_add, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNwidth->text().toInt() + 1;
        ledit_DOWNwidth->setText(QString::number(value));
    });
    
    connect(btn_DOWNwidth_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNwidth->text().toInt() - 1;
        if (value >= 0) ledit_DOWNwidth->setText(QString::number(value));
    });
    
    connect(btn_DOWNheight_add, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNheight->text().toInt() + 1;
        ledit_DOWNheight->setText(QString::number(value));
    });
    
    connect(btn_DOWNheight_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNheight->text().toInt() - 1;
        if (value >= 0) ledit_DOWNheight->setText(QString::number(value));
    });
    
    connect(btn_DOWNsigma_add, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNsigma->text().toInt() + 1;
        ledit_DOWNsigma->setText(QString::number(value));
    });
    
    connect(btn_DOWNsigma_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNsigma->text().toInt() - 1;
        if (value >= 0) ledit_DOWNsigma->setText(QString::number(value));
    });
    
    connect(btn_DOWNstartBS_add, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNstartBS->text().toInt() + 1;
        ledit_DOWNstartBS->setText(QString::number(value));
    });
    
    connect(btn_DOWNstartBS_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DOWNstartBS->text().toInt() - 1;
        if (value >= 0) ledit_DOWNstartBS->setText(QString::number(value));
    });

    // 初始化编辑框的值
    // 初始化编辑框的值
    ledit_DOWNStartDistance->setText(QString::number(downBoxPositionParams.frameStartDistance));
    ledit_DOWNRectDistance->setText(QString::number(downBoxPositionParams.framesHorizontalDistance));
    ledit_DOWNwidth->setText(QString::number(downBoxPositionParams.frameWidth));
    ledit_DOWNheight->setText(QString::number(downBoxPositionParams.frameHeight));
    ledit_DOWNsigma->setText(QString::number(downBoxPositionParams.frameSigmaCode));
    ledit_DOWNstartBS->setText(QString::number(downBoxPositionParams.frameEdgeGradientThreshold));
}
void CigVisionParams::initCigStickROIWidgets()
{
    QHBoxLayout* zu1_cigStick_paraHBoxLayout = new QHBoxLayout();//整体布局
    cigStickROISet_widget->setLayout(zu1_cigStick_paraHBoxLayout);
    //下烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_cigStick_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_cigStick_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_cigStick_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_cigStick_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
    //上烟起始距离
    QPushButton* btn_UpCigStickStart_add = new QPushButton();
    QPushButton* btn_UpCigStickStart_dec = new QPushButton();
    QLabel* lab_UpCigStickStart = new QLabel();
    QLineEdit* ledit_UpCigStickStart = new QLineEdit();

    //上烟烟棒长
    QPushButton* btn_UpCigStickLong_add = new QPushButton();
    QPushButton* btn_UpCigStickLong_dec = new QPushButton();
    QLabel* lab_UpCigStickLong = new QLabel();
    QLineEdit* ledit_UpCigStickLong = new QLineEdit();

    //上烟宽内例外
    QPushButton* btn_UpInnerExcept_add = new QPushButton();
    QPushButton* btn_UpInnerExcept_dec = new QPushButton();
    QLabel* lab_UpInnerExcept = new QLabel();
    QLineEdit* ledit_UpInnerExcept = new QLineEdit();

    //下烟起始距离
    QPushButton* btn_DownCigStickStart_add = new QPushButton();
    QPushButton* btn_DownCigStickStart_dec = new QPushButton();
    QLabel* lab_DownCigStickStart = new QLabel();
    QLineEdit* ledit_DownCigStickStart = new QLineEdit();

    //下烟烟棒长
    QPushButton* btn_DownCigStickLong_add = new QPushButton();
    QPushButton* btn_DownCigStickLong_dec = new QPushButton();
    QLabel* lab_DownCigStickLong = new QLabel();
    QLineEdit* ledit_DownCigStickLong = new QLineEdit();

    //下烟宽内例外
    QPushButton* btn_DownInnerExcept_add = new QPushButton();
    QPushButton* btn_DownInnerExcept_dec = new QPushButton();
    QLabel* lab_DownInnerExcept = new QLabel();
    QLineEdit* ledit_DownInnerExcept = new QLineEdit();

    btn_UpCigStickStart_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigStickStart_add->setMaximumSize(50, 50);
    btn_UpCigStickStart_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigStickStart_dec->setMaximumSize(50, 50);
    lab_UpCigStickStart->setText(QStringLiteral("上烟棒检测区域距左端起始距离"));
    lab_UpCigStickStart->setMaximumSize(220, 50);
    lab_UpCigStickStart->setAlignment(Qt::AlignCenter);
    lab_UpCigStickStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigStickStart->setMaximumSize(50, 50);
    ledit_UpCigStickStart->setAlignment(Qt::AlignCenter);
    ledit_UpCigStickStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigStickStart->setMaxLength(3);

    btn_UpCigStickLong_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigStickLong_add->setMaximumSize(50, 50);
    btn_UpCigStickLong_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigStickLong_dec->setMaximumSize(50, 50);
    lab_UpCigStickLong->setText(QStringLiteral("上烟棒检测区域长度"));
    lab_UpCigStickLong->setMaximumSize(220, 50);
    lab_UpCigStickLong->setAlignment(Qt::AlignCenter);
    lab_UpCigStickLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigStickLong->setMaximumSize(50, 50);
    ledit_UpCigStickLong->setAlignment(Qt::AlignCenter);
    ledit_UpCigStickLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigStickLong->setMaxLength(3);

    btn_UpInnerExcept_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpInnerExcept_add->setMaximumSize(50, 50);
    btn_UpInnerExcept_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpInnerExcept_dec->setMaximumSize(50, 50);
    lab_UpInnerExcept->setText(QStringLiteral("上烟棒检测区域内部例外"));
    lab_UpInnerExcept->setMaximumSize(220, 50);
    lab_UpInnerExcept->setAlignment(Qt::AlignCenter);
    lab_UpInnerExcept->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpInnerExcept->setMaximumSize(50, 50);
    ledit_UpInnerExcept->setAlignment(Qt::AlignCenter);
    ledit_UpInnerExcept->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpInnerExcept->setMaxLength(3);

    zu1_00_v1_h1->addWidget(btn_UpCigStickStart_dec);
    zu1_00_v1_h1->addWidget(lab_UpCigStickStart);
    zu1_00_v1_h1->addWidget(ledit_UpCigStickStart);
    zu1_00_v1_h1->addWidget(btn_UpCigStickStart_add);

    zu1_00_v1_h2->addWidget(btn_UpCigStickLong_dec);
    zu1_00_v1_h2->addWidget(lab_UpCigStickLong);
    zu1_00_v1_h2->addWidget(ledit_UpCigStickLong);
    zu1_00_v1_h2->addWidget(btn_UpCigStickLong_add);

    zu1_00_v1_h3->addWidget(btn_UpInnerExcept_dec);
    zu1_00_v1_h3->addWidget(lab_UpInnerExcept);
    zu1_00_v1_h3->addWidget(ledit_UpInnerExcept);
    zu1_00_v1_h3->addWidget(btn_UpInnerExcept_add);

    btn_DownCigStickStart_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigStickStart_add->setMaximumSize(50, 50);
    btn_DownCigStickStart_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigStickStart_dec->setMaximumSize(50, 50);
    lab_DownCigStickStart->setText(QStringLiteral("上烟棒检测区域距左端起始距离"));
    lab_DownCigStickStart->setMaximumSize(220, 50);
    lab_DownCigStickStart->setAlignment(Qt::AlignCenter);
    lab_DownCigStickStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigStickStart->setMaximumSize(50, 50);
    ledit_DownCigStickStart->setAlignment(Qt::AlignCenter);
    ledit_DownCigStickStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigStickStart->setMaxLength(3);

    btn_DownCigStickLong_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigStickLong_add->setMaximumSize(50, 50);
    btn_DownCigStickLong_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigStickLong_dec->setMaximumSize(50, 50);
    lab_DownCigStickLong->setText(QStringLiteral("上烟棒检测区域长度"));
    lab_DownCigStickLong->setMaximumSize(220, 50);
    lab_DownCigStickLong->setAlignment(Qt::AlignCenter);
    lab_DownCigStickLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigStickLong->setMaximumSize(50, 50);
    ledit_DownCigStickLong->setAlignment(Qt::AlignCenter);
    ledit_DownCigStickLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigStickLong->setMaxLength(3);

    btn_DownInnerExcept_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownInnerExcept_add->setMaximumSize(50, 50);
    btn_DownInnerExcept_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownInnerExcept_dec->setMaximumSize(50, 50);
    lab_DownInnerExcept->setText(QStringLiteral("上烟棒检测区域内部例外"));
    lab_DownInnerExcept->setMaximumSize(220, 50);
    lab_DownInnerExcept->setAlignment(Qt::AlignCenter);
    lab_DownInnerExcept->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownInnerExcept->setMaximumSize(50, 50);
    ledit_DownInnerExcept->setAlignment(Qt::AlignCenter);
    ledit_DownInnerExcept->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownInnerExcept->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_DownCigStickStart_dec);
    zu1_00_v2_h1->addWidget(lab_DownCigStickStart);
    zu1_00_v2_h1->addWidget(ledit_DownCigStickStart);
    zu1_00_v2_h1->addWidget(btn_DownCigStickStart_add);

    zu1_00_v2_h2->addWidget(btn_DownCigStickLong_dec);
    zu1_00_v2_h2->addWidget(lab_DownCigStickLong);
    zu1_00_v2_h2->addWidget(ledit_DownCigStickLong);
    zu1_00_v2_h2->addWidget(btn_DownCigStickLong_add);

    zu1_00_v2_h3->addWidget(btn_DownInnerExcept_dec);
    zu1_00_v2_h3->addWidget(lab_DownInnerExcept);
    zu1_00_v2_h3->addWidget(ledit_DownInnerExcept);
    zu1_00_v2_h3->addWidget(btn_DownInnerExcept_add);

    connect(btn_UpCigStickStart_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigStickStart->text().toInt() + 1;
        ledit_UpCigStickStart->setText(QString::number(value));
    });
    connect(btn_UpCigStickStart_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigStickStart->text().toInt() - 1;
        ledit_UpCigStickStart->setText(QString::number(value));
    });

    connect(btn_UpCigStickLong_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigStickLong->text().toInt() + 1;
        ledit_UpCigStickLong->setText(QString::number(value));
    });
    connect(btn_UpCigStickLong_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigStickLong->text().toInt() - 1;
        ledit_UpCigStickLong->setText(QString::number(value));
    });

    connect(btn_UpInnerExcept_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpInnerExcept->text().toInt() + 1;
        ledit_UpInnerExcept->setText(QString::number(value));
    });
    connect(btn_UpInnerExcept_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpInnerExcept->text().toInt() - 1;
        ledit_UpInnerExcept->setText(QString::number(value));
    });

    connect(btn_DownCigStickStart_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigStickStart->text().toInt() + 1;
        ledit_DownCigStickStart->setText(QString::number(value));
    });
    connect(btn_DownCigStickStart_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigStickStart->text().toInt() - 1;
        ledit_DownCigStickStart->setText(QString::number(value));
    });

    connect(btn_DownCigStickLong_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigStickLong->text().toInt() + 1;
        ledit_DownCigStickLong->setText(QString::number(value));
    });
    connect(btn_DownCigStickLong_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigStickLong->text().toInt() - 1;
        ledit_DownCigStickLong->setText(QString::number(value));
    });

    connect(btn_DownInnerExcept_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownInnerExcept->text().toInt() + 1;
        ledit_DownInnerExcept->setText(QString::number(value));
    });
    connect(btn_DownInnerExcept_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownInnerExcept->text().toInt() - 1;
        ledit_DownInnerExcept->setText(QString::number(value));
    });
    connect(ledit_UpCigStickStart, &QLineEdit::textChanged, [=](const QString& text) {
        upCigBodyPositionParams.bodyStartDistance = text.toInt();
    });
    connect(ledit_UpCigStickLong, &QLineEdit::textChanged, [=](const QString& text) {
        upCigBodyPositionParams.bodyLength = text.toInt();
    });
    connect(ledit_UpInnerExcept, &QLineEdit::textChanged, [=](const QString& text) {
        upCigBodyPositionParams.innerEdgeThreshold = text.toInt();
    });
    connect(ledit_DownCigStickStart, &QLineEdit::textChanged, [=](const QString& text) {
        downCigBodyPositionParams.bodyStartDistance = text.toInt();
    });
    connect(ledit_DownCigStickLong, &QLineEdit::textChanged, [=](const QString& text) {
        downCigBodyPositionParams.bodyLength = text.toInt();
    });
    connect(ledit_DownInnerExcept, &QLineEdit::textChanged, [=](const QString& text) {
        downCigBodyPositionParams.innerEdgeThreshold = text.toInt();
    });
    
    // 初始化编辑框的值
    ledit_UpCigStickStart->setText(QString::number(upCigBodyPositionParams.bodyStartDistance));
    ledit_UpCigStickLong->setText(QString::number(upCigBodyPositionParams.bodyLength));
    ledit_UpInnerExcept->setText(QString::number(upCigBodyPositionParams.innerEdgeThreshold));
    ledit_DownCigStickStart->setText(QString::number(downCigBodyPositionParams.bodyStartDistance));
    ledit_DownCigStickLong->setText(QString::number(downCigBodyPositionParams.bodyLength));
    ledit_DownInnerExcept->setText(QString::number(downCigBodyPositionParams.innerEdgeThreshold));
}

void CigVisionParams::initCigFilterROIWidgets()
{
    QHBoxLayout* zu1_cigFilter_paraHBoxLayout = new QHBoxLayout();//整体布局
    cigFilterROISet_widget->setLayout(zu1_cigFilter_paraHBoxLayout);
    //下烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_cigFilter_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_cigFilter_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_cigFilter_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_cigFilter_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
    //上烟嘴棒起始距离
    QPushButton* btn_UpCigFilterStart_add = new QPushButton();
    QPushButton* btn_UpCigFilterStart_dec = new QPushButton();
    QLabel* lab_UpCigFilterStart = new QLabel();
    QLineEdit* ledit_UpCigFilterStart = new QLineEdit();

    //上烟嘴棒长
    QPushButton* btn_UpCigFilterLong_add = new QPushButton();
    QPushButton* btn_UpCigFilterLong_dec = new QPushButton();
    QLabel* lab_UpCigFilterLong = new QLabel();
    QLineEdit* ledit_UpCigFilterLong = new QLineEdit();

    //上烟嘴棒宽内例外
    QPushButton* btn_UpCigFilterInnerExcept_add = new QPushButton();
    QPushButton* btn_UpCigFilterInnerExcept_dec = new QPushButton();
    QLabel* lab_UpCigFilterInnerExcept = new QLabel();
    QLineEdit* ledit_UpCigFilterInnerExcept = new QLineEdit();

    //下烟嘴棒起始距离
    QPushButton* btn_DownCigFilterStart_add = new QPushButton();
    QPushButton* btn_DownCigFilterStart_dec = new QPushButton();
    QLabel* lab_DownCigFilterStart = new QLabel();
    QLineEdit* ledit_DownCigFilterStart = new QLineEdit();

    //下烟嘴棒长
    QPushButton* btn_DownCigFilterLong_add = new QPushButton();
    QPushButton* btn_DownCigFilterLong_dec = new QPushButton();
    QLabel* lab_DownCigFilterLong = new QLabel();
    QLineEdit* ledit_DownCigFilterLong = new QLineEdit();

    //下烟嘴棒宽内例外
    QPushButton* btn_DownCigFilterInnerExcept_add = new QPushButton();
    QPushButton* btn_DownCigFilterInnerExcept_dec = new QPushButton();
    QLabel* lab_DownCigFilterInnerExcept = new QLabel();
    QLineEdit* ledit_DownCigFilterInnerExcept = new QLineEdit();

    btn_UpCigFilterStart_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigFilterStart_add->setMaximumSize(50, 50);
    btn_UpCigFilterStart_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigFilterStart_dec->setMaximumSize(50, 50);
    lab_UpCigFilterStart->setText(QStringLiteral("上烟嘴棒检测区域距左端起始距离"));
    lab_UpCigFilterStart->setMaximumSize(220, 50);
    lab_UpCigFilterStart->setAlignment(Qt::AlignCenter);
    lab_UpCigFilterStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterStart->setMaximumSize(50, 50);
    ledit_UpCigFilterStart->setAlignment(Qt::AlignCenter);
    ledit_UpCigFilterStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterStart->setMaxLength(3);

    btn_UpCigFilterLong_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigFilterLong_add->setMaximumSize(50, 50);
    btn_UpCigFilterLong_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigFilterLong_dec->setMaximumSize(50, 50);
    lab_UpCigFilterLong->setText(QStringLiteral("上烟嘴棒检测区域长度"));
    lab_UpCigFilterLong->setMaximumSize(220, 50);
    lab_UpCigFilterLong->setAlignment(Qt::AlignCenter);
    lab_UpCigFilterLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterLong->setMaximumSize(50, 50);
    ledit_UpCigFilterLong->setAlignment(Qt::AlignCenter);
    ledit_UpCigFilterLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterLong->setMaxLength(3);

    btn_UpCigFilterInnerExcept_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigFilterInnerExcept_add->setMaximumSize(50, 50);
    btn_UpCigFilterInnerExcept_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigFilterInnerExcept_dec->setMaximumSize(50, 50);
    lab_UpCigFilterInnerExcept->setText(QStringLiteral("上烟嘴棒检测区域内部例外"));
    lab_UpCigFilterInnerExcept->setMaximumSize(220, 50);
    lab_UpCigFilterInnerExcept->setAlignment(Qt::AlignCenter);
    lab_UpCigFilterInnerExcept->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterInnerExcept->setMaximumSize(50, 50);
    ledit_UpCigFilterInnerExcept->setAlignment(Qt::AlignCenter);
    ledit_UpCigFilterInnerExcept->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterInnerExcept->setMaxLength(3);

    zu1_00_v1_h1->addWidget(btn_UpCigFilterStart_dec);
    zu1_00_v1_h1->addWidget(lab_UpCigFilterStart);
    zu1_00_v1_h1->addWidget(ledit_UpCigFilterStart);
    zu1_00_v1_h1->addWidget(btn_UpCigFilterStart_add);

    zu1_00_v1_h2->addWidget(btn_UpCigFilterLong_dec);
    zu1_00_v1_h2->addWidget(lab_UpCigFilterLong);
    zu1_00_v1_h2->addWidget(ledit_UpCigFilterLong);
    zu1_00_v1_h2->addWidget(btn_UpCigFilterLong_add);

    zu1_00_v1_h3->addWidget(btn_UpCigFilterInnerExcept_dec);
    zu1_00_v1_h3->addWidget(lab_UpCigFilterInnerExcept);
    zu1_00_v1_h3->addWidget(ledit_UpCigFilterInnerExcept);
    zu1_00_v1_h3->addWidget(btn_UpCigFilterInnerExcept_add);

    btn_DownCigFilterStart_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigFilterStart_add->setMaximumSize(50, 50);
    btn_DownCigFilterStart_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigFilterStart_dec->setMaximumSize(50, 50);
    lab_DownCigFilterStart->setText(QStringLiteral("下烟棒检测区域距左端起始距离"));
    lab_DownCigFilterStart->setMaximumSize(220, 50);
    lab_DownCigFilterStart->setAlignment(Qt::AlignCenter);
    lab_DownCigFilterStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterStart->setMaximumSize(50, 50);
    ledit_DownCigFilterStart->setAlignment(Qt::AlignCenter);
    ledit_DownCigFilterStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterStart->setMaxLength(3);

    btn_DownCigFilterLong_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigFilterLong_add->setMaximumSize(50, 50);
    btn_DownCigFilterLong_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigFilterLong_dec->setMaximumSize(50, 50);
    lab_DownCigFilterLong->setText(QStringLiteral("下烟棒检测区域长度"));
    lab_DownCigFilterLong->setMaximumSize(220, 50);
    lab_DownCigFilterLong->setAlignment(Qt::AlignCenter);
    lab_DownCigFilterLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterLong->setMaximumSize(50, 50);
    ledit_DownCigFilterLong->setAlignment(Qt::AlignCenter);
    ledit_DownCigFilterLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterLong->setMaxLength(3);

    btn_DownCigFilterInnerExcept_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigFilterInnerExcept_add->setMaximumSize(50, 50);
    btn_DownCigFilterInnerExcept_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigFilterInnerExcept_dec->setMaximumSize(50, 50);
    lab_DownCigFilterInnerExcept->setText(QStringLiteral("下烟棒检测区域内部例外"));
    lab_DownCigFilterInnerExcept->setMaximumSize(220, 50);
    lab_DownCigFilterInnerExcept->setAlignment(Qt::AlignCenter);
    lab_DownCigFilterInnerExcept->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterInnerExcept->setMaximumSize(50, 50);
    ledit_DownCigFilterInnerExcept->setAlignment(Qt::AlignCenter);
    ledit_DownCigFilterInnerExcept->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterInnerExcept->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_DownCigFilterStart_dec);
    zu1_00_v2_h1->addWidget(lab_DownCigFilterStart);
    zu1_00_v2_h1->addWidget(ledit_DownCigFilterStart);
    zu1_00_v2_h1->addWidget(btn_DownCigFilterStart_add);

    zu1_00_v2_h2->addWidget(btn_DownCigFilterLong_dec);
    zu1_00_v2_h2->addWidget(lab_DownCigFilterLong);
    zu1_00_v2_h2->addWidget(ledit_DownCigFilterLong);
    zu1_00_v2_h2->addWidget(btn_DownCigFilterLong_add);

    zu1_00_v2_h3->addWidget(btn_DownCigFilterInnerExcept_dec);
    zu1_00_v2_h3->addWidget(lab_DownCigFilterInnerExcept);
    zu1_00_v2_h3->addWidget(ledit_DownCigFilterInnerExcept);
    zu1_00_v2_h3->addWidget(btn_DownCigFilterInnerExcept_add);

    connect(btn_UpCigFilterStart_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterStart->text().toInt() + 1;
        ledit_UpCigFilterStart->setText(QString::number(value));
        });
    connect(btn_UpCigFilterStart_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterStart->text().toInt() - 1;
        ledit_UpCigFilterStart->setText(QString::number(value));
        });

    connect(btn_UpCigFilterLong_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterLong->text().toInt() + 1;
        ledit_UpCigFilterLong->setText(QString::number(value));
        });
    connect(btn_UpCigFilterLong_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterLong->text().toInt() - 1;
        ledit_UpCigFilterLong->setText(QString::number(value));
        });

    connect(btn_UpCigFilterInnerExcept_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterInnerExcept->text().toInt() + 1;
        ledit_UpCigFilterInnerExcept->setText(QString::number(value));
        });
    connect(btn_UpCigFilterInnerExcept_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterInnerExcept->text().toInt() - 1;
        ledit_UpCigFilterInnerExcept->setText(QString::number(value));
        });

    connect(btn_DownCigFilterStart_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterStart->text().toInt() + 1;
        ledit_DownCigFilterStart->setText(QString::number(value));
        });
    connect(btn_DownCigFilterStart_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterStart->text().toInt() - 1;
        ledit_DownCigFilterStart->setText(QString::number(value));
        });

    connect(btn_DownCigFilterLong_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterLong->text().toInt() + 1;
        ledit_DownCigFilterLong->setText(QString::number(value));
        });
    connect(btn_DownCigFilterLong_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterLong->text().toInt() - 1;
        ledit_DownCigFilterLong->setText(QString::number(value));
        });

    connect(btn_DownCigFilterInnerExcept_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterInnerExcept->text().toInt() + 1;
        ledit_DownCigFilterInnerExcept->setText(QString::number(value));
        });
    connect(btn_DownCigFilterInnerExcept_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterInnerExcept->text().toInt() - 1;
        ledit_DownCigFilterInnerExcept->setText(QString::number(value));
        });
    connect(ledit_UpCigFilterStart, &QLineEdit::textChanged, [=](const QString& text) {
        upFilterPositionParams.filterStartDistance = text.toInt();
        });
    connect(ledit_UpCigFilterLong, &QLineEdit::textChanged, [=](const QString& text) {
        upFilterPositionParams.filterLength = text.toInt();
        });
    connect(ledit_UpCigFilterInnerExcept, &QLineEdit::textChanged, [=](const QString& text) {
        upFilterPositionParams.innerEdgeThreshold = text.toInt();
        });
    connect(ledit_DownCigFilterStart, &QLineEdit::textChanged, [=](const QString& text) {
        downFilterPositionParams.filterStartDistance = text.toInt();
        });
    connect(ledit_DownCigFilterLong, &QLineEdit::textChanged, [=](const QString& text) {
        downFilterPositionParams.filterLength = text.toInt();
        });
    connect(ledit_DownCigFilterInnerExcept, &QLineEdit::textChanged, [=](const QString& text) {
        downFilterPositionParams.innerEdgeThreshold = text.toInt();
        });

    // 初始化编辑框的值
    ledit_UpCigFilterStart->setText(QString::number(upFilterPositionParams.filterStartDistance));
    ledit_UpCigFilterLong->setText(QString::number(upFilterPositionParams.filterLength));
    ledit_UpCigFilterInnerExcept->setText(QString::number(upFilterPositionParams.innerEdgeThreshold));
    ledit_DownCigFilterStart->setText(QString::number(downFilterPositionParams.filterStartDistance));
    ledit_DownCigFilterLong->setText(QString::number(downFilterPositionParams.filterLength));
    ledit_DownCigFilterInnerExcept->setText(QString::number(downFilterPositionParams.innerEdgeThreshold));
}
void CigVisionParams::initCigJointROIWidgets()
{
QHBoxLayout* zu1_cigJoint_paraHBoxLayout = new QHBoxLayout();//整体布局
    cigJointerROISet_widget->setLayout(zu1_cigJoint_paraHBoxLayout);
    //下烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_cigJoint_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_cigJoint_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_cigJoint_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_cigJoint_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
    //上烟拼接段起始距离
    QPushButton* btn_UpCigJointStart_add = new QPushButton();
    QPushButton* btn_UpCigJointStart_dec = new QPushButton();
    QLabel* lab_UpCigJointStart = new QLabel();
    QLineEdit* ledit_UpCigJointStart = new QLineEdit();

    //上烟拼接段长度
    QPushButton* btn_UpCigJointLong_add = new QPushButton();
    QPushButton* btn_UpCigJointLong_dec = new QPushButton();
    QLabel* lab_UpCigJointLong = new QLabel();
    QLineEdit* ledit_UpCigJointLong = new QLineEdit();


    //下烟拼接段起始距离
    QPushButton* btn_DownCigJointStart_add = new QPushButton();
    QPushButton* btn_DownCigJointStart_dec = new QPushButton();
    QLabel* lab_DownCigJointStart = new QLabel();
    QLineEdit* ledit_DownCigJointStart = new QLineEdit();

    //下烟拼接段长度
    QPushButton* btn_DownCigJointLong_add = new QPushButton();
    QPushButton* btn_DownCigJointLong_dec = new QPushButton();
    QLabel* lab_DownCigJointLong = new QLabel();
    QLineEdit* ledit_DownCigJointLong = new QLineEdit();


    btn_UpCigJointStart_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigJointStart_add->setMaximumSize(50, 50);
    btn_UpCigJointStart_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigJointStart_dec->setMaximumSize(50, 50);
    lab_UpCigJointStart->setText(QStringLiteral("上烟拼接段距左端起始距离"));
    lab_UpCigJointStart->setMaximumSize(220, 50);
    lab_UpCigJointStart->setAlignment(Qt::AlignCenter);
    lab_UpCigJointStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigJointStart->setMaximumSize(50, 50);
    ledit_UpCigJointStart->setAlignment(Qt::AlignCenter);
    ledit_UpCigJointStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigJointStart->setMaxLength(3);

    btn_UpCigJointLong_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigJointLong_add->setMaximumSize(50, 50);
    btn_UpCigJointLong_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigJointLong_dec->setMaximumSize(50, 50);
    lab_UpCigJointLong->setText(QStringLiteral("上烟拼接段长度"));
    lab_UpCigJointLong->setMaximumSize(220, 50);
    lab_UpCigJointLong->setAlignment(Qt::AlignCenter);
    lab_UpCigJointLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigJointLong->setMaximumSize(50, 50);
    ledit_UpCigJointLong->setAlignment(Qt::AlignCenter);
    ledit_UpCigJointLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigJointLong->setMaxLength(3);

    zu1_00_v1_h1->addWidget(btn_UpCigJointStart_dec);
    zu1_00_v1_h1->addWidget(lab_UpCigJointStart);
    zu1_00_v1_h1->addWidget(ledit_UpCigJointStart);
    zu1_00_v1_h1->addWidget(btn_UpCigJointStart_add);

    zu1_00_v1_h2->addWidget(btn_UpCigJointLong_dec);
    zu1_00_v1_h2->addWidget(lab_UpCigJointLong);
    zu1_00_v1_h2->addWidget(ledit_UpCigJointLong);
    zu1_00_v1_h2->addWidget(btn_UpCigJointLong_add);

    btn_DownCigJointStart_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigJointStart_add->setMaximumSize(50, 50);
    btn_DownCigJointStart_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigJointStart_dec->setMaximumSize(50, 50);
    lab_DownCigJointStart->setText(QStringLiteral("下烟拼接段距左端起始距离"));
    lab_DownCigJointStart->setMaximumSize(220, 50);
    lab_DownCigJointStart->setAlignment(Qt::AlignCenter);
    lab_DownCigJointStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigJointStart->setMaximumSize(50, 50);
    ledit_DownCigJointStart->setAlignment(Qt::AlignCenter);
    ledit_DownCigJointStart->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigJointStart->setMaxLength(3);

    btn_DownCigJointLong_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigJointLong_add->setMaximumSize(50, 50);
    btn_DownCigJointLong_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigJointLong_dec->setMaximumSize(50, 50);
    lab_DownCigJointLong->setText(QStringLiteral("下烟拼接段长度"));
    lab_DownCigJointLong->setMaximumSize(220, 50);
    lab_DownCigJointLong->setAlignment(Qt::AlignCenter);
    lab_DownCigJointLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigJointLong->setMaximumSize(50, 50);
    ledit_DownCigJointLong->setAlignment(Qt::AlignCenter);
    ledit_DownCigJointLong->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigJointLong->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_DownCigJointStart_dec);
    zu1_00_v2_h1->addWidget(lab_DownCigJointStart);
    zu1_00_v2_h1->addWidget(ledit_DownCigJointStart);
    zu1_00_v2_h1->addWidget(btn_DownCigJointStart_add);

    zu1_00_v2_h2->addWidget(btn_DownCigJointLong_dec);
    zu1_00_v2_h2->addWidget(lab_DownCigJointLong);
    zu1_00_v2_h2->addWidget(ledit_DownCigJointLong);
    zu1_00_v2_h2->addWidget(btn_DownCigJointLong_add);

    connect(btn_UpCigJointStart_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigJointStart->text().toInt() + 1;
        ledit_UpCigJointStart->setText(QString::number(value));
        });
    connect(btn_UpCigJointStart_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigJointStart->text().toInt() - 1;
        ledit_UpCigJointStart->setText(QString::number(value));
        });

    connect(btn_UpCigJointLong_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigJointLong->text().toInt() + 1;
        ledit_UpCigJointLong->setText(QString::number(value));
        });
    connect(btn_UpCigJointLong_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigJointLong->text().toInt() - 1;
        ledit_UpCigJointLong->setText(QString::number(value));
        });

    connect(btn_DownCigJointStart_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigJointStart->text().toInt() + 1;
        ledit_DownCigJointStart->setText(QString::number(value));
        });
    connect(btn_DownCigJointStart_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigJointStart->text().toInt() - 1;
        ledit_DownCigJointStart->setText(QString::number(value));
        });

    connect(btn_DownCigJointLong_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigJointLong->text().toInt() + 1;
        ledit_DownCigJointLong->setText(QString::number(value));
        });
    connect(btn_DownCigJointLong_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigJointLong->text().toInt() - 1;
        ledit_DownCigJointLong->setText(QString::number(value));
        });

    connect(ledit_UpCigJointStart, &QLineEdit::textChanged, [=](const QString& text) {
        upJointPositionParams.jointStartDistance = text.toInt();
        });
    connect(ledit_UpCigJointLong, &QLineEdit::textChanged, [=](const QString& text) {
        upJointPositionParams.jointLength = text.toInt();
        });
    connect(ledit_DownCigJointStart, &QLineEdit::textChanged, [=](const QString& text) {
        downJointPositionParams.jointStartDistance = text.toInt();
        });
    connect(ledit_DownCigJointLong, &QLineEdit::textChanged, [=](const QString& text) {
        downJointPositionParams.jointLength = text.toInt();
        });

    // 初始化编辑框的值
    ledit_UpCigJointStart->setText(QString::number(upJointPositionParams.jointStartDistance));
    ledit_UpCigJointLong->setText(QString::number(upJointPositionParams.jointLength));
    ledit_DownCigJointStart->setText(QString::number(downJointPositionParams.jointStartDistance));
    ledit_DownCigJointLong->setText(QString::number(downJointPositionParams.jointLength));
}

void CigVisionParams::initStickNGWidgets()
{
    QHBoxLayout* zu1_stickNG_paraHBoxLayout = new QHBoxLayout();//整体布局
    stickNGSet_widget->setLayout(zu1_stickNG_paraHBoxLayout);
    //下烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_stickNG_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_stickNG_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_stickNG_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_stickNG_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
    //上烟烟棒暗点面积阈值
    QPushButton* btn_UpCigStickDarkArea_add = new QPushButton();
    QPushButton* btn_UpCigStickDarkArea_dec = new QPushButton();
    QLabel* lab_UpCigStickDarkArea = new QLabel();
    QLineEdit* ledit_UpCigStickDarkArea = new QLineEdit();

    //上烟烟棒暗点灰度阈值
    QPushButton* btn_UpCigStickDarkGray_add = new QPushButton();
    QPushButton* btn_UpCigStickDarkGray_dec = new QPushButton();
    QLabel* lab_UpCigStickDarkGray = new QLabel();
    QLineEdit* ledit_UpCigStickDarkGray = new QLineEdit();

    //下烟烟棒暗点面积阈值
    QPushButton* btn_DownCigStickDarkArea_add = new QPushButton();
    QPushButton* btn_DownCigStickDarkArea_dec = new QPushButton();
    QLabel* lab_DownCigStickDarkArea = new QLabel();
    QLineEdit* ledit_DownCigStickDarkArea = new QLineEdit();

    //下烟烟棒暗点灰度阈值
    QPushButton* btn_DownCigStickDarkGray_add = new QPushButton();
    QPushButton* btn_DownCigStickDarkGray_dec = new QPushButton();
    QLabel* lab_DownCigStickDarkGray = new QLabel();
    QLineEdit* ledit_DownCigStickDarkGray = new QLineEdit();

    btn_UpCigStickDarkArea_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigStickDarkArea_add->setMaximumSize(50, 50);
    btn_UpCigStickDarkArea_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigStickDarkArea_dec->setMaximumSize(50, 50);
    lab_UpCigStickDarkArea->setText(QStringLiteral("上烟烟棒暗点面积阈值"));
    lab_UpCigStickDarkArea->setMaximumSize(220, 50);
    lab_UpCigStickDarkArea->setAlignment(Qt::AlignCenter);
    lab_UpCigStickDarkArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigStickDarkArea->setMaximumSize(50, 50);
    ledit_UpCigStickDarkArea->setAlignment(Qt::AlignCenter);
    ledit_UpCigStickDarkArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigStickDarkArea->setMaxLength(3);

    btn_UpCigStickDarkGray_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigStickDarkGray_add->setMaximumSize(50, 50);
    btn_UpCigStickDarkGray_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigStickDarkGray_dec->setMaximumSize(50, 50);
    lab_UpCigStickDarkGray->setText(QStringLiteral("上烟烟棒暗点灰度阈值"));
    lab_UpCigStickDarkGray->setMaximumSize(220, 50);
    lab_UpCigStickDarkGray->setAlignment(Qt::AlignCenter);
    lab_UpCigStickDarkGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigStickDarkGray->setMaximumSize(50, 50);
    ledit_UpCigStickDarkGray->setAlignment(Qt::AlignCenter);
    ledit_UpCigStickDarkGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigStickDarkGray->setMaxLength(3);

    zu1_00_v1_h1->addWidget(btn_UpCigStickDarkArea_dec);
    zu1_00_v1_h1->addWidget(lab_UpCigStickDarkArea);
    zu1_00_v1_h1->addWidget(ledit_UpCigStickDarkArea);
    zu1_00_v1_h1->addWidget(btn_UpCigStickDarkArea_add);

    zu1_00_v1_h2->addWidget(btn_UpCigStickDarkGray_dec);
    zu1_00_v1_h2->addWidget(lab_UpCigStickDarkGray);
    zu1_00_v1_h2->addWidget(ledit_UpCigStickDarkGray);
    zu1_00_v1_h2->addWidget(btn_UpCigStickDarkGray_add);

    btn_DownCigStickDarkArea_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigStickDarkArea_add->setMaximumSize(50, 50);
    btn_DownCigStickDarkArea_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigStickDarkArea_dec->setMaximumSize(50, 50);
    lab_DownCigStickDarkArea->setText(QStringLiteral("下烟烟棒暗点面积阈值"));
    lab_DownCigStickDarkArea->setMaximumSize(220, 50);
    lab_DownCigStickDarkArea->setAlignment(Qt::AlignCenter);
    lab_DownCigStickDarkArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigStickDarkArea->setMaximumSize(50, 50);
    ledit_DownCigStickDarkArea->setAlignment(Qt::AlignCenter);
    ledit_DownCigStickDarkArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigStickDarkArea->setMaxLength(3);

    btn_DownCigStickDarkGray_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigStickDarkGray_add->setMaximumSize(50, 50);
    btn_DownCigStickDarkGray_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigStickDarkGray_dec->setMaximumSize(50, 50);
    lab_DownCigStickDarkGray->setText(QStringLiteral("下烟烟棒暗点灰度阈值"));
    lab_DownCigStickDarkGray->setMaximumSize(220, 50);
    lab_DownCigStickDarkGray->setAlignment(Qt::AlignCenter);
    lab_DownCigStickDarkGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigStickDarkGray->setMaximumSize(50, 50);
    ledit_DownCigStickDarkGray->setAlignment(Qt::AlignCenter);
    ledit_DownCigStickDarkGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigStickDarkGray->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_DownCigStickDarkArea_dec);
    zu1_00_v2_h1->addWidget(lab_DownCigStickDarkArea);
    zu1_00_v2_h1->addWidget(ledit_DownCigStickDarkArea);
    zu1_00_v2_h1->addWidget(btn_DownCigStickDarkArea_add);

    zu1_00_v2_h2->addWidget(btn_DownCigStickDarkGray_dec);
    zu1_00_v2_h2->addWidget(lab_DownCigStickDarkGray);
    zu1_00_v2_h2->addWidget(ledit_DownCigStickDarkGray);
    zu1_00_v2_h2->addWidget(btn_DownCigStickDarkGray_add);

    connect(btn_UpCigStickDarkArea_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigStickDarkArea->text().toInt() + 1;
        ledit_UpCigStickDarkArea->setText(QString::number(value));
        });
    connect(btn_UpCigStickDarkArea_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigStickDarkArea->text().toInt() - 1;
        ledit_UpCigStickDarkArea->setText(QString::number(value));
        });

    connect(btn_UpCigStickDarkGray_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigStickDarkGray->text().toInt() + 1;
        ledit_UpCigStickDarkGray->setText(QString::number(value));
        });
    connect(btn_UpCigStickDarkGray_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigStickDarkGray->text().toInt() - 1;
        ledit_UpCigStickDarkGray->setText(QString::number(value));
        });

    connect(btn_DownCigStickDarkArea_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigStickDarkArea->text().toInt() + 1;
        ledit_DownCigStickDarkArea->setText(QString::number(value));
        });
    connect(btn_DownCigStickDarkArea_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigStickDarkArea->text().toInt() - 1;
        ledit_DownCigStickDarkArea->setText(QString::number(value));
        });

    connect(btn_DownCigStickDarkGray_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigStickDarkGray->text().toInt() + 1;
        ledit_DownCigStickDarkGray->setText(QString::number(value));
        });
    connect(btn_DownCigStickDarkGray_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigStickDarkGray->text().toInt() - 1;
        ledit_DownCigStickDarkGray->setText(QString::number(value));
        });

    connect(ledit_UpCigStickDarkArea, &QLineEdit::textChanged, [=](const QString& text) {
        upCigStickDarkDefectParams.darkPointAreasValue = text.toInt();
        });
    connect(ledit_UpCigStickDarkGray, &QLineEdit::textChanged, [=](const QString& text) {
        upCigStickDarkDefectParams.darkPointGrayValue = text.toInt();
        });
    connect(ledit_DownCigStickDarkArea, &QLineEdit::textChanged, [=](const QString& text) {
        downCigStickDarkDefectParams.darkPointAreasValue = text.toInt();
        });
    connect(ledit_DownCigStickDarkGray, &QLineEdit::textChanged, [=](const QString& text) {
        downCigStickDarkDefectParams.darkPointGrayValue = text.toInt();
        });

    // 初始化编辑框的值
    ledit_UpCigStickDarkArea->setText(QString::number(upCigStickDarkDefectParams.darkPointAreasValue));
    ledit_UpCigStickDarkGray->setText(QString::number(upCigStickDarkDefectParams.darkPointGrayValue));
    ledit_DownCigStickDarkArea->setText(QString::number(downCigStickDarkDefectParams.darkPointAreasValue));
    ledit_DownCigStickDarkGray->setText(QString::number(downCigStickDarkDefectParams.darkPointGrayValue));
}

void CigVisionParams::initFilterDarkNGWidgets()
{
    QHBoxLayout* zu1_filterDarkNG_paraHBoxLayout = new QHBoxLayout();//整体布局
    filgerDarkNGSet_widget->setLayout(zu1_filterDarkNG_paraHBoxLayout);
    //下烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_filterDarkNG_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_filterDarkNG_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_filterDarkNG_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_filterDarkNG_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
    //上烟滤嘴暗点面积阈值
    QPushButton* btn_UpCigFilterDarkArea_add = new QPushButton();
    QPushButton* btn_UpCigFilterDarkArea_dec = new QPushButton();
    QLabel* lab_UpCigFilterDarkArea = new QLabel();
    QLineEdit* ledit_UpCigFilterDarkArea = new QLineEdit();

    //上烟滤嘴暗点灰度阈值
    QPushButton* btn_UpCigFilterDarkGray_add = new QPushButton();
    QPushButton* btn_UpCigFilterDarkGray_dec = new QPushButton();
    QLabel* lab_UpCigFilterDarkGray = new QLabel();
    QLineEdit* ledit_UpCigFilterDarkGray = new QLineEdit();

    //下烟滤嘴暗点面积阈值
    QPushButton* btn_DownCigFilterDarkArea_add = new QPushButton();
    QPushButton* btn_DownCigFilterDarkArea_dec = new QPushButton();
    QLabel* lab_DownCigFilterDarkArea = new QLabel();
    QLineEdit* ledit_DownCigFilterDarkArea = new QLineEdit();

    //下烟滤嘴暗点灰度阈值
    QPushButton* btn_DownCigFilterDarkGray_add = new QPushButton();
    QPushButton* btn_DownCigFilterDarkGray_dec = new QPushButton();
    QLabel* lab_DownCigFilterDarkGray = new QLabel();
    QLineEdit* ledit_DownCigFilterDarkGray = new QLineEdit();

    btn_UpCigFilterDarkArea_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigFilterDarkArea_add->setMaximumSize(50, 50);
    btn_UpCigFilterDarkArea_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigFilterDarkArea_dec->setMaximumSize(50, 50);
    lab_UpCigFilterDarkArea->setText(QStringLiteral("上烟滤嘴暗点面积阈值"));
    lab_UpCigFilterDarkArea->setMaximumSize(220, 50);
    lab_UpCigFilterDarkArea->setAlignment(Qt::AlignCenter);
    lab_UpCigFilterDarkArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterDarkArea->setMaximumSize(50, 50);
    ledit_UpCigFilterDarkArea->setAlignment(Qt::AlignCenter);
    ledit_UpCigFilterDarkArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterDarkArea->setMaxLength(3);

    btn_UpCigFilterDarkGray_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigFilterDarkGray_add->setMaximumSize(50, 50);
    btn_UpCigFilterDarkGray_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigFilterDarkGray_dec->setMaximumSize(50, 50);
    lab_UpCigFilterDarkGray->setText(QStringLiteral("上烟滤嘴暗点灰度阈值"));
    lab_UpCigFilterDarkGray->setMaximumSize(220, 50);
    lab_UpCigFilterDarkGray->setAlignment(Qt::AlignCenter);
    lab_UpCigFilterDarkGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterDarkGray->setMaximumSize(50, 50);
    ledit_UpCigFilterDarkGray->setAlignment(Qt::AlignCenter);
    ledit_UpCigFilterDarkGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterDarkGray->setMaxLength(3);

    zu1_00_v1_h1->addWidget(btn_UpCigFilterDarkArea_dec);
    zu1_00_v1_h1->addWidget(lab_UpCigFilterDarkArea);
    zu1_00_v1_h1->addWidget(ledit_UpCigFilterDarkArea);
    zu1_00_v1_h1->addWidget(btn_UpCigFilterDarkArea_add);

    zu1_00_v1_h2->addWidget(btn_UpCigFilterDarkGray_dec);
    zu1_00_v1_h2->addWidget(lab_UpCigFilterDarkGray);
    zu1_00_v1_h2->addWidget(ledit_UpCigFilterDarkGray);
    zu1_00_v1_h2->addWidget(btn_UpCigFilterDarkGray_add);

    btn_DownCigFilterDarkArea_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigFilterDarkArea_add->setMaximumSize(50, 50);
    btn_DownCigFilterDarkArea_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigFilterDarkArea_dec->setMaximumSize(50, 50);
    lab_DownCigFilterDarkArea->setText(QStringLiteral("下烟烟棒暗点面积阈值"));
    lab_DownCigFilterDarkArea->setMaximumSize(220, 50);
    lab_DownCigFilterDarkArea->setAlignment(Qt::AlignCenter);
    lab_DownCigFilterDarkArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterDarkArea->setMaximumSize(50, 50);
    ledit_DownCigFilterDarkArea->setAlignment(Qt::AlignCenter);
    ledit_DownCigFilterDarkArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterDarkArea->setMaxLength(3);

    btn_DownCigFilterDarkGray_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigFilterDarkGray_add->setMaximumSize(50, 50);
    btn_DownCigFilterDarkGray_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigFilterDarkGray_dec->setMaximumSize(50, 50);
    lab_DownCigFilterDarkGray->setText(QStringLiteral("下烟烟棒暗点灰度阈值"));
    lab_DownCigFilterDarkGray->setMaximumSize(220, 50);
    lab_DownCigFilterDarkGray->setAlignment(Qt::AlignCenter);
    lab_DownCigFilterDarkGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterDarkGray->setMaximumSize(50, 50);
    ledit_DownCigFilterDarkGray->setAlignment(Qt::AlignCenter);
    ledit_DownCigFilterDarkGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterDarkGray->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_DownCigFilterDarkArea_dec);
    zu1_00_v2_h1->addWidget(lab_DownCigFilterDarkArea);
    zu1_00_v2_h1->addWidget(ledit_DownCigFilterDarkArea);
    zu1_00_v2_h1->addWidget(btn_DownCigFilterDarkArea_add);

    zu1_00_v2_h2->addWidget(btn_DownCigFilterDarkGray_dec);
    zu1_00_v2_h2->addWidget(lab_DownCigFilterDarkGray);
    zu1_00_v2_h2->addWidget(ledit_DownCigFilterDarkGray);
    zu1_00_v2_h2->addWidget(btn_DownCigFilterDarkGray_add);

    connect(btn_UpCigFilterDarkArea_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterDarkArea->text().toInt() + 1;
        ledit_UpCigFilterDarkArea->setText(QString::number(value));
        });
    connect(btn_UpCigFilterDarkArea_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterDarkArea->text().toInt() - 1;
        ledit_UpCigFilterDarkArea->setText(QString::number(value));
        });

    connect(btn_UpCigFilterDarkGray_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterDarkGray->text().toInt() + 1;
        ledit_UpCigFilterDarkGray->setText(QString::number(value));
        });
    connect(btn_UpCigFilterDarkGray_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterDarkGray->text().toInt() - 1;
        ledit_UpCigFilterDarkGray->setText(QString::number(value));
        });

    connect(btn_DownCigFilterDarkArea_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterDarkArea->text().toInt() + 1;
        ledit_DownCigFilterDarkArea->setText(QString::number(value));
        });
    connect(btn_DownCigFilterDarkArea_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterDarkArea->text().toInt() - 1;
        ledit_DownCigFilterDarkArea->setText(QString::number(value));
        });

    connect(btn_DownCigFilterDarkGray_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterDarkGray->text().toInt() + 1;
        ledit_DownCigFilterDarkGray->setText(QString::number(value));
        });
    connect(btn_DownCigFilterDarkGray_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterDarkGray->text().toInt() - 1;
        ledit_DownCigFilterDarkGray->setText(QString::number(value));
        });

    connect(ledit_UpCigFilterDarkArea, &QLineEdit::textChanged, [=](const QString& text) {
        upCigFilterDarkDefectParams.darkPointAreasValue = text.toInt();
        });
    connect(ledit_UpCigFilterDarkGray, &QLineEdit::textChanged, [=](const QString& text) {
        upCigFilterDarkDefectParams.darkPointGrayValue = text.toInt();
        });
    connect(ledit_DownCigFilterDarkArea, &QLineEdit::textChanged, [=](const QString& text) {
        downCigFilterDarkDefectParams.darkPointAreasValue = text.toInt();
        });
    connect(ledit_DownCigFilterDarkGray, &QLineEdit::textChanged, [=](const QString& text) {
        downCigFilterDarkDefectParams.darkPointGrayValue = text.toInt();
        });

    // 初始化编辑框的值
    ledit_UpCigFilterDarkArea->setText(QString::number(upCigFilterDarkDefectParams.darkPointAreasValue));
    ledit_UpCigFilterDarkGray->setText(QString::number(upCigFilterDarkDefectParams.darkPointGrayValue));
    ledit_DownCigFilterDarkArea->setText(QString::number(downCigFilterDarkDefectParams.darkPointAreasValue));
    ledit_DownCigFilterDarkGray->setText(QString::number(downCigFilterDarkDefectParams.darkPointGrayValue));
}
void CigVisionParams::initFilterWhiteNGWidgets()
{
    QHBoxLayout* zu1_filterWhiteNG_paraHBoxLayout = new QHBoxLayout();//整体布局
    filgerWhiteNGSet_widget->setLayout(zu1_filterWhiteNG_paraHBoxLayout);
    //下烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_filterWhiteNG_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_filterWhiteNG_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_filterWhiteNG_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_filterWhiteNG_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
    //上烟滤嘴白点面积阈值
    QPushButton* btn_UpCigFilterWhiteArea_add = new QPushButton();
    QPushButton* btn_UpCigFilterWhiteArea_dec = new QPushButton();
    QLabel* lab_UpCigFilterWhiteArea = new QLabel();
    QLineEdit* ledit_UpCigFilterWhiteArea = new QLineEdit();

    //上烟滤嘴白点灰度阈值
    QPushButton* btn_UpCigFilterWhiteGray_add = new QPushButton();
    QPushButton* btn_UpCigFilterWhiteGray_dec = new QPushButton();
    QLabel* lab_UpCigFilterWhiteGray = new QLabel();
    QLineEdit* ledit_UpCigFilterWhiteGray = new QLineEdit();

    //下烟滤嘴白点面积阈值
    QPushButton* btn_DownCigFilterWhiteArea_add = new QPushButton();
    QPushButton* btn_DownCigFilterWhiteArea_dec = new QPushButton();
    QLabel* lab_DownCigFilterWhiteArea = new QLabel();
    QLineEdit* ledit_DownCigFilterWhiteArea = new QLineEdit();

    //下烟滤嘴白点灰度阈值
    QPushButton* btn_DownCigFilterWhiteGray_add = new QPushButton();
    QPushButton* btn_DownCigFilterWhiteGray_dec = new QPushButton();
    QLabel* lab_DownCigFilterWhiteGray = new QLabel();
    QLineEdit* ledit_DownCigFilterWhiteGray = new QLineEdit();

    btn_UpCigFilterWhiteArea_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigFilterWhiteArea_add->setMaximumSize(50, 50);
    btn_UpCigFilterWhiteArea_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigFilterWhiteArea_dec->setMaximumSize(50, 50);
    lab_UpCigFilterWhiteArea->setText(QStringLiteral("上烟滤嘴亮点面积阈值"));
    lab_UpCigFilterWhiteArea->setMaximumSize(220, 50);
    lab_UpCigFilterWhiteArea->setAlignment(Qt::AlignCenter);
    lab_UpCigFilterWhiteArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterWhiteArea->setMaximumSize(50, 50);
    ledit_UpCigFilterWhiteArea->setAlignment(Qt::AlignCenter);
    ledit_UpCigFilterWhiteArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterWhiteArea->setMaxLength(3);

    btn_UpCigFilterWhiteGray_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigFilterWhiteGray_add->setMaximumSize(50, 50);
    btn_UpCigFilterWhiteGray_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigFilterWhiteGray_dec->setMaximumSize(50, 50);
    lab_UpCigFilterWhiteGray->setText(QStringLiteral("上烟滤嘴亮点灰度阈值"));
    lab_UpCigFilterWhiteGray->setMaximumSize(220, 50);
    lab_UpCigFilterWhiteGray->setAlignment(Qt::AlignCenter);
    lab_UpCigFilterWhiteGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterWhiteGray->setMaximumSize(50, 50);
    ledit_UpCigFilterWhiteGray->setAlignment(Qt::AlignCenter);
    ledit_UpCigFilterWhiteGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigFilterWhiteGray->setMaxLength(3);

    zu1_00_v1_h1->addWidget(btn_UpCigFilterWhiteArea_dec);
    zu1_00_v1_h1->addWidget(lab_UpCigFilterWhiteArea);
    zu1_00_v1_h1->addWidget(ledit_UpCigFilterWhiteArea);
    zu1_00_v1_h1->addWidget(btn_UpCigFilterWhiteArea_add);

    zu1_00_v1_h2->addWidget(btn_UpCigFilterWhiteGray_dec);
    zu1_00_v1_h2->addWidget(lab_UpCigFilterWhiteGray);
    zu1_00_v1_h2->addWidget(ledit_UpCigFilterWhiteGray);
    zu1_00_v1_h2->addWidget(btn_UpCigFilterWhiteGray_add);

    btn_DownCigFilterWhiteArea_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigFilterWhiteArea_add->setMaximumSize(50, 50);
    btn_DownCigFilterWhiteArea_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigFilterWhiteArea_dec->setMaximumSize(50, 50);
    lab_DownCigFilterWhiteArea->setText(QStringLiteral("下烟烟棒亮点面积阈值"));
    lab_DownCigFilterWhiteArea->setMaximumSize(220, 50);
    lab_DownCigFilterWhiteArea->setAlignment(Qt::AlignCenter);
    lab_DownCigFilterWhiteArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterWhiteArea->setMaximumSize(50, 50);
    ledit_DownCigFilterWhiteArea->setAlignment(Qt::AlignCenter);
    ledit_DownCigFilterWhiteArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterWhiteArea->setMaxLength(3);

    btn_DownCigFilterWhiteGray_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigFilterWhiteGray_add->setMaximumSize(50, 50);
    btn_DownCigFilterWhiteGray_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigFilterWhiteGray_dec->setMaximumSize(50, 50);
    lab_DownCigFilterWhiteGray->setText(QStringLiteral("下烟烟棒亮点灰度阈值"));
    lab_DownCigFilterWhiteGray->setMaximumSize(220, 50);
    lab_DownCigFilterWhiteGray->setAlignment(Qt::AlignCenter);
    lab_DownCigFilterWhiteGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterWhiteGray->setMaximumSize(50, 50);
    ledit_DownCigFilterWhiteGray->setAlignment(Qt::AlignCenter);
    ledit_DownCigFilterWhiteGray->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigFilterWhiteGray->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_DownCigFilterWhiteArea_dec);
    zu1_00_v2_h1->addWidget(lab_DownCigFilterWhiteArea);
    zu1_00_v2_h1->addWidget(ledit_DownCigFilterWhiteArea);
    zu1_00_v2_h1->addWidget(btn_DownCigFilterWhiteArea_add);

    zu1_00_v2_h2->addWidget(btn_DownCigFilterWhiteGray_dec);
    zu1_00_v2_h2->addWidget(lab_DownCigFilterWhiteGray);
    zu1_00_v2_h2->addWidget(ledit_DownCigFilterWhiteGray);
    zu1_00_v2_h2->addWidget(btn_DownCigFilterWhiteGray_add);

    connect(btn_UpCigFilterWhiteArea_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterWhiteArea->text().toInt() + 1;
        ledit_UpCigFilterWhiteArea->setText(QString::number(value));
        });
    connect(btn_UpCigFilterWhiteArea_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterWhiteArea->text().toInt() - 1;
        ledit_UpCigFilterWhiteArea->setText(QString::number(value));
        });

    connect(btn_UpCigFilterWhiteGray_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterWhiteGray->text().toInt() + 1;
        ledit_UpCigFilterWhiteGray->setText(QString::number(value));
        });
    connect(btn_UpCigFilterWhiteGray_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigFilterWhiteGray->text().toInt() - 1;
        ledit_UpCigFilterWhiteGray->setText(QString::number(value));
        });

    connect(btn_DownCigFilterWhiteArea_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterWhiteArea->text().toInt() + 1;
        ledit_DownCigFilterWhiteArea->setText(QString::number(value));
        });
    connect(btn_DownCigFilterWhiteArea_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterWhiteArea->text().toInt() - 1;
        ledit_DownCigFilterWhiteArea->setText(QString::number(value));
        });

    connect(btn_DownCigFilterWhiteGray_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterWhiteGray->text().toInt() + 1;
        ledit_DownCigFilterWhiteGray->setText(QString::number(value));
        });
    connect(btn_DownCigFilterWhiteGray_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigFilterWhiteGray->text().toInt() - 1;
        ledit_DownCigFilterWhiteGray->setText(QString::number(value));
        });

    connect(ledit_UpCigFilterWhiteArea, &QLineEdit::textChanged, [=](const QString& text) {
        upCigFilterWhiteDefectParams.brightPointAreasValue = text.toInt();
        });
    connect(ledit_UpCigFilterWhiteGray, &QLineEdit::textChanged, [=](const QString& text) {
        upCigFilterWhiteDefectParams.brightPointGrayValue = text.toInt();
        });
    connect(ledit_DownCigFilterWhiteArea, &QLineEdit::textChanged, [=](const QString& text) {
        downCigFilterWhiteDefectParams.brightPointAreasValue = text.toInt();
        });
    connect(ledit_DownCigFilterWhiteGray, &QLineEdit::textChanged, [=](const QString& text) {
        downCigFilterWhiteDefectParams.brightPointGrayValue = text.toInt();
        });

    // 初始化编辑框的值
    ledit_UpCigFilterWhiteArea->setText(QString::number(upCigFilterWhiteDefectParams.brightPointAreasValue));
    ledit_UpCigFilterWhiteGray->setText(QString::number(upCigFilterWhiteDefectParams.brightPointGrayValue));
    ledit_DownCigFilterWhiteArea->setText(QString::number(downCigFilterWhiteDefectParams.brightPointAreasValue));
    ledit_DownCigFilterWhiteGray->setText(QString::number(downCigFilterWhiteDefectParams.brightPointGrayValue));
}

void CigVisionParams::initOutNGWidgets()
{
    QHBoxLayout* zu1_cigOutNG_paraHBoxLayout = new QHBoxLayout();//整体布局
    cigOutNGSet_widget->setLayout(zu1_cigOutNG_paraHBoxLayout);
    //下烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_cigOutNG_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_cigOutNG_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_cigOutNG_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_cigOutNG_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
    //上烟烟支轮廓向外扩展像素
    QPushButton* btn_UpCigOutPixArea_add = new QPushButton();
    QPushButton* btn_UpCigOutPixArea_dec = new QPushButton();
    QLabel* lab_UpCigOutPixArea = new QLabel();
    QLineEdit* ledit_UpCigOutPixArea = new QLineEdit();

    //上烟矩形度参数
    QPushButton* btn_UpCigOutrectangularity_add = new QPushButton();
    QPushButton* btn_UpCigOutrectangularity_dec = new QPushButton();
    QLabel* lab_UpCigOutrectangularity = new QLabel();
    QLineEdit* ledit_UpCigOutrectangularity = new QLineEdit();

    //上烟凸度参数
    QPushButton* btn_UpCigOutConvexity_add = new QPushButton();
    QPushButton* btn_UpCigOutConvexity_dec = new QPushButton();
    QLabel* lab_UpCigOutConvexity = new QLabel();
    QLineEdit* ledit_UpCigOutConvexity = new QLineEdit();

    //下烟烟支轮廓向外扩展像素
    QPushButton* btn_DownCigOutPixArea_add = new QPushButton();
    QPushButton* btn_DownCigOutPixArea_dec = new QPushButton();
    QLabel* lab_DownCigOutPixArea = new QLabel();
    QLineEdit* ledit_DownCigOutPixArea = new QLineEdit();

    //下烟矩形度参数
    QPushButton* btn_DownCigOutrectangularity_add = new QPushButton();
    QPushButton* btn_DownCigOutrectangularity_dec = new QPushButton();
    QLabel* lab_DownCigOutrectangularity = new QLabel();
    QLineEdit* ledit_DownCigOutrectangularity = new QLineEdit();

    //下烟凸度参数
    QPushButton* btn_DownCigOutConvexity_add = new QPushButton();
    QPushButton* btn_DownCigOutConvexity_dec = new QPushButton();
    QLabel* lab_DownCigOutConvexity = new QLabel();
    QLineEdit* ledit_DownCigOutConvexity = new QLineEdit();

    btn_UpCigOutPixArea_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigOutPixArea_add->setMaximumSize(50, 50);
    btn_UpCigOutPixArea_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigOutPixArea_dec->setMaximumSize(50, 50);
    lab_UpCigOutPixArea->setText(QStringLiteral("上烟烟支轮廓向外扩展像素"));
    lab_UpCigOutPixArea->setMaximumSize(220, 50);
    lab_UpCigOutPixArea->setAlignment(Qt::AlignCenter);
    lab_UpCigOutPixArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigOutPixArea->setMaximumSize(50, 50);
    ledit_UpCigOutPixArea->setAlignment(Qt::AlignCenter);
    ledit_UpCigOutPixArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigOutPixArea->setMaxLength(3);

    btn_UpCigOutrectangularity_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigOutrectangularity_add->setMaximumSize(50, 50);
    btn_UpCigOutrectangularity_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigOutrectangularity_dec->setMaximumSize(50, 50);
    lab_UpCigOutrectangularity->setText(QStringLiteral("上烟矩形度参数"));
    lab_UpCigOutrectangularity->setMaximumSize(220, 50);
    lab_UpCigOutrectangularity->setAlignment(Qt::AlignCenter);
    lab_UpCigOutrectangularity->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigOutrectangularity->setMaximumSize(50, 50);
    ledit_UpCigOutrectangularity->setAlignment(Qt::AlignCenter);
    ledit_UpCigOutrectangularity->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigOutrectangularity->setMaxLength(3);

    btn_UpCigOutConvexity_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigOutConvexity_add->setMaximumSize(50, 50);
    btn_UpCigOutConvexity_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigOutConvexity_dec->setMaximumSize(50, 50);
    lab_UpCigOutConvexity->setText(QStringLiteral("上烟凸度参数"));
    lab_UpCigOutConvexity->setMaximumSize(220, 50);
    lab_UpCigOutConvexity->setAlignment(Qt::AlignCenter);
    lab_UpCigOutConvexity->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigOutConvexity->setMaximumSize(50, 50);
    ledit_UpCigOutConvexity->setAlignment(Qt::AlignCenter);
    ledit_UpCigOutConvexity->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigOutConvexity->setMaxLength(3);


    zu1_00_v1_h1->addWidget(btn_UpCigOutPixArea_dec);
    zu1_00_v1_h1->addWidget(lab_UpCigOutPixArea);
    zu1_00_v1_h1->addWidget(ledit_UpCigOutPixArea);
    zu1_00_v1_h1->addWidget(btn_UpCigOutPixArea_add);

    zu1_00_v1_h2->addWidget(btn_UpCigOutrectangularity_dec);
    zu1_00_v1_h2->addWidget(lab_UpCigOutrectangularity);
    zu1_00_v1_h2->addWidget(ledit_UpCigOutrectangularity);
    zu1_00_v1_h2->addWidget(btn_UpCigOutrectangularity_add);

    zu1_00_v1_h3->addWidget(btn_UpCigOutConvexity_dec);
    zu1_00_v1_h3->addWidget(lab_UpCigOutConvexity);
    zu1_00_v1_h3->addWidget(ledit_UpCigOutConvexity);
    zu1_00_v1_h3->addWidget(btn_UpCigOutConvexity_add);

    btn_DownCigOutPixArea_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigOutPixArea_add->setMaximumSize(50, 50);
    btn_DownCigOutPixArea_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigOutPixArea_dec->setMaximumSize(50, 50);
    lab_DownCigOutPixArea->setText(QStringLiteral("下烟烟支轮廓向外扩展像素"));
    lab_DownCigOutPixArea->setMaximumSize(220, 50);
    lab_DownCigOutPixArea->setAlignment(Qt::AlignCenter);
    lab_DownCigOutPixArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigOutPixArea->setMaximumSize(50, 50);
    ledit_DownCigOutPixArea->setAlignment(Qt::AlignCenter);
    ledit_DownCigOutPixArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigOutPixArea->setMaxLength(3);

    btn_DownCigOutrectangularity_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigOutrectangularity_add->setMaximumSize(50, 50);
    btn_DownCigOutrectangularity_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigOutrectangularity_dec->setMaximumSize(50, 50);
    lab_DownCigOutrectangularity->setText(QStringLiteral("下烟矩形度参数"));
    lab_DownCigOutrectangularity->setMaximumSize(220, 50);
    lab_DownCigOutrectangularity->setAlignment(Qt::AlignCenter);
    lab_DownCigOutrectangularity->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigOutrectangularity->setMaximumSize(50, 50);
    ledit_DownCigOutrectangularity->setAlignment(Qt::AlignCenter);
    ledit_DownCigOutrectangularity->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigOutrectangularity->setMaxLength(3);

    btn_DownCigOutConvexity_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigOutConvexity_add->setMaximumSize(50, 50);
    btn_DownCigOutConvexity_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigOutConvexity_dec->setMaximumSize(50, 50);
    lab_DownCigOutConvexity->setText(QStringLiteral("下烟凸度参数"));
    lab_DownCigOutConvexity->setMaximumSize(220, 50);
    lab_DownCigOutConvexity->setAlignment(Qt::AlignCenter);
    lab_DownCigOutConvexity->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigOutConvexity->setMaximumSize(50, 50);
    ledit_DownCigOutConvexity->setAlignment(Qt::AlignCenter);
    ledit_DownCigOutConvexity->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigOutConvexity->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_DownCigOutPixArea_dec);
    zu1_00_v2_h1->addWidget(lab_DownCigOutPixArea);
    zu1_00_v2_h1->addWidget(ledit_DownCigOutPixArea);
    zu1_00_v2_h1->addWidget(btn_DownCigOutPixArea_add);

    zu1_00_v2_h2->addWidget(btn_DownCigOutrectangularity_dec);
    zu1_00_v2_h2->addWidget(lab_DownCigOutrectangularity);
    zu1_00_v2_h2->addWidget(ledit_DownCigOutrectangularity);
    zu1_00_v2_h2->addWidget(btn_DownCigOutrectangularity_add);

    zu1_00_v2_h3->addWidget(btn_DownCigOutConvexity_dec);
    zu1_00_v2_h3->addWidget(lab_DownCigOutConvexity);
    zu1_00_v2_h3->addWidget(ledit_DownCigOutConvexity);
    zu1_00_v2_h3->addWidget(btn_DownCigOutConvexity_add);

    connect(btn_UpCigOutPixArea_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigOutPixArea->text().toInt() + 1;
        ledit_UpCigOutPixArea->setText(QString::number(value));
        });

    connect(btn_UpCigOutPixArea_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigOutPixArea->text().toInt() - 1;
        ledit_UpCigOutPixArea->setText(QString::number(value));
        });

    connect(btn_UpCigOutrectangularity_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigOutrectangularity->text().toInt() + 1;
        ledit_UpCigOutrectangularity->setText(QString::number(value));
        });

    connect(btn_UpCigOutrectangularity_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigOutrectangularity->text().toInt() - 1;
        ledit_UpCigOutrectangularity->setText(QString::number(value));
        });

    connect(btn_UpCigOutConvexity_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigOutConvexity->text().toInt() + 1;
        ledit_UpCigOutConvexity->setText(QString::number(value));
        });

    connect(btn_UpCigOutConvexity_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigOutConvexity->text().toInt() - 1;
        ledit_UpCigOutConvexity->setText(QString::number(value));
        });

    connect(btn_DownCigOutPixArea_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigOutPixArea->text().toInt() + 1;
        ledit_DownCigOutPixArea->setText(QString::number(value));
        });

    connect(btn_DownCigOutPixArea_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigOutPixArea->text().toInt() - 1;
        ledit_DownCigOutPixArea->setText(QString::number(value));
        });

    connect(btn_DownCigOutrectangularity_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigOutrectangularity->text().toInt() + 1;
        ledit_DownCigOutrectangularity->setText(QString::number(value));
        });

    connect(btn_DownCigOutrectangularity_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigOutrectangularity->text().toInt() - 1;
        ledit_DownCigOutrectangularity->setText(QString::number(value));
        });

    connect(btn_DownCigOutConvexity_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigOutConvexity->text().toInt() + 1;
        ledit_DownCigOutConvexity->setText(QString::number(value));
        });

    connect(btn_DownCigOutConvexity_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigOutConvexity->text().toInt() - 1;
        ledit_DownCigOutConvexity->setText(QString::number(value));
        });
    
    connect(ledit_UpCigOutPixArea, &QLineEdit::textChanged, [=](const QString& text) {
        upOutDefectParams.outPix = text.toInt();
        });
    connect(ledit_UpCigOutrectangularity, &QLineEdit::textChanged, [=](const QString& text) {
        upOutDefectParams.rectangularity = text.toDouble();
        });
    connect(ledit_UpCigOutConvexity, &QLineEdit::textChanged, [=](const QString& text) {
        upOutDefectParams.convexity = text.toDouble();
        });
    connect(ledit_DownCigOutPixArea, &QLineEdit::textChanged, [=](const QString& text) {
        downOutDefectParams.outPix = text.toInt();
        });
    connect(ledit_DownCigOutrectangularity, &QLineEdit::textChanged, [=](const QString& text) {
        downOutDefectParams.rectangularity = text.toDouble();
        });
    connect(ledit_DownCigOutConvexity, &QLineEdit::textChanged, [=](const QString& text) {
        downOutDefectParams.convexity = text.toDouble();
        });
    
    // 初始化编辑框的值
    ledit_UpCigOutPixArea->setText(QString::number(upOutDefectParams.outPix));
    ledit_UpCigOutrectangularity->setText(QString::number(upOutDefectParams.rectangularity));
    ledit_UpCigOutConvexity->setText(QString::number(upOutDefectParams.convexity));
    ledit_DownCigOutPixArea->setText(QString::number(downOutDefectParams.outPix));
    ledit_DownCigOutrectangularity->setText(QString::number(downOutDefectParams.rectangularity));
    ledit_DownCigOutConvexity->setText(QString::number(downOutDefectParams.convexity));
}

void CigVisionParams::initJointNGWidgets()
{
    QHBoxLayout* zu1_JointNG_paraHBoxLayout = new QHBoxLayout();//整体布局
    jointNGSet_widget->setLayout(zu1_JointNG_paraHBoxLayout);
    //下烟
    QVBoxLayout* zu1_00_v1 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v2 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v3 = new QVBoxLayout();//行
    QVBoxLayout* zu1_00_v4 = new QVBoxLayout();//行
    QSpacerItem* spacer1 = new QSpacerItem(50, 30);//填充弹簧

    zu1_JointNG_paraHBoxLayout->addLayout(zu1_00_v1, 1);
    //zu1_paraHBoxLayout->addItem(spacer1);
    zu1_JointNG_paraHBoxLayout->addLayout(zu1_00_v2, 1);
    zu1_JointNG_paraHBoxLayout->addLayout(zu1_00_v3, 1);
    zu1_JointNG_paraHBoxLayout->addLayout(zu1_00_v4, 1);

    QHBoxLayout* zu1_00_v1_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v1_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v2_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v2_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v3_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v3_h4 = new QHBoxLayout();//列

    QHBoxLayout* zu1_00_v4_h1 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h2 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h3 = new QHBoxLayout();//列
    QHBoxLayout* zu1_00_v4_h4 = new QHBoxLayout();//列

    zu1_00_v1->addLayout(zu1_00_v1_h1);
    zu1_00_v1->addLayout(zu1_00_v1_h2);
    zu1_00_v1->addLayout(zu1_00_v1_h3);
    zu1_00_v1->addLayout(zu1_00_v1_h4);

    zu1_00_v2->addLayout(zu1_00_v2_h1);
    zu1_00_v2->addLayout(zu1_00_v2_h2);
    zu1_00_v2->addLayout(zu1_00_v2_h3);
    zu1_00_v2->addLayout(zu1_00_v2_h4);

    zu1_00_v3->addLayout(zu1_00_v3_h1);
    zu1_00_v3->addLayout(zu1_00_v3_h2);
    zu1_00_v3->addLayout(zu1_00_v3_h3);
    zu1_00_v3->addLayout(zu1_00_v3_h4);

    zu1_00_v4->addLayout(zu1_00_v4_h1);
    zu1_00_v4->addLayout(zu1_00_v4_h2);
    zu1_00_v4->addLayout(zu1_00_v4_h3);
    zu1_00_v4->addLayout(zu1_00_v4_h4);

    //上图
    //上烟拼接缺陷面积阈值
    QPushButton* btn_UpCigJointArea_add = new QPushButton();
    QPushButton* btn_UpCigJointArea_dec = new QPushButton();
    QLabel* lab_UpCigJointArea = new QLabel();
    QLineEdit* ledit_UpCigJointArea = new QLineEdit();

    //下烟拼接缺陷面积阈值
    QPushButton* btn_DownCigJointArea_add = new QPushButton();
    QPushButton* btn_DownCigJointArea_dec = new QPushButton();
    QLabel* lab_DownCigJointArea = new QLabel();
    QLineEdit* ledit_DownCigJointArea = new QLineEdit();

    btn_UpCigJointArea_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_UpCigJointArea_add->setMaximumSize(50, 50);
    btn_UpCigJointArea_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_UpCigJointArea_dec->setMaximumSize(50, 50);
    lab_UpCigJointArea->setText(QStringLiteral("上烟拼接缺陷面积阈值"));
    lab_UpCigJointArea->setMaximumSize(220, 50);
    lab_UpCigJointArea->setAlignment(Qt::AlignCenter);
    lab_UpCigJointArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigJointArea->setMaximumSize(50, 50);
    ledit_UpCigJointArea->setAlignment(Qt::AlignCenter);
    ledit_UpCigJointArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_UpCigJointArea->setMaxLength(3);

    zu1_00_v1_h1->addWidget(btn_UpCigJointArea_dec);
    zu1_00_v1_h1->addWidget(lab_UpCigJointArea);
    zu1_00_v1_h1->addWidget(ledit_UpCigJointArea);
    zu1_00_v1_h1->addWidget(btn_UpCigJointArea_add);

    btn_DownCigJointArea_add->setIcon(QPixmap(QStringLiteral("icons/use/arrow-right-bold (green).png")));
    btn_DownCigJointArea_add->setMaximumSize(50, 50);
    btn_DownCigJointArea_dec->setIcon(QPixmap(QStringLiteral("icons/use/arrow-left-bold (green).png")));
    btn_DownCigJointArea_dec->setMaximumSize(50, 50);
    lab_DownCigJointArea->setText(QStringLiteral("下烟拼接缺陷面积阈值"));
    lab_DownCigJointArea->setMaximumSize(220, 50);
    lab_DownCigJointArea->setAlignment(Qt::AlignCenter);
    lab_DownCigJointArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigJointArea->setMaximumSize(50, 50);
    ledit_DownCigJointArea->setAlignment(Qt::AlignCenter);
    ledit_DownCigJointArea->setStyleSheet("background-color: rgb(200,200,200); font-size: 14px; ");
    ledit_DownCigJointArea->setMaxLength(3);

    zu1_00_v2_h1->addWidget(btn_DownCigJointArea_dec);
    zu1_00_v2_h1->addWidget(lab_DownCigJointArea);
    zu1_00_v2_h1->addWidget(ledit_DownCigJointArea);
    zu1_00_v2_h1->addWidget(btn_DownCigJointArea_add);

    connect(btn_UpCigJointArea_add, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigJointArea->text().toInt() + 1;
        ledit_UpCigJointArea->setText(QString::number(value));
        });
    connect(btn_UpCigJointArea_dec, &QPushButton::clicked, [=]() {
        int value = ledit_UpCigJointArea->text().toInt() - 1;
        ledit_UpCigJointArea->setText(QString::number(value));
        });

    connect(btn_DownCigJointArea_add, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigJointArea->text().toInt() + 1;
        ledit_DownCigJointArea->setText(QString::number(value));
        });
    connect(btn_DownCigJointArea_dec, &QPushButton::clicked, [=]() {
        int value = ledit_DownCigJointArea->text().toInt() - 1;
        ledit_DownCigJointArea->setText(QString::number(value));
        });
    connect(ledit_UpCigJointArea, &QLineEdit::textChanged, [=](const QString& text) {
        upJointDefectParams.defectArea = text.toInt();
        });
    connect(ledit_DownCigJointArea, &QLineEdit::textChanged, [=](const QString& text) {
        downJointDefectParams.defectArea = text.toInt();
        });
    // 初始化编辑框的值
    ledit_UpCigJointArea->setText(QString::number(upJointDefectParams.defectArea));
    ledit_DownCigJointArea->setText(QString::number(downJointDefectParams.defectArea));
}
void CigVisionParams::onOperatorTableWidgetItemClicked(QTableWidgetItem* Item) {
	qDebug() << "OperatorTableItem clicked";
	int row = Item->row();
	select_operatorRow = row;
	showParamsWidgets(select_processRow, row);
}
void CigVisionParams::onProcessTableWidgetItemClicked(QTableWidgetItem* Item) {
	    qDebug() << "ProcessTableItem clicked";
	    int row = Item->row();
	    select_processRow = row;
	    QFont font;
	    font.setBold(true); // 设置为粗体
	    font.setPointSize(10); // 设置字体大小为10
	    
	    zu1_operatorTableWidget->verticalHeader()->hide();
	    zu1_operatorTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	    zu1_operatorTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	    zu1_operatorTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);//不可编辑
	    zu1_operatorTableWidget->setShowGrid(false);//不显示网格
	    zu1_operatorTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);//固定宽
	    zu1_operatorTableWidget->horizontalHeader()->setFont(font);
	    zu1_operatorTableWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	    zu1_operatorTableWidget->setColumnWidth(0, 50);//序号列，宽度
	    zu1_operatorTableWidget->setColumnWidth(1, 150);//序号列，宽度
	    zu1_operatorTableWidget->setColumnWidth(2, 150);//序号列，宽度
	
	    zu1_operatorTableWidget->horizontalHeader()->setSectionsClickable(false);//不可点表头
	    zu1_operatorTableWidget->setStyleSheet("QTableWidget { gridline-color: transparent; }");
	    
	    switch (row)
	    {
	    case 0://烟支定位
	        zu1_operatorTableWidget->clear();
	        zu1_operatorTableWidget->setRowCount(5);
	        zu1_operatorTableWidget->setItem(0, 0, new QTableWidgetItem("1"));
	        zu1_operatorTableWidget->setItem(1, 0, new QTableWidgetItem("2"));
	        zu1_operatorTableWidget->setItem(2, 0, new QTableWidgetItem("3"));
	        zu1_operatorTableWidget->setItem(3, 0, new QTableWidgetItem("4"));
            zu1_operatorTableWidget->setItem(4, 0, new QTableWidgetItem("5"));
	        
	        zu1_operatorTableWidget->setItem(0, 1, new QTableWidgetItem(QStringLiteral("端定位框")));
	        zu1_operatorTableWidget->setItem(1, 1, new QTableWidgetItem(QStringLiteral("烟边定位/宽度计算")));
	        zu1_operatorTableWidget->setItem(2, 1, new QTableWidgetItem(QStringLiteral("烟棒定位框")));
	        zu1_operatorTableWidget->setItem(3, 1, new QTableWidgetItem(QStringLiteral("嘴棒定位框")));
            zu1_operatorTableWidget->setItem(4, 1, new QTableWidgetItem(QStringLiteral("拼接定位框")));
            
	
	        zu1_operatorTableWidget->setItem(0, 2, new QTableWidgetItem(QStringLiteral("识别定位")));
	        zu1_operatorTableWidget->setItem(1, 2, new QTableWidgetItem(QStringLiteral("识别定位")));
	        zu1_operatorTableWidget->setItem(2, 2, new QTableWidgetItem(QStringLiteral("检测区间")));
	        zu1_operatorTableWidget->setItem(3, 2, new QTableWidgetItem(QStringLiteral("检测区间")));
            zu1_operatorTableWidget->setItem(4, 2, new QTableWidgetItem(QStringLiteral("检测区间/算法类型")));
            
	        
	        break;
	    case 1://烟棒缺陷
	        zu1_operatorTableWidget->clear();
	        zu1_operatorTableWidget->setRowCount(1);
	        zu1_operatorTableWidget->setItem(0, 0, new QTableWidgetItem("1"));
	
	        zu1_operatorTableWidget->setItem(0, 1, new QTableWidgetItem(QStringLiteral("烟棒暗点检测")));
	
	        zu1_operatorTableWidget->setItem(0, 2, new QTableWidgetItem(QStringLiteral("检测灵敏度")));
	        break;
	    case 2://滤棒缺陷
	        zu1_operatorTableWidget->clear();
	        zu1_operatorTableWidget->setRowCount(2);
	        zu1_operatorTableWidget->setItem(0, 0, new QTableWidgetItem("1"));
            zu1_operatorTableWidget->setItem(1, 0, new QTableWidgetItem("2"));
	        zu1_operatorTableWidget->setItem(0, 1, new QTableWidgetItem(QStringLiteral("嘴棒暗点检测")));
            zu1_operatorTableWidget->setItem(1, 1, new QTableWidgetItem(QStringLiteral("嘴棒亮点检测")));
	        zu1_operatorTableWidget->setItem(0, 2, new QTableWidgetItem(QStringLiteral("检测灵敏度")));
            zu1_operatorTableWidget->setItem(1, 2, new QTableWidgetItem(QStringLiteral("检测灵敏度")));
	        break;
	    case 3://外形缺陷
            zu1_operatorTableWidget->clear();
            zu1_operatorTableWidget->setRowCount(2);
            zu1_operatorTableWidget->setItem(0, 0, new QTableWidgetItem("1"));
            zu1_operatorTableWidget->setItem(0, 1, new QTableWidgetItem(QStringLiteral("外形缺陷检测")));
            zu1_operatorTableWidget->setItem(0, 2, new QTableWidgetItem(QStringLiteral("检测灵敏度")));
	        break;
	    case 4://搭扣缺陷
            zu1_operatorTableWidget->clear();
            zu1_operatorTableWidget->setRowCount(1);
            zu1_operatorTableWidget->setItem(0, 0, new QTableWidgetItem("1"));
            zu1_operatorTableWidget->setItem(0, 1, new QTableWidgetItem(QStringLiteral("搭口缺陷检测")));
            zu1_operatorTableWidget->setItem(0, 2, new QTableWidgetItem(QStringLiteral("检测灵敏度")));
	        break;
        case 5://深度学习
			zu1_operatorTableWidget->clear();
			zu1_operatorTableWidget->setRowCount(1);
			zu1_operatorTableWidget->setItem(0, 0, new QTableWidgetItem("1"));

			zu1_operatorTableWidget->setItem(0, 1, new QTableWidgetItem(QStringLiteral("深度学习")));

			zu1_operatorTableWidget->setItem(0, 2, new QTableWidgetItem(QStringLiteral("置信度设置")));
            break;
	    default:
	        break;
	    }
	    QStringList operator_class_stringlist;
	    operator_class_stringlist << QStringLiteral("编号") << QStringLiteral("算  法") << QStringLiteral("作  用");
	    zu1_operatorTableWidget->setHorizontalHeaderLabels(operator_class_stringlist);
	    for (int row = 0; row < zu1_operatorTableWidget->rowCount(); ++row) {
	        for (int col = 0; col < zu1_operatorTableWidget->columnCount(); ++col)
	        {
	            QTableWidgetItem* item = zu1_operatorTableWidget->item(row, col);
	            if (item) {
	                item->setBackground(QBrush(QColor(Qt::white)));
	                item->setTextAlignment(Qt::AlignCenter);
	            }
	        }
	    }
	}

void CigVisionParams::showParamsWidgets(int processID, int operatorID)//显示具体画面，添加tabWidgets
{

    if (processID == 0 && operatorID == 0) //烟支定位，烟端定位
    {
        paramsTabWidget->clear();
        paramsTabWidget->addTab(upCigTopPosParaSet_widget, QStringLiteral("上烟端检测 "));
        paramsTabWidget->addTab(downCigTopPosParaSet_widget, QStringLiteral("下烟端检测 "));
        current_process_step=process_step::Loc_Top;
        // 更新上烟端检测参数
        QList<QLineEdit*> upLineEdits = upCigTopPosParaSet_widget->findChildren<QLineEdit*>();
        foreach(QLineEdit* edit, upLineEdits) {
            if(edit->objectName() == "ledit_UPCenterX")
                edit->setText(QString::number(getUpCigTopPositionParams().centerX));
            else if(edit->objectName() == "ledit_UPCenterY")  
                edit->setText(QString::number(getUpCigTopPositionParams().centerY));
            else if(edit->objectName() == "ledit_UPwidth")
                edit->setText(QString::number(getUpCigTopPositionParams().width));
            else if(edit->objectName() == "ledit_UPheight")
                edit->setText(QString::number(getUpCigTopPositionParams().height));
            else if(edit->objectName() == "ledit_UPsigma")
                edit->setText(QString::number(getUpCigTopPositionParams().sigmaCode));
            else if(edit->objectName() == "ledit_UPstartBS")
                edit->setText(QString::number(getUpCigTopPositionParams().edgeGradientStartThreshold));
            else if(edit->objectName() == "ledit_UPstepBS")
                edit->setText(QString::number(getUpCigTopPositionParams().stepGradientThreshold));
            else if(edit->objectName() == "ledit_UPmodelX")
                edit->setText(QString::number(getUpCigTopModelPositionParams().modelPositionX));
            else if(edit->objectName() == "ledit_UPmodelY")
                edit->setText(QString::number(getUpCigTopModelPositionParams().modelPositionY));
        }

        // 更新下烟端检测参数
        QList<QLineEdit*> downLineEdits = downCigTopPosParaSet_widget->findChildren<QLineEdit*>();
        foreach(QLineEdit* edit, downLineEdits) {
            if(edit->objectName() == "ledit_DownCenterX")
                edit->setText(QString::number(getDownCigTopPositionParams().centerX));
            else if(edit->objectName() == "ledit_DownCenterY")
                edit->setText(QString::number(getDownCigTopPositionParams().centerY));
            else if(edit->objectName() == "ledit_Downwidth")
                edit->setText(QString::number(getDownCigTopPositionParams().width));
            else if(edit->objectName() == "ledit_Downheight")
                edit->setText(QString::number(getDownCigTopPositionParams().height));
            else if(edit->objectName() == "ledit_Downsigma")
                edit->setText(QString::number(getDownCigTopPositionParams().sigmaCode));
            else if(edit->objectName() == "ledit_DownstartBS")
                edit->setText(QString::number(getDownCigTopPositionParams().edgeGradientStartThreshold));
            else if(edit->objectName() == "ledit_DownstepBS")
                edit->setText(QString::number(getDownCigTopPositionParams().stepGradientThreshold));
            else if(edit->objectName() == "ledit_DownmodelX")
                edit->setText(QString::number(getDownCigTopModelPositionParams().modelPositionX));
            else if(edit->objectName() == "ledit_DownmodelY")
                edit->setText(QString::number(getDownCigTopModelPositionParams().modelPositionY));
        }
    }
    if (processID == 0 and operatorID == 1)//烟支定位，边定位
    {
        paramsTabWidget->clear();
        paramsTabWidget->addTab(upCigBoundryPosParaSet_widget, QStringLiteral("上烟边检测 "));
        paramsTabWidget->addTab(downCigBoundryPosParaSet_widget, QStringLiteral("下烟边检测 "));

        current_process_step = process_step::Loc_Side;
        // 更新上烟边检测参数
        QList<QLineEdit*> upLineEdits = upCigBoundryPosParaSet_widget->findChildren<QLineEdit*>();
        foreach(QLineEdit* edit, upLineEdits) {
            if(edit->objectName() == "ledit_UPStartDistance")
                edit->setText(QString::number(getUpBoxPositionParams().frameStartDistance));
            else if(edit->objectName() == "ledit_UPRectDistance")
                edit->setText(QString::number(getUpBoxPositionParams().framesHorizontalDistance));
            else if(edit->objectName() == "ledit_UPwidth")
                edit->setText(QString::number(getUpBoxPositionParams().frameWidth));
            else if(edit->objectName() == "ledit_UPheight")
                edit->setText(QString::number(getUpBoxPositionParams().frameHeight));
            else if(edit->objectName() == "ledit_UPsigma")
                edit->setText(QString::number(getUpBoxPositionParams().frameSigmaCode));
            else if(edit->objectName() == "ledit_UPstartBS")
                edit->setText(QString::number(getUpBoxPositionParams().frameEdgeGradientThreshold));
        }
        // 更新下烟边检测参数
        QList<QLineEdit*> downLineEdits = downCigBoundryPosParaSet_widget->findChildren<QLineEdit*>();
        foreach(QLineEdit* edit, downLineEdits) {
            if(edit->objectName() == "ledit_DOWNStartDistance")
                edit->setText(QString::number(getDownBoxPositionParams().frameStartDistance));
            else if(edit->objectName() == "ledit_DOWNRectDistance")
                edit->setText(QString::number(getDownBoxPositionParams().framesHorizontalDistance));
            else if(edit->objectName() == "ledit_DOWNwidth")
                edit->setText(QString::number(getDownBoxPositionParams().frameWidth));
            else if(edit->objectName() == "ledit_DOWNheight")
                edit->setText(QString::number(getDownBoxPositionParams().frameHeight));
            else if(edit->objectName() == "ledit_DOWNsigma")
                edit->setText(QString::number(getDownBoxPositionParams().frameSigmaCode));
            else if(edit->objectName() == "ledit_DOWNstartBS")
                edit->setText(QString::number(getDownBoxPositionParams().frameEdgeGradientThreshold));
        }
        
    }
    if (processID == 0 and operatorID == 2)//烟支定位，烟棒定位框
    {
        paramsTabWidget->clear();
        paramsTabWidget->addTab(cigStickROISet_widget, QStringLiteral("烟框参数设置 "));
        current_process_step = process_step::Loc_stick;
    }
    if (processID == 0 and operatorID == 3)//烟支定位，嘴棒定位框
    {
        paramsTabWidget->clear();
        paramsTabWidget->addTab(cigFilterROISet_widget, QStringLiteral("滤嘴框参数设置 "));
        current_process_step = process_step::Loc_filter;
    }
    if (processID == 0 and operatorID == 4)//烟支定位，拼接检测定位框
    {
        paramsTabWidget->clear();
        paramsTabWidget->addTab(cigJointerROISet_widget, QStringLiteral("拼接框参数设置 "));
        current_process_step = process_step::Loc_joint;
    }
    if (processID == 1 and operatorID == 0)//烟棒缺陷检测
	{
		paramsTabWidget->clear();
        paramsTabWidget->addTab(stickNGSet_widget, QStringLiteral("烟棒暗点参数设置 "));
        current_process_step = process_step::Stick_dark;
    }
    if (processID == 2 and operatorID == 0)//滤嘴暗点缺陷检测
    {
        paramsTabWidget->clear();
        paramsTabWidget->addTab(filgerDarkNGSet_widget, QStringLiteral("滤嘴暗点参数设置 "));
        current_process_step = process_step::Filter_dark;
    }
    if (processID == 2 and operatorID == 1)//滤嘴亮点缺陷检测
    {
        paramsTabWidget->clear();
        paramsTabWidget->addTab(filgerWhiteNGSet_widget, QStringLiteral("滤嘴亮点参数设置 "));
        current_process_step = process_step::Filter_white;
    }
    if (processID == 3 and operatorID == 0)//外形缺陷检测
    {
        paramsTabWidget->clear();
        paramsTabWidget->addTab(cigOutNGSet_widget, QStringLiteral("外型缺陷参数设置 "));
        current_process_step = process_step::Cig_Out;
    }
    if (processID == 4 and operatorID == 0)//搭口缺陷检测
    {
        paramsTabWidget->clear();
        paramsTabWidget->addTab(jointNGSet_widget, QStringLiteral("搭口缺陷参数设置 "));
        current_process_step = process_step::Joint;
    }
	if (processID == 5 and operatorID == 0)//深度学习置信度设置
	{
		paramsTabWidget->clear();
        paramsTabWidget->addTab(stickNGSet_widget, QStringLiteral("深度学习参数设置 "));
        current_process_step = process_step::Deep;
	}
    //
}

bool CigVisionParams::loadBrandParams(const QString& brandName)
{
    QString paraPath = brandsBasePath + "/" + brandName + "/para.ini";
    if (!QFile::exists(paraPath)) {
        qDebug() << "Parameter file does not exist:" << paraPath;
        return false;
    }

    QSettings settings(paraPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");  // 设置UTF-8编码
    
    // 加载上烟端定位参数
    settings.beginGroup("UpCigTopPosition");
    upCigTopPositionParams.centerX = settings.value("centerX", 0).toInt();
    upCigTopPositionParams.centerY = settings.value("centerY", 0).toInt();
    upCigTopPositionParams.width = settings.value("width", 0).toInt();
    upCigTopPositionParams.height = settings.value("height", 0).toInt();
    upCigTopPositionParams.sigmaCode = settings.value("sigmaCode", 0).toInt();
    upCigTopPositionParams.edgeGradientStartThreshold = settings.value("edgeGradientStartThreshold", 0).toInt();
    upCigTopPositionParams.stepGradientThreshold = settings.value("stepGradientThreshold", 0).toInt();
    settings.endGroup();

    // 加载下烟端定位参数
    settings.beginGroup("DownCigTopPosition");
    downCigTopPositionParams.centerX = settings.value("centerX", 0).toInt();
    downCigTopPositionParams.centerY = settings.value("centerY", 0).toInt();
    downCigTopPositionParams.width = settings.value("width", 0).toInt();
    downCigTopPositionParams.height = settings.value("height", 0).toInt();
    downCigTopPositionParams.sigmaCode = settings.value("sigmaCode", 0).toInt();
    downCigTopPositionParams.edgeGradientStartThreshold = settings.value("edgeGradientStartThreshold", 0).toInt();
    downCigTopPositionParams.stepGradientThreshold = settings.value("stepGradientThreshold", 0).toInt();
    settings.endGroup();

    // 加载上烟端模版参数
    settings.beginGroup("UpCigTopModel");
    upCigTopModelPositionParams.modelPositionX = settings.value("modelPositionX", 0).toInt();
    upCigTopModelPositionParams.modelPositionY = settings.value("modelPositionY", 0).toInt();
    upCigTopModelPositionParams.modelWidth = settings.value("modelWidth", 0).toInt();
    settings.endGroup();

    // 加载下烟端模版参数
    settings.beginGroup("DownCigTopModel");
    downCigTopModelPositionParams.modelPositionX = settings.value("modelPositionX", 0).toInt();
    downCigTopModelPositionParams.modelPositionY = settings.value("modelPositionY", 0).toInt();
    downCigTopModelPositionParams.modelWidth = settings.value("modelWidth", 0).toInt();
    settings.endGroup();

    // 加载上框定位参数
    settings.beginGroup("UpBoxPosition");
    upBoxPositionParams.frameStartDistance = settings.value("frameStartDistance", 0).toInt();
    upBoxPositionParams.framesHorizontalDistance = settings.value("framesHorizontalDistance", 0).toInt();
    upBoxPositionParams.frameHeight = settings.value("frameHeight", 0).toInt();
    upBoxPositionParams.frameWidth = settings.value("frameWidth", 0).toInt();
    upBoxPositionParams.frameSigmaCode = settings.value("frameSigmaCode", 0).toInt();
    upBoxPositionParams.frameEdgeGradientThreshold = settings.value("frameEdgeGradientThreshold", 0).toInt();
    settings.endGroup();

    // 加载下框定位参数
    settings.beginGroup("DownBoxPosition");
    downBoxPositionParams.frameStartDistance = settings.value("frameStartDistance", 0).toInt();
    downBoxPositionParams.framesHorizontalDistance = settings.value("framesHorizontalDistance", 0).toInt();
    downBoxPositionParams.frameHeight = settings.value("frameHeight", 0).toInt();
    downBoxPositionParams.frameWidth = settings.value("frameWidth", 0).toInt();
    downBoxPositionParams.frameSigmaCode = settings.value("frameSigmaCode", 0).toInt();
    downBoxPositionParams.frameEdgeGradientThreshold = settings.value("frameEdgeGradientThreshold", 0).toInt();
    settings.endGroup();

    // 加载上框模版参数
    settings.beginGroup("UpBoxModel");
    upBoxModelPositionParams.model1PositionX = settings.value("model1PositionX", 0).toInt();
    upBoxModelPositionParams.model1PositionY = settings.value("model1PositionY", 0).toInt();
    upBoxModelPositionParams.model1Width = settings.value("model1Width", 0).toInt();
    upBoxModelPositionParams.model2PositionX = settings.value("model2PositionX", 0).toInt();
    upBoxModelPositionParams.model2PositionY = settings.value("model2PositionY", 0).toInt();
    upBoxModelPositionParams.model2Width = settings.value("model2Width", 0).toInt();
    upBoxModelPositionParams.model3PositionX = settings.value("model3PositionX", 0).toInt();
    upBoxModelPositionParams.model3PositionY = settings.value("model3PositionY", 0).toInt();
    upBoxModelPositionParams.model3Width = settings.value("model3Width", 0).toInt();
    settings.endGroup();

    // 加载下框模版参数
    settings.beginGroup("DownBoxModel");
    downBoxModelPositionParams.model1PositionX = settings.value("model1PositionX", 0).toInt();
    downBoxModelPositionParams.model1PositionY = settings.value("model1PositionY", 0).toInt();
    downBoxModelPositionParams.model1Width = settings.value("model1Width", 0).toInt();
    downBoxModelPositionParams.model2PositionX = settings.value("model2PositionX", 0).toInt();
    downBoxModelPositionParams.model2PositionY = settings.value("model2PositionY", 0).toInt();
    downBoxModelPositionParams.model2Width = settings.value("model2Width", 0).toInt();
    downBoxModelPositionParams.model3PositionX = settings.value("model3PositionX", 0).toInt();
    downBoxModelPositionParams.model3PositionY = settings.value("model3PositionY", 0).toInt();
    downBoxModelPositionParams.model3Width = settings.value("model3Width", 0).toInt();
    settings.endGroup();

    // 加载其他参数...
    // 烟体定位参数
    settings.beginGroup("UpCigBodyPosition");
    upCigBodyPositionParams.bodyStartDistance = settings.value("bodyStartDistance", 0).toInt();
    upCigBodyPositionParams.bodyLength = settings.value("bodyLength", 0).toInt();
    upCigBodyPositionParams.innerEdgeThreshold = settings.value("innerEdgeThreshold", 0).toInt();
    settings.endGroup();
    settings.beginGroup("DownCigBodyPosition");
    downCigBodyPositionParams.bodyStartDistance = settings.value("bodyStartDistance", 0).toInt();
    downCigBodyPositionParams.bodyLength = settings.value("bodyLength", 0).toInt();
    downCigBodyPositionParams.innerEdgeThreshold = settings.value("innerEdgeThreshold", 0).toInt();
    settings.endGroup();

    // 嘴棒定位参数
    settings.beginGroup("UpFilterPosition");
    upFilterPositionParams.filterStartDistance = settings.value("filterStartDistance", 0).toInt();
    upFilterPositionParams.filterLength = settings.value("filterLength", 0).toInt();
    upFilterPositionParams.innerEdgeThreshold = settings.value("innerEdgeThreshold", 0).toInt();

    settings.endGroup();
    settings.beginGroup("DownFilterPosition");
    downFilterPositionParams.filterStartDistance = settings.value("filterStartDistance", 0).toInt();
    downFilterPositionParams.filterLength = settings.value("filterLength", 0).toInt();
    downFilterPositionParams.innerEdgeThreshold = settings.value("innerEdgeThreshold", 0).toInt();

    settings.endGroup();

    // 拼接定位参数
    settings.beginGroup("UpJointPosition");
    upJointPositionParams.jointStartDistance = settings.value("jointStartDistance", 0).toInt();
    upJointPositionParams.jointLength = settings.value("jointLength", 0).toInt();
    upJointPositionParams.isLinear = settings.value("isLinear", false).toBool();
    upJointPositionParams.isTopDown = settings.value("isTopDown", false).toBool();
    upJointPositionParams.isDeepLearning = settings.value("isDeepLearning", false).toBool();
    settings.endGroup();
    settings.beginGroup("DownJointPosition");
    downJointPositionParams.jointStartDistance = settings.value("jointStartDistance", 0).toInt();
    downJointPositionParams.jointLength = settings.value("jointLength", 0).toInt();
    downJointPositionParams.isLinear = settings.value("isLinear", false).toBool();
    downJointPositionParams.isTopDown = settings.value("isTopDown", false).toBool();
    downJointPositionParams.isDeepLearning = settings.value("isDeepLearning", false).toBool();
    settings.endGroup();

    // 加载烟体缺陷参数
    settings.beginGroup("UpCigStickDarkDefect");
    upCigStickDarkDefectParams.darkPointAreasValue = settings.value("darkPointAreasValue", 0).toInt();
    upCigStickDarkDefectParams.darkPointGrayValue = settings.value("darkPointGrayValue", 0).toInt();
    settings.endGroup();
    settings.beginGroup("DownCigStickDarkDefect");
    downCigStickDarkDefectParams.darkPointAreasValue = settings.value("darkPointAreasValue", 0).toInt();
    downCigStickDarkDefectParams.darkPointGrayValue = settings.value("darkPointGrayValue", 0).toInt();
    settings.endGroup();

    // 加载嘴棒缺陷参数
    settings.beginGroup("UpFilterDarkDefect");
    upCigFilterDarkDefectParams.darkPointAreasValue = settings.value("darkPointAreasValue", 0).toInt();
    upCigFilterDarkDefectParams.darkPointGrayValue = settings.value("darkPointGrayValue", 0).toInt();
    settings.endGroup();
    settings.beginGroup("DownFilterDarkDefect");
    downCigFilterDarkDefectParams.darkPointAreasValue = settings.value("darkPointAreasValue", 0).toInt();
    downCigFilterDarkDefectParams.darkPointGrayValue = settings.value("darkPointGrayValue", 0).toInt();
    settings.endGroup();

    settings.beginGroup("UpFilterWhiteDefect");
    upCigFilterWhiteDefectParams.brightPointAreasValue = settings.value("darkPointAreasValue", 0).toInt();
    upCigFilterWhiteDefectParams.brightPointGrayValue = settings.value("darkPointGrayValue", 0).toInt();
    settings.endGroup();
    settings.beginGroup("DownFilterWhiteDefect");
    downCigFilterWhiteDefectParams.brightPointAreasValue = settings.value("darkPointAreasValue", 0).toInt();
    downCigFilterWhiteDefectParams.brightPointGrayValue = settings.value("darkPointGrayValue", 0).toInt();
    settings.endGroup();
    // 加载拼接搓牙参数
    settings.beginGroup("UpJointDefect");
    upJointDefectParams.defectArea = settings.value("defectArea", 0).toInt();
    settings.endGroup();
    settings.beginGroup("DownJointDefect");
    downJointDefectParams.defectArea = settings.value("defectArea", 0).toInt();
    settings.endGroup();

	// 加载深度学习参数
	settings.beginGroup("DeepLearningParams");
	deepLearningParams.jointRollThreshold = settings.value("jointRollThreshold", 0.5).toDouble();
	deepLearningParams.flyingTobaccoThreshold = settings.value("flyingTobaccoThreshold", 0.5).toDouble();
	deepLearningParams.tobaccoClipsThreshold = settings.value("tobaccoClipsThreshold", 0.5).toDouble();
	deepLearningParams.filterWrinkleThreshold = settings.value("filterWrinkleThreshold", 0.5).toDouble();
	deepLearningParams.missingFilterThreshold = settings.value("missingFilterThreshold", 0.5).toDouble();
	deepLearningParams.rodDamageThreshold = settings.value("rodDamageThreshold", 0.5).toDouble();
	deepLearningParams.rodStainThreshold = settings.value("rodStainThreshold", 0.5).toDouble();
	settings.endGroup();
    return true;
}

bool CigVisionParams::saveBrandParams(const QString& brandName)
{
    QString paraPath = brandsBasePath + "/" + brandName + "/para.ini";
    
    // 确保品牌目录存在
    QDir brandDir(brandsBasePath + "/" + brandName);
    if (!brandDir.exists()) {
        brandDir.mkpath(".");
    }

    QSettings settings(paraPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");  // 设置UTF-8编码
    
    // 保存上烟端定位参数
    settings.beginGroup("UpCigTopPosition");
    settings.setValue("centerX", upCigTopPositionParams.centerX);
    settings.setValue("centerY", upCigTopPositionParams.centerY);
    settings.setValue("width", upCigTopPositionParams.width);
    settings.setValue("height", upCigTopPositionParams.height);
    settings.setValue("sigmaCode", upCigTopPositionParams.sigmaCode);
    settings.setValue("edgeGradientStartThreshold", upCigTopPositionParams.edgeGradientStartThreshold);
    settings.setValue("stepGradientThreshold", upCigTopPositionParams.stepGradientThreshold);
    settings.endGroup();

    // 保存下烟端定位参数
    settings.beginGroup("DownCigTopPosition");
    settings.setValue("centerX", downCigTopPositionParams.centerX);
    settings.setValue("centerY", downCigTopPositionParams.centerY);
    settings.setValue("width", downCigTopPositionParams.width);
    settings.setValue("height", downCigTopPositionParams.height);
    settings.setValue("sigmaCode", downCigTopPositionParams.sigmaCode);
    settings.setValue("edgeGradientStartThreshold", downCigTopPositionParams.edgeGradientStartThreshold);
    settings.setValue("stepGradientThreshold", downCigTopPositionParams.stepGradientThreshold);
    settings.endGroup();

    // 保存上烟端模版参数
    settings.beginGroup("UpCigTopModel");
    settings.setValue("modelPositionX", upCigTopModelPositionParams.modelPositionX);
    settings.setValue("modelPositionY", upCigTopModelPositionParams.modelPositionY);
    settings.setValue("modelWidth", upCigTopModelPositionParams.modelWidth);
    settings.endGroup();

    // 保存下烟端模版参数
    settings.beginGroup("DownCigTopModel");
    settings.setValue("modelPositionX", downCigTopModelPositionParams.modelPositionX);
    settings.setValue("modelPositionY", downCigTopModelPositionParams.modelPositionY);
    settings.setValue("modelWidth", downCigTopModelPositionParams.modelWidth);
    settings.endGroup();

    // 保存上框定位参数
    settings.beginGroup("UpBoxPosition");
    settings.setValue("frameStartDistance", upBoxPositionParams.frameStartDistance);
    settings.setValue("framesHorizontalDistance", upBoxPositionParams.framesHorizontalDistance);
    settings.setValue("frameHeight", upBoxPositionParams.frameHeight);
    settings.setValue("frameWidth", upBoxPositionParams.frameWidth);
    settings.setValue("frameSigmaCode", upBoxPositionParams.frameSigmaCode);
    settings.setValue("frameEdgeGradientThreshold", upBoxPositionParams.frameEdgeGradientThreshold);
    settings.endGroup();

    // 保存下框定位参数
    settings.beginGroup("DownBoxPosition");
    settings.setValue("frameStartDistance", downBoxPositionParams.frameStartDistance);
    settings.setValue("framesHorizontalDistance", downBoxPositionParams.framesHorizontalDistance);
    settings.setValue("frameHeight", downBoxPositionParams.frameHeight);
    settings.setValue("frameWidth", downBoxPositionParams.frameWidth);
    settings.setValue("frameSigmaCode", downBoxPositionParams.frameSigmaCode);
    settings.setValue("frameEdgeGradientThreshold", downBoxPositionParams.frameEdgeGradientThreshold);
    settings.endGroup();

    // 保存上框模版参数
    settings.beginGroup("UpBoxModel");
    settings.setValue("model1PositionX", upBoxModelPositionParams.model1PositionX);
    settings.setValue("model1PositionY", upBoxModelPositionParams.model1PositionY);
    settings.setValue("model1Width", upBoxModelPositionParams.model1Width);
    settings.setValue("model2PositionX", upBoxModelPositionParams.model2PositionX);
    settings.setValue("model2PositionY", upBoxModelPositionParams.model2PositionY);
    settings.setValue("model2Width", upBoxModelPositionParams.model2Width);
    settings.setValue("model3PositionX", upBoxModelPositionParams.model3PositionX);
    settings.setValue("model3PositionY", upBoxModelPositionParams.model3PositionY);
    settings.setValue("model3Width", upBoxModelPositionParams.model3Width);
    settings.endGroup();

    // 保存下框模版参数
    settings.beginGroup("DownBoxModel");
    settings.setValue("model1PositionX", downBoxModelPositionParams.model1PositionX);
    settings.setValue("model1PositionY", downBoxModelPositionParams.model1PositionY);
    settings.setValue("model1Width", downBoxModelPositionParams.model1Width);
    settings.setValue("model2PositionX", downBoxModelPositionParams.model2PositionX);
    settings.setValue("model2PositionY", downBoxModelPositionParams.model2PositionY);
    settings.setValue("model2Width", downBoxModelPositionParams.model2Width);
    settings.setValue("model3PositionX", downBoxModelPositionParams.model3PositionX);
    settings.setValue("model3PositionY", downBoxModelPositionParams.model3PositionY);
    settings.setValue("model3Width", downBoxModelPositionParams.model3Width);
    settings.endGroup();

    // 保存其他参数...
    settings.beginGroup("UpCigBodyPosition");
    settings.setValue("bodyStartDistance", upCigBodyPositionParams.bodyStartDistance);
    settings.setValue("bodyLength", upCigBodyPositionParams.bodyLength);
    settings.setValue("innerEdgeThreshold", upCigBodyPositionParams.innerEdgeThreshold);
    settings.endGroup();
    settings.beginGroup("DownCigBodyPosition");
    settings.setValue("bodyStartDistance", downCigBodyPositionParams.bodyStartDistance);
    settings.setValue("bodyLength", downCigBodyPositionParams.bodyLength);
    settings.setValue("innerEdgeThreshold", downCigBodyPositionParams.innerEdgeThreshold);
    settings.endGroup();

    settings.beginGroup("UpFilterPosition");
    settings.setValue("filterStartDistance", upFilterPositionParams.filterStartDistance);
    settings.setValue("filterLength", upFilterPositionParams.filterLength);
    settings.setValue("innerEdgeThreshold", upFilterPositionParams.innerEdgeThreshold);
    settings.endGroup();
    settings.beginGroup("DownFilterPosition");
    settings.setValue("filterStartDistance", downFilterPositionParams.filterStartDistance);
    settings.setValue("filterLength", downFilterPositionParams.filterLength);
    settings.setValue("innerEdgeThreshold", downFilterPositionParams.innerEdgeThreshold);
    settings.endGroup();

    settings.beginGroup("UpJointPosition");
    settings.setValue("jointStartDistance", upJointPositionParams.jointStartDistance);
    settings.setValue("jointLength", upJointPositionParams.jointLength);
    settings.setValue("isLinear", upJointPositionParams.isLinear);
    settings.setValue("isTopDown", upJointPositionParams.isTopDown);
    settings.setValue("isDeepLearning", upJointPositionParams.isDeepLearning);
    settings.endGroup();
    settings.beginGroup("DownJointPosition");
    settings.setValue("jointStartDistance", downJointPositionParams.jointStartDistance);
    settings.setValue("jointLength", downJointPositionParams.jointLength);
    settings.setValue("isLinear", downJointPositionParams.isLinear);
    settings.setValue("isTopDown", downJointPositionParams.isTopDown);
    settings.setValue("isDeepLearning", downJointPositionParams.isDeepLearning);
    settings.endGroup();

    settings.beginGroup("UpCigStickDarkDefect");
    settings.setValue("darkPointAreasValue", upCigStickDarkDefectParams.darkPointAreasValue);
    settings.setValue("darkPointGrayValue", upCigStickDarkDefectParams.darkPointGrayValue);
    settings.endGroup();
    settings.beginGroup("DownCigStickDarkDefect");
    settings.setValue("darkPointAreasValue", downCigStickDarkDefectParams.darkPointAreasValue);
    settings.setValue("darkPointGrayValue", downCigStickDarkDefectParams.darkPointGrayValue);
    settings.endGroup();

    settings.beginGroup("UpFilterDarkDefect");
    settings.setValue("darkPointAreasValue", upCigFilterDarkDefectParams.darkPointAreasValue);
    settings.setValue("darkPointGrayValue", upCigFilterDarkDefectParams.darkPointGrayValue);
    settings.endGroup();
    settings.beginGroup("DownFilterDarkDefect");
    settings.setValue("darkPointAreasValue", downCigFilterDarkDefectParams.darkPointAreasValue);
    settings.setValue("darkPointGrayValue", downCigFilterDarkDefectParams.darkPointGrayValue);
    settings.endGroup();

    settings.beginGroup("UpFilterWhiteDefect");
    settings.setValue("brightPointAreasValue", upCigFilterWhiteDefectParams.brightPointAreasValue);
    settings.setValue("brightPointGrayValue", upCigFilterWhiteDefectParams.brightPointGrayValue);
    settings.endGroup();
    settings.beginGroup("DownFilterWhiteDefect");
    settings.setValue("brightPointAreasValue", downCigFilterWhiteDefectParams.brightPointAreasValue);
    settings.setValue("brightPointGrayValue", downCigFilterWhiteDefectParams.brightPointGrayValue);
    settings.endGroup();

    settings.beginGroup("UpJointDefect");
    settings.setValue("defectArea", upJointDefectParams.defectArea);
    settings.endGroup();
    settings.beginGroup("DownJointDefect");
    settings.setValue("defectArea", downJointDefectParams.defectArea);
    settings.endGroup();

    settings.beginGroup("UpOutDefect");
    settings.setValue("outPix", upOutDefectParams.outPix);
    settings.setValue("rectangularity", upOutDefectParams.rectangularity);
    settings.setValue("convexity", upOutDefectParams.convexity);
    settings.endGroup();
    settings.beginGroup("DownOutDefect");
    settings.setValue("outPix", downOutDefectParams.outPix);
    settings.setValue("rectangularity", downOutDefectParams.rectangularity);
    settings.setValue("convexity", downOutDefectParams.convexity);
    settings.endGroup();

    // 确保设置被写入文件
    settings.sync();
    
    // 检查是否有错误发生
    if (settings.status() != QSettings::NoError) {
        qDebug() << "保存参数时发生错误: " << paraPath;
        return false;
    }
    
    qDebug() << "成功保存参数到: " << paraPath;
    return true;
}

void CigVisionParams::initBrandSelectWidget()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(brandSelectWidget);
    
    // 当前品牌显示区域
    QGroupBox* currentBrandGroup = new QGroupBox(QStringLiteral("当前品牌"));
    QHBoxLayout* currentBrandLayout = new QHBoxLayout();
    currentBrandLabel = new QLabel();
    currentBrandLabel->setText(currentBrand);
    currentBrandLabel->setStyleSheet("QLabel { font-size: 16pt; font-weight: bold; }");
    currentBrandLayout->addWidget(currentBrandLabel);
    currentBrandGroup->setLayout(currentBrandLayout);
    
    // 品牌选择区域
    QGroupBox* brandSelectGroup = new QGroupBox(QStringLiteral("品牌选择"));
    QHBoxLayout* brandSelectLayout = new QHBoxLayout();
    
    brandComboBox = new QComboBox();
    brandComboBox->setStyleSheet("QComboBox { font-size: 14pt; min-height: 30px; }");
    // 获取所有品牌并添加到下拉框
    QDir brandDir(brandsBasePath);
    QStringList brands = brandDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    brandComboBox->addItems(brands);
    // 设置当前选中的品牌
    brandComboBox->setCurrentText(currentBrand);
    
    newBrandButton = new QPushButton(QStringLiteral("新建品牌"));
    newBrandButton->setStyleSheet(
        "QPushButton {"
        "    font-size: 14pt;"
        "    min-height: 30px;"
        "    padding: 5px 15px;"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:pressed { background-color: #3d8b40; }"
    );
    
    brandSelectLayout->addWidget(brandComboBox, 2);
    brandSelectLayout->addWidget(newBrandButton, 1);
    brandSelectGroup->setLayout(brandSelectLayout);
    
    // 添加到主布局
    mainLayout->addWidget(currentBrandGroup);
    mainLayout->addWidget(brandSelectGroup);
    mainLayout->addStretch();
    
    // 连接信号和槽
    connect(brandComboBox, &QComboBox::currentTextChanged, this, &CigVisionParams::onBrandComboBoxChanged);
    connect(newBrandButton, &QPushButton::clicked, this, &CigVisionParams::onNewBrandButtonClicked);
}

void CigVisionParams::onBrandComboBoxChanged(const QString& brandName)
{
    if (!brandName.isEmpty() && brandName != currentBrand) {
        qDebug() << "Switching brand from" << currentBrand << "to" << brandName;
        
        // 更新当前品牌
        currentBrand = brandName;
        
        // 保存到配置文件
        QSettings settings(configPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.setValue("General/CurrentBrand", currentBrand);
        settings.sync();
        
        // 加载新品牌的参数
        if (!loadBrandParams(currentBrand)) {
            qDebug() << "Warning: Failed to load parameters for brand:" << brandName;
        }
        
        // 更新界面显示
        brandComboBox->setCurrentText(currentBrand);
        currentBrandLabel->setText(currentBrand);
        setCurrentBrand(brandName);
        qDebug() << "Brand switch completed:" << currentBrand;
    }
    //发送新消息
    emit selectNewBrand();
}

void CigVisionParams::createNewBrand(const QString& brandName)
{
    // 创建品牌目录
    QString brandPath = brandsBasePath + "/" + brandName;
    QDir().mkpath(brandPath);
    
    // 创建模版图片文件夹
    QString templatePath = brandPath + QStringLiteral("/模版图片");
    QDir().mkpath(templatePath);
    
    // 创建参数配置文件
    QString paraPath = brandPath + "/para.ini";
    QSettings settings(paraPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    
    // 使用默认参数初始化配置文件
    initDefaultParams();
    saveBrandParams(brandName);
    
    // 更新品牌列表
    brandComboBox->addItem(brandName);
    
    // 切换到新品牌
    setCurrentBrand(brandName);
    currentBrandLabel->setText(brandName);
    brandComboBox->setCurrentText(brandName);
}

void CigVisionParams::onNewBrandButtonClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("新建品牌"));
    dialog.setFixedSize(400, 200);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLineEdit* brandNameEdit = new QLineEdit();
    brandNameEdit->setPlaceholderText(QStringLiteral("请输入品牌名称"));
    brandNameEdit->setStyleSheet("QLineEdit { font-size: 14pt; padding: 5px; }");

    // 为brandNameEdit添加事件过滤器，用于显示软键盘
    brandNameEdit->installEventFilter(this);
    
    // 设置软键盘的目标输入框

    layout->addWidget(brandNameEdit);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okButton = new QPushButton(QStringLiteral("确定"));
    QPushButton* cancelButton = new QPushButton(QStringLiteral("取消"));

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, &dialog, [&]() {
        QString newBrandName = brandNameEdit->text().trimmed();
        if (!newBrandName.isEmpty()) {
            createNewBrand(newBrandName);
            dialog.accept();
        }
    });
    ShellExecute(NULL, L"open", L"osk.exe", NULL, NULL, SW_SHOW);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}

// 实现深度学习参数的getter和setter方法
void CigVisionParams::setDeepLearningParams(const DeepLearningParams& params)
{
    deepLearningParams = params;
    saveToIni();
}

void CigVisionParams::initSysParamsWidgets()
{
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(sysParamsWidget);
    
    // 创建标题
    QLabel* titleLabel = new QLabel(QStringLiteral("系统参数配置"));
    titleLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 24px;"
        "   font-weight: bold;"
        "   color: #333333;"
        "   padding: 10px;"
        "   background-color: #f0f0f0;"
        "   border-bottom: 2px solid #cccccc;"
        "}"
    );
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 创建水平布局容器
    QHBoxLayout* contentLayout = new QHBoxLayout();
    mainLayout->addLayout(contentLayout);
    // 初始化并添加系统参数组
    initSystemParamsGroup();
    contentLayout->addWidget(systemParamsGroup);
    
    // 初始化并添加相机参数组
    initCameraParamsGroup();
    contentLayout->addWidget(cameraParamsGroup);
    
    // 设置整体样式
    sysParamsWidget->setStyleSheet(
        "QWidget {"
        "   background-color: white;"
        "}"
        "QGroupBox {"
        "   font-size: 16px;"
        "   font-weight: bold;"
        "   border: 2px solid #cccccc;"
        "   border-radius: 8px;"
        "   margin-top: 1ex;"
        "   padding: 10px;"
        "}"
        "QGroupBox::title {"
        "   subcontrol-origin: margin;"
        "   subcontrol-position: top center;"
        "   padding: 0 5px;"
        "   color: #333333;"
        "}"
    );

    // 添加底部按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* saveButton = new QPushButton(QStringLiteral("保存设置"));
    QPushButton* quitButton = new QPushButton(QStringLiteral("退 出"));
    
    saveButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   border: none;"
        "   padding: 8px 16px;"
        "   font-size: 14px;"
        "   border-radius: 4px;"
        "   min-width: 100px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #45a049;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #3d8b40;"
        "}"
    );
    
    quitButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #f44336;"
        "   color: white;"
        "   border: none;"
        "   padding: 8px 16px;"
        "   font-size: 14px;"
        "   border-radius: 4px;"
        "   min-width: 100px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #da190b;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #c41810;"
        "}"
    );
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(saveButton);
    buttonLayout->addSpacing(20);
    buttonLayout->addWidget(quitButton);
    buttonLayout->addStretch();
    
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    mainLayout->addSpacing(20);

    
    // 保持现有的按钮样式设置不变 ...

    // 连接保存按钮的点击信号
    connect(saveButton, &QPushButton::clicked, this, [this]() {
        // 获取所有参数控件的值
	   // 保存参数
		// 获取所有参数控件的值
		SystemParams newParams;

		// 获取所有SpinBox控件
		QList<QSpinBox*> spinBoxes = sysParamsWidget->findChildren<QSpinBox*>();
		for (QSpinBox* spinBox : spinBoxes) {
			QString name = spinBox->objectName();
			if (name == QStringLiteral("pulseCount"))
				newParams.pulseCount = spinBox->value();
			else if (name == QStringLiteral("component1ToReject"))
				newParams.component1ToReject = spinBox->value();
			else if (name == QStringLiteral("component2ToReject"))
				newParams.component2ToReject = spinBox->value();
			else if (name == QStringLiteral("rejectStart"))
				newParams.rejectStart = spinBox->value();
			else if (name == QStringLiteral("rejectEnd"))
				newParams.rejectEnd = spinBox->value();
		}

		// 获取CheckBox的值
		if (QCheckBox* checkBox = sysParamsWidget->findChild<QCheckBox*>(QStringLiteral("rejectEnabled")))
			newParams.rejectEnabled = checkBox->isChecked();
		if (QCheckBox* checkBox = sysParamsWidget->findChild<QCheckBox*>(QStringLiteral("positionDisplayEnabled")))
			newParams.positionDisplayEnabled = checkBox->isChecked();

		// 获取ComboBox的值
		if (QComboBox* comboBox = sysParamsWidget->findChild<QComboBox*>(QStringLiteral("comPort")))
			newParams.comPort = comboBox->currentText();

		// 保存系统参数
		setSystemParams(newParams);

		// 获取相机参数控件的值
		CameraParams newCameraParams;

		// 获取所有LineEdit控件
		QList<QLineEdit*> lineEdits = sysParamsWidget->findChildren<QLineEdit*>();
		for (QLineEdit* lineEdit : lineEdits) {
			QString name = lineEdit->objectName();
			if (name == QStringLiteral("camera1SerialNum"))
				newCameraParams.camera1SerialNum = lineEdit->text();
			else if (name == QStringLiteral("camera2InnerSerialNum"))
				newCameraParams.camera2InnerSerialNum = lineEdit->text();
			else if (name == QStringLiteral("camera2OuterSerialNum"))
				newCameraParams.camera2OuterSerialNum = lineEdit->text();
		}

		// 获取相机参数的SpinBox值
		for (QSpinBox* spinBox : spinBoxes) {
			QString name = spinBox->objectName();
			if (name == QStringLiteral("lightTriggerDelay1"))
				newCameraParams.lightTriggerDelay1 = spinBox->value();
			else if (name == QStringLiteral("lightExposureTime1"))
				newCameraParams.lightExposureTime1 = spinBox->value();
			else if (name == QStringLiteral("cameraTriggerStart1"))
				newCameraParams.cameraTriggerStart1 = spinBox->value();
			else if (name == QStringLiteral("lightTriggerDelay2"))
				newCameraParams.lightTriggerDelay2 = spinBox->value();
			else if (name == QStringLiteral("lightExposureTime2"))
				newCameraParams.lightExposureTime2 = spinBox->value();
			else if (name == QStringLiteral("cameraTriggerStart2"))
				newCameraParams.cameraTriggerStart2 = spinBox->value();
		}

		// 保存相机参数
		setCameraParams(newCameraParams);

        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("参数保存成功！"));
    });

    // 连接退出按钮的点击信号
    connect(quitButton, &QPushButton::clicked, this, [this]() {
        

        // 隐藏系统参数设置窗口
        if(sysParamsWidget) {
            sysParamsWidget->hide();
        }

        // 发送退出信号
        emit systemParaWidgetQuit();
    });

    buttonLayout->addWidget(saveButton);
    buttonLayout->addSpacing(20);
    buttonLayout->addWidget(quitButton);
    buttonLayout->addStretch();
    
    // 将系统参数初始化到界面

	// 获取所有控件
	QList<QSpinBox*> spinBoxes = sysParamsWidget->findChildren<QSpinBox*>();
    QList<QCheckBox*> checkBoxes = sysParamsWidget->findChildren<QCheckBox*>();
    QList<QComboBox*> comboBoxes = sysParamsWidget->findChildren<QComboBox*>();
    QList<QLineEdit*> lineEdits = sysParamsWidget->findChildren<QLineEdit*>();
    // 设置系统参数、相机参数到SpinBox控件
	for (QSpinBox* spinBox : spinBoxes) {
		QString name = spinBox->objectName();
		if (name == QStringLiteral("pulseCount"))
            spinBox->setValue(systemParams.pulseCount);
		else if (name == QStringLiteral("component1ToReject"))
            spinBox->setValue(systemParams.component1ToReject);
		else if (name == QStringLiteral("component2ToReject"))
            spinBox->setValue(systemParams.component2ToReject);
		else if (name == QStringLiteral("rejectStart"))
            spinBox->setValue(systemParams.rejectStart);
		else if (name == QStringLiteral("rejectEnd"))
			spinBox->setValue(systemParams.rejectEnd);
		else if (name == QStringLiteral("lightTriggerDelay1"))
			spinBox->setValue(cameraParams.lightTriggerDelay1);
		else if (name == QStringLiteral("lightExposureTime1"))
			spinBox->setValue(cameraParams.lightExposureTime1);
		else if (name == QStringLiteral("cameraTriggerStart1"))
			spinBox->setValue(cameraParams.cameraTriggerStart1);
		else if (name == QStringLiteral("lightTriggerDelay2"))
			spinBox->setValue(cameraParams.lightTriggerDelay2);
		else if (name == QStringLiteral("lightExposureTime2"))
			spinBox->setValue(cameraParams.lightExposureTime2);
		else if (name == QStringLiteral("cameraTriggerStart2"))
			spinBox->setValue(cameraParams.cameraTriggerStart2);
	}
    // 设置CheckBox控件
    for (QCheckBox* checkBox : checkBoxes) {
        QString name = checkBox->objectName();
		if (name == QStringLiteral("rejectEnabled"))
            checkBox->setChecked(systemParams.rejectEnabled);
            
		else if (name == QStringLiteral("positionDisplayEnabled"))
            checkBox->setChecked(systemParams.positionDisplayEnabled);
    }
    
    // 设置ComboBox控件
    for (QComboBox* comboBox : comboBoxes) {
        QString name = comboBox->objectName();
		if (name == QStringLiteral("comPort"))
            comboBox->setCurrentText(systemParams.comPort);
    }
  
    // 将相机参数初始化到界面
    // 设置相机序列号到LineEdit控件
    for (QLineEdit* lineEdit : lineEdits) {
        QString name = lineEdit->objectName();
		if (name == QStringLiteral("camera1SerialNum"))
            lineEdit->setText(cameraParams.camera1SerialNum);
		else if (name == QStringLiteral("camera2InnerSerialNum"))
			lineEdit->setText(cameraParams.camera2InnerSerialNum);
		else if (name == QStringLiteral("camera2OuterSerialNum"))
			lineEdit->setText(cameraParams.camera2OuterSerialNum);
    }

}

void CigVisionParams::initSystemParamsGroup()
{
    // 创建系统参数组
    systemParamsGroup = new QGroupBox(QStringLiteral("系统参数"));
    systemParamsGroup->setFixedWidth(300);
    QGridLayout* layout = new QGridLayout(systemParamsGroup);
    //layout->setSpacing(15);
    //layout->setContentsMargins(20, 20, 20, 20);
    
    QString labelStyle = 
        "QLabel {"
        "   font-size: 14px;"
        "   color: #333333;"
        "}";
        
	QString spinBoxStyle =
		"QSpinBox {"
		"   padding: 5px;"
		"   border: 1px solid #cccccc;"
		"   border-radius: 4px;"
		"   background-color: white;"
		"   font-size: 14px;"
		"   min-width: 80px;"
		"}"
		"QSpinBox::up-button,QSpinBox::down-button{width:50px}"
		"QSpinBox:hover {"
		"   border-color: #4CAF50;"
		"}"
		"QSpinBox:focus {"
		"   border-color: #4CAF50;"
		"   border-width: 2px;"
		"}";
    
        
    QString checkBoxStyle = 
        "QCheckBox {"
        "   font-size: 14px;"
        "   spacing: 8px;"
        "}"
        "QCheckBox::indicator {"
        "   width: 18px;"
        "   height: 18px;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "   border: 2px solid #cccccc;"
        "   border-radius: 4px;"
        "   background-color: white;"
        "}"
        "QCheckBox::indicator:checked {"
        "   border: 2px solid #4CAF50;"
        "   border-radius: 4px;"
        "   background-color: #4CAF50;"
        "   image: url(icons/use/check.png);"
        "}";
   
    QString comboBoxStyle =
		"QComboBox {"
		"   min-height: 30px;"
		"   max-height: 30px;"
		"   padding: 5px;"
		"   border: 1px solid #cccccc;"
		"   border-radius: 4px;"
		"   background-color: white;"
		"   font-size: 14px;"
		"}"
		"QComboBox::drop-down {"
		"   width: 20px;"
		"   border: none;"
		"}"
		"QComboBox::down-arrow {"
		"   width: 12px;"
		"   height: 12px;"
		"   margin-right: 5px;"
		"}"
		"QComboBox:hover {"
		"   border-color: #4CAF50;"
		"}"
		"QComboBox:focus {"
		"   border-color: #4CAF50;"
		"   border-width: 2px;"
		"}";
	;

    // 创建并设置参数输入控件
    auto createParamRow = [&](int row, const QString& labelText,const QString& editName, int min, int max) {
        QLabel* label = new QLabel(labelText);
        label->setStyleSheet(labelStyle);
        QSpinBox* spinBox = new QSpinBox();
        spinBox->setObjectName(editName);
        //spinBox->setObjectName("pulseCount");
        spinBox->setFixedSize(150,50);
        spinBox->setRange(min, max);
        spinBox->setStyleSheet(spinBoxStyle);
        layout->addWidget(label, row, 0);
        layout->addWidget(spinBox, row, 1);
        return spinBox;
    };

    // P1：移位链脉冲数
    createParamRow(0, QStringLiteral("移位链脉冲数："), QStringLiteral("pulseCount"), 0, 9);
	
    // P2：1组件到剔除口工位数
    createParamRow(1, QStringLiteral("1组件到剔除口工位数："), QStringLiteral("component1ToReject"), 10, 30);
    
    // P3：2组件到剔除口工位数
    createParamRow(2, QStringLiteral("2组件到剔除口工位数："), QStringLiteral("component2ToReject"), 20, 40);
    
    // P4：剔除开启(MCP)
    createParamRow(3, QStringLiteral("剔除开启(MCP)："), QStringLiteral("rejectStart"), 0, 5);
    
    // P5：剔除关闭(MCP)
    createParamRow(4, QStringLiteral("剔除关闭(MCP)："), QStringLiteral("rejectEnd"), 5, 9);
    
    // P6：剔除功能开关
    QLabel* rejectLabel = new QLabel(QStringLiteral("剔除功能："));
    rejectLabel->setStyleSheet(labelStyle);
    QCheckBox* rejectCheckBox = new QCheckBox(QStringLiteral("启用"));
    rejectCheckBox->setStyleSheet(checkBoxStyle);
    rejectCheckBox->setObjectName(QStringLiteral("rejectEnabled"));
    layout->addWidget(rejectLabel, 5, 0);
    layout->addWidget(rejectCheckBox, 5, 1);
    
    // P7：定位效果显示开关
    QLabel* positionLabel = new QLabel(QStringLiteral("定位效果显示："));
    positionLabel->setStyleSheet(labelStyle);
    QCheckBox* positionCheckBox = new QCheckBox(QStringLiteral("启用"));
    positionCheckBox->setObjectName(QStringLiteral("positionDisplayEnabled"));
    positionCheckBox->setStyleSheet(checkBoxStyle);
    layout->addWidget(positionLabel, 6, 0);
    layout->addWidget(positionCheckBox, 6, 1);
    
	// P8：COM口
	QLabel* comLabel = new QLabel(QStringLiteral("COM口设置："));
    QComboBox* comBox = new QComboBox();
    comBox->setObjectName(QStringLiteral("comPort"));
    comBox->setStyleSheet(comboBoxStyle);
    comBox->addItem(QStringLiteral("COM1"));
    comBox->addItem(QStringLiteral("COM2"));
    comBox->addItem(QStringLiteral("COM3"));
    
	layout->addWidget(comLabel, 7, 0);
	layout->addWidget(comBox, 7, 1);
    // 设置列拉伸因子
    //layout->setColumnStretch(0, 1);
    //layout->setColumnStretch(1, 1);
}

void CigVisionParams::initCameraParamsGroup()
{
    // 创建相机参数组
    cameraParamsGroup = new QGroupBox(QStringLiteral("相机参数"));
    QGridLayout* mainLayout = new QGridLayout(cameraParamsGroup);
    cameraParamsGroup->setFixedWidth(300);
    // 定义样式
    QString labelStyle = 
        "QLabel {"
        "   font-size: 14px;"
        "   color: #333333;"
        "}";
        
    QString lineEditStyle = 
        "QLineEdit {"
        "   padding: 5px;"
        "   border: 1px solid #cccccc;"
        "   border-radius: 4px;"
        "   background-color: white;"
        "   font-size: 14px;"
        "   min-height: 30px;"
        "   max-height: 30px;"
        "}"
        "QLineEdit:hover {"
        "   border-color: #4CAF50;"
        "}"
        "QLineEdit:focus {"
        "   border-color: #4CAF50;"
        "   border-width: 2px;"
        "}";
        
    QString spinBoxStyle = 
        "QSpinBox {"
        "   padding: 5px;"
        "   border: 1px solid #cccccc;"
        "   border-radius: 4px;"
        "   background-color: white;"
        "   font-size: 14px;"
        "   min-width: 80px;"
        "}"
        "QSpinBox::up-button,QSpinBox::down-button{width:50px}"
        "QSpinBox:hover {"
        "   border-color: #4CAF50;"
        "}"
        "QSpinBox:focus {"
        "   border-color: #4CAF50;"
        "   border-width: 2px;"
        "}";

    // 一组件参数标题
    QLabel* component1Title = new QLabel(QStringLiteral("一组件参数"));
    component1Title->setStyleSheet("QLabel { font-size: 16px; font-weight: bold; }");
    mainLayout->addWidget(component1Title, 0, 0, 1, 2);

    // 一组件参数控件
    auto addComponent1Row = [&](int row, const QString& labelText, QWidget* widget) {
        QLabel* label = new QLabel(labelText);
        label->setStyleSheet(labelStyle);
        mainLayout->addWidget(label, row, 0);
        mainLayout->addWidget(widget, row, 1);
        widget->setFixedWidth(150);
    };

    QLineEdit* camera1SerialEdit = new QLineEdit();
    camera1SerialEdit->setObjectName("camera1SerialNum");
    camera1SerialEdit->setStyleSheet(lineEditStyle);

    QSpinBox* lightDelay1SpinBox = new QSpinBox();
    lightDelay1SpinBox->setObjectName("lightTriggerDelay1");
    lightDelay1SpinBox->setRange(0, 20);
    lightDelay1SpinBox->setStyleSheet(spinBoxStyle);
    lightDelay1SpinBox->setFixedSize(150, 50);

    QSpinBox* lightExposure1SpinBox = new QSpinBox();
    lightExposure1SpinBox->setObjectName("lightExposureTime1");
    lightExposure1SpinBox->setRange(0, 200);
    lightExposure1SpinBox->setStyleSheet(spinBoxStyle);
    lightExposure1SpinBox->setFixedSize(150, 50);

    QSpinBox* cameraTrigger1SpinBox = new QSpinBox();
    cameraTrigger1SpinBox->setObjectName("cameraTriggerStart1");
    cameraTrigger1SpinBox->setRange(1, 9);
    cameraTrigger1SpinBox->setStyleSheet(spinBoxStyle);
    cameraTrigger1SpinBox->setFixedSize(150, 50);

    addComponent1Row(1, QStringLiteral("相机序列号:"), camera1SerialEdit);
    addComponent1Row(2, QStringLiteral("光源触发延时(us):"), lightDelay1SpinBox);
    addComponent1Row(3, QStringLiteral("光源曝光时间(us):"), lightExposure1SpinBox);
    addComponent1Row(4, QStringLiteral("相机触发开始(MCP):"), cameraTrigger1SpinBox);

    // 添加分隔空间
    QSpacerItem* spacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed);
    mainLayout->addItem(spacer, 5, 0, 1, 2);

    // 二组件参数标题
    QLabel* component2Title = new QLabel(QStringLiteral("二组件参数"));
    component2Title->setStyleSheet("QLabel { font-size: 16px; font-weight: bold; }");
    mainLayout->addWidget(component2Title, 6, 0, 1, 2);


    // 二组件参数控件
    QLineEdit* camera2InnerSerialEdit = new QLineEdit();
    camera2InnerSerialEdit->setObjectName("camera2InnerSerialNum");
    camera2InnerSerialEdit->setStyleSheet(lineEditStyle);

    QLineEdit* camera2OuterSerialEdit = new QLineEdit();
    camera2OuterSerialEdit->setObjectName("camera2OuterSerialNum");
    camera2OuterSerialEdit->setStyleSheet(lineEditStyle);

    QSpinBox* lightDelay2SpinBox = new QSpinBox();
    lightDelay2SpinBox->setObjectName("lightTriggerDelay2");
    lightDelay2SpinBox->setRange(0, 20);
    lightDelay2SpinBox->setStyleSheet(spinBoxStyle);
    lightDelay2SpinBox->setFixedSize(150, 50);

    QSpinBox* lightExposure2SpinBox = new QSpinBox();
    lightExposure2SpinBox->setObjectName("lightExposureTime2");
    lightExposure2SpinBox->setRange(0, 200);
    lightExposure2SpinBox->setStyleSheet(spinBoxStyle);
    lightExposure2SpinBox->setFixedSize(150, 50);

    QSpinBox* cameraTrigger2SpinBox = new QSpinBox();
    cameraTrigger2SpinBox->setObjectName("cameraTriggerStart2");
    cameraTrigger2SpinBox->setRange(1, 9);
    cameraTrigger2SpinBox->setStyleSheet(spinBoxStyle);
    cameraTrigger2SpinBox->setFixedSize(150, 50);

	addComponent1Row(7, QStringLiteral("相机1(内)序列号:"), camera2InnerSerialEdit);
	addComponent1Row(8, QStringLiteral("相机2(外)序列号:"), camera2OuterSerialEdit);
	addComponent1Row(9, QStringLiteral("光源触发延时(us):"), lightDelay2SpinBox);
    addComponent1Row(10, QStringLiteral("光源曝光时间(us):"), lightExposure2SpinBox);
    addComponent1Row(11, QStringLiteral("相机触发开始(MCP)::"), cameraTrigger2SpinBox);
    

    //mainLayout->addWidget(new QLabel(QStringLiteral("相机1(内)序列号:")), 7, 0);
    //mainLayout->addWidget(camera2InnerSerialEdit, 7, 1);
    //mainLayout->addWidget(new QLabel(QStringLiteral("相机2(外)序列号:")), 8, 0);
    //mainLayout->addWidget(camera2OuterSerialEdit, 8, 1);
    //mainLayout->addWidget(new QLabel(QStringLiteral("光源触发延时(us):")), 9, 0);
    //mainLayout->addWidget(lightDelay2SpinBox, 9, 1);
    //mainLayout->addWidget(new QLabel(QStringLiteral("光源曝光时间(us):")), 10, 0);
    //mainLayout->addWidget(lightExposure2SpinBox, 10, 1);
    //mainLayout->addWidget(new QLabel(QStringLiteral("相机触发开始(MCP):")), 11, 0);
    //mainLayout->addWidget(cameraTrigger2SpinBox, 11, 1);

    // 连接信号和槽
    connect(camera1SerialEdit, &QLineEdit::textChanged, [this](const QString& text) {
        cameraParams.camera1SerialNum = text;
    });

    connect(lightDelay1SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        cameraParams.lightTriggerDelay1 = value;
    });

    connect(lightExposure1SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        cameraParams.lightExposureTime1 = value;
    });

    connect(cameraTrigger1SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        cameraParams.cameraTriggerStart1 = value;
    });

    connect(camera2InnerSerialEdit, &QLineEdit::textChanged, [this](const QString& text) {
        cameraParams.camera2InnerSerialNum = text;
    });

    connect(camera2OuterSerialEdit, &QLineEdit::textChanged, [this](const QString& text) {
        cameraParams.camera2OuterSerialNum = text;
    });

    connect(lightDelay2SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        cameraParams.lightTriggerDelay2 = value;
    });

    connect(lightExposure2SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        cameraParams.lightExposureTime2 = value;
    });

    connect(cameraTrigger2SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        cameraParams.cameraTriggerStart2 = value;
    });

    // 设置初始值
    camera1SerialEdit->setText(cameraParams.camera1SerialNum);
    lightDelay1SpinBox->setValue(cameraParams.lightTriggerDelay1);
    lightExposure1SpinBox->setValue(cameraParams.lightExposureTime1);
    cameraTrigger1SpinBox->setValue(cameraParams.cameraTriggerStart1);

    camera2InnerSerialEdit->setText(cameraParams.camera2InnerSerialNum);
    camera2OuterSerialEdit->setText(cameraParams.camera2OuterSerialNum);
    lightDelay2SpinBox->setValue(cameraParams.lightTriggerDelay2);
    lightExposure2SpinBox->setValue(cameraParams.lightExposureTime2);
    cameraTrigger2SpinBox->setValue(cameraParams.cameraTriggerStart2);
}

// 系统参数setter
void CigVisionParams::setSystemParams(const SystemParams& params)
{
    systemParams = params;
    saveSystemParams();
}

// 相机参数setter
void CigVisionParams::setCameraParams(const CameraParams& params) 
{
    cameraParams = params;
    saveSystemParams();
}

// 保存系统参数到config.ini
bool CigVisionParams::saveSystemParams()
{
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    // 保存系统参数
    settings.beginGroup("SystemParams");
    settings.setValue("pulseCount", systemParams.pulseCount);
    settings.setValue("component1ToReject", systemParams.component1ToReject);
    settings.setValue("component2ToReject", systemParams.component2ToReject);
    settings.setValue("rejectStart", systemParams.rejectStart);
    settings.setValue("rejectEnd", systemParams.rejectEnd);
    settings.setValue("rejectEnabled", systemParams.rejectEnabled);
    settings.setValue("positionDisplayEnabled", systemParams.positionDisplayEnabled);
    settings.setValue("comPort", systemParams.comPort);
    settings.endGroup();

    // 保存相机参数
    settings.beginGroup("CameraParams");
    // 一组件参数
    settings.setValue("camera1SerialNum", cameraParams.camera1SerialNum);
    settings.setValue("lightTriggerDelay1", cameraParams.lightTriggerDelay1);
    settings.setValue("lightExposureTime1", cameraParams.lightExposureTime1);
    settings.setValue("cameraTriggerStart1", cameraParams.cameraTriggerStart1);
    // 二组件参数
    settings.setValue("camera2InnerSerialNum", cameraParams.camera2InnerSerialNum);
    settings.setValue("camera2OuterSerialNum", cameraParams.camera2OuterSerialNum);
    settings.setValue("lightTriggerDelay2", cameraParams.lightTriggerDelay2);
    settings.setValue("lightExposureTime2", cameraParams.lightExposureTime2);
    settings.setValue("cameraTriggerStart2", cameraParams.cameraTriggerStart2);
    settings.endGroup();

    return settings.status() == QSettings::NoError;
}

// 从config.ini加载系统参数
bool CigVisionParams::loadSystemParams()
{
    if (!QFile::exists(configPath)) {
        qDebug() << "Config file does not exist:" << configPath;
        return false;
    }

    QSettings settings(configPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    // 加载系统参数
    settings.beginGroup("SystemParams");
    systemParams.pulseCount = settings.value("pulseCount", 0).toInt();
    systemParams.component1ToReject = settings.value("component1ToReject", 10).toInt();
    systemParams.component2ToReject = settings.value("component2ToReject", 20).toInt();
    systemParams.rejectStart = settings.value("rejectStart", 0).toInt();
    systemParams.rejectEnd = settings.value("rejectEnd", 5).toInt();
    systemParams.rejectEnabled = settings.value("rejectEnabled", false).toBool();
    systemParams.positionDisplayEnabled = settings.value("positionDisplayEnabled", false).toBool();
    systemParams.comPort = settings.value("comPort", "COM1").toString();
    settings.endGroup();

    // 加载相机参数
    settings.beginGroup("CameraParams");
    // 一组件参数
    cameraParams.camera1SerialNum = settings.value("camera1SerialNum", "").toString();
    cameraParams.lightTriggerDelay1 = settings.value("lightTriggerDelay1", 0).toInt();
    cameraParams.lightExposureTime1 = settings.value("lightExposureTime1", 0).toInt();
    cameraParams.cameraTriggerStart1 = settings.value("cameraTriggerStart1", 1).toInt();
    // 二组件参数
    cameraParams.camera2InnerSerialNum = settings.value("camera2InnerSerialNum", "").toString();
    cameraParams.camera2OuterSerialNum = settings.value("camera2OuterSerialNum", "").toString();
    cameraParams.lightTriggerDelay2 = settings.value("lightTriggerDelay2", 0).toInt();
    cameraParams.lightExposureTime2 = settings.value("lightExposureTime2", 0).toInt();
    cameraParams.cameraTriggerStart2 = settings.value("cameraTriggerStart2", 1).toInt();
    settings.endGroup();
	// 从已存在的配置文件加载当前品牌名称
	
	currentBrand = settings.value("General/CurrentBrand").toString();
	qDebug() << "Loaded current brand from config:" << currentBrand;
	// 检查品牌参数文件夹是否存在
	currentBrandPath = brandsBasePath + "/" + currentBrand;
	if (!QDir(currentBrandPath).exists()) {
		qDebug() << "Brand directory does not exist:" << currentBrandPath;
		return false;
	}
	currentBrandModelPicturesPath = currentBrandPath + "/" + QStringLiteral("模版图片");
	if (!QDir(currentBrandModelPicturesPath).exists()) {
		qDebug() << "Brand pictures directory does not exist:" << currentBrandModelPicturesPath;
		return false;
	}
    return settings.status() == QSettings::NoError;
}

// 添加槽函数实现
void CigVisionParams::onModelPicButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (button)
    {
        // 获取被点击按钮的编号
        QString objectName = button->objectName();
        int buttonNumber = objectName.mid(8).toInt(); // 从"modelPic1"中提取数字
        QString modelPic = currentBrandModelPicturesPath+"/"+QStringLiteral("%1.jpg").arg(buttonNumber);
        single_step(current_process_step,modelPic);//处理与显示
        // 在这里处理按钮点击事件
        //qDebug() << QStringLiteral("模板图片按钮 %1 被点击").arg(buttonNumber);
        //view->loadJpgImage(modelPic);
        // TODO: 添加您的具体处理逻辑
        // 例如：加载对应的模板图片、更新显示等
        
       //dllpro();
    }
}

bool CigVisionParams::single_step(process_step step, QString modelPic)
{
    // 检查文件是否存在
    if (!QFile::exists(modelPic)) {
        qDebug() << "图像文件不存在: " << modelPic;
        return false;
    }
    
    // 先尝试使用QImage加载图片并显示
    if (view->loadJpgImage(modelPic)) {
        qDebug() << "成功使用Qt加载图像: " << modelPic;
        
        // 清除之前的所有图形项
        view->clearItems();
        
        // 加载DLL
        HMODULE hlibrary = ::LoadLibrary(L"process.dll");
        if (hlibrary == NULL) {
            qDebug() << "无法加载process.dll";
            return false;
        }
        
        FindLeftPoint FindLeftPoint_dllpro = (FindLeftPoint)::GetProcAddress(hlibrary, "FindLeftPoint");
        if (FindLeftPoint_dllpro == NULL) {
            qDebug() << "无法找到FindLeftPoint函数";
            ::FreeLibrary(hlibrary);
            return false;
        }
        FindSidePoint FindSidePoint_dllpro=(FindSidePoint)::GetProcAddress(hlibrary, "FindSidePoint");
        if (FindSidePoint_dllpro == NULL) {
            qDebug() << "无法找到FindSidePoint函数";
            ::FreeLibrary(hlibrary);
            return false;
        }
        // 根据不同的处理步骤调用不同的函数
        switch(step) {
           
            case process_step::Loc_Top:
            {
                int up_left_pointX = 0, up_left_pointY = 0, down_left_pointX = 0, down_left_pointY = 0;
                local_top(modelPic, FindLeftPoint_dllpro, up_left_pointX, up_left_pointY, down_left_pointX, down_left_pointY);
                break;
            }
            case process_step::Loc_Side:
            {
                int in_top_points_array[2][3] = { 0 };
                local_top(modelPic, FindLeftPoint_dllpro, in_top_points_array[0][0], in_top_points_array[0][1], in_top_points_array[1][0], in_top_points_array[1][1]);
                //需要得到的坐标
                int in_model_side_array[2][3][3] = { 0 };
                int out_model_side_array[2][3][3] = { 0 };
                int out_side_array[2][3] = { 0 };
                bool set_new_model = false;
                local_side(modelPic, FindSidePoint_dllpro, in_top_points_array, in_model_side_array,
                    set_new_model, out_side_array, out_model_side_array);
                break;
            }
            case process_step::Loc_stick:
            {
                int in_top_points_array[2][3] = { 0 };
                local_top(modelPic, FindLeftPoint_dllpro, in_top_points_array[0][0], in_top_points_array[0][1], in_top_points_array[1][0], in_top_points_array[1][1]);
                //需要得到的坐标
                int in_model_side_array[2][3][3] = { 0 };
                int out_model_side_array[2][3][3] = { 0 };
                int out_side_array[2][3] = { 0 };
                bool set_new_model = false;
                local_side(modelPic, FindSidePoint_dllpro, in_top_points_array, in_model_side_array,
                    set_new_model, out_side_array, out_model_side_array);

                local_stick(in_top_points_array, out_side_array);
                break;
            }
            case process_step::Loc_filter:
            {
                int in_top_points_array[2][3] = { 0 };
                local_top(modelPic, FindLeftPoint_dllpro, in_top_points_array[0][0], in_top_points_array[0][1], in_top_points_array[1][0], in_top_points_array[1][1]);
                //需要得到的坐标
                int in_model_side_array[2][3][3] = { 0 };
                int out_model_side_array[2][3][3] = { 0 };
                int out_side_array[2][3] = { 0 };
                bool set_new_model = false;
                local_side(modelPic, FindSidePoint_dllpro, in_top_points_array, in_model_side_array,
                    set_new_model, out_side_array, out_model_side_array);

                local_filter(in_top_points_array, out_side_array);
                break;
            }
            case process_step::Loc_joint:
            {
                int in_top_points_array[2][3] = { 0 };
                local_top(modelPic, FindLeftPoint_dllpro, in_top_points_array[0][0], in_top_points_array[0][1], in_top_points_array[1][0], in_top_points_array[1][1]);
                //需要得到的坐标
                int in_model_side_array[2][3][3] = { 0 };
                int out_model_side_array[2][3][3] = { 0 };
                int out_side_array[2][3] = { 0 };
                bool set_new_model = false;
                local_side(modelPic, FindSidePoint_dllpro, in_top_points_array, in_model_side_array,
                    set_new_model, out_side_array, out_model_side_array);

                local_joint(in_top_points_array, out_side_array);
                break;
            }
            case process_step::Stick_dark:
            {
                int in_top_points_array[2][3] = { 0 };
                local_top(modelPic, FindLeftPoint_dllpro, in_top_points_array[0][0], in_top_points_array[0][1], in_top_points_array[1][0], in_top_points_array[1][1]);
                //需要得到的坐标
                int in_model_side_array[2][3][3] = { 0 };
                int out_model_side_array[2][3][3] = { 0 };
                int out_side_array[2][3] = { 0 };
                bool set_new_model = false;
                local_side(modelPic, FindSidePoint_dllpro, in_top_points_array, in_model_side_array,
                    set_new_model, out_side_array, out_model_side_array);
                //in_top_points_array[0][0]:up_leftX;in_top_points_array[0][1]:up_leftY ; 
                //in_top_points_array[1][0]:down_leftX;in_top_points_array[1][1]:down_leftY
                

            }
            // 其他处理步骤...
            default:
                qDebug() << "未实现的处理步骤: " << static_cast<int>(step);
                break;
        }
        
        ::FreeLibrary(hlibrary);
        
        // 强制更新视图
        view->update();
        
        return true;
    }
    
    qDebug() << "无法加载图像: " << modelPic;
    return false;
}
ErrorCode CigVisionParams::local_top(QString modelPic, FindLeftPoint dllpro,int &up_left_pointX,int &up_left_pointY,int &down_left_pointX,int &down_left_pointY) {
                // 如果参数为0，使用默认值
                int upCigTopPositionParams_centerX = upCigTopPositionParams.centerX > 0 ? upCigTopPositionParams.centerX : 400;
                int upCigTopPositionParams_centerY = upCigTopPositionParams.centerY > 0 ? upCigTopPositionParams.centerY : 300;
                int upCigTopPositionParams_width = upCigTopPositionParams.width > 0 ? upCigTopPositionParams.width : 200;
                int upCigTopPositionParams_height = upCigTopPositionParams.height > 0 ? upCigTopPositionParams.height : 100;
                int downCigTopPositionParams_centerX = downCigTopPositionParams.centerX > 0 ? downCigTopPositionParams.centerX : 400;
                int downCigTopPositionParams_centerY = downCigTopPositionParams.centerY > 0 ? downCigTopPositionParams.centerY : 300;
                int downCigTopPositionParams_width = downCigTopPositionParams.width > 0 ? downCigTopPositionParams.width : 200;
                int downCigTopPositionParams_height = downCigTopPositionParams.height > 0 ? downCigTopPositionParams.height : 100;
                

                // 绘制ROI区域 - 使用固定值测试
    QRectF upCigTopPositionParams_roi(upCigTopPositionParams_centerX - upCigTopPositionParams_width / 2,
        upCigTopPositionParams_centerY - upCigTopPositionParams_height / 2,
        upCigTopPositionParams_width, upCigTopPositionParams_height);
    QRectF downCigTopPositionParams_roi(downCigTopPositionParams_centerX - downCigTopPositionParams_width / 2,
        downCigTopPositionParams_centerY - downCigTopPositionParams_height / 2,
        downCigTopPositionParams_width, downCigTopPositionParams_height);
                //qDebug() << "绘制矩形: " << roi;
                // 画出矩形
    view->drawFixedRectangle(upCigTopPositionParams_roi, QPen(Qt::blue, 2));
    view->drawFixedRectangle(downCigTopPositionParams_roi, QPen(Qt::cyan, 2));
                
                //qDebug() << "已绘制测试图形";
                
                // 加载Halcon图像
                HObject ho_image;
                try {
                    // 将QString转换为std::string，再转换为const char*
                    std::string modelPicStr = modelPic.toStdString();
                    const char* modelPicChar = modelPicStr.c_str();
                    
                    // 读取图像到HObject
                    ReadImage(&ho_image, modelPicChar);
                    
                    // 上烟端定位
     
        ErrorCode up_result = dllpro(ho_image, upCigTopPositionParams_centerX, upCigTopPositionParams_centerY, upCigTopPositionParams_width, upCigTopPositionParams_height, upCigTopPositionParams.sigmaCode,
            upCigTopPositionParams.edgeGradientStartThreshold, upCigTopPositionParams.stepGradientThreshold, up_left_pointX, up_left_pointY);
        ErrorCode down_result = dllpro(ho_image, downCigTopPositionParams_centerX, downCigTopPositionParams_centerY, downCigTopPositionParams_width, downCigTopPositionParams_height, downCigTopPositionParams.sigmaCode,
            downCigTopPositionParams.edgeGradientStartThreshold, downCigTopPositionParams.stepGradientThreshold, down_left_pointX, down_left_pointY);
                    if (up_result == ErrorCode::Success && down_result == ErrorCode::Success) {
                        qDebug() << "定位成功，结果点: (" << up_left_pointX << ", " << up_left_pointY << ")";
                        // 在视图上绘制结果
                        view->drawPoint(QPointF(up_left_pointX, up_left_pointY), QPen(Qt::red, 3));
                        view->drawPoint(QPointF(down_left_pointX, down_left_pointY), QPen(Qt::red, 3));
            return ErrorCode::Success;
        }
        else {
                        qDebug() << "定位失败，错误码: " << static_cast<int>(up_result) << " " << static_cast<int>(down_result);
                    }
                }
                catch (HException& exception) {
                    // 处理Halcon异常
                    qDebug() << "Halcon异常: " << exception.ErrorMessage().Text();
                }
                catch (std::exception& e) {
                    // 处理标准异常
                    qDebug() << "标准异常: " << e.what();
                }
                catch (...) {
                    // 处理未知异常
                    qDebug() << "未知异常";
                }
    return ErrorCode::Failed;
}

ErrorCode CigVisionParams::local_side(QString modelPic, FindSidePoint FindSidePoint_dllpro,int in_top_points_array[2][3],int in_model_side_array[2][3][3],
    bool set_new_model, int(& out_side_array)[2][3], int(& out_model_side_array)[2][3][3])
{
    // 如果参数为0，使用默认值
    int upBoxPositionParams_frameStartDistance = upBoxPositionParams.frameStartDistance > 0 ? upBoxPositionParams.frameStartDistance : 50;
    int upBoxPositionParams_framesHorizontalDistance = upBoxPositionParams.framesHorizontalDistance > 0 ? upBoxPositionParams.framesHorizontalDistance : 100;
    int upBoxPositionParams_frameWidth = upBoxPositionParams.frameWidth > 0 ? upBoxPositionParams.frameWidth : 50;
    int upBoxPositionParams_frameHeight = upBoxPositionParams.frameHeight > 0 ? upBoxPositionParams.frameHeight : 200;
    int upBoxPositionParams_frameSigmaCode = upBoxPositionParams.frameSigmaCode > 0 ? upBoxPositionParams.frameSigmaCode : 1;
    int upBoxPositionParams_frameEdgeGradientThreshold = upBoxPositionParams.frameEdgeGradientThreshold > 0 ? upBoxPositionParams.frameEdgeGradientThreshold : 20;

    int downBoxPositionParams_frameStartDistance = downBoxPositionParams.frameStartDistance > 0 ? downBoxPositionParams.frameStartDistance : 50;
    int downBoxPositionParams_framesHorizontalDistance = downBoxPositionParams.framesHorizontalDistance > 0 ? downBoxPositionParams.framesHorizontalDistance : 100;
    int downBoxPositionParams_frameWidth = downBoxPositionParams.frameWidth > 0 ? downBoxPositionParams.frameWidth : 50;
    int downBoxPositionParams_frameHeight = downBoxPositionParams.frameHeight > 0 ? downBoxPositionParams.frameHeight : 200;
    int downBoxPositionParams_frameSigmaCode = downBoxPositionParams.frameSigmaCode > 0 ? downBoxPositionParams.frameSigmaCode : 1;
    int downBoxPositionParams_frameEdgeGradientThreshold = downBoxPositionParams.frameEdgeGradientThreshold > 0 ? downBoxPositionParams.frameEdgeGradientThreshold : 20;

    // 计算上烟边定位框的位置
    int upBoxPositionParams_startX = in_top_points_array[0][0] + upBoxPositionParams_frameStartDistance;
    int upBoxPositionParams_startY = in_top_points_array[0][1] - upBoxPositionParams_frameHeight / 2;

    // 计算下烟边定位框的位置
    int downBoxPositionParams_startX = in_top_points_array[1][0] + downBoxPositionParams_frameStartDistance;
    int downBoxPositionParams_startY = in_top_points_array[1][1] - downBoxPositionParams_frameHeight / 2;

    //int side_pointY1, side_pointY2, side_width = 0;
    // 绘制上烟边定位框
    for (int i = 0; i < 3; i++) {
        QRectF upBoxRect(upBoxPositionParams_startX + i * upBoxPositionParams_framesHorizontalDistance,
            upBoxPositionParams_startY,
            upBoxPositionParams_frameWidth,
            upBoxPositionParams_frameHeight);

        view->drawFixedRectangle(upBoxRect, QPen(Qt::blue, 2));
    }

    // 绘制下烟边定位框
    for (int i = 0; i < 3; i++) {
        QRectF downBoxRect(downBoxPositionParams_startX + i * downBoxPositionParams_framesHorizontalDistance,
            downBoxPositionParams_startY,
            downBoxPositionParams_frameWidth,
            downBoxPositionParams_frameHeight);
        view->drawFixedRectangle(downBoxRect, QPen(Qt::cyan, 2));
    }

    // 加载Halcon图像
    HObject ho_image;
    try {
        // 将QString转换为std::string，再转换为const char*
        std::string modelPicStr = modelPic.toStdString();
        const char* modelPicChar = modelPicStr.c_str();

        // 读取图像到HObject
        ReadImage(&ho_image, modelPicChar);

        // 调用DLL函数进行边缘检测
        ErrorCode up_result = FindSidePoint_dllpro(ho_image, in_top_points_array[0][0], in_top_points_array[0][1], upBoxPositionParams_frameStartDistance, upBoxPositionParams_framesHorizontalDistance,
            upBoxPositionParams_frameHeight, upBoxPositionParams_frameWidth, upBoxPositionParams_frameSigmaCode,
            upBoxPositionParams_frameEdgeGradientThreshold, in_model_side_array[0], set_new_model, out_side_array[0][0], out_side_array[0][1], out_side_array[0][2], out_model_side_array[0]);
        // 对每个框执行边缘检测
        for (int i = 0; i < 3; i++) {
            view->drawPoint(QPoint(upBoxPositionParams_startX + i * upBoxPositionParams_framesHorizontalDistance + upBoxPositionParams_frameWidth / 2, out_side_array[0][0]), QPen(Qt::red, 2));
            view->drawPoint(QPoint(upBoxPositionParams_startX + i * upBoxPositionParams_framesHorizontalDistance + upBoxPositionParams_frameWidth / 2, out_side_array[0][1]), QPen(Qt::red, 2));
        }

        // 调用DLL函数进行边缘检测
        ErrorCode down_result = FindSidePoint_dllpro(ho_image, in_top_points_array[1][0], in_top_points_array[1][1], downBoxPositionParams_frameStartDistance, downBoxPositionParams_framesHorizontalDistance,
            downBoxPositionParams_frameHeight, downBoxPositionParams_frameWidth, downBoxPositionParams_frameSigmaCode,
            downBoxPositionParams_frameEdgeGradientThreshold, in_model_side_array[1], set_new_model, out_side_array[1][0], out_side_array[1][1], out_side_array[1][2], out_model_side_array[1]);
        // 对每个框执行边缘检测
        for (int i = 0; i < 3; i++) {
            view->drawPoint(QPoint(downBoxPositionParams_startX + i * downBoxPositionParams_framesHorizontalDistance + downBoxPositionParams_frameWidth / 2, out_side_array[1][0]), QPen(Qt::red, 2));
            view->drawPoint(QPoint(downBoxPositionParams_startX + i * downBoxPositionParams_framesHorizontalDistance + downBoxPositionParams_frameWidth / 2, out_side_array[1][1]), QPen(Qt::red, 2));
        }
        return ErrorCode::Success;
    }
    catch (HException& exception) {
        // 处理Halcon异常
        qDebug() << "Halcon异常: " << exception.ErrorMessage().Text();
    }
    catch (std::exception& e) {
        // 处理标准异常
        qDebug() << "标准异常: " << e.what();
    }
    catch (...) {
        // 处理未知异常
        qDebug() << "未知异常";
    }
    return ErrorCode::Failed;
}

ErrorCode CigVisionParams::local_stick(int in_top_points_array[2][3], int in_side_array[2][3])
{
    int upCigBodyPositionParams_bodyLength = upCigBodyPositionParams.bodyLength > 0 ? upCigBodyPositionParams.bodyLength : 400;
    int upCigBodyPositionParams_bodyStartDistance = upCigBodyPositionParams.bodyStartDistance > 0 ? upCigBodyPositionParams.bodyStartDistance : 5;
    int upCigBodyPositionParams_innerEdgeThreshold = upCigBodyPositionParams.innerEdgeThreshold > 0 ? upCigBodyPositionParams.innerEdgeThreshold : 10;
    int downCigBodyPositionParams_bodyLength = downCigBodyPositionParams.bodyLength > 0 ? downCigBodyPositionParams.bodyLength : 400;
    int downCigBodyPositionParams_bodyStartDistance = downCigBodyPositionParams.bodyStartDistance > 0 ? downCigBodyPositionParams.bodyStartDistance : 5;
    int downCigBodyPositionParams_innerEdgeThreshold = downCigBodyPositionParams.innerEdgeThreshold > 0 ? downCigBodyPositionParams.innerEdgeThreshold : 10;
    
    // 绘制ROI区域 - 使用固定值测试
    QRectF upCigTopPositionParams_roi(in_top_points_array[0][0] + upCigBodyPositionParams_bodyStartDistance,
        in_side_array[0][1] + upCigBodyPositionParams_innerEdgeThreshold,
        upCigBodyPositionParams_bodyLength, in_side_array[0][2] - 2*upCigBodyPositionParams_innerEdgeThreshold);
    QRectF downCigTopPositionParams_roi(in_top_points_array[1][0] + downCigBodyPositionParams_bodyStartDistance,
        in_side_array[1][1] + downCigBodyPositionParams_innerEdgeThreshold,
        downCigBodyPositionParams_bodyLength, in_side_array[1][2] - 2*downCigBodyPositionParams_innerEdgeThreshold);
    
    //qDebug() << "绘制矩形: " << roi;
    // 画出矩形
    view->drawFixedRectangle(upCigTopPositionParams_roi, QPen(Qt::green, 2));
    view->drawFixedRectangle(downCigTopPositionParams_roi, QPen(Qt::green, 2));

    // DownCigBodyPositionParams downCigBodyPositionParams;
    return ErrorCode::Success;
}

ErrorCode CigVisionParams::local_filter(int in_top_points_array[2][3], int in_side_array[2][3])
{
    int upCigFilterPositionParams_bodyLength = upFilterPositionParams.filterLength > 0 ? upFilterPositionParams.filterLength : 100;
    int upCigFilterPositionParams_bodyStartDistance = upFilterPositionParams.filterStartDistance > 0 ? upFilterPositionParams.filterStartDistance : 500;
    int upCigFilterPositionParams_innerEdgeThreshold = upFilterPositionParams.innerEdgeThreshold > 0 ? upFilterPositionParams.innerEdgeThreshold : 15;
    int downCigFilterPositionParams_bodyLength = downFilterPositionParams.filterLength > 0 ? downFilterPositionParams.filterLength : 100;
    int downCigFilterPositionParams_bodyStartDistance = downFilterPositionParams.filterStartDistance > 0 ? downFilterPositionParams.filterStartDistance : 500;
    int downCigFilterPositionParams_innerEdgeThreshold = downFilterPositionParams.innerEdgeThreshold > 0 ? downFilterPositionParams.innerEdgeThreshold : 15;

    // 绘制ROI区域 - 使用固定值测试
    QRectF upCigFilterPositionParams_roi(in_top_points_array[0][0] + upCigFilterPositionParams_bodyStartDistance,
        in_side_array[0][1] + upCigFilterPositionParams_innerEdgeThreshold,
        upCigFilterPositionParams_bodyLength, in_side_array[0][2] - 2 * upCigFilterPositionParams_innerEdgeThreshold);
    QRectF downCigFilterPositionParams_roi(in_top_points_array[1][0] + downCigFilterPositionParams_bodyStartDistance,
        in_side_array[1][1] + downCigFilterPositionParams_innerEdgeThreshold,
        downCigFilterPositionParams_bodyLength, in_side_array[1][2] - 2 * downCigFilterPositionParams_innerEdgeThreshold);

    //qDebug() << "绘制矩形: " << roi;
    // 画出矩形
    view->drawFixedRectangle(upCigFilterPositionParams_roi, QPen(Qt::green, 2));
    view->drawFixedRectangle(downCigFilterPositionParams_roi, QPen(Qt::green, 2));

    // DownCigBodyPositionParams downCigBodyPositionParams;
    return ErrorCode::Success;
}
ErrorCode CigVisionParams::local_joint(int in_top_points_array[2][3], int in_side_array[2][3])
{
    int upJointPositionParams_jointLength = upJointPositionParams.jointLength > 0 ? upJointPositionParams.jointLength : 100;
    int upJointPositionParams_jointStartDistance = upJointPositionParams.jointStartDistance > 0 ? upJointPositionParams.jointStartDistance : 500;
    int downJointPositionParams_jointLength = downJointPositionParams.jointLength > 0 ? downJointPositionParams.jointLength : 100;
    int downJointPositionParams_jointStartDistance = downJointPositionParams.jointStartDistance > 0 ? downJointPositionParams.jointStartDistance : 500;

    // 绘制ROI区域 - 使用固定值测试
    QRectF upJointPositionParams_roi(in_top_points_array[0][0] + upJointPositionParams_jointStartDistance,in_side_array[0][1] ,
        upJointPositionParams_jointLength, in_side_array[0][2]);
    QRectF downJointPositionParams_roi(in_top_points_array[1][0] + downJointPositionParams_jointStartDistance,in_side_array[1][1] ,
        downJointPositionParams_jointLength, in_side_array[1][2]);

    //qDebug() << "绘制矩形: " << roi;
    // 画出矩形
    view->drawFixedRectangle(upJointPositionParams_roi, QPen(Qt::green, 2));
    view->drawFixedRectangle(downJointPositionParams_roi, QPen(Qt::green, 2));

    return ErrorCode::Success;
}
