#include "search.h"

search::search(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	searchInit();
	//QItemSelectionModel *d= ui.tableView->selectionModel();

	
}

search::~search()
{
}
void search::on_btnBack_clicked() {
	this->hide();
}
void search::searchInit() {
	//组件选择
	QMap<QString, int> zu_value;
	zu_value.insert(QString::fromLocal8Bit("第一组件"), 1);
	zu_value.insert(QString::fromLocal8Bit("第二组件"), 2);
	ui.comboBox->clear();
	QMap<QString, int>::const_iterator i;
	for (i = zu_value.constBegin(); i!=zu_value.constEnd(); ++i)
	{
		QString temp = i.key();
		ui.comboBox->addItem(temp, i.value());
	}
	//日期选择
	QDateTime curDateTime = QDateTime::currentDateTime();
	ui.start_dateTimeEdit->setDateTime(curDateTime.addDays(-1));
	ui.end_dateTimeEdit->setDateTime(curDateTime);

	//缺陷类型选择
	ui.chk_shape->setChecked(true);
	ui.chk_CigHole->setChecked(true);

	//初始化表格视图
	model = new QStandardItemModel();
	ui.tableView->setModel(model);

	model->setColumnCount(2);
	model->setHeaderData(0, Qt::Horizontal, QString::fromLocal8Bit("图片名称"));
	model->setHeaderData(1, Qt::Horizontal, QString::fromLocal8Bit("保存时间"));
	ui.tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	//ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);//自适应列宽
	ui.tableView->setColumnWidth(0, 200);
	ui.tableView->setColumnWidth(1, 200);
	

	//halcon显示绑定
	Hlong winId = (Hlong)ui.pictrueShow->winId();
	int labHeight = ui.pictrueShow->height();// (Hlong)ui.picture1->height();
	int labWidth = ui.pictrueShow->width();//(Hlong)ui.picture1->width();
	OpenWindow(0, 0, (Hlong)labWidth, (Hlong)labHeight, winId, "visible", "", &hv_WindowHandle);
}
//void search::on_btnSearch_clicked() {
//
//	model->clear();
//	//选择相机组件
//	int selectZu = ui.comboBox->currentData().toInt();
//	//选项时间段
//	QString start_timeStr = ui.start_dateTimeEdit->text();
//	QString end_timeStr = ui.end_dateTimeEdit->text();
//	//选择查询缺陷类型
//	bool shape_isChecked = ui.chk_shape->isChecked();
//	bool cigHole_isChecked = ui.chk_CigHole->isChecked();
//	QString NGDetailMySQLStr="";
//	if (shape_isChecked)
//	{
//		if (!NGDetailMySQLStr.isEmpty())//如果不为空，字符串前加“，”
//			NGDetailMySQLStr = NGDetailMySQLStr + ",";
//		NGDetailMySQLStr = NGDetailMySQLStr + "'1'";
//	}
//	if (cigHole_isChecked)
//	{
//		if (!NGDetailMySQLStr.isEmpty())//如果不为空，字符串前加“，”
//			NGDetailMySQLStr = NGDetailMySQLStr + ",";
//		NGDetailMySQLStr = NGDetailMySQLStr + "'2'";
//	}
//	if(!NGDetailMySQLStr.isEmpty())
//		NGDetailMySQLStr = NGDetailMySQLStr.left(NGDetailMySQLStr.length() - 1);
//	if (!NGDetailMySQLStr.isEmpty())
//		NGDetailMySQLStr = NGDetailMySQLStr.right(NGDetailMySQLStr.length() - 1);
//	//return;
//	//数据库操作
//	db = QSqlDatabase::addDatabase("QODBC");
//	db.setHostName("127.0.0.1");
//	db.setPort(3306);
//	db.setDatabaseName("mysql");
//	db.setUserName("root");
//	db.setPassword("root");
//	query = new QSqlQuery(db);
//	bool ok = db.open();
//	if (ok)
//	{
//		//QString sql1 = QString("select into ngcigmaster (ngCigTimeStamp,picFileName,picAddriess,checkTime,belowBanciID) values ('%1','%2','%3','%4','%5') ").arg(timestamp).arg(name).arg(dir).arg(checkTime).arg(banciID);
//		QString sql = QString("SELECT t1.ngCigTimeStamp,t1.picAddress,t1.picFileName,t1.checkTime,t2.ngclassID,\
//			t2.value1,t2.value2,t2.value3,t2.value4 FROM \
//			ngcigmaster t1, ngcigdetail t2 \
//			where t1.ngCigTimeStamp = t2.ngCigTimeStamp and \
//			t2.ngclassID in('%1') and t1.checkTime between '%2' and '%3' \
//			order by t1.ngCigTimeStamp desc").arg(NGDetailMySQLStr).arg(start_timeStr).arg(end_timeStr);
//		QString ngCigTimeStampStr,value1Str,picAddress;
//		int noclassID = 0;
//		QString picFileName,checkTimeStr;
//		bool result1 = query->exec(sql);
//		
//		if (!result1)//主记录已写入
//		{
//			return ;//失败
//		}
//		else
//		{
//			ngCigTimeStampList.clear();//清空图片时间戳
//			ngPicAdressList.clear();//清空图片地址
//			ngPicNameList.clear();//清空图片名称
//			int i = 0;
//			bool isExist = false;
//			while (query->next())
//			{
//				ngCigTimeStampStr = query->value(0).toString();
//				if (!ngCigTimeStampList.isEmpty())
//				{
//					isExist = ngCigTimeStampList.contains(ngCigTimeStampStr);
//					if (isExist)
//					{
//						continue;
//					}
//				}
//				ngCigTimeStampList.append(ngCigTimeStampStr);
//				picAddress = query->value(1).toString();
//				ngPicAdressList.append(picAddress);
//				picFileName = query->value(2).toString();
//				ngPicNameList.append(picFileName);
//
//				checkTimeStr = query->value(3).toString();
//				noclassID = query->value(4).toInt();
//				model->setItem(i, 0, new QStandardItem(picFileName));
//				model->setItem(i, 1, new QStandardItem(checkTimeStr));
//				i++;
//				//HTuple filePath;
//				//HObject ho_Image, global_gray_image;
//				//HTuple hv_Width, hv_Height;
//				//ReadImage(&ho_Image, filePath);
//				//Rgb1ToGray(ho_Image, &global_gray_image);//得到灰度图
//				//GetImageSize(global_gray_image, &hv_Width, &hv_Height);
//				//
//				//SetPart(hv_WindowHandle, 0, 0, hv_Height, hv_Width);
//				//DispObj(global_gray_image, hv_WindowHandle);
//			}
//		}
//		 
//		connect(ui.tableView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(on_currentChanged(QModelIndex, QModelIndex)));
//		//return;
//		//QString sql2 = "insert into ngcigmaster (picFileName,picAddriess) values ('2','3') ";
//		//bool result1 = query->exec(sql1);
//		//if (!result1)//主记录已写入
//		//{
//		//	return ;//失败
//		//}
//		//bool result2 = query->exec(sql2);
//	}
//}
//void search::on_currentChanged(const QModelIndex &current, const QModelIndex &previous)
//{
//	int rowNumber = current.row();
//	QString picStamp = ngCigTimeStampList.at(rowNumber);
//	QString picAddress = ngPicAdressList.at(rowNumber);
//	QString picName = ngPicNameList.at(rowNumber);
//	QString picFilePath = picAddress + picName;
//	//int colNumber = current.column();
//	HObject ho_Image;
//	HTuple hv_Width, hv_Height;
//	HObject global_gray_image;//全局灰色图像原图
//	ReadImage(&ho_Image, HTuple(picFilePath.toStdString().c_str()));
//	Rgb1ToGray(ho_Image, &global_gray_image);//得到灰度图
//	GetImageSize(global_gray_image, &hv_Width, &hv_Height);
//	SetPart(hv_WindowHandle, 0, 0, hv_Height, hv_Width);
//	DispObj(global_gray_image, hv_WindowHandle);
//
//	QString sql = QString("select t1.ngCigTimeStamp,t1.ngclassID,t1.value1,t1.value2,t1.value3\
//		,t1.value4 from ngcigdetail t1 where t1.ngCigTimeStamp='%1'").arg(picStamp);
//	bool result1 = query->exec(sql);
//	if (!result1)
//	{
//		return;
//	}
//	else {
//		while (query->next())
//		{
//			QString ngClassIDStr, value1Str, value2Str, value3Str, value4Str;
//			ngClassIDStr = query->value(1).toString();
//			value1Str = query->value(2).toString();
//			value2Str = query->value(3).toString();
//			value3Str = query->value(4).toString();
//			value4Str = query->value(5).toString();
//			if (ngClassIDStr == "2")//
//			{
//				SetColor(hv_WindowHandle, "red");
//				SetDraw(hv_WindowHandle, "margin");
//				SetLineWidth(hv_WindowHandle, 3);
//				DispCircle(hv_WindowHandle, HTuple(value1Str.toInt()), HTuple(value2Str.toInt()), HTuple(value3Str.toInt()));
//
//			}
//		}
//	}
//
//}