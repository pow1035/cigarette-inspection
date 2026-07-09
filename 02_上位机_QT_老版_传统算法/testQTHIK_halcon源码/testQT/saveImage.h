#pragma once
#include<testQT.h>
#include<HalconCpp.h>
#include<qthread.h>
using namespace HalconCpp;
struct picSaveStruct;
class saveImage : public QObject, public QRunnable
{
	Q_OBJECT
public:
	saveImage(testQT* pdlg);
	~saveImage();

	void run();

	bool createFileDir(QString dir);
	bool QStringCNFileNameSaveImage(QString str, HObject &ho_image);
	bool saveNGImageByClass(QString dir, QString name, picSaveStruct *picSaveStruct1);
	testQT* pdlg;
};

