#include "testQT.h"
#include <QtWidgets/QApplication>


int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	testQT w;
	w.showMaximized();
	//w.show();
	
	return a.exec();
}
