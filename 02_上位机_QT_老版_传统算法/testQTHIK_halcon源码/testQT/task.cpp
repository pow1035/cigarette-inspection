#include<task.h>
#include <QThread>
#include <iostream>
#include<math.h>
#include<exception>
#include<qdebug.h>


class testQT;
//以下openCV算法
//bool getAllMask(InputArray  _originalMat, OutputArray _allMask, std::vector< cv::Point> &rectPoints, testQT* pCamTask);
//bool getEdge(InputArray  _originalMat, int lineType, OutputArray _allMask, Vec4i& line, testQT* pCamTask);//type=0滤嘴烟支分离线
//bool getEdge1(InputArray  _originalMat, int lineType, OutputArray _allMask, Vec4i& line, testQT* pCamTask);
//bool getEdge(InputArray  _originalMat, int divThreshold, int lineType, OutputArray _allMask, Vec4i& line, std::vector<Vec4i>& lines, testQT* pCamTask);
//bool getUpDownEdgeAndWidth(InputArray  _originalMat, int &cigWidth, OutputArray _allMask, Vec4i& upLine, Vec4i& downLine, testQT* pCamTask);//获取宽度
//bool getUpDownEdgeAndWidth(InputArray  _originalMat, int divThreshold, int& cigWidth, OutputArray _allMask, Vec4i& upLine, Vec4i& downLine, testQT* pCamTask);
//void  drawDashRect(InputOutputArray  _originalMat, int linelength, int dashlength, Rect* blob, Scalar color, int thickness);
//bool findBroken(InputOutputArray  _originalMat, OutputArray _allMask, testQT* pCamTask);


task::task(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, testQT* pCam)
{
    pDataTask = pData;
    pDlg = pCam;
    pFrameInfoTask = pFrameInfo;
	camera_number = 0;
	photo_number = 0;
    ////图片数据输入输出参数            
    //MV_SAVE_IMAGE_PARAM_EX stParam;

    ////源数据                 
    //stParam.pData = pDataTask;                //原始图像数据
    //stParam.nDataLen = pFrameInfoTask->nFrameLen;    //原始图像数据长度
    //stParam.enPixelType = pFrameInfoTask->enPixelType;  //原始图像数据的像素格式
    //stParam.nWidth = pFrameInfoTask->nWidth;       //图像宽
    //stParam.nHeight = pFrameInfoTask->nHeight;      //图像高 
    //stParam.nJpgQuality = 70;						  //JPEG图片编码质量  

    //获取一帧数据的大小
    //MVCC_INTVALUE stIntvalue = { 0 };
    //int nRet = MV_CC_GetIntValue(pCamTask->m_pcMyCamera[0]->m_hDevHandle, "PayloadSize", &stIntvalue);
    //if (nRet != MV_OK)
    //{
    //    
    //    return;
    //}
    //int nBufSize = stIntvalue.nCurValue; //一帧数据大小
    //unsigned char* pFrameBuf = NULL;
    //pFrameBuf = (unsigned char*)malloc(nBufSize);
    ////目标数据
    //stParam.enImageType = MV_Image_Jpeg;            //需要保存的图像类型，转换成JPEG格式
    //stParam.nBufferSize = nBufSize;                 //存储节点的大小
    //unsigned char* pImage = (unsigned char*)malloc(nBufSize);
    //stParam.pImageBuffer = pImage;                   //输出数据缓冲区，存放转换之后的图片数据         

    

    
}
task::task(HTuple filePath, testQT* pCam) {
	//QStringList type = QSqlDatabase::drivers();
	//QDebug() << type;

	QStartCount = initTime();//初始化时间

	this->pDlg = pCam;//窗口类句柄
	if (!pDlg->halconWindowHandleInit)
	{
		qDebug() << "hanlcon windows not init" << endl;
		return;
	}
	BoolInitHalconWindow = true;//窗口已初始化

	//qDebug() << "hello" << endl;
	//initHalconWindowHandle();//初始化halcon窗口
	testPic = 1;//图片测试标记
	HObject ho_Image;
	HTuple hv_Width, hv_Height;
	ReadImage(&ho_Image, filePath);
	expendTime = getExpendTime(QStartCount);
	qDebug() << "task readImage expend" << expendTime << endl;
	Rgb1ToGray(ho_Image, &global_gray_image);//得到灰度图
	GetImageSize(global_gray_image, &hv_Width, &hv_Height);
	pFrameInfoTask = new MV_FRAME_OUT_INFO();
	pFrameInfoTask->nHeight = hv_Height.I();
	pFrameInfoTask->nWidth = hv_Width.I();
	expendTime = getExpendTime(QStartCount);
	qDebug() << "task getGrayImageBeforeDeal expend" << expendTime << endl;
	showImage();
	uint result = whiteCigDarkLightCircleFilterProcess();

	expendTime = getExpendTime(QStartCount);
	qDebug() << "task dealFinish expend" << expendTime << endl;

	if (result & 0x00000FFF)//第一支烟检测有问题
	{
		//QString QTs = "D:/test/";
		QString picFileName = "";
		QString picAddriess = "D:\\test1\\";
		QDateTime dateTime = QDateTime::currentDateTime();
		QString dateTimeS = dateTime.toString("yyyyMMddhhmmsszzz");
		QString checkTime = dateTime.toString("yyyy-MM-dd hh:mm:ss");
		QString nowBanciNumber = "1";
		//picFileName =QString::number(camera_number) + "_" + QString::number(photo_number) + "_" + dateTimeS + ".jpg";
		picFileName = "1_1_" + dateTimeS + ".jpg";
		saveNGImageByClass(picAddriess, picFileName, result);
		/*bool result1=addMasterMySQL(picAddriess, picFileName, checkTime, nowBanciNumber);
		if (result1)
		{
			addDetailMySQL();
		}*/


	}
	//RotateImage(ho_Image, &ho_rotateImage, 90, "constant");
	//GetImageSize(ho_Image, &hv_Width, &hv_Height);
	expendTime = getExpendTime(QStartCount);
	qDebug() << "task allTime expend" << expendTime << endl;


}
task::task(QString fileName, testQT* pCam) {
	//QStringList type = QSqlDatabase::drivers();
	//QDebug() << type;

	QStartCount = initTime();//初始化时间

	this->pDlg = pCam;//窗口类句柄
	if (!pDlg->halconWindowHandleInit)
	{
		qDebug() << "hanlcon windows not init" << endl;
		return;
	}
	BoolInitHalconWindow = true;//窗口已初始化

	//qDebug() << "hello" << endl;
	//initHalconWindowHandle();//初始化halcon窗口
	testPic = 1;//图片测试标记
	HObject ho_Image;
	HTuple hv_Width, hv_Height;
	//ReadImage(&ho_Image, filePath);
	QStringCNFileNameReadImage(fileName, ho_Image);
	expendTime = getExpendTime(QStartCount);
	qDebug() << "task readImage expend" << expendTime << endl;
	Rgb1ToGray(ho_Image, &global_gray_image);//得到灰度图
	GetImageSize(global_gray_image, &hv_Width, &hv_Height);
	pFrameInfoTask = new MV_FRAME_OUT_INFO();
	pFrameInfoTask->nHeight = hv_Height.I();
	pFrameInfoTask->nWidth = hv_Width.I();
	expendTime = getExpendTime(QStartCount);
	qDebug() << "task getGrayImageBeforeDeal expend" << expendTime << endl;
	showImage();
	uint result = whiteCigDarkLightCircleFilterProcess();

	expendTime = getExpendTime(QStartCount);
	qDebug() << "task dealFinish expend" << expendTime << endl;

	if (result & 0x00000FFF)//第一支烟检测有问题
	{
		//QString QTs = "D:/test/";
		QString picFileName = "";
		QString picAddriess = "D:\\test1\\";
		QDateTime dateTime = QDateTime::currentDateTime();
		QString dateTimeS = dateTime.toString("yyyyMMddhhmmsszzz");
		QString checkTime = dateTime.toString("yyyy-MM-dd hh:mm:ss");
		QString nowBanciNumber = "1";
		//picFileName =QString::number(camera_number) + "_" + QString::number(photo_number) + "_" + dateTimeS + ".jpg";
		picFileName = "1_1_" + dateTimeS + ".jpg";
		saveNGImageByClass(picAddriess, picFileName, result);
		/*bool result1=addMasterMySQL(picAddriess, picFileName, checkTime, nowBanciNumber);
		if (result1)
		{
			addDetailMySQL();
		}*/


	}
	//RotateImage(ho_Image, &ho_rotateImage, 90, "constant");
	//GetImageSize(ho_Image, &hv_Width, &hv_Height);
	expendTime = getExpendTime(QStartCount);
	qDebug() << "task allTime expend" << expendTime << endl;


}
task::task(unsigned char* pData, MV_FRAME_OUT_INFO* pFrameInfo, testQT* pCam, int camera_number, uchar photo_number)
{
	QStartCount = initTime();//初始化时间

	
	this->pDlg = pCam;//窗口类句柄
	this->camera_number = camera_number;
	this->photo_number = photo_number;

	if (!pDlg->halconWindowHandleInit)
	{
		qDebug() << "hanlcon windows not init" << endl;
		return;
	}
	BoolInitHalconWindow = true;//窗口已初始化
	//initHalconWindowHandle();//初始化halcon窗口
	//pFrameInfoTask = { 0 };
	pFrameInfoTask = (MV_FRAME_OUT_INFO *)malloc(sizeof(MV_FRAME_OUT_INFO));//分配空间
	memcpy(pFrameInfoTask, pFrameInfo, sizeof(MV_FRAME_OUT_INFO));

	pDataTask = (unsigned char*)malloc(pDlg->picDataValue[camera_number]);//手动分配图像存储堆
	//memcpy(pDataTask, pData, pCamTask->picDataValue[camera_number]);//复制图像到堆
	//pDataTask=new unsigned char[pCamTask->picDataValue[camera_number]];
	memcpy(pDataTask, pData, pDlg->picDataValue[camera_number]);//复制图像到堆
	bool getGrayImage = getGrayHImageFromRGBData(pDataTask, global_gray_image);
	
	//bool getGrayImage = getOneOfRGBHImageFromRGBData(pDataTask, 3, global_gray_image);
	if (getGrayImage)
	{
		//uint result = whiteCigDarkLightCircleFilterProcess();
		uint result = 0;//模拟无故障
		qDebug() << "result" << result << endl;
		
		
		if (result > 1000)
		{
			return;
		}
		pDlg->checkCigNumber += 1;//烟支检验数+1
		if (result & 0x00000FFF)//第一支烟检测有问题
		{
			writeIOCard();//剔除信号
			pDlg->ngCigNumber += 1;//烟支剔除数+1
			//获取矩形度和凸包度，并显示
			QString picFileName = "";
			QString picAddriess = "D:\\test1\\";
			QDateTime dateTime = QDateTime::currentDateTime();
			QString dateTimeS = dateTime.toString("yyyyMMddhhmmsszzz");
			QString checkTime = dateTime.toString("yyyy-MM-dd hh:mm:ss");
			QString resultS = QString::number(result);
			QString nowBanciNumber = "1";
			//picFileName =QString::number(camera_number) + "_" + QString::number(photo_number) + "_" + dateTimeS + ".jpg";
			picFileName = "1_1_" + dateTimeS + "_" + resultS + ".jpg";
			saveNGImageByClass(picAddriess, picFileName, result);
			
		}
		expendTime = getExpendTime(QStartCount);
		qDebug() << "dealtime expend" << expendTime << endl;
		if (pDlg->nowPictureNumber >= pDlg->picShowInterval)
		{
			showImage();//显示
			pDlg->nowPictureNumber = 0;
		}
		pDlg->nowPictureNumber++;//显示图像number++
		float tempRate= (float)(pDlg->checkCigNumber - pDlg->ngCigNumber) / (float)pDlg->checkCigNumber;
		pDlg->qualifiedRate = ((float)((int)((tempRate + 0.005) * 100))) / 100;
		//if (result)
		//{
		//	QString QTs = "D:/test/";
		//	//saveNGImage(QTs);
		//}
	}
	
}

