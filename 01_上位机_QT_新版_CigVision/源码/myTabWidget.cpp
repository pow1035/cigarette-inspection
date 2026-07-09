#include "myTabWidget.h"
#include <QPainter>

myTabWidget::myTabWidget(QWidget* parent) : QTabWidget(parent)
{
	customTabBar = new CusTabBar(this);
	setTabBar(customTabBar);
	
	// 设置样式
	setDocumentMode(true);
	setTabPosition(QTabWidget::North);
	setStyleSheet(
		"QTabWidget::pane { "
		"   border: 1px solid #b4b4b4;"
		"   background: white; "
		"} "
		"QTabWidget::tab-bar { "
		"   left: 0px; "
		"}"
	);
}

myTabWidget::~myTabWidget()
{
}

void myTabWidget::paintEvent(QPaintEvent* event)
{
	QTabWidget::paintEvent(event);
	
	// 如果需要额外的绘制，可以在这里添加
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
}