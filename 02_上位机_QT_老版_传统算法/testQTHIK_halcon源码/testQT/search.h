#pragma once

#include <QWidget>
#include "ui_search.h"
//#include <searchForm.ui>
//mysql数据库相关
//#include<qsqldatabase.h>
//#include<qsqlquery.h>
///////////////////////////
#include<qstandarditemmodel.h>
#include<qitemselectionmodel.h>
//#include<qabstractitemmodel.h>
#include<HalconCpp.h>
using namespace HalconCpp;
class search : public QWidget
{
	Q_OBJECT

public:
	search(QWidget *parent = Q_NULLPTR);
	~search();
	void searchInit();
private slots:
	void on_btnBack_clicked();
	//void on_btnSearch_clicked();
	//void on_currentChanged(const QModelIndex& current, const QModelIndex& previous);
private:
	Ui::search ui;
	
public:
	//QSqlDatabase db;//数据库
	//QSqlQuery* query;
	QList<QString> ngCigTimeStampList;//查询到的NG图像名称队列
	QList<QString> ngPicAdressList;//查询到的NG图像保存目录列表
	QList<QString> ngPicNameList;//查询到的NG图像保存目录列表
	QStandardItemModel* model;//表格视图数据
	//QItemSelectionModel* theSelect;//选择模型
	HTuple hv_WindowHandle;
	QModelIndex current,previous;
};