task::~task()
{
	if (BoolInitHalconWindow)//正常过程
	{
		if (!testPic)
		{
			free(pDataTask);
			if (pDataTask)
				pDataTask = NULL;
		}


		free(pFrameInfoTask);
		pFrameInfoTask = NULL;
	}
	
}
void task::one_camera_hardware_save_image_task()
{
	int picHeght = pFrameInfoTask->nHeight;
	int picWidth = pFrameInfoTask->nWidth;
	unsigned char* dataRed = new unsigned char[picWidth * picHeght];
	unsigned char* dataGreen = new unsigned char[picWidth * picHeght];
	unsigned char* dataBlue = new unsigned char[picWidth * picHeght];
	//unsigned char* data = new unsigned char[picWidth * picHeght * 3];

	//memcpy(data, pDataTask, picWidth * picHeght * 3);

	for (int i = 0; i < picWidth * picHeght; i++)
	{
		dataRed[i] = pDataTask[3 * i];
		dataGreen[i] = pDataTask[3 * i + 1];
		dataBlue[i] = pDataTask[3 * i + 2];
	}
	GenImage3(&global_gray_image, "byte", picWidth, picHeght, (Hlong)(dataRed), (Hlong)(dataGreen), (Hlong)(dataBlue));
	QString QTs = "D:/test/";
	QDateTime dateTime = QDateTime::currentDateTime();
	QString dateTimeS = dateTime.toString("MMddhhmmsszzz");
	QTs = QTs + QString::number(camera_number) + "_" + QString::number(photo_number) + "_" + dateTimeS + ".jpg";

	HTuple hs = QTs.toStdString().c_str();
	WriteImage(global_gray_image, "jpg", 0, hs);
	//内存释放
	//delete data;
	delete dataRed;
	delete dataGreen;
	delete dataBlue;

	//Mat originalReadRGBMat;//数据读入
	//originalReadRGBMat = Mat(pFrameInfoTask->nHeight, pFrameInfoTask->nWidth, CV_8UC3, (uchar*)pDataTask);
	////Mat readRGBMatClone = originalReadRGBMat.clone();
	////int ii = 0;
	////Mat cutOriginalRGBMat;
	////originalReadRGBMat(Rect(pCamTask->pic1RectX, pCamTask->pic1RectY, pCamTask->pic1RectWidth, pCamTask->pic1RectHeight)).copyTo(cutOriginalRGBMat);
	//Mat cutOriginalGrayMat;//原始灰度图
	//cvtColor(originalReadRGBMat, cutOriginalGrayMat, COLOR_RGB2GRAY);
	//QDateTime dateTime = QDateTime::currentDateTime();
	//QString dateTimeS = dateTime.toString("MMddhhmmsszzz");
	//QString QTs = "D:/test_pictures/";
	//QTs = QTs+QString::number(camera_number)+"_"+ dateTimeS +".jpg";
	//std::string s=QTs.toStdString();
	///*namedWindow("111", WINDOW_AUTOSIZE);
	//imshow("111", originalReadRGBMat);
	//waitKey(0);*/
	//imwrite(s.c_str(), cutOriginalGrayMat);
}
uint task::whiteCigDarkLightCircleFilterProcess()//处理过程：硬红
{
	
	HObject ho_Regions, ho_RegionOpening, ho_ConnectedRegions, ho_SelectedRegions, ho_Rectangle1, ho_GrayImage, ho_ImageReduced1, ho_Region, ho_Region1Closing, ho_SelectedRectangularityRegions;
	HObject ho_RegionComplement, ho_ConnectedRegionComplement, ho_SelectedResionsComplement, ho_RectangleNG;
	HTuple hv_Number,hv_Row, hv_Column, hv_Phi, hv_Length1, hv_Length2, hv_RectangleNumber, hv_Rectangularity, hv_Convexity, hv_textRowCount, hv_smallestCheckArea, hv_NumberNG, hv_RowNG1, hv_ColumnNG1;
	HTuple hv_RowNG2, hv_ColumnNG2,hv_Index1;
	int cigNumber = 0;//识别出的烟支数量
	bool cigUpCheck = false;//上烟检验
	bool cigDownCheck = false;//下烟检验
	uint result = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格
	//一般二值化效果更精确
	Threshold(global_gray_image, &ho_Regions, para.all_gray_thr_min, para.all_gray_thr_max);
	/*showEffect(ho_Regions);
	return 0;*/
	OpeningRectangle1(ho_Regions, &ho_RegionOpening, para.open_width, para.open_height);
	Connection(ho_RegionOpening, &ho_ConnectedRegions);
	SelectShape(ho_ConnectedRegions, &ho_SelectedRegions, "area", "and", para.cig_area_min, 100000);
	CountObj(ho_SelectedRegions, &hv_Number);//获取烟支数量

	expendTime = getExpendTime(QStartCount);
	//qDebug() << "task getCigNumber expend" << expendTime << endl;
	/*showEffect(ho_SelectedRegions);
	return 0;*/
	//int hv_number_int = hv_Number[0].I();//调试用
	if (hv_Number[0].I() == 0)//无烟
	{
		return Err_NotFindCig;
	}
	if (hv_Number[0].I() == 1)//一支烟，判断上下，缺陷识别或者检验
	{
		cigNumber = 1;
	}
	if (hv_Number[0].I() == 2)//两支烟，判断上下，分别识别和检验
	{
		cigNumber = 2;
	}
	if (hv_Number[0].I() > 2)//大于两支烟，直接剔除
	{
		return Err_FindMoreThan2Cig;
	}

	SmallestRectangle2(ho_SelectedRegions, &hv_Row, &hv_Column, &hv_Phi, &hv_Length1, &hv_Length2);
	/*
		phi:长边与X轴的夹角-1.5707* 到1.5707*，X轴算起逆时针角度为正，顺时针角度为负
	*/
	
	if (cigNumber == 1)//一支烟
	{
		if (hv_Row[0] < pFrameInfoTask->nHeight / 2)//上半区，检测
		{
			//第一支检验标志
			cigUpCheck = true;
		}
		if (hv_Row[0] >= pFrameInfoTask->nHeight / 2)//下半区，验证
		{
			if (para.checkCig2 == 1)
			{
				//第二支烟检验标志
				cigDownCheck = true;
			}
		}
	}
	//qDebug() << "cigNumber :" << cigNumber <<endl;
	if (cigNumber == 2)//两支烟
	{
		if ((hv_Row[0] < pFrameInfoTask->nHeight / 2 && hv_Row[1] < pFrameInfoTask->nHeight / 2)||(hv_Row[0] > pFrameInfoTask->nHeight / 2 && hv_Row[1] > pFrameInfoTask->nHeight / 2))//都在上半区或者下半区，剔除
		{
			return Err_SameSide2Cig;
		}
		//第一支检验标志
		cigUpCheck = true;
		if (para.checkCig2 == 1)
		{
			//第二支烟检验标志
			cigDownCheck = true;
		}
	}
	if (cigUpCheck)//检验上烟
	{
		int regionNumber = 0;//区域中目标在第几个
		//算法流程
		GenRectangle2(&ho_Rectangle1, HTuple(hv_Row[regionNumber]), HTuple(hv_Column[regionNumber]),
			HTuple(hv_Phi[regionNumber]), HTuple(hv_Length1[regionNumber]), HTuple(hv_Length2[regionNumber]));
		//截取图片
		ReduceDomain(global_gray_image, ho_Rectangle1, &ho_ImageReduced1);
		/*showEffect(ho_ImageReduced1);
		return 0;*/
		//切割图像1二值化
		Threshold(ho_ImageReduced1, &ho_Region, para.cig1_thr, 255);
		
		ClosingRectangle1(ho_Region, &ho_Region1Closing, 5, 5);
		//判断图像1的矩形度，如果矩形度不够直接标记
		SelectShape(ho_Region1Closing, &ho_SelectedRectangularityRegions, (HTuple("convexity").Append("rectangularity")),
			"and", (HTuple(0.9).Append(0.95)), (HTuple(2).Append(2)));

		CountObj(ho_SelectedRectangularityRegions, &hv_RectangleNumber);

		SetColor(pDlg->hv_WindowHandle1, "red");
		SetDraw(pDlg->hv_WindowHandle1, "margin");
		SetLineWidth(pDlg->hv_WindowHandle1, 3);
		//}
		//if (0 != (int(hv_RectangleNumber == 0)))
		//expendTime = getExpendTime(QStartCount);
		//qDebug() << "task checkShapeAndConve expend" << expendTime << endl;
		if (hv_RectangleNumber.I() == 0)
		{
			Rectangularity(ho_Region1Closing, &hv_Rectangularity);
			Convexity(ho_Region1Closing, &hv_Convexity);

			result = result|0x00000001;//最低位置1，矩形度和凸包度不合格
			
			HTuple hv_Convexity_length, hv_Rectangularity_length;
			TupleLength(hv_Convexity, &hv_Convexity_length);
			TupleLength(hv_Rectangularity, &hv_Rectangularity_length);
		
			//存数据库用，有缺陷
			/*int temp1 = hv_Rectangularity_length.I();
			int temp2 = hv_Convexity_length.I();
			if (hv_Rectangularity_length.I() > 0 && hv_Convexity_length.I()>0)
			{
				if (temp1 == 1 && temp2 == 1)
				{
					CigNGDetail ngStu;
					ngStu.noclassID = 1;
					ngStu.value1 = hv_Rectangularity[0];
					ngStu.value2 = hv_Convexity[0];
					ngList.append(ngStu);
				}
				
			}*/
			
		}
		else
		{
			//烟内孔洞判断
			hv_smallestCheckArea = (HTuple)para.smallestCheckArea;
			//反选区域
			Complement(ho_Region, &ho_RegionComplement);
			Connection(ho_RegionComplement, &ho_ConnectedRegionComplement);
			SelectShape(ho_ConnectedRegionComplement, &ho_SelectedResionsComplement,
			(HTuple("area").Append("column1")), "and", hv_smallestCheckArea.TupleConcat(HTuple(hv_Column[regionNumber]) - HTuple(hv_Length1[regionNumber])),
			((HTuple(hv_Length1[regionNumber])* HTuple(hv_Length2[regionNumber])) * 2).TupleConcat((HTuple(hv_Column[regionNumber]) + HTuple(hv_Length1[regionNumber])) - 100));
	
			CountObj(ho_SelectedResionsComplement, &hv_NumberNG);
			//expendTime = getExpendTime(QStartCount);
			//qDebug() << "task checkCigHoles expend" << expendTime << endl;

			int NGNumber=hv_NumberNG.I();
			if (0 != (int(hv_NumberNG > 0)))
			{

				HTuple hv_area, hv_row, hv_column;
				result = result | 0x00000002;//第二位置1，有内部缺陷
				AreaCenter(ho_SelectedResionsComplement, &hv_area, &hv_row, &hv_column);
				
				for (int i = 0; i < NGNumber; i++)
				{
					CigNGDetail ngStu;
					ngStu.noclassID = "2";
					double temp = hv_area[i].D();
					temp = temp / 3.14;
					temp = sqrt(temp);
					if (temp < para.smallestDrawR)
					{
						temp = para.smallestDrawR;//最小半径
					}
					ngStu.value1 = QString::number((int)hv_row[i].D());
					ngStu.value2 = QString::number((int)hv_column[i].D());
					ngStu.value3 = QString::number((int)temp);
					ngList.append(ngStu);
				}
			
			}
			//搭口缺陷判断
			HObject ho_div_region, ho_div_image,ho_div_thr_region,ho_gen_region,ho_diff_region,ho_diff_region_connection,ho_div_result;
			HTuple hv_canny_region_left_up_Row, hv_canny_region_left_up_Column, hv_canny_region_right_down_Row, hv_canny_region_right_down_Column;
			HTuple hv_out_Row, hv_out_Column, hv_out_phi, hv_out_length1, hv_out_length2,hv_div_result_number;
			hv_canny_region_left_up_Row = 0;
			hv_canny_region_left_up_Column = para.divding_x1;
			hv_canny_region_right_down_Row = pFrameInfoTask->nHeight/2;
			hv_canny_region_right_down_Column = para.divding_x2;
			GenRectangle1(&ho_div_region, hv_canny_region_left_up_Row, hv_canny_region_left_up_Column, hv_canny_region_right_down_Row, hv_canny_region_right_down_Column);
			ReduceDomain(global_gray_image, ho_div_region, &ho_div_image);
			Threshold(ho_div_image, &ho_div_thr_region, para.div_thr, 255);
			SmallestRectangle2(ho_div_thr_region, &hv_out_Row, &hv_out_Column, &hv_out_phi, &hv_out_length1, &hv_out_length2);
			GenRectangle2(&ho_gen_region, hv_out_Row, hv_out_Column, hv_out_phi, hv_out_length1, hv_out_length2);
			Difference(ho_gen_region, ho_div_thr_region, &ho_diff_region);
			Connection(ho_diff_region, &ho_diff_region_connection);
			SelectShape(ho_diff_region_connection, &ho_div_result, "area", "and", para.div_diff_area,9999);
			CountObj(ho_div_result, &hv_div_result_number);
			if (0 != (int(hv_div_result_number > 0)))//搭口缺陷
			{
				result = result | 0x00000004;//第3位置1，有搭口缺陷
			}
		}
	}
	if (cigDownCheck)//检验下烟
	{
		int regionNumber = 0;//区域中目标在第几个
		if (cigNumber == 2)
		{
			regionNumber = 1;//两支烟时取下面那支
		}
		//算法流程
	}
	
	return result;
}
uint task::whiteCigWhiteFilterProcess()
{
	HObject ho_all_thr_region, ho_all_thr_clo_region,ho_all_connect_regions,ho_fill_regions, ho_cig_SelectedRegions;
	HTuple hv_cig_Number,hv_Row,hv_Column,hv_Phi,hv_Length1,hv_Length2;
	int cigNumber = 0;//识别出的烟支数量
	bool cigUpCheck = false;//上烟检验
	bool cigDownCheck = false;//下烟检验
	uint result = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格
	//一般二值化效果更精确
	VarThreshold(global_gray_image, &ho_all_thr_region, 30, 30, 0.2, 2, "light");
	ClosingRectangle1(ho_all_thr_region, &ho_all_thr_clo_region, 5, 5);
	Connection(ho_all_thr_clo_region, &ho_all_connect_regions);
	FillUp(ho_all_connect_regions, &ho_fill_regions);
	SelectShape(ho_fill_regions, &ho_cig_SelectedRegions, "area", "and", para1.cig_area_min, 100000);
	CountObj(ho_cig_SelectedRegions, &hv_cig_Number);//获取烟支数量
	if (hv_cig_Number[0].I() == 0)//无烟
	{
		return Err_NotFindCig;
	}
	if (hv_cig_Number[0].I() == 1)//一支烟，判断上下，缺陷识别或者检验
	{
		cigNumber = 1;
	}
	if (hv_cig_Number[0].I() == 2)//两支烟，判断上下，分别识别和检验
	{
		cigNumber = 2;
	}
	if (hv_cig_Number[0].I() > 2)//大于两支烟，直接剔除
	{
		return Err_FindMoreThan2Cig;
	}
	SmallestRectangle2(ho_cig_SelectedRegions, &hv_Row, &hv_Column, &hv_Phi, &hv_Length1, &hv_Length2);
	if (cigNumber == 1)//一支烟
	{
		if (hv_Row[0] < pFrameInfoTask->nHeight / 2)//上半区，检测
		{
			//第一支检验标志
			cigUpCheck = true;
		}
		if (hv_Row[0] >= pFrameInfoTask->nHeight / 2)//下半区，验证
		{
			if (para.checkCig2 == 1)
			{
				//第二支烟检验标志
				cigDownCheck = true;
			}
		}
	}
	if (cigNumber == 2)//两支烟
	{
		if ((hv_Row[0] < pFrameInfoTask->nHeight / 2 && hv_Row[1] < pFrameInfoTask->nHeight / 2) || (hv_Row[0] > pFrameInfoTask->nHeight / 2 && hv_Row[1] > pFrameInfoTask->nHeight / 2))//都在上半区或者下半区，剔除
		{
			return Err_SameSide2Cig;
		}
		//第一支检验标志
		cigUpCheck = true;
		if (para.checkCig2 == 1)
		{
			//第二支烟检验标志
			cigDownCheck = true;
		}
	}
	if (cigUpCheck)//检验上烟
	{

	}
}
bool task::saveNGImage(QString dir, QString name) {
	//QString QTs = "D:/test/";
	QString temp = dir + name;
	HTuple hs = temp.toStdString().c_str();
	WriteImage(global_gray_image, "jpg", 0, hs);
	return 1;
}
bool task::saveNGImageByClass(QString dir, QString name,uint result) {
	SetHcppInterfaceStringEncodingIsUtf8(false);
	int wei = 1;//检查缺陷位
	QString saveDir;
	QString tempS;
	HTuple hs;
	bool dirExist=false;
	while (result)
	{
		bool temp = result & 0x00000001;//缺陷位结果
		if (temp)
		{
			switch (wei)
			{
			case 1://烟棒形状缺陷
				saveDir = dir + QStringLiteral("烟棒外形\\");
				createFileDir(saveDir);
				tempS = saveDir + name;
				QStringCNFileNameSaveImage(tempS);
				/*QByteArray =
				hs = tempS.toStdString().c_str();
				WriteImage(global_gray_image, "jpg", 0, hs);*/
				break;
			case 2://烟棒黑点缺陷
				saveDir = dir + QStringLiteral("烟棒黑点\\");
				createFileDir(saveDir);
				tempS = saveDir + name;
				QStringCNFileNameSaveImage(tempS);
				/*hs = tempS.toStdString().c_str();
				WriteImage(global_gray_image, "jpg", 0, hs);*/
				break;
			case 3://搭口缺陷
				saveDir = dir + QStringLiteral("搭口\\");
				createFileDir(saveDir);
				tempS = saveDir + name;
				QStringCNFileNameSaveImage(tempS);
				/*hs = tempS.toStdString().c_str();
				WriteImage(global_gray_image, "jpg", 0, hs);*/
				break;
			case 4:
				break;
			case 5:
				break;
			case 6:
				break;
			case 7:
				break;
			case 8:
				break;
			case 9:
				break;
			case 10:
				break;
			case 11:
				break;
			case 12:
				break;
			case 13:
				break;
			case 14:
				break;
			case 15:
				break;
			case 16:
				break;
			default:
				break;
			}
		}
		result = result >> 1;
		wei += 1;
	}
	return 1;
}
//bool task::addMasterMySQL(QString dir, QString name, QString checkTime, QString banciID) {
//	//用主线程的定义可能出现竞争关系
//	
//	////QSqlQuery que(pDlg->db);
//	//bool result = pDlg->query->prepare(sql);
//	//bool result1 = pDlg->query->exec();
//	//事务
//	/*expendTime = getExpendTime(QStartCount);
//	qDebug() << "task beforeConnetDB expend" << expendTime << endl;
//	if(QSqlDatabase::contains("qt_sql_default_connection"))
//		db = QSqlDatabase::database("qt_sql_default_connection");
//	else
//		db = QSqlDatabase::addDatabase("QODBC");
//
//	db.setHostName("127.0.0.1");
//	db.setPort(3306);
//	db.setDatabaseName("mysql");
//	db.setUserName("root");
//	db.setPassword("root");
//	query = new QSqlQuery(db);
//	bool ok = db.open();
//	expendTime = getExpendTime(QStartCount);
//	qDebug() << "task finishConnetDB expend" << expendTime << endl;*/
//	QTime timeNow = QTime::currentTime();
//	timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
//	expendTime = getExpendTime(QStartCount);
//	qDebug() << "task beginInertMaster expend" << expendTime << endl;
//	pDlg->addMasterMySQL(timestamp,dir,name,checkTime,banciID);
//	expendTime = getExpendTime(QStartCount);
//	qDebug() << "task finishInertMaster expend" << expendTime << endl;
//	//if (ok)
//	//{
//	//	
//	//	QTime timeNow = QTime::currentTime();
//	//	timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
//	//	
//	//	dir=dir.replace("\\","\\\\");
//	//	QString sql1 = QString("insert into ngcigmaster (ngCigTimeStamp,picFileName,picAddress,checkTime,belowBanciID) values ('%1','%2','%3','%4','%5') ").arg(timestamp).arg(name).arg(dir).arg(checkTime).arg(banciID);
//	//	
//	//	bool result1 = query->exec(sql1);
//	//	if (!result1)//主记录已写入
//	//	{
//	//		return 0;//失败
//	//	}
//	//	expendTime = getExpendTime(QStartCount);
//	//	qDebug() << "task finishInertMaster expend" << expendTime << endl;
//	//}
//	
//
//	/*if (db.transaction())
//	{
//		query = new QSqlQuery();
//		query->exec(sql1);
//		query->exec(sql2);
//		if (!db.commit())
//		{
//			db.rollback();
//			return 0;
//		}
//	}*/
//	return 1;
//}
////bool task::addDetailMySQL(QString classID, QString value1, QString value2, QString value3, QString value4)
//bool task::addDetailMySQL()
//{
//	for (int i = 0; i < ngList.size(); i++)
//	{
//		expendTime = getExpendTime(QStartCount);
//		qDebug() << "task beforeInertDetail expend" << expendTime << endl;
//		CigNGDetail ng = (CigNGDetail)ngList.at(i);
//
//		QString sql1 = QString("insert into ngcigdetail (ngCigTimeStamp,ngclassID,value1,value2,value3,value4) values ('%1','%2','%3','%4','%5','%6') ").arg(timestamp).arg(ng.noclassID).arg(ng.value1).arg(ng.value2).arg(ng.value3).arg(ng.value4);
//		bool result1 = query->exec(sql1);
//		expendTime = getExpendTime(QStartCount);
//		qDebug() << "task finishInertDetail expend" << expendTime << endl;
//	}
//	//QString sql1 = QString("insert into ngcigdetail (ngCigTimeStamp,noclassID,value1,value2,value3,value4) values ('%1','%2','%3','%4','%5','%6') ").arg(timestamp).arg(classID).arg(value1).arg(value2).arg(value3).arg(value4);
//	//QString sql1 = QString("insert into ngcigdetail (ngCigTimeStamp,noclassID,value1,value2,value3,value4) values ('%1','%2','%3','%4','%5','%6') ").arg(timestamp).arg(classID).arg(value1).arg(value2).arg(value3).arg(value4);
//	//bool result1 = query->exec(sql1);
//	return 1;
//}

