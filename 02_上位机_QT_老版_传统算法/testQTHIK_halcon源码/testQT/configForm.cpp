#include "configForm.h"
#include <Qsettings.h>

configForm::configForm(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	//QString iniFilePath = QCoreApplication::applicationDirPath() + "/config.ini";
	configIniWrite = new QSettings("config.ini", QSettings::IniFormat);
	//设置编码，使支持中文
	configIniWrite->setIniCodec("UTF-8");
	//设置配置文件值，“节点 + / + 键”，值
	//configIniWrite->setValue("ADDRESS/detail", "zjk");
	pic1RectX = configIniWrite->value("rect1/x").toString();
	pic1RectY = configIniWrite->value("rect1/y").toString();
	pic1RectWidth = configIniWrite->value("rect1/width").toString();
	pic1RectHeight = configIniWrite->value("rect1/height").toString();
	//line_rho, line_theta, line_threshold, line_minLineLength, line_maxLineGap;
	line_rho = configIniWrite->value("HoughLinesP/rho").toString();
	line_theta = configIniWrite->value("HoughLinesP/theta").toString();
	line_threshold = configIniWrite->value("HoughLinesP/threshold").toString();
	line_minLineLength = configIniWrite->value("HoughLinesP/minLineLength").toString();
	line_maxLineGap = configIniWrite->value("HoughLinesP/maxLineGap").toString();
	approxPolyDPSet = configIniWrite->value("approxPolyDP/approxPolyDPSet").toString();

	minDisOfTwoPoints = configIniWrite->value("dingwei/minDisOfTwoPoints").toString();

	pic1DividingX = configIniWrite->value("rect1Dividing/x").toString();
	pic1DividingY = configIniWrite->value("rect1Dividing/y").toString();
	pic1DividingHeight = configIniWrite->value("rect1Dividing/height").toString();
	pic1DividingWidth = configIniWrite->value("rect1Dividing/width").toString();
	pic1DividingThreshold = configIniWrite->value("rect1Dividing/threshold").toString();
	if (configIniWrite->value("rect1Dividing/verticalRelative").toInt()==1)
	{
		filterCigDividingVerticalRelative = 1;
	}
	else
	{
		filterCigDividingVerticalRelative = 0;
	}

	filterDividingRectX = configIniWrite->value("filterDividingRect/x").toString();
	filterDividingRectY = configIniWrite->value("filterDividingRect/y").toString();
	filterDividingRectHeight = configIniWrite->value("filterDividingRect/height").toString();
	filterDividingRectWidth = configIniWrite->value("filterDividingRect/width").toString();
	filterDividingRectThreshold = configIniWrite->value("filterDividingRect/threshold").toString();
	if (configIniWrite->value("filterDividingRect/verticalRelative").toInt() == 1)
	{
		filterVerticalRelative = 1;
	}
	else
	{
		filterVerticalRelative = 0;
	}

	cigDividingRectX = configIniWrite->value("cigDividingRect/x").toString();
	cigDividingRectY = configIniWrite->value("cigDividingRect/y").toString();
	cigDividingRectHeight = configIniWrite->value("cigDividingRect/height").toString();
	cigDividingRectWidth = configIniWrite->value("cigDividingRect/width").toString();
	cigDividingRectThreshold = configIniWrite->value("cigDividingRect/threshold").toString();
	if (configIniWrite->value("cigDividingRect/verticalRelative").toInt() == 1)
	{
		cigVerticalRelative = 1;
	}
	else
	{
		cigVerticalRelative = 0;
	}

	cigUpDividingRectX = configIniWrite->value("cigUpDividingRect/x").toString();
	cigUpDividingRectY = configIniWrite->value("cigUpDividingRect/y").toString();
	cigUpDividingRectHeight = configIniWrite->value("cigUpDividingRect/height").toString();
	cigUpDividingRectWidth = configIniWrite->value("cigUpDividingRect/width").toString();
	cigUpDividingRectThreshold = configIniWrite->value("cigUpDividingRect/threshold").toString();

	cigDownDividingRectX = configIniWrite->value("cigDownDividingRect/x").toString();
	cigDownDividingRectY = configIniWrite->value("cigDownDividingRect/y").toString();
	cigDownDividingRectHeight = configIniWrite->value("cigDownDividingRect/height").toString();
	cigDownDividingRectWidth = configIniWrite->value("cigDownDividingRect/width").toString();
	cigDownDividingRectThreshold = configIniWrite->value("cigDownDividingRect/threshold").toString();

	filterWidthRectX = configIniWrite->value("filterWidthRect/x").toString();
	filterWidthRectY = configIniWrite->value("filterWidthRect/y").toString();
	filterWidthRectHeight = configIniWrite->value("filterWidthRect/height").toString();
	filterWidthRectWidth = configIniWrite->value("filterWidthRect/width").toString();
	filterWidthRectThreshold = configIniWrite->value("filterWidthRect/threshold").toString();

	cigNearFilterWidthRectX = configIniWrite->value("cigNearFilterWidthRect/x").toString();
	cigNearFilterWidthRectY = configIniWrite->value("cigNearFilterWidthRect/y").toString();
	cigNearFilterWidthRectHeight = configIniWrite->value("cigNearFilterWidthRect/height").toString();
	cigNearFilterWidthRectWidth = configIniWrite->value("cigNearFilterWidthRect/width").toString();

	cigTopWidthRectX = configIniWrite->value("cigTopWidthRect/x").toString();
	cigTopWidthRectY = configIniWrite->value("cigTopWidthRect/y").toString();
	cigTopWidthRectHeight = configIniWrite->value("cigTopWidthRect/height").toString();
	cigTopWidthRectWidth = configIniWrite->value("cigTopWidthRect/width").toString();

	cigBrokenX = configIniWrite->value("cigBroken/x").toString();
	cigBrokenY = configIniWrite->value("cigBroken/y").toString();
	cigBrokenRectHeight = configIniWrite->value("cigBroken/height").toString();
	cigBrokenRectWidth = configIniWrite->value("cigBroken/width").toString();
	cigBrokenRectThreshold = configIniWrite->value("cigBroken/threshold").toString();
	if (configIniWrite->value("cigBroken/relative").toInt() == 1)
	{
		cigBrokenRelative = 1;
	}
	else
	{
		cigBrokenRelative = 0;
	}

	ui.rectX->setPlainText(pic1RectX);
	ui.rectY->setPlainText(pic1RectY);
	ui.rectWidth->setPlainText(pic1RectWidth);
	ui.rectHeight->setPlainText(pic1RectHeight);

	ui.pic1DividingX->setPlainText(pic1DividingX);
	ui.pic1DividingY->setPlainText(pic1DividingY);
	ui.pic1DividingWidth->setPlainText(pic1DividingWidth);
	ui.pic1DividingHeight->setPlainText(pic1DividingHeight); 
	ui.pic1DividingThreshold->setPlainText(pic1DividingThreshold);
	if (filterCigDividingVerticalRelative==1)
	{
		ui.filterCigDividingVerticalRelative->setChecked(true);
	}
	else
	{
		ui.filterCigDividingVerticalRelative->setChecked(false);
	}
	

	ui.filterDividingRectX->setPlainText(filterDividingRectX);
	ui.filterDividingRectY->setPlainText(filterDividingRectY);
	ui.filterDividingRectWidth->setPlainText(filterDividingRectWidth);
	ui.filterDividingRectHeight->setPlainText(filterDividingRectHeight);
	ui.filterDividingRectThreshold->setPlainText(filterDividingRectThreshold);
	if (filterVerticalRelative == 1)
	{
		ui.filterVerticalRelative->setChecked(true);
	}
	else
	{
		ui.filterVerticalRelative->setChecked(false);
	}

	ui.cigDividingRectX->setPlainText(cigDividingRectX);
	ui.cigDividingRectY->setPlainText(cigDividingRectY);
	ui.cigDividingRectWidth->setPlainText(cigDividingRectWidth);
	ui.cigDividingRectHeight->setPlainText(cigDividingRectHeight);
	ui.cigDividingRectThreshold->setPlainText(cigDividingRectThreshold);
	if (cigVerticalRelative == 1)
	{
		ui.cigVerticalRelative->setChecked(true);
	}
	else
	{
		ui.cigVerticalRelative->setChecked(false);
	}

	ui.cigUpDividingRectX->setPlainText(cigUpDividingRectX);
	ui.cigUpDividingRectY->setPlainText(cigUpDividingRectY);
	ui.cigUpDividingRectWidth->setPlainText(cigUpDividingRectWidth);
	ui.cigUpDividingRectHeight->setPlainText(cigUpDividingRectHeight);
	ui.cigUpDividingRectThreshold->setPlainText(cigUpDividingRectThreshold);

	ui.cigDownDividingRectX->setPlainText(cigDownDividingRectX);
	ui.cigDownDividingRectY->setPlainText(cigDownDividingRectY);
	ui.cigDownDividingRectWidth->setPlainText(cigDownDividingRectWidth);
	ui.cigDownDividingRectHeight->setPlainText(cigDownDividingRectHeight);
	ui.cigDownDividingRectThreshold->setPlainText(cigDownDividingRectThreshold);

	ui.filterWidthRectX->setPlainText(filterWidthRectX);
	ui.filterWidthRectY->setPlainText(filterWidthRectY);
	ui.filterWidthRectWidth->setPlainText(filterWidthRectWidth);
	ui.filterWidthRectHeight->setPlainText(filterWidthRectHeight);
	ui.filterWidthRectThreshold->setPlainText(filterWidthRectThreshold);

	ui.cigNearFilterWidthRectX->setPlainText(cigNearFilterWidthRectX);
	ui.cigNearFilterWidthRectY->setPlainText(cigNearFilterWidthRectY);
	ui.cigNearFilterWidthRectWidth->setPlainText(cigNearFilterWidthRectWidth);
	ui.cigNearFilterWidthRectHeight->setPlainText(cigNearFilterWidthRectHeight);

	ui.cigTopWidthRectX->setPlainText(cigTopWidthRectX);
	ui.cigTopWidthRectY->setPlainText(cigTopWidthRectY);
	ui.cigTopWidthRectWidth->setPlainText(cigTopWidthRectWidth);
	ui.cigTopWidthRectHeight->setPlainText(cigTopWidthRectHeight);


	ui.line_rho->setPlainText(line_rho);
	ui.line_theta->setPlainText(line_theta);
	ui.line_threshold->setPlainText(line_threshold);
	ui.line_minLineLength->setPlainText(line_minLineLength);
	ui.line_maxLineGap->setPlainText(line_maxLineGap);
	
	ui.approxPolyDPSet->setPlainText(approxPolyDPSet);

	ui.minDisOfTwoPoints->setPlainText(minDisOfTwoPoints);

	ui.cigBrokenX->setPlainText(cigBrokenX);
	ui.cigBrokenY->setPlainText(cigBrokenY);
	ui.cigBrokenRectWidth->setPlainText(cigBrokenRectWidth);
	ui.cigBrokenRectHeight->setPlainText(cigBrokenRectHeight);
	ui.cigBrokenRectThreshold->setPlainText(cigBrokenRectThreshold);
	if (cigBrokenRelative == 1)
	{
		ui.cigBrokenRelative->setChecked(true);
	}
	else
	{
		ui.cigBrokenRelative->setChecked(false);
	}
}

