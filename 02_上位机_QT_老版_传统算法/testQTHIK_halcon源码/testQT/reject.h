#pragma once
#include <QObject>
#include <QRunnable>
#include "testQT.h"
#include <QThreadPool>//Ïß³Ì³Ø
#include<qdir.h>
#include<qqueue.h>
#include"C:/Advantech/DAQNavi/Inc/bdaqctrl.h"
using namespace Automation::BDaq;
class reject : public QObject, public QRunnable
{
	Q_OBJECT
public:
	reject(testQT* pdlg);
	~reject();
	testQT* pdlg;
protected:
	void run();
};