bool task::initHalconWindowHandle()
{
	expendTime = getExpendTime(QStartCount);
	qDebug() << "task beginInitHalconWindows expend" << expendTime << endl;
	bool temp = pDlg->halconWindowHandleInit;
	if (!pDlg->halconWindowHandleInit)
	{

		/*pDlg->initHalconShowWindow();
		expendTime = getExpendTime(QStartCount);
		qDebug() << "task endInitHalconWindows expend" << expendTime << endl;*/
		return true;
	}
	expendTime = getExpendTime(QStartCount);
		qDebug() << "task checkShapeAndConve expend" << expendTime << endl;
	return false;
}
bool task::showImage() {
	//GetImageSize(ho_Image, &hv_Width, &hv_Height);
	//Hlong winId = (Hlong)ui.picture1->winId();
	//int labHeight = (Hlong)ui.picture1->height();
	//int labWidth = (Hlong)ui.picture1->width();
	//OpenWindow(0, 0, (Hlong)labWidth, (Hlong)labHeight, winId, "visible", "", &hv_WindowHandle);
	if (pDlg->nowShowPicNumber == pDlg->picShowInterval)
	{
		pDlg->nowShowPicNumber = 0;//复位到第一张
	}
	if (pDlg->nowShowPicNumber == 0)
	{
		SetPart(pDlg->hv_WindowHandle1, 0, 0, pFrameInfoTask->nHeight, pFrameInfoTask->nWidth);
		DispObj(global_gray_image, pDlg->hv_WindowHandle1);
	}
	pDlg->nowShowPicNumber++;
	
	return 1;
}
bool task::showEffect(HObject image) {

	SetPart(pDlg->hv_WindowHandle1, 0, 0, pFrameInfoTask->nHeight, pFrameInfoTask->nWidth);
	DispObj(image, pDlg->hv_WindowHandle1);
	return 1;
}
bool task::sendResultToIOCard() {
	return 1;
}
bool task::getRGBHImageFromRGBData(unsigned char* in_pData, HObject& out_image)
{
	int picHeght = pFrameInfoTask->nHeight;
	int picWidth = pFrameInfoTask->nWidth;
	unsigned char* dataRed = new unsigned char[picWidth * picHeght];
	unsigned char* dataGreen = new unsigned char[picWidth * picHeght];
	unsigned char* dataBlue = new unsigned char[picWidth * picHeght];
	/*unsigned char* data = new unsigned char[picWidth * picHeght * 3];

	memcpy(data, in_pData, picWidth * picHeght * 3);*/

	for (int i = 0; i < pDlg->picDataValue[camera_number]; i++)
	{
		dataRed[i] = pDataTask[3 * i];
		dataGreen[i] = pDataTask[3 * i + 1];
		dataBlue[i] = pDataTask[3 * i + 2];
	}
	GenImage3(&out_image, "byte", picWidth, picHeght, (Hlong)(dataRed), (Hlong)(dataGreen), (Hlong)(dataBlue));
	//内存释放
	//delete data;
	delete dataRed;
	delete dataGreen;
	delete dataBlue;
	return true;
}
bool task::getOneOfRGBHImageFromRGBData(unsigned char* in_pData, int RGB_Number, HObject& out_image)
{
	int picHeght = pFrameInfoTask->nHeight;
	int picWidth = pFrameInfoTask->nWidth;
	unsigned char* dataRed = new unsigned char[picWidth * picHeght];
	unsigned char* dataGreen = new unsigned char[picWidth * picHeght];
	unsigned char* dataBlue = new unsigned char[picWidth * picHeght];
	/*unsigned char* data = new unsigned char[picWidth * picHeght * 3];

	memcpy(data, in_pData, picWidth * picHeght * 3);*/

	for (int i = 0; i < picWidth * picHeght; i++)
	{
		dataRed[i] = pDataTask[3 * i];
		dataGreen[i] = pDataTask[3 * i + 1];
		dataBlue[i] = pDataTask[3 * i + 2];
	}
	switch (RGB_Number)
	{
	case 1://R红色
		GenImage1(&out_image, "byte", picWidth, picHeght, (Hlong)(dataRed));
		break;
	case 2://G绿色
		GenImage1(&out_image, "byte", picWidth, picHeght, (Hlong)(dataGreen));
		break;
	case 3://B蓝色
		GenImage1(&out_image, "byte", picWidth, picHeght, (Hlong)(dataBlue));
		break;
	}

	//内存释放
	//delete data;
	delete dataRed;
	delete dataGreen;
	delete dataBlue;
	return true;
}
bool task::getGrayHImageFromRGBData(unsigned char* in_pData, HObject& out_image)//图像需要是RGB
{
	int picHeght = pFrameInfoTask->nHeight;
	int picWidth = pFrameInfoTask->nWidth;
	unsigned char* dataGray = new unsigned char[picWidth * picHeght];

	/*unsigned char* data = new unsigned char[picWidth * picHeght * 3];

	memcpy(data, in_pData, picWidth * picHeght * 3);*/

	for (int i = 0; i < picWidth * picHeght; i++)
	{
		dataGray[i] = (pDataTask[3 * i])*0.299 + pDataTask[3 * i + 1]*0.587+ pDataTask[3 * i + 2]*0.114;

	}
	GenImage1(&out_image, "byte", picWidth, picHeght, (Hlong)(dataGray));
	//内存释放
	//delete data;
	delete dataGray;
	return true;
}
//bool task::getCigaretteNumberAndPositon(HObject &in_img, HTuple &out_number, HTuple &out_row, HTuple& out_col, HTuple& out_Phi, HTuple& out_width, HTuple& out_height)//获得烟支的定位信息
bool task::getCigaretteNumberAndPositon(HObject& in_img,int gray_thr_min, int gray_thr_max , int close_width, int close_height,int area_min, HObject& ho_SelectedRegions)//获得烟支的定位信息
{
	HObject ho_Regions, ho_RegionOpening, ho_ConnectedRegions;
	//HTuple hv_Number, hv_Row, hv_Column, hv_Phi, hv_Length1, hv_Length2;
	//一般二值化效果更精确
	Threshold(in_img, &ho_Regions, gray_thr_min, gray_thr_max);
	
	//自动二值化加入高斯滤波参数，细节更多
	//auto_threshold (GrayImage, Regions, 10.0)

	//动态二值化，有效果，考虑速度问题
	//mean_image (GrayImage, GrayImageMean, 5, 5)
	//dyn_threshold (GrayImage, GrayImageMean, RegionDynThresh, 12, 'light')

	OpeningRectangle1(ho_Regions, &ho_RegionOpening, close_width, close_height);
	Connection(ho_RegionOpening, &ho_ConnectedRegions);
	SelectShape(ho_ConnectedRegions, &ho_SelectedRegions, "area", "and", area_min, 100000);

	//CountObj(ho_SelectedRegions, &out_number);//烟支数量
	//if (out_number.I() == 0)
	//{
	//	return false;//没有找到烟支
	//}
	////最小外接矩形
	//SmallestRectangle2(ho_SelectedRegions, &out_row, &out_col, &out_Phi, &out_width, &out_height);

	return true;
}
int task::getXCigaretteGrayImage(HObject &in_img, int in_cigNumber, HTuple& out_row, HTuple& out_col, HTuple& out_Phi, HTuple& out_length1, HTuple& out_length2, HObject& out_img)//获得第X支烟棒的图片
{
	HObject ho_Rectangle;
	//矩形区域
	if (in_cigNumber == 0||in_cigNumber>2) {
		//非法的请求
		return 1;
	}
	if (out_row.TupleLength()<in_cigNumber) {
		//数组数量不够
		return 2;
	}
	GenRectangle2(&ho_Rectangle, HTuple(out_row[in_cigNumber]), HTuple(out_col[in_cigNumber]), HTuple(out_Phi[in_cigNumber]), HTuple(out_length1[in_cigNumber]), HTuple(out_length2[in_cigNumber]));
	
	ReduceDomain(in_img, ho_Rectangle, &out_img);
	
	return 0;//没错
}
int task::getCigConAndRectRightNumber(HObject& in_img, int in_gray_thr_min, int in_gray_thr_max, int in_close_width, int in_close_height) //获得烟支图像凸度与矩形度合格数量
{
	HObject ho_Region, ho_RegionClosing, ho_SelectedRectangularityRegions;
	HTuple hv_RectangleNumber;
	Threshold(in_img, &ho_Region, in_gray_thr_min, in_gray_thr_max);
	//Threshold(in_img, &ho_Region, 60, 255);
	ClosingRectangle1(ho_Region, &ho_RegionClosing, in_close_width, in_close_height);
	//筛选出凸度和矩形度达标的区域
	SelectShape(ho_RegionClosing, &ho_SelectedRectangularityRegions, (HTuple("convexity").Append("rectangularity")),"and", (HTuple(0.9).Append(0.95)), (HTuple(2).Append(2)));
	CountObj(ho_SelectedRectangularityRegions, &hv_RectangleNumber);
	return hv_RectangleNumber.I();
}
int task::checkCigWidthAndHeight(HTuple& in_width, HTuple& in_height, int in_cigNumber, int in_width_threshold, int in_height_threshold, int& out_width, int& out_height)//检查宽度和高度
{
	if (in_width.TupleLength() == 0)//无烟支信息
	{
		out_width = 0;
		out_height = 0;
		return 1;
	}
	if (in_width.TupleLength() < in_cigNumber)//输入烟支数量信息错误
	{
		out_width = 0;
		out_height = 0;
		return 2;
	}
	out_width = in_width[in_cigNumber];
	out_height = in_height[in_cigNumber];
	if (in_width[in_cigNumber] < in_width_threshold)//烟支长度不达标
	{

		return 3;

	}
	if (in_height[in_cigNumber] < in_height_threshold)//烟支宽度不达标
	{

		return 4;

	}

	return 0;//正常
	
}