configForm::~configForm()
{
	delete configIniWrite;
}
void configForm::on_btnSave_clicked() {
	
	configIniWrite->setValue("rect1/x", ui.rectX->toPlainText());
	configIniWrite->setValue("rect1/y", ui.rectY->toPlainText());
	configIniWrite->setValue("rect1/width", ui.rectWidth->toPlainText());
	configIniWrite->setValue("rect1/height", ui.rectHeight->toPlainText());
	
	

	configIniWrite->setValue("HoughLinesP/rho", ui.line_rho->toPlainText());
	configIniWrite->setValue("HoughLinesP/theta", ui.line_theta->toPlainText());
	configIniWrite->setValue("HoughLinesP/threshold", ui.line_threshold->toPlainText());
	configIniWrite->setValue("HoughLinesP/minLineLength", ui.line_minLineLength->toPlainText());
	configIniWrite->setValue("HoughLinesP/maxLineGap", ui.line_maxLineGap->toPlainText());

	configIniWrite->setValue("approxPolyDP/approxPolyDPSet", ui.approxPolyDPSet->toPlainText());

	configIniWrite->setValue("dingwei/minDisOfTwoPoints", ui.minDisOfTwoPoints->toPlainText());

	configIniWrite->setValue("rect1Dividing/width", ui.pic1DividingWidth->toPlainText());
	configIniWrite->setValue("rect1Dividing/height", ui.pic1DividingHeight->toPlainText());
	configIniWrite->setValue("rect1Dividing/threshold", ui.pic1DividingThreshold->toPlainText());
	if (ui.pic1DividingX->toPlainText().toInt() < ui.rectWidth->toPlainText().toInt())
	{
		configIniWrite->setValue("rect1Dividing/x", ui.pic1DividingX->toPlainText());
		QPalette palette = ui.pic1DividingX->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.pic1DividingX->setPalette(palette);
	}
	else {
		QPalette palette = ui.pic1DividingX->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.pic1DividingX->setPalette(palette);
	}
	if (ui.pic1DividingY->toPlainText().toInt() < ui.rectHeight->toPlainText().toInt())
	{
		configIniWrite->setValue("rect1Dividing/y", ui.pic1DividingY->toPlainText());
		QPalette palette = ui.pic1DividingY->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.pic1DividingY->setPalette(palette);
	}
	else {
		QPalette palette = ui.pic1DividingY->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.pic1DividingY->setPalette(palette);
	}
	if (ui.filterCigDividingVerticalRelative->isChecked())
	{
		configIniWrite->setValue("rect1Dividing/verticalRelative", 1);
	}
	else
	{
		configIniWrite->setValue("rect1Dividing/verticalRelative", 0);
	}
	
	configIniWrite->setValue("filterDividingRect/width", ui.filterDividingRectWidth->toPlainText());
	configIniWrite->setValue("filterDividingRect/height", ui.filterDividingRectHeight->toPlainText());
	configIniWrite->setValue("filterDividingRect/threshold", ui.filterDividingRectThreshold->toPlainText());
	if (ui.filterDividingRectX->toPlainText().toInt() < ui.rectWidth->toPlainText().toInt())
	{
		configIniWrite->setValue("filterDividingRect/x", ui.filterDividingRectX->toPlainText());
		QPalette palette = ui.filterDividingRectX->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.filterDividingRectX->setPalette(palette);
	}
	else {
		QPalette palette = ui.filterDividingRectX->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.filterDividingRectX->setPalette(palette);
	}
	if (ui.filterDividingRectY->toPlainText().toInt() < ui.rectHeight->toPlainText().toInt())
	{
		configIniWrite->setValue("filterDividingRect/y", ui.filterDividingRectY->toPlainText());
		QPalette palette = ui.filterDividingRectY->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.filterDividingRectY->setPalette(palette);
	}
	else {
		QPalette palette = ui.filterDividingRectY->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.filterDividingRectY->setPalette(palette);
	}
	if (ui.filterVerticalRelative->isChecked())
	{
		configIniWrite->setValue("filterDividingRect/verticalRelative", 1);
	}
	else
	{
		configIniWrite->setValue("filterDividingRect/verticalRelative", 0);
	}

	configIniWrite->setValue("cigDividingRect/width", ui.cigDividingRectWidth->toPlainText());
	configIniWrite->setValue("cigDividingRect/height", ui.cigDividingRectHeight->toPlainText());
	configIniWrite->setValue("cigDividingRect/threshold", ui.cigDividingRectThreshold->toPlainText());
	if (ui.cigDividingRectX->toPlainText().toInt() < ui.rectWidth->toPlainText().toInt())
	{
		configIniWrite->setValue("cigDividingRect/x", ui.cigDividingRectX->toPlainText());
		QPalette palette = ui.cigDividingRectX->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigDividingRectX->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigDividingRectX->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigDividingRectX->setPalette(palette);
	}
	if (ui.cigDividingRectY->toPlainText().toInt() < ui.rectHeight->toPlainText().toInt())
	{
		configIniWrite->setValue("cigDividingRect/y", ui.cigDividingRectY->toPlainText());
		QPalette palette = ui.cigDividingRectY->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigDividingRectY->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigDividingRectY->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigDividingRectY->setPalette(palette);
	}
	if (ui.cigVerticalRelative->isChecked())
	{
		configIniWrite->setValue("cigDividingRect/verticalRelative", 1);
	}
	else
	{
		configIniWrite->setValue("cigDividingRect/verticalRelative", 0);
	}

	configIniWrite->setValue("cigUpDividingRect/width", ui.cigUpDividingRectWidth->toPlainText());
	configIniWrite->setValue("cigUpDividingRect/height", ui.cigUpDividingRectHeight->toPlainText());
	configIniWrite->setValue("cigUpDividingRect/threshold", ui.cigUpDividingRectThreshold->toPlainText());
	if (ui.cigUpDividingRectX->toPlainText().toInt() < ui.rectWidth->toPlainText().toInt())
	{
		configIniWrite->setValue("cigUpDividingRect/x", ui.cigUpDividingRectX->toPlainText());
		QPalette palette = ui.cigUpDividingRectX->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigUpDividingRectX->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigUpDividingRectX->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigUpDividingRectX->setPalette(palette);
	}
	if (ui.cigUpDividingRectY->toPlainText().toInt() < ui.rectHeight->toPlainText().toInt())
	{
		configIniWrite->setValue("cigUpDividingRect/y", ui.cigUpDividingRectY->toPlainText());
		QPalette palette = ui.cigUpDividingRectY->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigUpDividingRectY->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigUpDividingRectY->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigUpDividingRectY->setPalette(palette);
	}

	configIniWrite->setValue("cigDownDividingRect/width", ui.cigDownDividingRectWidth->toPlainText());
	configIniWrite->setValue("cigDownDividingRect/height", ui.cigDownDividingRectHeight->toPlainText());
	configIniWrite->setValue("cigDownDividingRect/threshold", ui.cigDownDividingRectThreshold->toPlainText());
	if (ui.cigDownDividingRectX->toPlainText().toInt() < ui.rectWidth->toPlainText().toInt())
	{
		configIniWrite->setValue("cigDownDividingRect/x", ui.cigDownDividingRectX->toPlainText());
		QPalette palette = ui.cigDownDividingRectX->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigDownDividingRectX->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigDownDividingRectX->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigDownDividingRectX->setPalette(palette);
	}
	if (ui.cigDownDividingRectY->toPlainText().toInt() < ui.rectHeight->toPlainText().toInt())
	{
		configIniWrite->setValue("cigDownDividingRect/y", ui.cigDownDividingRectY->toPlainText());
		QPalette palette = ui.cigDownDividingRectY->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigDownDividingRectY->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigDownDividingRectY->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigDownDividingRectY->setPalette(palette);
	}

	configIniWrite->setValue("filterWidthRect/width", ui.filterWidthRectWidth->toPlainText());
	configIniWrite->setValue("filterWidthRect/height", ui.filterWidthRectHeight->toPlainText());
	configIniWrite->setValue("filterWidthRect/threshold", ui.filterWidthRectThreshold->toPlainText());

	if (ui.filterWidthRectX->toPlainText().toInt() < ui.rectWidth->toPlainText().toInt())
	{
		configIniWrite->setValue("filterWidthRect/x", ui.filterWidthRectX->toPlainText());
		QPalette palette = ui.filterWidthRectX->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.filterWidthRectX->setPalette(palette);
	}
	else {
		QPalette palette = ui.filterWidthRectX->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.filterWidthRectX->setPalette(palette);
	}
	if (ui.filterWidthRectY->toPlainText().toInt() < ui.rectHeight->toPlainText().toInt())
	{
		configIniWrite->setValue("filterWidthRect/y", ui.filterWidthRectY->toPlainText());
		QPalette palette = ui.filterWidthRectY->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.filterWidthRectY->setPalette(palette);
	}
	else {
		QPalette palette = ui.filterWidthRectY->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.filterWidthRectY->setPalette(palette);
	}

	configIniWrite->setValue("cigNearFilterWidthRect/width", ui.cigNearFilterWidthRectWidth->toPlainText());
	configIniWrite->setValue("cigNearFilterWidthRect/height", ui.cigNearFilterWidthRectHeight->toPlainText());
	if (ui.cigNearFilterWidthRectX->toPlainText().toInt() < ui.rectWidth->toPlainText().toInt())
	{
		configIniWrite->setValue("cigNearFilterWidthRect/x", ui.cigNearFilterWidthRectX->toPlainText());
		QPalette palette = ui.cigNearFilterWidthRectX->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigNearFilterWidthRectX->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigNearFilterWidthRectX->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigNearFilterWidthRectX->setPalette(palette);
	}
	if (ui.cigNearFilterWidthRectY->toPlainText().toInt() < ui.rectHeight->toPlainText().toInt())
	{
		configIniWrite->setValue("cigNearFilterWidthRect/y", ui.cigNearFilterWidthRectY->toPlainText());
		QPalette palette = ui.cigNearFilterWidthRectY->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigNearFilterWidthRectY->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigNearFilterWidthRectY->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigNearFilterWidthRectY->setPalette(palette);
	}

	configIniWrite->setValue("cigTopWidthRect/width", ui.cigTopWidthRectWidth->toPlainText());
	configIniWrite->setValue("cigTopWidthRect/height", ui.cigTopWidthRectHeight->toPlainText());
	if (ui.cigTopWidthRectX->toPlainText().toInt() < ui.rectWidth->toPlainText().toInt())
	{
		configIniWrite->setValue("cigTopWidthRect/x", ui.cigTopWidthRectX->toPlainText());
		QPalette palette = ui.cigTopWidthRectX->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigTopWidthRectX->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigTopWidthRectX->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigTopWidthRectX->setPalette(palette);
	}
	if (ui.cigTopWidthRectY->toPlainText().toInt() < ui.rectHeight->toPlainText().toInt())
	{
		configIniWrite->setValue("cigTopWidthRect/y", ui.cigTopWidthRectY->toPlainText());
		QPalette palette = ui.cigTopWidthRectY->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigTopWidthRectY->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigTopWidthRectY->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigTopWidthRectY->setPalette(palette);
	}

	configIniWrite->setValue("cigBroken/width", ui.cigBrokenRectWidth->toPlainText());
	configIniWrite->setValue("cigBroken/height", ui.cigBrokenRectHeight->toPlainText());
	configIniWrite->setValue("cigBroken/threshold", ui.cigBrokenRectThreshold->toPlainText());
	if (ui.cigBrokenX->toPlainText().toInt() < ui.rectWidth->toPlainText().toInt())
	{
		configIniWrite->setValue("cigBroken/x", ui.cigBrokenX->toPlainText());
		QPalette palette = ui.cigBrokenX->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigBrokenX->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigBrokenX->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigBrokenX->setPalette(palette);
	}
	if (ui.cigBrokenY->toPlainText().toInt() < ui.rectHeight->toPlainText().toInt())
	{
		configIniWrite->setValue("cigBroken/y", ui.cigBrokenY->toPlainText());
		QPalette palette = ui.cigBrokenY->palette();
		palette.setColor(QPalette::Background, QColor(0, 255, 0));
		ui.cigBrokenY->setPalette(palette);
	}
	else {
		QPalette palette = ui.cigBrokenY->palette();
		palette.setColor(QPalette::Background, QColor(255, 0, 0));
		ui.cigBrokenY->setPalette(palette);
	}
	if (ui.cigBrokenRelative->isChecked())
	{
		configIniWrite->setValue("cigBroken/relative", 1);
	}
	else
	{
		configIniWrite->setValue("cigBroken/relative", 0);
	}
	emit updateConfig();

}
