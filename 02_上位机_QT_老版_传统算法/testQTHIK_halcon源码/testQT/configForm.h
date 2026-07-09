#pragma once

#include <QWidget>
#include "ui_configForm.h"
#include <Qsettings.h>

class configForm : public QWidget
{
	Q_OBJECT

public:
	configForm(QWidget *parent = Q_NULLPTR);
	~configForm();

signals:
	void updateConfig();
private slots:
	void on_btnSave_clicked();
private:
	Ui::configForm ui;
	
	QSettings* configIniWrite;

	QString pic1RectX, pic1RectY, pic1RectHeight, pic1RectWidth;//截取矩形参数

	QString filterWidthRectX, filterWidthRectY, filterWidthRectHeight, filterWidthRectWidth, filterWidthRectThreshold;//滤棒端宽度分界区域
	QString cigNearFilterWidthRectX, cigNearFilterWidthRectY, cigNearFilterWidthRectHeight, cigNearFilterWidthRectWidth;//烟支近滤棒端宽度分界区域
	QString cigTopWidthRectX, cigTopWidthRectY, cigTopWidthRectHeight, cigTopWidthRectWidth;//烟支端部宽度分界区域

	QString filterDividingRectX, filterDividingRectY, filterDividingRectHeight, filterDividingRectWidth, filterDividingRectThreshold;//滤嘴端分界区域
	int filterVerticalRelative;
	
	QString pic1DividingX, pic1DividingY, pic1DividingHeight, pic1DividingWidth, pic1DividingThreshold ;//滤嘴烟支分界区域
	int filterCigDividingVerticalRelative;

	QString cigDividingRectX, cigDividingRectY, cigDividingRectHeight, cigDividingRectWidth, cigDividingRectThreshold;//卷烟端分界区域
	int cigVerticalRelative;
	
	QString cigUpDividingRectX, cigUpDividingRectY, cigUpDividingRectHeight, cigUpDividingRectWidth, cigUpDividingRectThreshold;//烟支上部分界区域
	QString cigDownDividingRectX, cigDownDividingRectY, cigDownDividingRectHeight, cigDownDividingRectWidth, cigDownDividingRectThreshold;//烟支下部分界区域

	QString cigBrokenX, cigBrokenY, cigBrokenRectHeight, cigBrokenRectWidth, cigBrokenRectThreshold;//烟支刺破检测区域
	int cigBrokenRelative;


	QString pic2RectX, pic2RectY, pic2RectHeight, pic2RectWidth;
	QString pic3RectX, pic3RectY, pic3RectHeight, pic3RectWidth;
	QString pic4RectX, pic4RectY, pic4RectHeight, pic4RectWidth;
	QString	line_rho, line_theta, line_threshold, line_minLineLength, line_maxLineGap;//霍夫直线获取
	QString approxPolyDPSet;//拟合多边形精度，参数越小精度越高建议0.8-1.2
	QString minDisOfTwoPoints;//拟合后，距离近点近似距离
};