//bool task::checkDarkPointsOnCigarette(HObject& ho_SelectedRegions, HTuple& out_row, HTuple& out_col, HTuple& out_Phi, HTuple& out_width, HTuple& out_height, HTuple& in_darkNGOnCigOriRow, HTuple& in_darkNGOnCigOriCol, HTuple& in_darkNGOnCigCorRow, HTuple& in_darkNGOnCigCorCol, int smallestCheckArea)
//{
//	HObject  ho_RegionComplement, ho_ConnectedRegionComplement, ho_SelectedResionsComplement;
//	HTuple hv_smallestCheckArea, hv_Column, hv_Length1, hv_Length2, hv_NumberNG;
//	Complement(region, &ho_RegionComplement);
//	Connection(ho_RegionComplement, &ho_ConnectedRegionComplement);
//	SelectShape(ho_ConnectedRegionComplement, &ho_SelectedResionsComplement,
//		(HTuple("area").Append("column1")), "and", hv_smallestCheckArea.TupleConcat(HTuple(hv_Column[0]) - HTuple(hv_Length1[0])),
//		((HTuple(hv_Length1[0]) * HTuple(hv_Length2[0])) * 2).TupleConcat((HTuple(hv_Column[0]) + HTuple(hv_Length1[0])) - 100));
//	CountObj(ho_SelectedResionsComplement, &hv_NumberNG);
//	return 0;
//}

