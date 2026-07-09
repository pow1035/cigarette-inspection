#include "saveImage.h"
QString mkMutiDir(const QString path);
saveImage::saveImage(testQT *pdlg)
{
	this->pdlg = pdlg;
}
saveImage::~saveImage()
{

}
void saveImage::run()
{
	while (pdlg->systemRun)
	{
		if (pdlg->picSaveList.isEmpty())
		{
			Sleep(0);
			continue;
		}
		if (pdlg->picSaveList.size() == 1)
		{
			Sleep(2);
		}
		QDateTime dateTime = QDateTime::currentDateTime();
		QString dateTimeS = dateTime.toString("yyyyMMddhhmmsszzz");
		picSaveStruct picSaveStruct1 = pdlg->picSaveList.dequeue();
		SetHcppInterfaceStringEncodingIsUtf8(false);
		QString picFileName = "";
		QString picAddriess = "";
		QString picRootAddriess = "D:\\test1\\";
		QString picDate= dateTime.toString("yyyy-MM-dd");
		QString picRejectZu = "camera"+QString::number(picSaveStruct1.zuNumber);
		//QString picRejectType= QString::number(picSaveStruct1.rejectType);
		QString picRejectType;//缺陷类型
		switch (picSaveStruct1.rejectType)
		{
		case 1:
			picRejectType = QString::fromLocal8Bit("烟支外形缺陷");
			break;
		case 2:
			picRejectType = QString::fromLocal8Bit("烟棒黑斑孔洞");
			break;
		case 3:
			picRejectType = QString::fromLocal8Bit("烟棒与嘴棒交接缺陷");
			break;
		case 4:
			//continue;//不保存
			picRejectType = QString::fromLocal8Bit("烟支滤嘴缺陷");
			break;
		case 5:
			picRejectType = QString::fromLocal8Bit("烟支外形缺陷");
			break;

		default:
			break;
		}
		picAddriess = picRootAddriess + picDate + "\\" + picRejectZu + "\\"+ picRejectType+"\\";
		//picFileName =QString::number(camera_number) + "_" + QString::number(photo_number) + "_" + dateTimeS + ".jpg";
		
		QString saveDir;
		QString tempS;
		HTuple hs;
		bool dirExist = false;
		
		picFileName = picSaveStruct1.zuNumber+"_" + dateTimeS + ".jpg";

		saveNGImageByClass(picAddriess, picFileName, &picSaveStruct1);
	}
	
}
bool saveImage::createFileDir(QString dir) {
	QDir qdir(dir);
	if (qdir.exists())
	{
		return true;
	}
	else
	{
		bool ok = qdir.mkdir(dir);
		return ok;
	}
}
bool saveImage::QStringCNFileNameSaveImage(QString str,HObject &ho_image)
{
	SetSystem("filename_encoding", "utf8");
	QByteArray temp = str.toLocal8Bit();
	char* ch = temp.data();
	HTuple hs(ch);
	WriteImage(ho_image, "jpg", 0, hs);
	return true;
}
bool saveImage::saveNGImageByClass(QString dir, QString name, picSaveStruct *picSaveStruct1) {
	SetHcppInterfaceStringEncodingIsUtf8(false);
	QString tempS;
	QString doubleImageDir,singleImageDir,drawNGImageDir;
	/*QString doubleImage = "originalPic";
	QString singleImage = "singlePic";
	QString drawNGImage = "NG";*/
	
	QString doubleImage;
	doubleImage = QString::fromLocal8Bit("灰度原图");
	QString singleImage;
	singleImage	=QString::fromLocal8Bit( "单支原图");
	QString drawNGImage;
	drawNGImage=QString::fromLocal8Bit( "NG标记图");
	
	doubleImageDir = dir+ doubleImage;
	doubleImageDir=mkMutiDir(doubleImageDir);
	QDir dir1(doubleImageDir);
	if (dir1.exists())
	{
		tempS = doubleImageDir +"\\" + name;
		QStringCNFileNameSaveImage(tempS, picSaveStruct1->ho_double_gray_image);
	}

	singleImageDir = dir + "\\" + singleImage;
	singleImageDir=mkMutiDir(singleImageDir);
	QDir dir2(singleImageDir);
	if (dir2.exists())
	{
		tempS = singleImageDir + "\\" + name;
		QStringCNFileNameSaveImage(tempS, picSaveStruct1->ho_single_gray_image);
	}

	drawNGImageDir = dir+ drawNGImage;
	drawNGImageDir=mkMutiDir(drawNGImageDir);
	QDir dir3(drawNGImageDir);
	if (dir3.exists())
	{
		tempS = drawNGImageDir + "\\" + name;
		QStringCNFileNameSaveImage(tempS, picSaveStruct1->ho_drawNG_image);
	}
	return 1;
}
QString mkMutiDir(const QString path)
{
	QDir dir(path);
	if (dir.exists())
	{
		return path;
	}
	QString parentDir = mkMutiDir(path.mid(0, path.lastIndexOf('\\')));
	QString dirname = path.mid(path.lastIndexOf('\\') + 1);
	QDir parentPath(parentDir);
	if (!dirname.isEmpty())
		parentPath.mkpath(dirname);
	return parentDir + '\\' + dirname;
}