void task::drawPictureOnQlabel() {
	//QImage img3((const unsigned char*)(cutOriginalDisplayRGBMat.data), cutOriginalDisplayRGBMat.cols, cutOriginalDisplayRGBMat.rows, cutOriginalDisplayRGBMat.cols * cutOriginalDisplayRGBMat.channels(), QImage::Format_RGB888);
	//img3 = img3.scaled(pCamTask->ui.picture3->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	//pCamTask->ui.picture3->setPixmap(QPixmap::fromImage(img3));//显示
}
//线程真正执行的内容
void task::run()
{
	if (pDlg->pool.activeThreadCount() < pDlg->pool.maxThreadCount()-2)
	{
		//one_camera_hardware_save_image_task();
	}

	return;

 //   Mat originalReadRGBMat;//数据读入
	//originalReadRGBMat = cv::Mat(pFrameInfoTask->nHeight, pFrameInfoTask->nWidth, CV_8UC3, (uchar*)pDataTask);

 //   Mat cutOriginalRGBMat;
	//Mat cutOriginalDisplayRGBMat;//用于显示
	//originalReadRGBMat(Rect(pCamTask->pic1RectX, pCamTask->pic1RectY, pCamTask->pic1RectWidth, pCamTask->pic1RectHeight)).copyTo(cutOriginalRGBMat);
	//Mat cutOriginalGrayMat;//原始灰度图
	//cutOriginalDisplayRGBMat = cutOriginalRGBMat.clone();
	//cvtColor(cutOriginalRGBMat, cutOriginalGrayMat, CV_RGB2GRAY);
	//Mat cutOriginalGrayDisplayMat;//灰度图，用于显示
	//cvtColor(cutOriginalGrayMat, cutOriginalGrayDisplayMat, CV_GRAY2RGB);
	//Mat mask;
	//std::vector< cv::Point> rectPoints;
	////整体烟支mask
	//
	///*if (getAllMask(cutOriginalGrayMat, mask, rectPoints,pCamTask) != true)
	//{
	//	return;
	//}
	//for (int i = 0; i < rectPoints.size();i++) {
	//	circle(cutOriginalDisplayRGBMat, rectPoints[i], 5, Scalar(255, 0, 0), 3, 8);
	//}*/
	//
	//
	//Mat cutOriginalGrayMat1;//灰度边界判定总图
	//Mat cutOriginalGrayThresholdMat;//灰度边界判定分割图
	//Mat thresholdMat;//边界判定结果返回灰度图
	//
	//Mat wrongDetectMat;//缺陷检测图
	//int lineType = 0;//边界类型：0：纵向 ；1：横向
	//int dividingThreshold = 0;//边界查找二值化阈值0-255
	//cutOriginalGrayMat1 = cutOriginalGrayMat.clone();
	//Rect regionRect;
	//Vec4i filterUpLine;//滤嘴宽度上界线段
	//Vec4i filterDownLine;//滤嘴宽度下界线段
	////滤棒端宽度与定位																																																	  //滤棒端线定位
	//{
	//	lineType = 1;//横向
	//	
	//	int width = 0;
	//	regionRect = Rect(pCamTask->filterWidthRectX, pCamTask->filterWidthRectY, pCamTask->filterWidthRectWidth, pCamTask->filterWidthRectHeight);
	//	cutOriginalGrayThresholdMat = cutOriginalGrayMat1(regionRect);
	//	dividingThreshold = pCamTask->filterWidthRectThreshold;
	//	if (getUpDownEdgeAndWidth(cutOriginalGrayThresholdMat, dividingThreshold, width, thresholdMat, filterUpLine, filterDownLine, pCamTask) != true)
	//	{
	//		return;
	//	}
	//	//Mat thresholdRGBMat;
	//	//thresholdRGBMat = cutOriginalRGBMat(regionRect);
	//	//cvtColor(thresholdMat, thresholdRGBMat, CV_GRAY2RGB);
	//	line(cutOriginalDisplayRGBMat, Point(filterUpLine[0] + regionRect.x, filterUpLine[1] + regionRect.y), Point(filterUpLine[2] + regionRect.x, filterUpLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0
	//	line(cutOriginalDisplayRGBMat, Point(filterDownLine[0] + regionRect.x, filterDownLine[1] + regionRect.y), Point(filterDownLine[2] + regionRect.x, filterDownLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0
	//	drawDashRect(cutOriginalGrayDisplayMat, 5, 3, &regionRect, cv::Scalar(0, 0, 255), 1);
	//}
	//Vec4i cigNearFilterLineUpLine;//上边界线段
	//Vec4i cigNearFilterLineDownLine;//上边界线段
	////烟支近滤棒端宽度与定位																																																	  //滤棒端线定位
	//{
	//	lineType = 1;//横向
	//	
	//	int width = 0;
	//	regionRect = Rect(pCamTask->cigNearFilterWidthRectX, pCamTask->cigNearFilterWidthRectY, pCamTask->cigNearFilterWidthRectWidth, pCamTask->cigNearFilterWidthRectHeight);
	//	cutOriginalGrayThresholdMat = cutOriginalGrayMat1(regionRect);

	//	if (getUpDownEdgeAndWidth(cutOriginalGrayThresholdMat, width, thresholdMat, cigNearFilterLineUpLine, cigNearFilterLineDownLine, pCamTask) != true)
	//	{
	//		return;
	//	}
	//	//Mat thresholdRGBMat;
	//	//thresholdRGBMat = cutOriginalRGBMat(regionRect);
	//	//cvtColor(thresholdMat, thresholdRGBMat, CV_GRAY2RGB);
	//	line(cutOriginalDisplayRGBMat, Point(cigNearFilterLineUpLine[0] + regionRect.x, cigNearFilterLineUpLine[1] + regionRect.y), Point(cigNearFilterLineUpLine[2] + regionRect.x, cigNearFilterLineUpLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0
	//	line(cutOriginalDisplayRGBMat, Point(cigNearFilterLineDownLine[0] + regionRect.x, cigNearFilterLineDownLine[1] + regionRect.y), Point(cigNearFilterLineDownLine[2] + regionRect.x, cigNearFilterLineDownLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0
	//	drawDashRect(cutOriginalGrayDisplayMat, 5, 3, &regionRect, cv::Scalar(0, 0, 255), 1);
	//}
	//Vec4i cigTopUpLine;//上边界线段
	//Vec4i cigTopDownLine;//上边界线段
	////烟支远端宽度与定位																																																	  //滤棒端线定位
	//{
	//	lineType = 1;//横向
	//	
	//	int width = 0;
	//	regionRect = Rect(pCamTask->cigTopWidthRectX, pCamTask->cigTopWidthRectY, pCamTask->cigTopWidthRectWidth, pCamTask->cigTopWidthRectHeight);
	//	cutOriginalGrayThresholdMat = cutOriginalGrayMat1(regionRect);

	//	if (getUpDownEdgeAndWidth(cutOriginalGrayThresholdMat, width, thresholdMat, cigTopUpLine, cigTopDownLine, pCamTask) != true)
	//	{
	//		return;
	//	}
	//	//Mat thresholdRGBMat;
	//	//thresholdRGBMat = cutOriginalRGBMat(regionRect);
	//	//cvtColor(thresholdMat, thresholdRGBMat, CV_GRAY2RGB);
	//	line(cutOriginalDisplayRGBMat, Point(cigTopUpLine[0] + regionRect.x, cigTopUpLine[1] + regionRect.y), Point(cigTopUpLine[2] + regionRect.x, cigTopUpLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0
	//	line(cutOriginalDisplayRGBMat, Point(cigTopDownLine[0] + regionRect.x, cigTopDownLine[1] + regionRect.y), Point(cigTopDownLine[2] + regionRect.x, cigTopDownLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0
	//	drawDashRect(cutOriginalGrayDisplayMat, 5, 3, &regionRect, cv::Scalar(0, 0, 255), 1);
	//}

	//
	//
	////滤棒端定位
	//{
	//	lineType = 0;//纵向
	//	Vec4i dividingLine;//边界线段
	//	dividingThreshold = pCamTask->filterDividingRectThreshold;
	//	regionRect = Rect(pCamTask->filterDividingRectX, pCamTask->filterDividingRectY, pCamTask->filterDividingRectWidth, pCamTask->filterDividingRectHeight);
	//	if (pCamTask->filterVerticalRelative == 1)
	//	{
	//		//Vec4i cigNearFilterLineUpLine;//上边界线段
	//		//Vec4i cigNearFilterLineDownLine;//上边界线段
	//		//regionRect
	//		int yMinTemp = (filterUpLine[1] + filterUpLine[3]) / 2 + pCamTask->filterWidthRectY;
	//		int yMaxTemp = (filterDownLine[1] + filterDownLine[3]) / 2 + pCamTask->filterWidthRectY;
	//		int rectY = yMinTemp + (yMaxTemp - yMinTemp) / 5;
	//		int rectHeight = (yMaxTemp - yMinTemp) * 3 / 5;
	//		if (rectY > pCamTask->pic1RectHeight)
	//		{
	//			rectY = pCamTask->pic1RectHeight / 2;
	//		}
	//		if (rectHeight < 20)
	//		{
	//			rectHeight = 20;
	//		}
	//		regionRect = Rect(pCamTask->filterDividingRectX, rectY, pCamTask->filterDividingRectX, rectHeight);
	//	}
	//	
	//	cutOriginalGrayThresholdMat = cutOriginalGrayMat1(regionRect);

	//	if (getEdge(cutOriginalGrayThresholdMat, lineType, thresholdMat, dividingLine, pCamTask) != true)
	//	{
	//		return;
	//	}
	//	//Mat thresholdRGBMat;
	//	//thresholdRGBMat = cutOriginalRGBMat(regionRect);
	//	//cvtColor(thresholdMat, thresholdRGBMat, CV_GRAY2RGB);
	//	line(cutOriginalDisplayRGBMat, Point(dividingLine[0] + regionRect.x, dividingLine[1] + regionRect.y), Point(dividingLine[2] + regionRect.x, dividingLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0
	//	drawDashRect(cutOriginalGrayDisplayMat, 5, 3, &regionRect, cv::Scalar(0, 0, 255), 1);
	//}

	////卷烟端定位																																																	  //滤棒端线定位
	//{
	//	lineType = 0;//纵向
	//	Vec4i dividingLine;//边界线段
	//	dividingThreshold = pCamTask->cigDividingRectThreshold;
	//	regionRect = Rect(pCamTask->cigDividingRectX, pCamTask->cigDividingRectY, pCamTask->cigDividingRectWidth, pCamTask->cigDividingRectHeight);
	//	if (pCamTask->cigVerticalRelative == 1)//相对宽度查找的Y轴位置
	//	{
	//		//Vec4i cigNearFilterLineUpLine;//上边界线段
	//		//Vec4i cigNearFilterLineDownLine;//上边界线段
	//		//regionRect
	//		int yMinTemp = (cigTopUpLine[1] + cigTopUpLine[3]) / 2 + pCamTask->cigTopWidthRectY;//宽度上边界Y的平均值
	//		int yMaxTemp = (cigTopDownLine[1] + cigTopDownLine[3]) / 2 + pCamTask->cigTopWidthRectY;//宽度下边界Y的平均值
	//		int rectY = yMinTemp + (yMaxTemp - yMinTemp) / 5;//起始位置Y最小 + 1/5的烟宽
	//		int rectHeight = (yMaxTemp - yMinTemp) * 3 / 5;//检测宽度3/5的烟宽，终端位置在烟支的4/5
	//		if (rectY > pCamTask->pic1RectHeight)//起始位置Y不大于1/2的位置
	//		{
	//			rectY = pCamTask->pic1RectHeight / 2;
	//		}
	//		if (rectHeight < 20)//宽度不小于20个像素
	//		{
	//			rectHeight = 20;
	//		}
	//		regionRect = Rect(pCamTask->cigDividingRectX, rectY, pCamTask->cigDividingRectWidth, rectHeight);
	//	}
	//	cutOriginalGrayThresholdMat = cutOriginalGrayMat1(regionRect);

	//	if (getEdge(cutOriginalGrayThresholdMat, lineType, thresholdMat, dividingLine, pCamTask) != true)
	//	{
	//		return;
	//	}
	//	//Mat thresholdRGBMat;
	//	//thresholdRGBMat = cutOriginalRGBMat(regionRect);
	//	//cvtColor(thresholdMat, thresholdRGBMat, CV_GRAY2RGB);
	//	line(cutOriginalDisplayRGBMat, Point(dividingLine[0] + regionRect.x, dividingLine[1] + regionRect.y), Point(dividingLine[2] + regionRect.x, dividingLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0
	//	drawDashRect(cutOriginalGrayDisplayMat, 5, 3, &regionRect, cv::Scalar(0, 0, 255), 1);
	//}
	////滤棒烟支分界线定位
	//Mat thresholdRGBMat;
	//{
	//	lineType = 0;//纵向
	//	Vec4i dividingLine;//边界线段
	//	std::vector<Vec4i> allLines;//所有线
	//	dividingThreshold = pCamTask->pic1DividingThreshold;
	//	regionRect = Rect(pCamTask->pic1DividingX, pCamTask->pic1DividingY, pCamTask->pic1DividingWidth, pCamTask->pic1DividingHeight);
	//	if (pCamTask->filterCigDividingVerticalRelative == 1)//相对宽度查找的Y轴位置
	//	{
	//		//Vec4i cigNearFilterLineUpLine;//上边界线段
	//		//Vec4i cigNearFilterLineDownLine;//上边界线段
	//		//regionRect
	//		int yMinTemp = (cigNearFilterLineUpLine[1] + cigNearFilterLineUpLine[3]) / 2 + pCamTask->cigNearFilterWidthRectY;//宽度上边界Y的平均值
	//		int yMaxTemp = (cigNearFilterLineDownLine[1] + cigNearFilterLineDownLine[3]) / 2 + pCamTask->cigNearFilterWidthRectY;//宽度下边界Y的平均值
	//		int rectY = yMinTemp + (yMaxTemp - yMinTemp) / 5;//起始位置Y最小 + 1/5的烟宽
	//		int rectHeight = (yMaxTemp - yMinTemp) * 3 / 5;//检测宽度3/5的烟宽，终端位置在烟支的4/5
	//		if (rectY > pCamTask->pic1RectHeight)//起始位置Y不大于1/2的位置
	//		{
	//			rectY = pCamTask->pic1RectHeight / 2;
	//		}
	//		if (rectHeight < 20)//宽度不小于20个像素
	//		{
	//			rectHeight = 20;
	//		}
	//		regionRect = Rect(pCamTask->pic1DividingX, rectY, pCamTask->pic1DividingWidth, rectHeight);
	//	}
	//	cutOriginalGrayThresholdMat = cutOriginalGrayMat1(regionRect);

	//	if (getEdge(cutOriginalGrayThresholdMat, dividingThreshold, lineType, thresholdMat, dividingLine, allLines, pCamTask) != true)
	//	{
	//		return;
	//	}
	//	
	//	//thresholdRGBMat = cutOriginalRGBMat(regionRect);
	//	//cvtColor(cutOriginalGrayThresholdMat, thresholdRGBMat, CV_GRAY2RGB);
	//	cvtColor(thresholdMat, thresholdRGBMat, CV_GRAY2RGB);
	//	for (int i = 0; i < allLines.size(); i++)
	//	{
	//		line(thresholdRGBMat, Point(allLines[i][0], allLines[i][1]), Point(allLines[i][2], allLines[i][3]), cv::Scalar(0, 0, 255), 1, 8, 0);
	//	}
	//	line(cutOriginalDisplayRGBMat, Point(dividingLine[0] + regionRect.x, dividingLine[1] + regionRect.y), Point(dividingLine[2] + regionRect.x, dividingLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0	}
	//	//rectangle(cutOriginalGrayDisplayMat, regionRect, cv::Scalar(0, 0, 255), 3, 8, 0);
	//	drawDashRect(cutOriginalGrayDisplayMat, 5, 3, &regionRect, cv::Scalar(0, 0, 255), 1); 
	//	//drawDashRect(cutOriginalDisplayRGBMat, 5, 3, &regionRect, cv::Scalar(0, 255, 0), 1);
	//	//drawDashRect(cutOriginalGrayDisplayMat, 5, 3, &regionRect, cv::Scalar(0, 255, 0), 2);
	//}
	//
	////烟支刺破
	///*
	//{
	//	regionRect = Rect(pCamTask->cigBrokenX, pCamTask->cigBrokenY, pCamTask->cigBrokenRectWidth, pCamTask->cigBrokenRectHeight);
	//	cutOriginalGrayThresholdMat = cutOriginalGrayMat1(regionRect);
	//	if (!findBroken(cutOriginalGrayThresholdMat, thresholdMat, pCamTask))
	//	{
	//		return;
	//	}
	//	drawDashRect(cutOriginalGrayDisplayMat, 5, 3, &regionRect, cv::Scalar(0, 255, 0), 1);
	//}
	//*/
	////卷烟上部定位																																																	  //滤棒端线定位
	////{
	////	lineType = 1;//横向
	////	Vec4i dividingLine;//边界线段
	////	dividingThreshold = pCamTask->cigUpDividingRectThreshold;
	////	regionRect = Rect(pCamTask->cigUpDividingRectX, pCamTask->cigUpDividingRectY, pCamTask->cigUpDividingRectWidth, pCamTask->cigUpDividingRectHeight);
	////	cutOriginalGrayThresholdMat = cutOriginalGrayMat1(regionRect);

	////	if (getEdge(cutOriginalGrayThresholdMat, dividingThreshold, lineType, thresholdMat, dividingLine, pCamTask) != true)
	////	{
	////		return;
	////	}
	////	Mat thresholdRGBMat;
	////	thresholdRGBMat = cutOriginalRGBMat(regionRect);
	////	cvtColor(thresholdMat, thresholdRGBMat, CV_GRAY2RGB);
	////	line(cutOriginalDisplayRGBMat, Point(dividingLine[0] + regionRect.x, dividingLine[1] + regionRect.y), Point(dividingLine[2] + regionRect.x, dividingLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0
	////	drawDashRect(cutOriginalGrayDisplayMat, 5, 3, &regionRect, cv::Scalar(0, 0, 255), 1);
	////}
	//
	//
	//
	////卷烟下部定位																																																	  //滤棒端线定位
	////{
	////	lineType = 1;//横向
	////	Vec4i dividingLine;//边界线段
	////	dividingThreshold = pCamTask->cigDownDividingRectThreshold;
	////	regionRect = Rect(pCamTask->cigDownDividingRectX, pCamTask->cigDownDividingRectY, pCamTask->cigDownDividingRectWidth, pCamTask->cigDownDividingRectHeight);
	////	cutOriginalGrayThresholdMat = cutOriginalGrayMat1(regionRect);

	////	if (getEdge(cutOriginalGrayThresholdMat, dividingThreshold, lineType, thresholdMat, dividingLine, pCamTask) != true)
	////	{
	////		return;
	////	}
	////	Mat thresholdRGBMat;
	////	thresholdRGBMat = cutOriginalRGBMat(regionRect);
	////	cvtColor(thresholdMat, thresholdRGBMat, CV_GRAY2RGB);
	////	line(cutOriginalDisplayRGBMat, Point(dividingLine[0] + regionRect.x, dividingLine[1] + regionRect.y), Point(dividingLine[2] + regionRect.x, dividingLine[3] + regionRect.y), cv::Scalar(0, 0, 255), 2, 8, 0);//图，点1，点2，线色，线宽，连线类型，0
	////	drawDashRect(cutOriginalGrayDisplayMat, 5, 3, &regionRect, cv::Scalar(0, 0, 255), 1);
	////}


	////画边界
	////for (int i = 0; i < rectPoints.size(); i++)
	////{
	////	if (i == rectPoints.size() - 1)
	////	{
	////		line(cutOriginalRGBMat, rectPoints.at(i), rectPoints.at(0), cv::Scalar(255, 0, 0), 2, 8, 0);
	////	}
	////	else
	////	{
	////		line(cutOriginalRGBMat, rectPoints.at(i), rectPoints.at(i + 1), cv::Scalar(255, 0, 0), 2, 8, 0);
	////	}
	////}
	////原始图像显示，图像截取用

	////QImage img1((const unsigned char*)(cutOriginalDisplayRGBMat.data), cutOriginalDisplayRGBMat.cols, cutOriginalDisplayRGBMat.rows, cutOriginalDisplayRGBMat.cols * cutOriginalDisplayRGBMat.channels(), QImage::Format_RGB888);
	////img1 = img1.scaled(pCamTask->ui.originalPicture->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	////pCamTask->ui.picture3->setPixmap(QPixmap::fromImage(img1));//显示

	////QImage img4((const unsigned char*)(cutOriginalGrayMat.data), cutOriginalGrayMat.cols, cutOriginalGrayMat.rows, cutOriginalGrayMat.step, QImage::Format_Indexed8);
	////img4 = img4.scaled(pCamTask->ui.picture3->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	////pCamTask->ui.picture3->setPixmap(QPixmap::fromImage(img4));//显示
	//QImage img1((const unsigned char*)(cutOriginalRGBMat.data), cutOriginalRGBMat.cols, cutOriginalRGBMat.rows, cutOriginalRGBMat.cols * cutOriginalRGBMat.channels(), QImage::Format_RGB888);
	//img1 = img1.scaled(pCamTask->ui.originalPicture->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	//pCamTask->ui.originalPicture->setPixmap(QPixmap::fromImage(img1));//显示

	//QImage img2((const unsigned char*)(cutOriginalGrayDisplayMat.data), cutOriginalGrayDisplayMat.cols, cutOriginalGrayDisplayMat.rows, cutOriginalGrayDisplayMat.cols * cutOriginalGrayDisplayMat.channels(), QImage::Format_RGB888);
	//img2 = img2.scaled(pCamTask->ui.picture1->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	//pCamTask->ui.picture1->setPixmap(QPixmap::fromImage(img2));//显示

	//
	//QImage img3((const unsigned char*)(cutOriginalDisplayRGBMat.data), cutOriginalDisplayRGBMat.cols, cutOriginalDisplayRGBMat.rows, cutOriginalDisplayRGBMat.cols* cutOriginalDisplayRGBMat.channels(), QImage::Format_RGB888);
	//img3 = img3.scaled(pCamTask->ui.picture3->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	//pCamTask->ui.picture3->setPixmap(QPixmap::fromImage(img3));//显示
	//
	/*
	QImage img4((const unsigned char*)(mask.data), mask.cols, mask.rows, mask.step, QImage::Format_Indexed8);
	img4 = img4.scaled(pCamTask->ui.picture3->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	pCamTask->ui.picture3->setPixmap(QPixmap::fromImage(img4));//显示
	*/
//演示显示
/*
	QImage img1((const unsigned char*)(cutOriginalDisplayRGBMat.data), cutOriginalDisplayRGBMat.cols, cutOriginalDisplayRGBMat.rows, cutOriginalDisplayRGBMat.cols* cutOriginalDisplayRGBMat.channels(), QImage::Format_RGB888);
	img1 = img1.scaled(pCamTask->ui.originalPicture->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	pCamTask->ui.originalPicture->setPixmap(QPixmap::fromImage(img1));//显示
	*/
	
}
//以下内容2019年测试写，基于OPENCV
LONGLONG task::initTime() {
	LONGLONG Qpart1;
	QueryPerformanceFrequency(&litmp);
	dfFreq = (double)litmp.QuadPart;
	QueryPerformanceCounter(&litmp);
	Qpart1 = litmp.QuadPart;//开始计时
	return Qpart1;
}
LONGLONG task::getExpendTime(LONGLONG startQpart)
{
	LONGLONG expendTime,Qpart2;//毫秒ms
	double dfMins,dfTime;
	QueryPerformanceCounter(&litmp);
	Qpart2 = litmp.QuadPart;//结束计时
	dfMins = (double)(Qpart2 - startQpart);
	dfTime = dfMins / dfFreq;
	expendTime = dfTime * 1000000;//us秒
	return expendTime;
}
bool task::createFileDir(QString dir) {
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
bool task::QStringCNFileNameSaveImage(QString str)
{
	SetSystem("filename_encoding", "utf8");
	QByteArray temp = str.toLocal8Bit();
	char* ch = temp.data();
	HTuple hs(ch);
	WriteImage(global_gray_image, "jpg", 0, hs);
	return true;
}
bool task::QStringCNFileNameReadImage(QString str,HObject &ho_image)
{
	SetSystem("filename_encoding", "utf8");
	QByteArray temp = str.toLocal8Bit();
	char* ch = temp.data();
	HTuple hs(ch);
	ReadImage(&ho_image, hs);
	return true;
}

bool task::writeIOCard()//写IO板卡
{
	ErrorCode        ret = Success;
	// Step 1: Create a instantDoCtrl for DO function.
	InstantDoCtrl* instantDoCtrl = InstantDoCtrl::Create();
	
	// Step 2: Select a device by device number or device description and specify the access mode.
	// in this example we use ModeWrite mode so that we can fully control the device, including configuring, sampling, etc.
	DeviceInformation devInfo(deviceDescription);
	ret = instantDoCtrl->setSelectedDevice(devInfo);
	if (ret != Success)
	{
		qDebug() << "IOCardWrite_setSelectedDevice_fail" << endl;
	}
		

	// Step 3: Write DO ports
	//Set port direction
	//Array<DioPort>* dioPort = instantDoCtrl->getPorts();
	//ret = dioPort->getItem(0).setDirectionMask(Output); //Setting port0 direction
	//CHK_RESULT(ret);
	uint8  bufferForWriting[64] = { 0 };//the first element is used for start port

	//uint32 data = 0;//the data is used to the 'WriteBit';
	
	bufferForWriting[0] = photo_number;

	ret = instantDoCtrl->Write(0, 1, bufferForWriting);

	return 1;
}
/*
bool getAllMask(InputArray  _originalMat, OutputArray _allMask, std::vector< cv::Point>& rectPoints, testQT* pCamTask) {
	Mat originalMat = _originalMat.getMat();
	_allMask.create(_originalMat.size(), CV_8UC1);
	Mat dst = _allMask.getMat();
	dst = Mat::zeros(_originalMat.size(), CV_8UC1);
	//_allMask.clear();
	//Mat mask = Mat::zeros(_originalMat.size(), CV_8UC1);
	//_allMask.clear();

	Mat mat2;
	//threshold(originalMat, mat2, 0, 255, CV_THRESH_OTSU | CV_THRESH_BINARY);//完成烟支的二值化
	threshold(originalMat, mat2, 50, 255, CV_THRESH_BINARY);



	//边缘检测
	Mat mat3 = Mat(mat2.rows, mat2.cols, CV_8UC3, Scalar(0, 0, 0));//直线查找结果显示
	std::vector< std::vector< cv::Point> > contours;
	findContours(mat2, contours, RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
	//QImage img4((const unsigned char*)(mat2.data), mat2.cols, mat2.rows, mat2.step, QImage::Format_Indexed8);
	//img4 = img4.scaled(pCamTask->ui.picture2->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	//pCamTask->ui.picture3->setPixmap(QPixmap::fromImage(img4));//显示
	if (contours.size() == 0)//防差错
	{
		return false;
	}

	//drawContours(mat3, contours, -1, Scalar(0, 255, 0),3,8);//画出所有轮廓
	//QImage img1((const unsigned char*)(mat3.data), mat3.cols, mat3.rows, mat3.cols * mat3.channels(), QImage::Format_RGB888);
	//img1 = img1.scaled(pCamTask->ui.originalPicture->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	//pCamTask->ui.picture3->setPixmap(QPixmap::fromImage(img1));//显示

	//最大区域边界查找方法
	int maxi = 0;//最大联通边界contours编号
	int maxINumber = 0;
	//int channelsSeven = sevenMat.channels();
	for (int i = 0; i < contours.size(); i++)
	{
		if (maxINumber < contours[i].size())
		{
			maxINumber = contours[i].size();
			maxi = i;
		}
	}

	//大致中心点坐标确认
	int lunkuoPointXSum = 0;//轮廓点X坐标和
	int lunkuoPointYSum = 0;//轮廓点Y坐标和
	int lunkuoPointXAvg = 0;//轮廓点X坐标平均
	int lunkuoPointYAvg = 0;//轮廓点Y坐标平均
	int lunkuoPointLeft = pCamTask->pic1RectWidth;//初始化轮廓左极坐标
	int lunkuoPointRight = 0;//初始化轮廓右极坐标
	int leftRightDelete = 0;//左右舍去

	for (int i = 0; i < contours[maxi].size(); i++)//找到特征值
	{
		lunkuoPointXSum += contours[maxi][i].x;
		lunkuoPointYSum += contours[maxi][i].y;
		if (contours[maxi][i].x < lunkuoPointLeft)
		{
			lunkuoPointLeft = contours[maxi][i].x;//左极坐标
		}
		if (contours[maxi][i].x > lunkuoPointRight)
		{
			lunkuoPointRight = contours[maxi][i].x;//右极坐标
		}
	}
	lunkuoPointXAvg = lunkuoPointXSum / contours[maxi].size();//平均值x
	lunkuoPointYAvg = lunkuoPointYSum / contours[maxi].size();//平均值y
	leftRightDelete = (lunkuoPointLeft + lunkuoPointRight) / 50;//上边界两侧近似距离
	std::vector< cv::Point> upLinesPoints, downLinesPoints, leftLinesPoints, rightLinesPoints;
	int minUpLinesPointsX = pCamTask->pic1RectWidth;
	int maxUpLinesPointsX = 0;
	int minDownLinesPointsX = pCamTask->pic1RectWidth;
	int maxDownLinesPointsX = 0;
	for (int i = 0; i < contours[maxi].size(); i++)//找到特征值
	{
		if (contours[maxi][i].x > lunkuoPointLeft + leftRightDelete && contours[maxi][i].x < lunkuoPointRight - leftRightDelete && contours[maxi][i].y < lunkuoPointYAvg)
		{
			upLinesPoints.push_back(contours[maxi][i]);//上边界直接拟合
			if (contours[maxi][i].x < minUpLinesPointsX)
			{
				minUpLinesPointsX = contours[maxi][i].x;
			}
			if (contours[maxi][i].x > maxUpLinesPointsX)
			{
				maxUpLinesPointsX = contours[maxi][i].x;
			}
		}
		if (contours[maxi][i].x > lunkuoPointLeft + leftRightDelete && contours[maxi][i].x < lunkuoPointRight - leftRightDelete && contours[maxi][i].y>lunkuoPointYAvg)
		{
			downLinesPoints.push_back(contours[maxi][i]);//下边界直接拟合
			if (contours[maxi][i].x < minDownLinesPointsX)
			{
				minDownLinesPointsX = contours[maxi][i].x;
			}
			if (contours[maxi][i].x > maxDownLinesPointsX)
			{
				maxDownLinesPointsX = contours[maxi][i].x;
			}
		}
		if (contours[maxi][i].x < lunkuoPointLeft + leftRightDelete)
		{
			leftLinesPoints.push_back(contours[maxi][i]);//初次分离左边界点
		}
		if (contours[maxi][i].x > lunkuoPointRight - leftRightDelete)
		{
			rightLinesPoints.push_back(contours[maxi][i]);//初次分离右边界点
		}

	}
	Vec4f fitlineUp, fitlineDown, fitlineLeft, fitlineRight;
	fitLine(upLinesPoints, fitlineUp, CV_DIST_L2, 0, 0.01, 0.01);//直线拟合
	fitLine(downLinesPoints, fitlineDown, CV_DIST_L2, 0, 0.01, 0.01);

	//直线的公式Ax+By+C=0
	//fit[1]x-fit[0]y+fit[0]*fit[3]-fit[1]*fit[2]
	//A=fit[1],B=-fit[0],C=fit[0]*fit[3]-fit[1]*fit[2]
	double k1, k2;
	double A1, A2, B1, B2, C1, C2;
	k1 = fitlineUp[1] / fitlineUp[0];
	A1 = fitlineUp[1];
	B1 = -fitlineUp[0];
	C1 = fitlineUp[0] * fitlineUp[3] - fitlineUp[1] * fitlineUp[2];


	k2 = fitlineDown[1] / fitlineDown[0];
	A2 = fitlineDown[1];
	B2 = -fitlineDown[0];
	C2 = fitlineDown[0] * fitlineDown[3] - fitlineDown[1] * fitlineDown[2];



	Point pointUp1, pointUp2, pointDown1, pointDown2;
	pointUp1.x = minUpLinesPointsX;
	pointUp1.y = (pointUp1.x - fitlineUp[2]) * k1 + fitlineUp[3];
	pointUp2.x = maxUpLinesPointsX;
	pointUp2.y = (pointUp2.x - fitlineUp[2]) * k1 + fitlineUp[3];

	pointDown1.x = minDownLinesPointsX;
	pointDown1.y = (pointDown1.x - fitlineDown[2]) * k2 + fitlineDown[3];
	pointDown2.x = maxDownLinesPointsX;
	pointDown2.y = (pointDown2.x - fitlineDown[2]) * k2 + fitlineDown[3];
	int dUp = 0;//点到上边直线距离
	int dDown = 0;//点到下边直线距离
	//double dMin = 10;//最小距离
	std::vector< cv::Point> leftLinesPointsA, rightLinesPointsA;//删除距离上部过近点
	leftLinesPointsA = leftLinesPoints;
	rightLinesPointsA = rightLinesPoints;
	int minLeftLinesPointsY = pCamTask->pic1RectHeight;
	int maxLeftLinesPointsY = 0;
	int minRightLinesPointsY = pCamTask->pic1RectHeight;
	int maxRightLinesPointsY = 0;
	for (int i = 0; i < leftLinesPoints.size(); i++)//获得左边Y最高点和最低点
	{
		if (leftLinesPoints[i].y < minLeftLinesPointsY)
		{
			minLeftLinesPointsY = leftLinesPoints[i].y;
		}
		if (leftLinesPoints[i].y > maxLeftLinesPointsY)
		{
			maxLeftLinesPointsY = leftLinesPoints[i].y;
		}
	}
	for (int i = 0; i < leftLinesPoints.size(); i++)//左边相对上下边界较近的点去除
	{
		dUp = abs(A1 * leftLinesPoints[i].x + B1 * leftLinesPoints[i].y + C1) / sqrt(A1 * A1 + B1 * B1);
		dDown = abs(A2 * leftLinesPoints[i].x + B2 * leftLinesPoints[i].y + C2) / sqrt(A2 * A2 + B2 * B2);
		if (dUp < (maxLeftLinesPointsY - minLeftLinesPointsY) / 6 || dDown < (maxLeftLinesPointsY - minLeftLinesPointsY) / 6)//5分之1的烟支近似宽度
		{
			for (int j = 0; j < leftLinesPointsA.size(); j++)
			{
				if (leftLinesPoints[i].x == leftLinesPointsA[j].x && leftLinesPoints[i].y == leftLinesPointsA[j].y)
				{
					leftLinesPointsA.erase(leftLinesPointsA.begin() + j);
					break;
				}
			}
		}
	}

	for (int i = 0; i < rightLinesPoints.size(); i++)//获得右边Y最高点和最低点
	{
		if (rightLinesPoints[i].y < minRightLinesPointsY)
		{
			minRightLinesPointsY = rightLinesPoints[i].y;
		}
		if (rightLinesPoints[i].y > maxRightLinesPointsY)
		{
			maxRightLinesPointsY = rightLinesPoints[i].y;
		}
	}
	for (int i = 0; i < rightLinesPoints.size(); i++)//右边相对上边界较近的点去除
	{
		dUp = abs(A1 * rightLinesPoints[i].x + B1 * rightLinesPoints[i].y + C1) / sqrt(A1 * A1 + B1 * B1);
		dDown = abs(A2 * rightLinesPoints[i].x + B2 * rightLinesPoints[i].y + C2) / sqrt(A2 * A2 + B2 * B2);
		if (dUp < (maxRightLinesPointsY - minRightLinesPointsY) / 6 || dDown < (maxRightLinesPointsY - minRightLinesPointsY) / 6)//5分之1的烟支近似宽度
		{
			for (int j = 0; j < rightLinesPointsA.size(); j++)
			{
				if (rightLinesPoints[i].x == rightLinesPointsA[j].x && rightLinesPoints[i].y == rightLinesPointsA[j].y)
				{
					rightLinesPointsA.erase(rightLinesPointsA.begin() + j);
					break;
				}
			}
		}
	}

	fitLine(leftLinesPointsA, fitlineLeft, CV_DIST_L2, 0, 0.01, 0.01);
	fitLine(rightLinesPointsA, fitlineRight, CV_DIST_L2, 0, 0.01, 0.01);

	double k3, k4;
	Point pointLeft1, pointLeft2, pointRight1, pointRight2;
	if (fitlineLeft[0] != 0)
	{
		k3 = fitlineLeft[1] / fitlineLeft[0];
		pointLeft1.y = minLeftLinesPointsY;
		pointLeft1.x = (pointLeft1.y - fitlineLeft[3]) / k3 + fitlineLeft[2];
		pointLeft2.y = maxLeftLinesPointsY;
		pointLeft2.x = (pointLeft2.y - fitlineLeft[3]) / k3 + fitlineLeft[2];
	}
	else
	{
		pointLeft1.y = minLeftLinesPointsY;
		pointLeft1.x = fitlineLeft[2];
		pointLeft2.y = maxLeftLinesPointsY;
		pointLeft2.x = fitlineLeft[2];

	}
	if (fitlineRight[0] != 0)
	{
		k4 = fitlineRight[1] / fitlineRight[0];
		pointRight1.y = minRightLinesPointsY;
		pointRight1.x = (pointRight1.y - fitlineRight[3]) / k4 + fitlineRight[2];
		pointRight2.y = maxRightLinesPointsY;
		pointRight2.x = (pointRight2.y - fitlineRight[3]) / k4 + fitlineRight[2];
	}
	else
	{
		pointRight1.y = minRightLinesPointsY;
		pointRight1.x = fitlineRight[2];
		pointRight2.y = minRightLinesPointsY;
		pointRight2.x = fitlineRight[2];
	}


	//line(mat3, pointUp1, pointUp2, cv::Scalar(0, 0, 255), 3, 8, 0);
	//line(mat3, pointDown1, pointDown2, cv::Scalar(0, 0, 255), 3, 8, 0);
	//line(mat3, pointLeft1, pointLeft2, cv::Scalar(0, 0, 255), 3, 8, 0);
	//line(mat3, pointRight1, pointRight2, cv::Scalar(0, 0, 255), 3, 8, 0);
	//QImage img1((const unsigned char*)(mat3.data), mat3.cols, mat3.rows, mat3.cols * mat3.channels(), QImage::Format_RGB888);
	//img1 = img1.scaled(pCamTask->ui.originalPicture->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	//pCamTask->ui.picture3->setPixmap(QPixmap::fromImage(img1));//显示

	Point cornerLeftUp, cornerLeftDown, cornerRightDown, cornerRightUp;
	//两直线焦点计算公式,由点斜式推导出
	//        f1[3]-f2[3]-k1f1[2]+k2f2[2]
	//X  =  ------------------------------
	//                 k2-k1
	//
	//Y  =   ( X - f1[2] )k1 + f1[3]
	//
	cornerLeftUp.x = round((fitlineUp[3] - fitlineLeft[3] - k1 * fitlineUp[2] + k3 * fitlineLeft[2]) / (k3 - k1));
	cornerLeftUp.y = round((cornerLeftUp.x - fitlineUp[2]) * k1 + fitlineUp[3]);

	cornerLeftDown.x = round((fitlineDown[3] - fitlineLeft[3] - k2 * fitlineDown[2] + k3 * fitlineLeft[2]) / (k3 - k2));
	cornerLeftDown.y = round((cornerLeftDown.x - fitlineDown[2]) * k2 + fitlineDown[3]);

	cornerRightUp.x = round((fitlineUp[3] - fitlineRight[3] - k1 * fitlineUp[2] + k4 * fitlineRight[2]) / (k4 - k1));
	cornerRightUp.y = round((cornerRightUp.x - fitlineUp[2]) * k1 + fitlineUp[3]);

	cornerRightDown.x = round((fitlineDown[3] - fitlineRight[3] - k2 * fitlineDown[2] + k4 * fitlineRight[2]) / (k4 - k2));
	cornerRightDown.y = round((cornerRightDown.x - fitlineDown[2]) * k2 + fitlineDown[3]);


	Point p1 = {1,2};
	// std::vector<Point>  contour;

	std::vector< std::vector<Point> > contours1;

	rectPoints.push_back(cornerLeftUp);
	rectPoints.push_back(cornerLeftDown);
	rectPoints.push_back(cornerRightDown);
	rectPoints.push_back(cornerRightUp);


	contours1.push_back(rectPoints);
	drawContours(dst, contours1, -1, cv::Scalar::all(255), CV_FILLED);

	//cutOriginalMat.at<Vec3b>(cornerLeftUp.y, cornerLeftUp.x) = Vec3b(255, 255, 255);//画点左上
	//cutOriginalMat.at<Vec3b>(cornerLeftDown.y, cornerLeftDown.x) = Vec3b(255, 255, 255);//画点左下
	//cutOriginalMat.at<Vec3b>(cornerRightUp.y, cornerRightUp.x) = Vec3b(255, 255, 255);//画点右上
	//cutOriginalMat.at<Vec3b>(cornerRightDown.y, cornerRightDown.x) = Vec3b(255, 255, 255);//画点右下

	return true;
}
bool getEdge(InputArray  _originalMat, int lineType, OutputArray _allMask, Vec4i& line, testQT* pCamTask)//type=0滤嘴烟支分离线;
{
	Mat originalMat = _originalMat.getMat();
	_allMask.create(_originalMat.size(), CV_8UC1);
	Mat dst = _allMask.getMat();

	//threshold(originalMat, dst, divThreshold, 255, CV_THRESH_BINARY);//完成烟支的二值化
	threshold(originalMat, dst, 0, 255, CV_THRESH_OTSU | CV_THRESH_BINARY);//完成烟支的二值化

	Canny(dst, dst, 50, 200, 3);

	Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
	dilate(dst, dst, kernel);

	std::vector<Vec4i> lines;
	HoughLinesP(dst, lines, pCamTask->line_rho, CV_PI / 180 * pCamTask->line_theta, pCamTask->line_threshold, pCamTask->line_minLineLength, pCamTask->line_maxLineGap);
	float a, b, c, cMax, k;
	int cMaxNumber = 0;
	cMax = 0;
	if (lines.size())
	{
		for (int i = 0; i < lines.size(); i++)
		{
			a = (float)lines[i][0] - (float)lines[i][2];
			b = (float)lines[i][1] - (float)lines[i][3];
			if (a != 0)
			{
				k = b / a;
				switch (lineType)//筛选直线角度
				{
				case 0:
					if (k > -1 || k < 1)//滤掉横线
					{
						continue;
					}
					break;
				case 1:
					if (k < -1 || k > 1)//滤掉纵线
					{
						continue;
					}
					break;
				default:
					break;
				}

			}
			c = pow(a, 2) + pow(b, 2);
			if (c > cMax)
			{
				cMax = c;
				cMaxNumber = i;
			}

		}
		line = lines[cMaxNumber];

	}



	return true;
}
bool getEdge1(InputArray  _originalMat, int lineType, OutputArray _allMask, Vec4i& line, testQT* pCamTask)//type=0滤嘴烟支分离线;
{
	Mat originalMat = _originalMat.getMat();
	_allMask.create(_originalMat.size(), CV_8UC1);
	Mat dst = _allMask.getMat();

	//threshold(originalMat, dst, divThreshold, 255, CV_THRESH_BINARY);//完成烟支的二值化
	threshold(originalMat, dst, 0, 255, CV_THRESH_OTSU | CV_THRESH_BINARY);//完成烟支的二值化



	Canny(dst, dst, 50, 200, 3);



	Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
	dilate(dst, dst, kernel);



	std::vector<Vec4i> lines;
	HoughLinesP(dst, lines, pCamTask->line_rho, CV_PI / 180 * pCamTask->line_theta, pCamTask->line_threshold, pCamTask->line_minLineLength, pCamTask->line_maxLineGap);



	float a, b, c, cMax, k;
	int cMaxNumber = 0;
	cMax = 0;
	if (lines.size())
	{
		for (int i = 0; i < lines.size(); i++)
		{
			a = (float)lines[i][0] - (float)lines[i][2];
			b = (float)lines[i][1] - (float)lines[i][3];
			if (a != 0)
			{
				k = b / a;
				switch (lineType)//筛选直线角度
				{
				case 0:
					if (k > -1 || k < 1)//滤掉横线
					{
						continue;
					}
					break;
				case 1:
					if (k < -1 || k > 1)//滤掉纵线
					{
						continue;
					}
					break;
				default:
					break;
				}

			}
			c = pow(a, 2) + pow(b, 2);
			if (c > cMax)
			{
				cMax = c;
				cMaxNumber = i;
			}

		}
		line = lines[cMaxNumber];

	}



	return true;
}
bool getEdge(InputArray  _originalMat, int divThreshold, int lineType, OutputArray _allMask, Vec4i& line, std::vector<Vec4i>& lines, testQT* pCamTask)//type=0滤嘴烟支分离线;
{
	Mat originalMat = _originalMat.getMat();
	_allMask.create(_originalMat.size(), CV_8UC1);
	Mat dst = _allMask.getMat();

	threshold(originalMat, dst, divThreshold, 255, CV_THRESH_BINARY);//完成烟支的二值化
	//threshold(originalMat, dst, 0, 255, CV_THRESH_OTSU | CV_THRESH_BINARY);//完成烟支的二值化


	Canny(dst, dst, 50, 200, 3);



	Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
	dilate(dst, dst, kernel);

	//QImage img1((const unsigned char*)(dst.data), dst.cols, dst.rows, dst.step, QImage::Format_Indexed8);
	//img1 = img1.scaled(pCamTask->ui.picture2->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	//pCamTask->ui.picture2->setPixmap(QPixmap::fromImage(img1));//显示

	//std::vector<Vec4i> lines;
	HoughLinesP(dst, lines, pCamTask->line_rho, CV_PI / 180 * pCamTask->line_theta, pCamTask->line_threshold, pCamTask->line_minLineLength, pCamTask->line_maxLineGap);
	//allLines = lines;
	float a, b, c, cMax, k;
	int cMaxNumber = 0;
	cMax = 0;
	if (lines.size())
	{
		for (int i = 0; i < lines.size(); i++)
		{
			a = (float)lines[i][0] - (float)lines[i][2];
			b = (float)lines[i][1] - (float)lines[i][3];
			if (a != 0)
			{
				k = b / a;
				switch (lineType)//筛选直线角度
				{
				case 0:
					if (k > -1 || k < 1)//滤掉横线
					{
						continue;
					}
					break;
				case 1:
					if (k < -1 || k > 1)//滤掉纵线
					{
						continue;
					}
					break;
				default:
					break;
				}

			}
			c = pow(a, 2) + pow(b, 2);
			if (c > cMax)
			{
				cMax = c;
				cMaxNumber = i;
			}

		}
		line = lines[cMaxNumber];

	}



	return true;
}
bool getUpDownEdgeAndWidth(InputArray  _originalMat, int& cigWidth, OutputArray _allMask, Vec4i& upLine, Vec4i& downLine, testQT* pCamTask)//type=0滤嘴烟支分离线;
{
	Mat originalMat = _originalMat.getMat();
	_allMask.create(_originalMat.size(), CV_8UC1);
	Mat dst = _allMask.getMat();

	//threshold(originalMat, dst, divThreshold, 255, CV_THRESH_BINARY);//完成烟支的二值化
	threshold(originalMat, dst, 0, 255, CV_THRESH_OTSU | CV_THRESH_BINARY);//完成烟支的二值化

	Canny(dst, dst, 50, 200, 3);
	Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
	dilate(dst, dst, kernel);

	std::vector<Vec4i> lines;
	HoughLinesP(dst, lines, pCamTask->line_rho, CV_PI / 180 * pCamTask->line_theta, pCamTask->line_threshold, pCamTask->line_minLineLength, pCamTask->line_maxLineGap);
	float a, b, c, cRectWidth, yMax, yMin, k, yTemp;
	int upLineNumber = 0;
	int downLineNumber = 0;
	yTemp = 0;
	yMax = 0;
	yMin = originalMat.rows;
	cRectWidth = pow((float)originalMat.cols, 2) * 3 / 4;
	if (lines.size())
	{
		for (int i = 0; i < lines.size(); i++)
		{
			a = (float)lines[i][0] - (float)lines[i][2];
			b = (float)lines[i][1] - (float)lines[i][3];
			if (a != 0)
			{
				k = b / a;
				c = pow(a, 2) + pow(b, 2);

				if (k < -1 || k > 1)//滤掉纵线
				{
					continue;
				}
				if (c < cRectWidth)//滤掉小于宽度3/4的线段
				{
					continue;
				}
				yTemp = abs(lines[i][1] + lines[i][3]);
				if (yTemp > yMax)
				{
					yMax = yTemp;
					downLineNumber = i;
				}
				if (yTemp < yMin)
				{
					yMin = yTemp;
					upLineNumber = i;
				}
			}

		}

		upLine = lines[upLineNumber];
		downLine = lines[downLineNumber];
		cigWidth = (lines[downLineNumber][1] + lines[downLineNumber][3] - lines[upLineNumber][1] - lines[upLineNumber][3]) / 2;
	}

	return true;
}
bool getUpDownEdgeAndWidth(InputArray  _originalMat, int divThreshold, int& cigWidth, OutputArray _allMask, Vec4i& upLine, Vec4i& downLine, testQT* pCamTask)//type=0滤嘴烟支分离线;
{
	Mat originalMat = _originalMat.getMat();
	_allMask.create(_originalMat.size(), CV_8UC1);
	Mat dst = _allMask.getMat();

	threshold(originalMat, dst, divThreshold, 255, CV_THRESH_BINARY);//完成烟支的二值化
	//threshold(originalMat, dst, 0, 255, CV_THRESH_OTSU | CV_THRESH_BINARY);//完成烟支的二值化

	Canny(dst, dst, 50, 200, 3);
	Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
	dilate(dst, dst, kernel);

	std::vector<Vec4i> lines;
	HoughLinesP(dst, lines, pCamTask->line_rho, CV_PI / 180 * pCamTask->line_theta, pCamTask->line_threshold, pCamTask->line_minLineLength, pCamTask->line_maxLineGap);
	float a, b, c, cRectWidth, yMax, yMin, k, yTemp;
	int upLineNumber = 0;
	int downLineNumber = 0;
	yTemp = 0;
	yMax = 0;
	yMin = originalMat.rows;
	cRectWidth = pow((float)originalMat.cols, 2) * 3 / 4;
	if (lines.size())
	{
		for (int i = 0; i < lines.size(); i++)
		{
			a = (float)lines[i][0] - (float)lines[i][2];
			b = (float)lines[i][1] - (float)lines[i][3];
			if (a != 0)
			{
				k = b / a;
				c = pow(a, 2) + pow(b, 2);

				if (k < -1 || k > 1)//滤掉纵线
				{
					continue;
				}
				if (c < cRectWidth)//滤掉小于宽度3/4的线段
				{
					continue;
				}
				yTemp = abs(lines[i][1] + lines[i][3]);
				if (yTemp > yMax)
				{
					yMax = yTemp;
					downLineNumber = i;
				}
				if (yTemp < yMin)
				{
					yMin = yTemp;
					upLineNumber = i;
				}
			}

		}

		upLine = lines[upLineNumber];
		downLine = lines[downLineNumber];
		cigWidth = (lines[downLineNumber][1] + lines[downLineNumber][3] - lines[upLineNumber][1] - lines[upLineNumber][3]) / 2;
	}

	return true;
}
//bool getDividingLine(InputArray  _originalMat, testQT* pCamTask)
//{
//	Mat originalMat = _originalMat.getMat();
//
//	return true;
//}

//画虚线矩形框
void  drawDashRect(InputOutputArray  _originalMat, int linelength, int dashlength, Rect* blob, Scalar color, int thickness)
{
	Mat originalMat = _originalMat.getMat();
	//_allMask.create(_originalMat.size(), CV_8UC3);
	int w = cvRound(blob->width);//width
	int h = cvRound(blob->height);//height


	int tl_x = cvRound(blob->x);//top left x
	int tl_y = cvRound(blob->y);//top  left y


	int totallength = dashlength + linelength;
	int nCountX = w / totallength;//
	int nCountY = h / totallength;//


	CvPoint start, end;//start and end point of each dash


	//draw the horizontal lines
	start.y = tl_y;
	start.x = tl_x;


	end.x = tl_x;
	end.y = tl_y;


	for (int i = 0; i < nCountX; i++)
	{
		end.x = tl_x + (i + 1) * totallength - dashlength;//draw top dash line
		end.y = tl_y;
		start.x = tl_x + i * totallength;
		start.y = tl_y;
		//cvLine(img, start, end, color, thickness);
		line(originalMat, start, end, color, thickness, 8, 0);
	}
	for (int i = 0; i < nCountX; i++)
	{
		start.x = tl_x + i * totallength;
		start.y = tl_y + h;
		end.x = tl_x + (i + 1) * totallength - dashlength;//draw bottom dash line
		end.y = tl_y + h;
		line(originalMat, start, end, color, thickness, 8, 0);
	}
	for (int i = 0; i < nCountY; i++)
	{
		start.x = tl_x;
		start.y = tl_y + i * totallength;
		end.y = tl_y + (i + 1) * totallength - dashlength;//draw left dash line
		end.x = tl_x;
		line(originalMat, start, end, color, thickness, 8, 0);
	}

	for (int i = 0; i < nCountY; i++)
	{
		start.x = tl_x + w;
		start.y = tl_y + i * totallength;
		end.y = tl_y + (i + 1) * totallength - dashlength;//draw right dash line
		end.x = tl_x + w;
		line(originalMat, start, end, color, thickness, 8, 0);
	}
}

bool findBroken(InputOutputArray  _originalMat,  OutputArray _allMask, testQT* pCamTask)
{
	Mat originalMat = _originalMat.getMat();

	Mat dst = _allMask.getMat();

	SimpleBlobDetector::Params blobParams;
	std::vector<KeyPoint> keyPoints;
	Ptr<SimpleBlobDetector> blobDetector = SimpleBlobDetector::create(blobParams);
	blobDetector->detect(originalMat, keyPoints);
	drawKeypoints(originalMat, keyPoints, dst, Scalar(255, 0, 0));
	return 1;
}
*/

