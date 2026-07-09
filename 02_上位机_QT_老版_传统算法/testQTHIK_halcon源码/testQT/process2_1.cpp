#include "process2_1.h"
process2_1::process2_1(testQT* pDlg,int camera_number, uchar photo_number) {

	testqt = pDlg;
}
process2_1::~process2_1()
{
	
}
void process2_1::run()
{
	int showIntervel = 50;
	int showNumber = 0;
	int ca_1_1_remain = 0;
	int ca_2_1_remain = 0;
	int ca_2_2_remain = 0;
	
	while (testqt->systemRun)//设备运行
	{
		if (testqt->showCheckRegion)
		{
			showCheckRegion = true;
		}else
		{
			showCheckRegion = false;
		}
		
		if (!testqt->picQueList2_1.isEmpty())//队列有图像
		{
			ca_2_1_remain = testqt->picQueList2_1.size();
		}
		else {
			ca_2_1_remain = 0;
		}
		
		//int temp = testqt->picQueList.size();
		//处理2组件1相机图像条件
		if (ca_2_1_remain!=0)
		{
			//处理2组件1相机图像
			if (ca_2_1_remain == 1)
			{
				if (testqt->mutexPicQueList2_1.tryLock())//是否存图完毕
				{
					testqt->mutexPicQueList2_1.unlock();//解锁
				}
				else
				{
					Sleep(1);//正在存图，等1毫秒
				}
			}
			picStruct ps = testqt->picQueList2_1.dequeue();//图片出队列
			//uint8 checkAgainRejectIO = 0;//验证返回剔除烟支读入值
			//int rejectType = 0;//剔除种类
			int cig1RejectType = 0;//cig1缺陷分类
			int cig2RejectType = 0;//cig2缺陷分类

			bool Bool_cig1RejectEnable = false;//烟支1检测剔除使能
			bool Bool_cig2RejectEnable = false;//烟支2检测剔除使能
			uint uint_cig1Result = 0;//烟支1检测输出结果
			uint uint_cig2Result = 0;//烟支1检测输出结果
			HTuple hv_cig1Param;//烟支1的缺陷参数，用以精确复检
			HObject ho_cig1DrawNGSingle, ho_cig2DrawNGSingle;
			uint8 uint8_cig2RejectNumberIO = 0;
			HObject ho_showCheckRegion;//用于显示带检测框的实时图

			if (showCheckRegion)//实时显示检测设定情况
			{
				ho_showCheckRegion = ps.ho_Cam_Image;//目前是灰度图
			}
			uint ret = whiteCigDarkLightCircleFilterProcess(ps, &Bool_cig1RejectEnable, &Bool_cig2RejectEnable, &uint_cig1Result, &uint_cig2Result, &ho_cig1DrawNGSingle, &ho_cig2DrawNGSingle, &uint8_cig2RejectNumberIO, &hv_cig1Param, &ho_showCheckRegion);//检测，返回烟支1检测结果

			if (Bool_cig2RejectEnable)//烟支2剔除
			{
				if (uint_cig2Result)//确定剔除类型
				{
					uint moveLeftTemp = 0x00000001;//循环左移工具
					for (int i = 1; i <= 16; i++)
					{
						if (uint_cig2Result & moveLeftTemp)//按位与
						{
							cig2RejectType = i;//剔除类型
							break;
						}
						moveLeftTemp = moveLeftTemp << 1;//左移一位循环
					}

				}
				testqt->rejectType[cig2RejectType] += 1;//错误计数+1
				testqt->ngCigNumber += 1;
				testqt->showPic2_1 = ho_cig2DrawNGSingle;//显示组件2缺陷图
				picSaveStruct picSaveStruct1;
				picSaveStruct1.ho_double_gray_image = ps.ho_Cam_Image;//原始图
				picSaveStruct1.ho_drawNG_image = ho_cig2DrawNGSingle;//标记图，显示错误
				HObject ho_half_ho_Cam_Image;
				CropPart(ps.ho_Cam_Image, &ho_half_ho_Cam_Image, ps.mv_frame->nHeight / 2, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//原图截一半用于调参
				picSaveStruct1.ho_single_gray_image = ho_half_ho_Cam_Image;//原图一半
				picSaveStruct1.rejectType = cig2RejectType;
				picSaveStruct1.zuNumber = 2;//组件号
				testqt->mutex.lock();//互斥锁定
				testqt->NGNumberList.enqueue(uint8_cig2RejectNumberIO);//加入剔除队列
				testqt->picSaveList.enqueue(picSaveStruct1);
				testqt->mutex.unlock();//互斥解除
				
			}
			testqt->checkCigNumber += 1;//测烟计数+1
			showNumber += 1;//显示计数+1
			uint result1 = uint_cig1Result & 0x00000FFF;//缺陷位结果
			//testqt->rejectTest = false;//测试剔除用
			if (result1)//第一支烟检测有问题
			{
				//testqt->ngCigNumber += 1;//坏烟+1
				//testqt->rejectTest = true;//测试剔除用

				//缺陷复检队列
				//uint resultMoveLeft = 0x00000001;//移位控制

				//uint NoNeedCheckSins = 0x00000001;//不用复检直接剔除
				//uchar NGNumber = 0;//缺陷分类

				if (result1 & needCheckAgainSigns)//输出结果与复检标志位过滤按位与
				{
					picCheckAgainStruct picCheckAgasinStruct1;
					picCheckAgasinStruct1.result = result1;
					picCheckAgasinStruct1.numberIO = ps.uchar_pic_IO;
					picCheckAgasinStruct1.hv_NGPara = hv_cig1Param;;//将缺陷参数传递，用以复检
					checkAgainHash.insert(ps.uchar_pic_number, picCheckAgasinStruct1);
				}
				if (Bool_cig1RejectEnable)//直接剔除
				{

					testqt->NGNumberList.enqueue(ps.uchar_pic_IO);//剔除功能
					testqt->ngCigNumber += 1;
					if (result1)//确定剔除类型
					{
						uint moveLeftTemp = 0x00000001;//循环左移工具
						for (int i = 1; i <= 16; i++)
						{
							if (result1 & moveLeftTemp)//按位与
							{
								cig1RejectType = i;//剔除类型
								break;
							}
							moveLeftTemp = moveLeftTemp << 1;//左移一位循环
						}

					}
					testqt->rejectType[cig1RejectType] += 1;
					testqt->showNGPic2_1 = ho_cig1DrawNGSingle;//显示组件1缺陷图
					picSaveStruct picSaveStruct1;
					picSaveStruct1.ho_double_gray_image = ps.ho_Cam_Image;//原始图
					picSaveStruct1.ho_drawNG_image = ho_cig1DrawNGSingle;//标记图，显示错误
					HObject ho_half_ho_Cam_Image;
					CropPart(ps.ho_Cam_Image, &ho_half_ho_Cam_Image, 0, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//原图截一半用于调参
					picSaveStruct1.ho_single_gray_image = ho_half_ho_Cam_Image;//原图一半
					picSaveStruct1.rejectType = cig1RejectType;
					picSaveStruct1.zuNumber = 1;//组件号
					testqt->mutex.lock();//互斥锁定
					testqt->picSaveList.enqueue(picSaveStruct1);
					testqt->mutex.unlock();//互斥解除
				}
			}
			//显示图片
			if (showNumber >= showIntervel)
			{
				//SetPart(testqt->hv_WindowHandle, 0, 0,ps.mv_frame->nHeight, ps.mv_frame->nWidth);
				//DispObj(ps.ho_Cam_Image, testqt->hv_WindowHandle);
				if (showCheckRegion)
				{
					testqt->showPic2_1 = ho_showCheckRegion;
				}
				else
				{
					testqt->showPic2_1 = ps.ho_Cam_Image;
				}
				showNumber = 0;
			}
			expendTime = getExpendTime(QStartCount);
			//qDebug() << "dealtime expend" << expendTime << "  picNumber" << ps.uchar_pic_number << endl;
			//qDebug() << "dealtime expend" << expendTime << "  picNumber" << ps.uchar_pic_IO << endl;
		}
		//处理2组件1相机图像条件

		//处理2组件2相机图像条件

	}
}
bool process2_1::whiteCigDarkLightCircleFilterProcess(picStruct& ps, bool *Bool_cig1RejectEnable, bool *Bool_cig2RejectEnable, uint *uint_cig1Result, uint *uint_cig2Result, HObject *ho_cig1DrawNGSingle, HObject *ho_cig2DrawNGSingle, uint8* uint8_cig2RejectNumberIO, HTuple* hv_cig1Param, HObject* ho_showCheckRegion)//处理过程：硬红
{
	
	HObject ho_Regions, ho_RegionOpening, ho_ConnectedRegions, ho_SelectedRegions, ho_Rectangle1, ho_GrayImage, ho_ImageReduced1, ho_Region, ho_Region1Closing, ho_SelectedRectangularityRegions;

	HObject ho_RegionComplement, ho_ConnectedRegionComplement, ho_SelectedResionsComplement, ho_RectangleNG;
	HTuple hv_Number, hv_Row, hv_Column, hv_Phi, hv_Length1, hv_Length2, hv_RectangleNumber, hv_Rectangularity, hv_Convexity, hv_textRowCount, hv_smallestCheckArea, hv_NumberNG, hv_RowNG1, hv_ColumnNG1;
	HTuple hv_RowNG2, hv_ColumnNG2, hv_Index1, hv_centerArea, hv_pt_centerX, hv_pt_centerY;
	//搭扣缺陷变量定义
	HObject ho_div_region, ho_div_image, ho_div_thr_region, ho_gen_region, ho_diff_region, ho_diff_region_connection, ho_div_result;
	HTuple hv_canny_region_left_up_Row, hv_canny_region_left_up_Column, hv_canny_region_right_down_Row, hv_canny_region_right_down_Column;
	HTuple hv_out_Row, hv_out_Column, hv_out_phi, hv_out_length1, hv_out_length2, hv_div_result_number;
	//滤嘴缺陷变量定义
	HTuple hv_filter_center_x, hv_filter_center_y, hv_filter_cig_width_diff, hv_filter_cig_height_diff, hv_filter1_exception_column, hv_filter_NumberNG;
	HTuple hv_filter1_RowNG1,hv_filter1_ColumnNG1, hv_filter1_RowNG2, hv_filter1_ColumnNG2;
	HObject ho_filter1_rect, ho_ImageReduced_filter1, ho_filter1_Region, ho_filter1_region_closing, ho_filter1_region_closing_connection, ho_Selected_filter1_region;
	HObject drawR, drawG, drawB;//画图

	int cigNumber = 0;//识别出的烟支数量
	bool cigUpCheck = false;//上烟检验
	bool cigDownCheck = false;//下烟检验
	uint result1 = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷,上烟。每支烟：1位圆度不合格，2位烟支内部缺陷，3位搭扣缺陷，4位滤嘴缺陷
	uint result2 = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格，下烟
	//uint resultCheckAgain = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格，下烟
	picCheckAgainStruct picCheckAgainStruct1;
	//一般二值化效果更精确
	Threshold(ps.ho_Cam_Image, &ho_Regions, para1.all_gray_thr_min, para1.all_gray_thr_max);
	/*showEffect(ho_Regions);
	return 0;*/
	OpeningRectangle1(ho_Regions, &ho_RegionOpening, para1.open_width, para1.open_height);
	Connection(ho_RegionOpening, &ho_ConnectedRegions);
	SelectShape(ho_ConnectedRegions, &ho_SelectedRegions, "area", "and", para1.cig_area_min, 100000);
	CountObj(ho_SelectedRegions, &hv_Number);//获取烟支数量

	//expendTime = getExpendTime(QStartCount);
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
	/*
		phi:长边与X轴的夹角-1.5707* 到1.5707*，X轴算起逆时针角度为正，顺时针角度为负
	*/
	SmallestRectangle2(ho_SelectedRegions, &hv_Row, &hv_Column, &hv_Phi, &hv_Length1, &hv_Length2);
	if (showCheckRegion)
	{
		HTuple hv_color;
		hv_color.Clear();
		hv_color[0] = 0;
		hv_color[1] = 162;
		hv_color[2] = 222;
		
		PaintRegion(ho_SelectedRegions, *ho_showCheckRegion, &drawR, hv_color[0], "fill");//检测框R通道画在原图上
		PaintRegion(ho_SelectedRegions, *ho_showCheckRegion, &drawG, hv_color[1], "fill");//检测框G通道画在原图上
		PaintRegion(ho_SelectedRegions, *ho_showCheckRegion, &drawB, hv_color[2], "fill");//检测框B通道画在原图上
		//Compose3(drawR, drawG, drawB, ho_showCheckRegion);
	}

	uint8 checkAgainNumber = 0;//复检烟支编号
	if (ps.uchar_pic_number == 1)//最小编号
	{
		checkAgainNumber = testqt->maxCigNumber;
	}
	else
	{
		checkAgainNumber = ps.uchar_pic_number - 1;
	}

	if (cigNumber == 1)//一支烟
	{
		if (hv_Row[0] < ps.mv_frame->nHeight / 2)//上半区，检测
		{
			//第一支检验标志
			cigUpCheck = true;
		}
		else
		{
			if (checkAgainHash.contains(checkAgainNumber))
			{
				cigDownCheck = true;
				picCheckAgainStruct1 = checkAgainHash.value(checkAgainNumber);
				checkAgainHash.remove(checkAgainNumber);//删除复检项
			}
		}
	}
	//qDebug() << "cigNumber :" << cigNumber <<endl;
	if (cigNumber == 2)//两支烟
	{
		if ((hv_Row[0] < ps.mv_frame->nHeight / 2 && hv_Row[1] < ps.mv_frame->nHeight / 2) || (hv_Row[0] > ps.mv_frame->nHeight / 2 && hv_Row[1] > ps.mv_frame->nHeight / 2))//都在上半区或者下半区，剔除
		{
			return Err_SameSide2Cig;
		}
		//第一支检验标志
		cigUpCheck = true;
		if (checkAgainHash.contains(checkAgainNumber))
		{
			cigDownCheck = true;
			picCheckAgainStruct1 = checkAgainHash.value(checkAgainNumber);
			checkAgainHash.remove(checkAgainNumber);//删除复检项
		}
	}
	int regionNumber = 0;//区域中目标在第几个
	//if (cigUpCheck)//检验上烟
	if (cigUpCheck || cigDownCheck)//有烟要检
	{
		regionNumber = 0;//区域中目标在第几个
		//int nowCheckNumber = 1;//当前流程处理烟支1:up;2:down
		if (cigUpCheck)
		{
			//第一支算法流程
			GenRectangle2(&ho_Rectangle1, HTuple(hv_Row[regionNumber]), HTuple(hv_Column[regionNumber]),
				HTuple(hv_Phi[regionNumber]), HTuple(hv_Length1[regionNumber]), HTuple(hv_Length2[regionNumber]));
			//截取图片
			ReduceDomain(ps.ho_Cam_Image, ho_Rectangle1, &ho_ImageReduced1);
			/*showEffect(ho_ImageReduced1);
			return 0;*/
			//切割图像1二值化
			Threshold(ho_ImageReduced1, &ho_Region, para1.cig1_thr, 255);

			ClosingRectangle1(ho_Region, &ho_Region1Closing, 5, 5);
			//判断图像1的矩形度，如果矩形度不够直接标记
			SelectShape(ho_Region1Closing, &ho_SelectedRectangularityRegions, (HTuple("convexity").Append("rectangularity")),
				"and", (HTuple(0.9).Append(0.95)), (HTuple(2).Append(2)));

			CountObj(ho_SelectedRectangularityRegions, &hv_RectangleNumber);
			HObject drawCig1;
			if (hv_RectangleNumber.I() == 0)
			{
				Rectangularity(ho_Region1Closing, &hv_Rectangularity);
				Convexity(ho_Region1Closing, &hv_Convexity);
				result1 = result1 | 0x00000001;//最低位置1，矩形度和凸包度不合格,上烟
				*uint_cig1Result = result1;
				*Bool_cig1RejectEnable = true;//烟支1缺陷剔除
				if (!ho_cig1DrawNGSingle->IsInitialized())
				{
					HTuple hv_color;
					hv_color.Clear();
					hv_color[0] = 255;
					hv_color[1] = 0;
					hv_color[2] = 0;
					HObject drawCig1R, drawCig1G, drawCig1B;
					PaintRegion(ho_Region1Closing, ps.ho_Cam_Image, &drawCig1R, hv_color[0], "margin");//检测框R通道画在原图上
					CropPart(drawCig1R, &drawCig1R, 0, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//带检测框图截一半用于显示
					PaintRegion(ho_Region1Closing, ps.ho_Cam_Image, &drawCig1G, hv_color[1], "margin");//检测框G通道画在原图上
					CropPart(drawCig1G, &drawCig1G, 0, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//带检测框图截一半用于显示
					PaintRegion(ho_Region1Closing, ps.ho_Cam_Image, &drawCig1B, hv_color[2], "margin");//检测框B通道画在原图上
					CropPart(drawCig1B, &drawCig1B, 0, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//带检测框图截一半用于显示

					Compose3(drawCig1R, drawCig1G, drawCig1B, ho_cig1DrawNGSingle);
				}
			}
			else//烟支1存在且无必须剔除缺陷
			{
				if (!*Bool_cig1RejectEnable)//烟支1完成烟棒外形判断，并未剔除
				{
					//烟内孔洞判断
					hv_smallestCheckArea = (HTuple)para1.smallestCheckArea;
					//反选区域
					Complement(ho_Region, &ho_RegionComplement);
					Connection(ho_RegionComplement, &ho_ConnectedRegionComplement);
					SelectShape(ho_ConnectedRegionComplement, &ho_SelectedResionsComplement,
						(HTuple("area").Append("column1")), "and", hv_smallestCheckArea.TupleConcat(HTuple(hv_Column[regionNumber]) - HTuple(hv_Length1[regionNumber]+20)),
						((HTuple(hv_Length1[regionNumber])* HTuple(hv_Length2[regionNumber])) * 2).TupleConcat((HTuple(hv_Column[regionNumber]) + HTuple(hv_Length1[regionNumber])) - 100));

					CountObj(ho_SelectedResionsComplement, &hv_NumberNG);

					int NGNumber = hv_NumberNG.I();

					if (0 != (int(hv_NumberNG > 0)))
					{
						
						AreaCenter(ho_SelectedResionsComplement, &hv_centerArea, &hv_pt_centerY, &hv_pt_centerX);
						*hv_cig1Param = hv_pt_centerX;//将缺陷中心作为参数传递，用以复检
						result1 = result1 | 0x00000002;//第二位置1，有内部缺陷
						*uint_cig1Result = result1;
					}
					//搭口缺陷判断
					//hv_canny_region_left_up_Row = 0;//截图范围有点大，改掉更精确
					hv_canny_region_left_up_Row = HTuple(hv_Row[regionNumber]) - HTuple(hv_Length2[regionNumber])-15;
					if (hv_canny_region_left_up_Row.D() < 0)
					{
						hv_canny_region_left_up_Row = 0;
					}
					hv_canny_region_left_up_Column = para1.divding_x1;
					//hv_canny_region_right_down_Row = ps.mv_frame->nHeight / 2;//截图范围有点大，改掉更精确
					hv_canny_region_right_down_Row = HTuple(hv_Row[regionNumber]) + HTuple(hv_Length2[regionNumber]) + 15;
					hv_canny_region_right_down_Column = para1.divding_x2;
					GenRectangle1(&ho_div_region, hv_canny_region_left_up_Row, hv_canny_region_left_up_Column, hv_canny_region_right_down_Row, hv_canny_region_right_down_Column);
					
					ReduceDomain(ps.ho_Cam_Image, ho_div_region, &ho_div_image);
					Threshold(ho_div_image, &ho_div_thr_region, para1.div_thr, 255);
					SmallestRectangle2(ho_div_thr_region, &hv_out_Row, &hv_out_Column, &hv_out_phi, &hv_out_length1, &hv_out_length2);
					GenRectangle2(&ho_gen_region, hv_out_Row, hv_out_Column, hv_out_phi, hv_out_length1, hv_out_length2);
					Difference(ho_gen_region, ho_div_thr_region, &ho_diff_region);
					Connection(ho_diff_region, &ho_diff_region_connection);
					SelectShape(ho_diff_region_connection, &ho_div_result, "area", "and", para1.div_diff_area, 9999);
					CountObj(ho_div_result, &hv_div_result_number);

					if (0 != (int(hv_div_result_number > 0)))//搭口缺陷
					{
						AreaCenter(ho_div_result, &hv_centerArea, &hv_pt_centerY, &hv_pt_centerX);
						//*hv_cig1Param = hv_centerArea;//将缺陷面积作为参数传递，用以复检；因多个缺陷共用该参数，暂时不传
						result1 = result1 | 0x00000004;//第3位置1，有搭口缺陷
						*uint_cig1Result = result1;
					}
					if (showCheckRegion)
					{
						HTuple hv_color;
						hv_color.Clear();
						hv_color[0] = 255;
						hv_color[1] = 97;
						hv_color[2] = 0;
						//HObject drawR, drawG, drawB;
						PaintRegion(ho_div_region, drawR, &drawR, hv_color[0], "fill");//检测框R通道画在原图上
						PaintRegion(ho_div_region, drawG, &drawG, hv_color[1], "fill");//检测框G通道画在原图上
						PaintRegion(ho_div_region, drawB, &drawB, hv_color[2], "fill");//检测框B通道画在原图上
						
					}
					//滤嘴缺陷判断

					//滤嘴中心与烟棒中心的水平和竖直偏差//弧度输入
					hv_filter_cig_width_diff = (HTuple(hv_Length1[regionNumber]) + (para1.filter_check_width / 2)) * (HTuple(hv_Phi[regionNumber]).TupleCos());
					hv_filter_cig_height_diff = (HTuple(hv_Length1[regionNumber]) + (para1.filter_check_width / 2)) * (HTuple(hv_Phi[regionNumber]).TupleSin());
					//滤嘴中心位置
					hv_filter_center_x = HTuple(hv_Column[regionNumber]) + hv_filter_cig_width_diff;
					hv_filter_center_y = HTuple(hv_Row[regionNumber]) - hv_filter_cig_height_diff;
					//获取滤嘴位置
					GenRectangle2(&ho_filter1_rect, hv_filter_center_y, hv_filter_center_x,HTuple(hv_Phi[regionNumber]), para1.filter_check_width / 2, para1.filter_check_height / 2);
					//截取滤嘴
					ReduceDomain(ps.ho_Cam_Image, ho_filter1_rect, &ho_ImageReduced_filter1);
					//切割图像1二值化
					Threshold(ho_ImageReduced_filter1, &ho_filter1_Region, para1.filter_thr,255);
					ClosingRectangle1(ho_filter1_Region, &ho_filter1_region_closing, 5,5);
					Connection(ho_filter1_region_closing, &ho_filter1_region_closing_connection);

					hv_filter1_exception_column = (hv_filter_center_x - (para1.filter_check_width / 2)) + para1.filter_except_max_offsetX;
					SelectShape(ho_filter1_region_closing_connection, &ho_Selected_filter1_region,
						(HTuple("area").Append("column")), "and", ((HTuple)para1.smallestCheckArea).TupleConcat(hv_filter1_exception_column),
						(HTuple(10000).Append(1000)));
					CountObj(ho_Selected_filter1_region, &hv_filter_NumberNG);
	
					if (0 != (int(hv_filter_NumberNG > 0)))//滤嘴缺陷
					{
						AreaCenter(ho_div_result, &hv_centerArea, &hv_pt_centerY, &hv_pt_centerX);
						//*hv_cig1Param = hv_pt_centerX;//将缺陷中心作为参数传递，用以复检，因多个缺陷共用该参数，暂时不传
						result1 = result1 | 0x00000008;//第4位置1，有滤嘴缺陷
						*uint_cig1Result = result1;
					}
					if (showCheckRegion)
					{
						HTuple hv_color;
						hv_color.Clear();
						hv_color[0] = 128;
						hv_color[1] = 42;
						hv_color[2] = 42;
						//HObject drawR, drawG, drawB;
						PaintRegion(ho_filter1_rect, drawR, &drawR, hv_color[0], "fill");//检测框R通道画在原图上
						PaintRegion(ho_filter1_rect, drawG, &drawG, hv_color[1], "fill");//检测框G通道画在原图上
						PaintRegion(ho_filter1_rect, drawB, &drawB, hv_color[2], "fill");//检测框B通道画在原图上
						Compose3(drawR, drawG, drawB, ho_showCheckRegion);
					}
				}
			}
		}
	}

	if (cigDownCheck)//检验下烟
	{
		if (cigUpCheck)
		{
			regionNumber = 1;//取第二支
		}
		else
		{
			regionNumber = 0;//取第一支
		}
		
		//第二支算法流程
		GenRectangle2(&ho_Rectangle1, HTuple(hv_Row[regionNumber]), HTuple(hv_Column[regionNumber]),
			HTuple(hv_Phi[regionNumber]), HTuple(hv_Length1[regionNumber]), HTuple(hv_Length2[regionNumber]));
		//截取图片
		ReduceDomain(ps.ho_Cam_Image, ho_Rectangle1, &ho_ImageReduced1);

		//切割图像2二值化
		Threshold(ho_ImageReduced1, &ho_Region, para1.cig1_thr, 255);

		ClosingRectangle1(ho_Region, &ho_Region1Closing, 5, 5);
		//判断图像2的矩形度，如果矩形度不够直接标记
		SelectShape(ho_Region1Closing, &ho_SelectedRectangularityRegions, (HTuple("convexity").Append("rectangularity")),
			"and", (HTuple(0.9).Append(0.95)), (HTuple(2).Append(2)));

		CountObj(ho_SelectedRectangularityRegions, &hv_RectangleNumber);
		uint resultMoveLeft = 0x00000001;//移位控制
		uchar Wei = 0;//缺陷分类
		bool cig2CheckFinish = false;//确认完成
		int NGNumber = 0;
		HObject drawCig2;
		while (Wei < 16 && cig2CheckFinish == false)//设计16类缺陷
		{
			Wei++;

			if (picCheckAgainStruct1.result & resultMoveLeft)//烟棒形状缺陷
			{
				switch (Wei)
				{
				case 1://烟棒形状缺陷，不复检

					break;
				case 2://烟棒黑点缺陷
					//烟内孔洞判断
					hv_smallestCheckArea = (HTuple)para1.smallestCheckArea;
					//反选区域
					Complement(ho_Region, &ho_RegionComplement);
					Connection(ho_RegionComplement, &ho_ConnectedRegionComplement);
					SelectShape(ho_ConnectedRegionComplement, &ho_SelectedResionsComplement,
						(HTuple("area").Append("column1")), "and", hv_smallestCheckArea.TupleConcat(HTuple(hv_Column[regionNumber]) - HTuple(hv_Length1[regionNumber])),
						((HTuple(hv_Length1[regionNumber]) * HTuple(hv_Length2[regionNumber])) * 2).TupleConcat((HTuple(hv_Column[regionNumber]) + HTuple(hv_Length1[regionNumber])) - 100));

					CountObj(ho_SelectedResionsComplement, &hv_NumberNG);
					//expendTime = getExpendTime(QStartCount);
					//qDebug() << "task checkCigHoles expend" << expendTime << endl;
					
					NGNumber = hv_NumberNG.I();
					

					if (0 != (int(hv_NumberNG > 0)))
					{
						AreaCenter(ho_SelectedResionsComplement, &hv_centerArea, &hv_pt_centerY, &hv_pt_centerX);
						int check2_Length = hv_pt_centerX.TupleLength();//获取复检数组长度
						int check1_Length = picCheckAgainStruct1.hv_NGPara.TupleLength();//获取参数数组长度
						HTuple check1_NGPara = picCheckAgainStruct1.hv_NGPara;//用于变量监控
						int check2_NG = false;//复检结果
						int diff_limit = 20;//复检限值,超过该值认为不是同一缺陷
						for (int i=0;i<check2_Length;i++)
						{
							for (int j = 0; j < check1_Length; j++)
							{
								double check2_value= hv_pt_centerX[i].D();
								double check1_value = picCheckAgainStruct1.hv_NGPara[j].D();
								int diff_temp = abs(check2_value - check1_value);
								if (diff_temp < diff_limit)
								{
									check2_NG = true;
									break;
								}
							}
							if (check2_NG)//已经确定缺陷
								break;
						}
						if (check2_NG)
						{
							//剔除动作
							*Bool_cig2RejectEnable = true;
							*uint8_cig2RejectNumberIO = picCheckAgainStruct1.numberIO;//烟支2剔除numberIO
							*uint_cig2Result = 0x00000002;//烟支2检验缺陷结果
							cig2CheckFinish = true;//确认完成
							if (!ho_cig2DrawNGSingle->IsInitialized())
							{
								//PaintRegion(ho_SelectedResionsComplement, ps.ho_Cam_Image, &drawCig2, 255, "margin");//检测框画在原图上
								//CropPart(drawCig2, ho_cig2DrawNGSingle, 0, ps.mv_frame->nHeight / 2, ps.mv_frame->nWidth, ps.mv_frame->nHeight);//带检测框图截一半用于显示

								HTuple hv_color;
								hv_color.Clear();
								hv_color[0] = 255;
								hv_color[1] = 0;
								hv_color[2] = 0;
								HObject drawCig2R, drawCig2G, drawCig2B;
								PaintRegion(ho_SelectedResionsComplement, ps.ho_Cam_Image, &drawCig2R, hv_color[0], "margin");//检测框R通道画在原图上
								CropPart(drawCig2R, &drawCig2R, ps.mv_frame->nHeight / 2, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//带检测框图截一半用于显示
								PaintRegion(ho_SelectedResionsComplement, ps.ho_Cam_Image, &drawCig2G, hv_color[1], "margin");//检测框G通道画在原图上
								CropPart(drawCig2G, &drawCig2G, ps.mv_frame->nHeight / 2, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//带检测框图截一半用于显示
								PaintRegion(ho_SelectedResionsComplement, ps.ho_Cam_Image, &drawCig2B, hv_color[2], "margin");//检测框B通道画在原图上
								CropPart(drawCig2B, &drawCig2B, ps.mv_frame->nHeight / 2, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//带检测框图截一半用于显示
								//PaintRegion(ho_Region1Closing, ps.ho_Cam_Image, &drawCig1, 255, "margin");//检测框画在原图上
								//CropPart(drawCig1, ho_cig1DrawNGSingle, 0, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//带检测框图截一半用于显示
								Compose3(drawCig2R, drawCig2G, drawCig2B, ho_cig2DrawNGSingle);
							}
						}
					}
					break;
				case 3://搭口缺陷
					hv_canny_region_left_up_Row = ps.mv_frame->nHeight / 2;
					hv_canny_region_left_up_Column = para1.divding_x1;
					hv_canny_region_right_down_Row = ps.mv_frame->nHeight - 1;
					hv_canny_region_right_down_Column = para1.divding_x2;
					GenRectangle1(&ho_div_region, hv_canny_region_left_up_Row, hv_canny_region_left_up_Column, hv_canny_region_right_down_Row, hv_canny_region_right_down_Column);
					ReduceDomain(ps.ho_Cam_Image, ho_div_region, &ho_div_image);
					Threshold(ho_div_image, &ho_div_thr_region, para1.div_thr, 255);
					SmallestRectangle2(ho_div_thr_region, &hv_out_Row, &hv_out_Column, &hv_out_phi, &hv_out_length1, &hv_out_length2);
					GenRectangle2(&ho_gen_region, hv_out_Row, hv_out_Column, hv_out_phi, hv_out_length1, hv_out_length2);
					Difference(ho_gen_region, ho_div_thr_region, &ho_diff_region);
					Connection(ho_diff_region, &ho_diff_region_connection);
					SelectShape(ho_diff_region_connection, &ho_div_result, "area", "and", para1.div_diff_area, 9999);
					CountObj(ho_div_result, &hv_div_result_number);

					if (0 != (int(hv_div_result_number > 0)))//搭口缺陷
					{
						//剔除动作
						*Bool_cig2RejectEnable = true;
						*uint8_cig2RejectNumberIO = picCheckAgainStruct1.numberIO;//烟支2剔除numberIO
						*uint_cig2Result = 0x00000004;//烟支2检验缺陷结果
						cig2CheckFinish = true;//确认完成
						if (!ho_cig2DrawNGSingle->IsInitialized())
						{
							//PaintRegion(ho_div_result, ps.ho_Cam_Image, &drawCig2, 255, "margin");//检测框画在原图上
							//CropPart(drawCig2, ho_cig2DrawNGSingle, 0, ps.mv_frame->nHeight / 2, ps.mv_frame->nWidth, ps.mv_frame->nHeight);//带检测框图截一半用于显示
							HTuple hv_color;
							hv_color.Clear();
							hv_color[0] = 255;
							hv_color[1] = 0;
							hv_color[2] = 0;
							HObject drawCig2R, drawCig2G, drawCig2B;
							PaintRegion(ho_div_result, ps.ho_Cam_Image, &drawCig2R, hv_color[0], "margin");//检测框R通道画在原图上
							CropPart(drawCig2R, &drawCig2R,  ps.mv_frame->nHeight / 2,0, ps.mv_frame->nWidth, ps.mv_frame->nHeight/2);//带检测框图截一半用于显示
							PaintRegion(ho_div_result, ps.ho_Cam_Image, &drawCig2G, hv_color[1], "margin");//检测框G通道画在原图上
							CropPart(drawCig2G, &drawCig2G,  ps.mv_frame->nHeight / 2,0, ps.mv_frame->nWidth, ps.mv_frame->nHeight/2);//带检测框图截一半用于显示
							PaintRegion(ho_div_result, ps.ho_Cam_Image, &drawCig2B, hv_color[2], "margin");//检测框B通道画在原图上
							CropPart(drawCig2B, &drawCig2B,  ps.mv_frame->nHeight / 2,0, ps.mv_frame->nWidth, ps.mv_frame->nHeight/2);//带检测框图截一半用于显示
							//PaintRegion(ho_Region1Closing, ps.ho_Cam_Image, &drawCig1, 255, "margin");//检测框画在原图上
							//CropPart(drawCig1, ho_cig1DrawNGSingle, 0, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//带检测框图截一半用于显示
							Compose3(drawCig2R, drawCig2G, drawCig2B, ho_cig2DrawNGSingle);
						}
							
					}
						
					break;
				case 4://滤嘴缺陷
					//滤嘴缺陷判断
					
					//滤嘴中心与烟棒中心的水平和竖直偏差//弧度输入
					hv_filter_cig_width_diff = (HTuple(hv_Length1[regionNumber]) + (para1.filter_check_width / 2)) * (HTuple(hv_Phi[regionNumber]).TupleCos());
					hv_filter_cig_height_diff = (HTuple(hv_Length1[regionNumber]) + (para1.filter_check_width / 2)) * (HTuple(hv_Phi[regionNumber]).TupleSin());
					//滤嘴中心位置
					hv_filter_center_x = HTuple(hv_Column[regionNumber]) + hv_filter_cig_width_diff;
					hv_filter_center_y = HTuple(hv_Row[regionNumber]) - hv_filter_cig_height_diff;
					//获取滤嘴位置
					GenRectangle2(&ho_filter1_rect, hv_filter_center_y, hv_filter_center_x, HTuple(hv_Phi[regionNumber]), para1.filter_check_width / 2, para1.filter_check_height / 2);
					//截取滤嘴
					ReduceDomain(ps.ho_Cam_Image, ho_filter1_rect, &ho_ImageReduced_filter1);
					//切割图像1二值化
					Threshold(ho_ImageReduced_filter1, &ho_filter1_Region, para1.filter_thr, 255);
					ClosingRectangle1(ho_filter1_Region, &ho_filter1_region_closing, 5, 5);
					Connection(ho_filter1_region_closing, &ho_filter1_region_closing_connection);

					hv_filter1_exception_column = (hv_filter_center_x - (para1.filter_check_width / 2)) + para1.filter_except_max_offsetX;
					SelectShape(ho_filter1_region_closing_connection, &ho_Selected_filter1_region,
						(HTuple("area").Append("column")), "and", ((HTuple)para1.smallestCheckArea).TupleConcat(hv_filter1_exception_column),
						(HTuple(10000).Append(1000)));
					CountObj(ho_Selected_filter1_region, &hv_filter_NumberNG);
					////最小外接矩形
					//SmallestRectangle1(ho_Selected_filter1_region, &hv_filter1_RowNG1,
					//	&hv_filter1_ColumnNG1, &hv_filter1_RowNG2, &hv_filter1_ColumnNG2);
					if (0 != (int(hv_filter_NumberNG > 0)))//滤嘴缺陷
					{
						result1 = result1 | 0x00000008;//第4位置1，有滤嘴缺陷
						*uint_cig1Result = result1;
						//剔除动作
						*Bool_cig2RejectEnable = true;
						*uint8_cig2RejectNumberIO = picCheckAgainStruct1.numberIO;//烟支2剔除numberIO
						*uint_cig2Result = 0x00000008;//烟支2检验缺陷结果
						cig2CheckFinish = true;//确认完成
						if (!ho_cig2DrawNGSingle->IsInitialized())
						{
							HTuple hv_color;
							hv_color.Clear();
							hv_color[0] = 255;
							hv_color[1] = 0;
							hv_color[2] = 0;
							HObject drawCig2R, drawCig2G, drawCig2B;
							PaintRegion(ho_Selected_filter1_region, ps.ho_Cam_Image, &drawCig2R, hv_color[0], "margin");//检测框R通道画在原图上
							CropPart(drawCig2R, &drawCig2R, ps.mv_frame->nHeight / 2, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight/2);//带检测框图截一半用于显示
							PaintRegion(ho_Selected_filter1_region, ps.ho_Cam_Image, &drawCig2G, hv_color[1], "margin");//检测框G通道画在原图上
							CropPart(drawCig2G, &drawCig2G, ps.mv_frame->nHeight / 2, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight/2);//带检测框图截一半用于显示
							PaintRegion(ho_Selected_filter1_region, ps.ho_Cam_Image, &drawCig2B, hv_color[2], "margin");//检测框B通道画在原图上
							CropPart(drawCig2B, &drawCig2B, ps.mv_frame->nHeight / 2, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight/2);//带检测框图截一半用于显示
							//PaintRegion(ho_Region1Closing, ps.ho_Cam_Image, &drawCig1, 255, "margin");//检测框画在原图上
							//CropPart(drawCig1, ho_cig1DrawNGSingle, 0, 0, ps.mv_frame->nWidth, ps.mv_frame->nHeight / 2);//带检测框图截一半用于显示
							Compose3(drawCig2R, drawCig2G, drawCig2B, ho_cig2DrawNGSingle);
						}
					}
					
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
			resultMoveLeft = resultMoveLeft << 1;
		}
		
	}

	return result1;
}
bool process2_1::whiteCigLightCircleFilterProcess(picStruct& ps, bool* Bool_cig1RejectEnable, bool* Bool_cig2RejectEnable, uint* uint_cig1Result, uint* uint_cig2Result, HObject* ho_cig1DrawNGSingle, HObject* ho_cig2DrawNGSingle, uint8* uint8_cig2RejectNumberIO, HTuple* hv_cig1Param, HObject* ho_showCheckRegion)
{
	
	HObject ho_cig1Regions,ho_rectangle1,ho_cig1RegionReduceImage,ho_cig1ThrReions, ho_RegionOpening, ho_RegionClosing,ho_connectionRegions,ho_cig1SelectRegion;
	HTuple hv_cig1_region_leftX, hv_cig1_region_leftY, hv_cig1_region_rightX, hv_cig1_region_rightY, hv_cig1SelectNumber , hv_smallestCheckArea,hv_cig1StickHoleNumberNG;
	HObject ho_cig1CheckRect, ho_cig1Reduced, ho_cig1StickRegion, ho_cig1StickRegionComplement, ho_cig1StickRegCompleConnection, ho_cig1StickHoleSelect;
	HTuple hv_cig1Rectangularity, hv_cig1Convexity, hv_cig1OutRow, hv_cig1OutColumn, hv_cig1OutPhi, hv_cig1OutHalfLength1, hv_cig1OutHalfLength2;

	uint resultCheck = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷,上烟。每支烟：1位圆度不合格，2位烟支内部缺陷，3位搭扣缺陷，4位滤嘴缺陷
	uint resultRecheck = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格，下烟
	picCheckAgainStruct picCheckAgainStruct1;
	//烟支1画区域
	GenRectangle1(&ho_rectangle1, hv_cig1_region_leftY, hv_cig1_region_leftX, hv_cig1_region_rightY, hv_cig1_region_rightX);
	//切割烟支1
	ReduceDomain(ps.ho_Cam_Image, ho_rectangle1, &ho_cig1RegionReduceImage);
	//烟支1二值化
	Threshold(ho_cig1RegionReduceImage, &ho_cig1ThrReions, para1.all_gray_thr_min, para1.all_gray_thr_max);
	//开运算去除边界噪声
	OpeningRectangle1(ho_cig1ThrReions, &ho_RegionOpening, para1.open_width, para1.open_height);
	//闭运算链接烟支与滤嘴
	ClosingRectangle1(ho_RegionOpening, &ho_RegionClosing, para1.close_width, para1.close_height);
	//链接
	Connection(ho_RegionClosing, &ho_connectionRegions);
	//筛选烟支区域面积
	SelectShape(ho_connectionRegions, &ho_cig1SelectRegion,HTuple("area"), "and", (HTuple)para1.cig_area_min,HTuple(100000));
	//满足筛选条件烟支数量
	CountObj(ho_cig1SelectRegion, &hv_cig1SelectNumber);
	if (hv_cig1SelectNumber[0].I() == 0)
	{
		//烟支1空槽
	}
	if (hv_cig1SelectNumber[0].I() > 1)
	{
		//烟支1多支，错乱 
	}
	if (hv_cig1SelectNumber[0].I() == 1)
	{
		//烟支1存在，继续检验
		Rectangularity(ho_cig1SelectRegion, &hv_cig1Rectangularity);
		Convexity(ho_cig1SelectRegion, &hv_cig1Convexity);
		resultCheck = resultCheck | 0x00000001;//最低位置1，矩形度和凸包度不合格,当前测烟
		*uint_cig1Result = resultCheck;
		*Bool_cig1RejectEnable = true;//烟支1缺陷剔除
		if (!*Bool_cig1RejectEnable)//烟支1完成烟棒外形判断，并未剔除
		{
			//烟支1外接矩形定位
			SmallestRectangle2(ho_cig1SelectRegion, &hv_cig1OutRow, &hv_cig1OutColumn, &hv_cig1OutPhi, &hv_cig1OutHalfLength1, &hv_cig1OutHalfLength2);
			//获取烟支1烟棒灰度图
			GenRectangle2(&ho_cig1CheckRect, hv_cig1OutRow, hv_cig1OutColumn, hv_cig1OutPhi, hv_cig1OutHalfLength1, hv_cig1OutHalfLength2);
			//截取烟支1烟棒灰度图
			ReduceDomain(ps.ho_Cam_Image, ho_cig1CheckRect, &ho_cig1Reduced);
			//切割烟支1烟棒二值化
			Threshold(ho_cig1Reduced, &ho_cig1StickRegion, para1.cig1_thr, 255);
			//翻转判断孔洞
			Complement(ho_cig1StickRegion, &ho_cig1StickRegionComplement);
			//链接
			Connection(ho_cig1StickRegionComplement, &ho_cig1StickRegCompleConnection);
			//烟内孔洞判断
			hv_smallestCheckArea = (HTuple)para1.smallestCheckArea;
			//判断孔洞
			SelectShape(ho_cig1StickRegCompleConnection, &ho_cig1StickHoleSelect,
				(HTuple("area").Append("column1")), "and", hv_smallestCheckArea.TupleConcat(HTuple(hv_cig1OutColumn[0]) - HTuple(hv_cig1OutHalfLength1[0] + 20)),
				((HTuple(hv_cig1OutHalfLength1[0]) * HTuple(hv_cig1OutHalfLength2[0])) * 2).TupleConcat((HTuple(hv_cig1OutColumn[0]) + HTuple(hv_cig1OutHalfLength1[0])) - 100));
			CountObj(ho_cig1StickHoleSelect, &hv_cig1StickHoleNumberNG);

		}
	}

}
//uint process::whiteCigDarkLightCircleFilterProcess(picStruct* ps, uint8 *checkAgainRejectIO, int* rejectType)//处理过程：硬红
//{
//
//	HObject ho_Regions, ho_RegionOpening, ho_ConnectedRegions, ho_SelectedRegions, ho_Rectangle1, ho_GrayImage, ho_ImageReduced1, ho_Region, ho_Region1Closing, ho_SelectedRectangularityRegions;
//	HObject ho_RegionComplement, ho_ConnectedRegionComplement, ho_SelectedResionsComplement, ho_RectangleNG;
//	HTuple hv_Number, hv_Row, hv_Column, hv_Phi, hv_Length1, hv_Length2, hv_RectangleNumber, hv_Rectangularity, hv_Convexity, hv_textRowCount, hv_smallestCheckArea, hv_NumberNG, hv_RowNG1, hv_ColumnNG1;
//	HTuple hv_RowNG2, hv_ColumnNG2, hv_Index1;
//	//搭扣缺陷变量定义
//	HObject ho_div_region, ho_div_image, ho_div_thr_region, ho_gen_region, ho_diff_region, ho_diff_region_connection, ho_div_result;
//	HTuple hv_canny_region_left_up_Row, hv_canny_region_left_up_Column, hv_canny_region_right_down_Row, hv_canny_region_right_down_Column;
//	HTuple hv_out_Row, hv_out_Column, hv_out_phi, hv_out_length1, hv_out_length2, hv_div_result_number;
//
//	int cigNumber = 0;//识别出的烟支数量
//	bool cigUpCheck = false;//上烟检验
//	bool cigDownCheck = false;//下烟检验
//	uint result1 = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格,上烟
//	uint result2 = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格，下烟
//	//uint resultCheckAgain = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格，下烟
//	picCheckAgainStruct picCheckAgainStruct1;
//	//一般二值化效果更精确
//	Threshold(ps->ho_Cam_Image, &ho_Regions, para1.all_gray_thr_min, para1.all_gray_thr_max);
//	/*showEffect(ho_Regions);
//	return 0;*/
//	OpeningRectangle1(ho_Regions, &ho_RegionOpening, para1.open_width, para1.open_height);
//	Connection(ho_RegionOpening, &ho_ConnectedRegions);
//	SelectShape(ho_ConnectedRegions, &ho_SelectedRegions, "area", "and", para1.cig_area_min, 100000);
//	CountObj(ho_SelectedRegions, &hv_Number);//获取烟支数量
//
//	//expendTime = getExpendTime(QStartCount);
//	//qDebug() << "task getCigNumber expend" << expendTime << endl;
//	/*showEffect(ho_SelectedRegions);
//	return 0;*/
//	//int hv_number_int = hv_Number[0].I();//调试用
//	if (hv_Number[0].I() == 0)//无烟
//	{
//		return Err_NotFindCig;
//	}
//	if (hv_Number[0].I() == 1)//一支烟，判断上下，缺陷识别或者检验
//	{
//		cigNumber = 1;
//	}
//	if (hv_Number[0].I() == 2)//两支烟，判断上下，分别识别和检验
//	{
//		cigNumber = 2;
//	}
//	if (hv_Number[0].I() > 2)//大于两支烟，直接剔除
//	{
//		return Err_FindMoreThan2Cig;
//	}
//
//	SmallestRectangle2(ho_SelectedRegions, &hv_Row, &hv_Column, &hv_Phi, &hv_Length1, &hv_Length2);
//	/*
//		phi:长边与X轴的夹角-1.5707* 到1.5707*，X轴算起逆时针角度为正，顺时针角度为负
//	*/
//	uint8 checkAgainNumber = 0;//复检烟支编号
//	if (ps->uchar_pic_number == 1)//最小编号
//	{
//		checkAgainNumber = testqt->maxCigNumber;
//	}
//	else
//	{
//		checkAgainNumber = ps->uchar_pic_number - 1;
//	}
//
//	if (cigNumber == 1)//一支烟
//	{
//		if (hv_Row[0] < ps->mv_frame->nHeight / 2)//上半区，检测
//		{
//			//第一支检验标志
//			cigUpCheck = true;
//		}
//		else
//		{
//			if (checkAgainHash.contains(checkAgainNumber))
//			{
//				cigDownCheck = true;
//				picCheckAgainStruct1 = checkAgainHash.value(checkAgainNumber);
//				checkAgainHash.remove(checkAgainNumber);//删除复检项
//			}
//		}
//	}
//	//qDebug() << "cigNumber :" << cigNumber <<endl;
//	if (cigNumber == 2)//两支烟
//	{
//		if ((hv_Row[0] < ps->mv_frame->nHeight / 2 && hv_Row[1] < ps->mv_frame->nHeight / 2) || (hv_Row[0] > ps->mv_frame->nHeight / 2 && hv_Row[1] > ps->mv_frame->nHeight / 2))//都在上半区或者下半区，剔除
//		{
//			return Err_SameSide2Cig;
//		}
//		//第一支检验标志
//		cigUpCheck = true;
//		if (checkAgainHash.contains(checkAgainNumber))
//		{
//			cigDownCheck = true;
//			picCheckAgainStruct1 = checkAgainHash.value(checkAgainNumber);
//			checkAgainHash.remove(checkAgainNumber);//删除复检项
//		}
//	}
//	int regionNumber = 0;//区域中目标在第几个
//	//if (cigUpCheck)//检验上烟
//	if (cigUpCheck || cigDownCheck)//有烟要检
//	{
//		regionNumber = 0;//区域中目标在第几个
//		//int nowCheckNumber = 1;//当前流程处理烟支1:up;2:down
//		if (cigUpCheck)
//		{
//			//第一支算法流程
//			GenRectangle2(&ho_Rectangle1, HTuple(hv_Row[regionNumber]), HTuple(hv_Column[regionNumber]),
//				HTuple(hv_Phi[regionNumber]), HTuple(hv_Length1[regionNumber]), HTuple(hv_Length2[regionNumber]));
//			//截取图片
//			ReduceDomain(ps->ho_Cam_Image, ho_Rectangle1, &ho_ImageReduced1);
//			/*showEffect(ho_ImageReduced1);
//			return 0;*/
//			//切割图像1二值化
//			Threshold(ho_ImageReduced1, &ho_Region, para1.cig1_thr, 255);
//
//			ClosingRectangle1(ho_Region, &ho_Region1Closing, 5, 5);
//			//判断图像1的矩形度，如果矩形度不够直接标记
//			SelectShape(ho_Region1Closing, &ho_SelectedRectangularityRegions, (HTuple("convexity").Append("rectangularity")),
//				"and", (HTuple(0.9).Append(0.95)), (HTuple(2).Append(2)));
//
//			CountObj(ho_SelectedRectangularityRegions, &hv_RectangleNumber);
//
//			if (hv_RectangleNumber.I() == 0)
//			{
//				Rectangularity(ho_Region1Closing, &hv_Rectangularity);
//				Convexity(ho_Region1Closing, &hv_Convexity);
//				result1 = result1 | 0x00000001;//最低位置1，矩形度和凸包度不合格,上烟
//
//				HTuple hv_Convexity_length, hv_Rectangularity_length;//仅计算用
//				TupleLength(hv_Convexity, &hv_Convexity_length);//仅计算用
//				TupleLength(hv_Rectangularity, &hv_Rectangularity_length);//仅计算用
//
//			}
//			else
//			{
//				//烟内孔洞判断
//				hv_smallestCheckArea = (HTuple)para1.smallestCheckArea;
//				//反选区域
//				Complement(ho_Region, &ho_RegionComplement);
//				Connection(ho_RegionComplement, &ho_ConnectedRegionComplement);
//				SelectShape(ho_ConnectedRegionComplement, &ho_SelectedResionsComplement,
//					(HTuple("area").Append("column1")), "and", hv_smallestCheckArea.TupleConcat(HTuple(hv_Column[regionNumber]) - HTuple(hv_Length1[regionNumber])),
//					((HTuple(hv_Length1[regionNumber]) * HTuple(hv_Length2[regionNumber])) * 2).TupleConcat((HTuple(hv_Column[regionNumber]) + HTuple(hv_Length1[regionNumber])) - 100));
//
//				CountObj(ho_SelectedResionsComplement, &hv_NumberNG);
//				//expendTime = getExpendTime(QStartCount);
//				//qDebug() << "task checkCigHoles expend" << expendTime << endl;
//
//				int NGNumber = hv_NumberNG.I();
//				
//				if (0 != (int(hv_NumberNG > 0)))
//				{
//
//					HTuple hv_area, hv_row, hv_column;
//					result1 = result1 | 0x00000002;//第二位置1，有内部缺陷
//					
//					AreaCenter(ho_SelectedResionsComplement, &hv_area, &hv_row, &hv_column);//仅用于显示
//
//				}
//				//搭口缺陷判断
//				
//				hv_canny_region_left_up_Row = 0;
//				hv_canny_region_left_up_Column = para1.divding_x1;
//				hv_canny_region_right_down_Row = ps->mv_frame->nHeight / 2;
//				hv_canny_region_right_down_Column = para1.divding_x2;
//				GenRectangle1(&ho_div_region, hv_canny_region_left_up_Row, hv_canny_region_left_up_Column, hv_canny_region_right_down_Row, hv_canny_region_right_down_Column);
//				ReduceDomain(ps->ho_Cam_Image, ho_div_region, &ho_div_image);
//				Threshold(ho_div_image, &ho_div_thr_region, para1.div_thr, 255);
//				SmallestRectangle2(ho_div_thr_region, &hv_out_Row, &hv_out_Column, &hv_out_phi, &hv_out_length1, &hv_out_length2);
//				GenRectangle2(&ho_gen_region, hv_out_Row, hv_out_Column, hv_out_phi, hv_out_length1, hv_out_length2);
//				Difference(ho_gen_region, ho_div_thr_region, &ho_diff_region);
//				Connection(ho_diff_region, &ho_diff_region_connection);
//				SelectShape(ho_diff_region_connection, &ho_div_result, "area", "and", para1.div_diff_area, 9999);
//				CountObj(ho_div_result, &hv_div_result_number);
//
//				if (0 != (int(hv_div_result_number > 0)))//搭口缺陷
//				{
//					result1 = result1 | 0x00000004;//第3位置1，有搭口缺陷
//				}
//
//			}
//		}
//		}
//		
//	if (cigDownCheck)//检验下烟
//	{
//		if (cigUpCheck)//上烟已检
//		{
//			regionNumber = 1;
//			//第二支算法流程
//			GenRectangle2(&ho_Rectangle1, HTuple(hv_Row[regionNumber]), HTuple(hv_Column[regionNumber]),
//				HTuple(hv_Phi[regionNumber]), HTuple(hv_Length1[regionNumber]), HTuple(hv_Length2[regionNumber]));
//			//截取图片
//			ReduceDomain(ps->ho_Cam_Image, ho_Rectangle1, &ho_ImageReduced1);
//
//			//切割图像2二值化
//			Threshold(ho_ImageReduced1, &ho_Region, para1.cig1_thr, 255);
//
//			ClosingRectangle1(ho_Region, &ho_Region1Closing, 5, 5);
//			//判断图像2的矩形度，如果矩形度不够直接标记
//			SelectShape(ho_Region1Closing, &ho_SelectedRectangularityRegions, (HTuple("convexity").Append("rectangularity")),
//				"and", (HTuple(0.9).Append(0.95)), (HTuple(2).Append(2)));
//
//			CountObj(ho_SelectedRectangularityRegions, &hv_RectangleNumber);
//			uint resultMoveLeft = 0x00000001;//移位控制
//			uchar Wei = 0;//缺陷分类
//			bool cig2CheckReject = false;//确认剔除
//			int NGNumber = 0;
//			while (Wei < 16&&cig2CheckReject==false)//设计16类缺陷
//			{
//				Wei++;
//
//				if (picCheckAgainStruct1.result & resultMoveLeft)//烟棒形状缺陷
//				{
//					switch (Wei)
//					{
//					case 1://烟棒形状缺陷，不复检
//						
//						break;
//					case 2://烟棒黑点缺陷
//						//烟内孔洞判断
//						hv_smallestCheckArea = (HTuple)para1.smallestCheckArea;
//						//反选区域
//						Complement(ho_Region, &ho_RegionComplement);
//						Connection(ho_RegionComplement, &ho_ConnectedRegionComplement);
//						SelectShape(ho_ConnectedRegionComplement, &ho_SelectedResionsComplement,
//							(HTuple("area").Append("column1")), "and", hv_smallestCheckArea.TupleConcat(HTuple(hv_Column[regionNumber]) - HTuple(hv_Length1[regionNumber])),
//							((HTuple(hv_Length1[regionNumber]) * HTuple(hv_Length2[regionNumber])) * 2).TupleConcat((HTuple(hv_Column[regionNumber]) + HTuple(hv_Length1[regionNumber])) - 100));
//
//						CountObj(ho_SelectedResionsComplement, &hv_NumberNG);
//						//expendTime = getExpendTime(QStartCount);
//						//qDebug() << "task checkCigHoles expend" << expendTime << endl;
//
//						NGNumber = hv_NumberNG.I();
//
//						if (0 != (int(hv_NumberNG > 0)))
//						{
//							//剔除动作
//							cig2CheckReject = true;
//							*checkAgainRejectIO = picCheckAgainStruct1.numberIO;
//							*rejectType = Wei;
//						}
//						break;
//					case 3://搭口缺陷
//						hv_canny_region_left_up_Row = ps->mv_frame->nHeight / 2;
//						hv_canny_region_left_up_Column = para1.divding_x1;
//						hv_canny_region_right_down_Row = ps->mv_frame->nHeight -1;
//						hv_canny_region_right_down_Column = para1.divding_x2;
//						GenRectangle1(&ho_div_region, hv_canny_region_left_up_Row, hv_canny_region_left_up_Column, hv_canny_region_right_down_Row, hv_canny_region_right_down_Column);
//						ReduceDomain(ps->ho_Cam_Image, ho_div_region, &ho_div_image);
//						Threshold(ho_div_image, &ho_div_thr_region, para1.div_thr, 255);
//						SmallestRectangle2(ho_div_thr_region, &hv_out_Row, &hv_out_Column, &hv_out_phi, &hv_out_length1, &hv_out_length2);
//						GenRectangle2(&ho_gen_region, hv_out_Row, hv_out_Column, hv_out_phi, hv_out_length1, hv_out_length2);
//						Difference(ho_gen_region, ho_div_thr_region, &ho_diff_region);
//						Connection(ho_diff_region, &ho_diff_region_connection);
//						SelectShape(ho_diff_region_connection, &ho_div_result, "area", "and", para1.div_diff_area, 9999);
//						CountObj(ho_div_result, &hv_div_result_number);
//
//						if (0 != (int(hv_div_result_number > 0)))//搭口缺陷
//						{
//							//剔除动作
//							cig2CheckReject = true;
//							*checkAgainRejectIO = picCheckAgainStruct1.numberIO;
//							*rejectType = Wei;
//						}
//						break;
//					case 4:
//						break;
//					case 5:
//						break;
//					case 6:
//						break;
//					case 7:
//						break;
//					case 8:
//						break;
//					case 9:
//						break;
//					case 10:
//						break;
//					case 11:
//						break;
//					case 12:
//						break;
//					case 13:
//						break;
//					case 14:
//						break;
//					case 15:
//						break;
//					case 16:
//						break;
//					default:
//						break;
//					}
//
//				}
//				resultMoveLeft = resultMoveLeft << 1;
//			}
//		}
//		else
//		{
//
//		}
//	}
//
//	return result1;
//}
//uint process::whiteCigDarkLightCircleFilterProcess(picStruct *ps)//处理过程：硬红
//{
//
//	HObject ho_Regions, ho_RegionOpening, ho_ConnectedRegions, ho_SelectedRegions, ho_Rectangle1, ho_GrayImage, ho_ImageReduced1, ho_Region, ho_Region1Closing, ho_SelectedRectangularityRegions;
//	HObject ho_RegionComplement, ho_ConnectedRegionComplement, ho_SelectedResionsComplement, ho_RectangleNG;
//	HTuple hv_Number, hv_Row, hv_Column, hv_Phi, hv_Length1, hv_Length2, hv_RectangleNumber, hv_Rectangularity, hv_Convexity, hv_textRowCount, hv_smallestCheckArea, hv_NumberNG, hv_RowNG1, hv_ColumnNG1;
//	HTuple hv_RowNG2, hv_ColumnNG2, hv_Index1;
//	int cigNumber = 0;//识别出的烟支数量
//	bool cigUpCheck = false;//上烟检验
//	bool cigDownCheck = false;//下烟检验
//	uint result1 = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格,上烟
//	uint result2 = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格，下烟
//	uint resultCheckAgain = 0x00000000;//共32位，低12位第一支烟，中12位第二支烟，高8位其他类缺陷。每支烟：1位圆度不合格，下烟
//	//一般二值化效果更精确
//	Threshold(ps->ho_Cam_Image, &ho_Regions, para1.all_gray_thr_min, para1.all_gray_thr_max);
//	/*showEffect(ho_Regions);
//	return 0;*/
//	OpeningRectangle1(ho_Regions, &ho_RegionOpening, para1.open_width, para1.open_height);
//	Connection(ho_RegionOpening, &ho_ConnectedRegions);
//	SelectShape(ho_ConnectedRegions, &ho_SelectedRegions, "area", "and", para1.cig_area_min, 100000);
//	CountObj(ho_SelectedRegions, &hv_Number);//获取烟支数量
//
//	//expendTime = getExpendTime(QStartCount);
//	//qDebug() << "task getCigNumber expend" << expendTime << endl;
//	/*showEffect(ho_SelectedRegions);
//	return 0;*/
//	//int hv_number_int = hv_Number[0].I();//调试用
//	if (hv_Number[0].I() == 0)//无烟
//	{
//		return Err_NotFindCig;
//	}
//	if (hv_Number[0].I() == 1)//一支烟，判断上下，缺陷识别或者检验
//	{
//		cigNumber = 1;
//	}
//	if (hv_Number[0].I() == 2)//两支烟，判断上下，分别识别和检验
//	{
//		cigNumber = 2;
//	}
//	if (hv_Number[0].I() > 2)//大于两支烟，直接剔除
//	{
//		return Err_FindMoreThan2Cig;
//	}
//
//	SmallestRectangle2(ho_SelectedRegions, &hv_Row, &hv_Column, &hv_Phi, &hv_Length1, &hv_Length2);
//	/*
//		phi:长边与X轴的夹角-1.5707* 到1.5707*，X轴算起逆时针角度为正，顺时针角度为负
//	*/
//	uchar checkAgainNumber = 0;//复检烟支编号
//	if (ps->uchar_pic_number == 1)//最小编号
//	{
//		checkAgainNumber = testqt->maxCigNumber;
//	}
//	else
//	{
//		checkAgainNumber = ps->uchar_pic_number - 1;
//	}
//	
//	if (cigNumber == 1)//一支烟
//	{
//		if (hv_Row[0] < ps->mv_frame->nHeight / 2)//上半区，检测
//		{
//			//第一支检验标志
//			cigUpCheck = true;
//		}
//		else
//		{
//			if (checkAgainHash.contains(checkAgainNumber))
//			{
//				cigDownCheck = true;
//				resultCheckAgain = checkAgainHash.value(checkAgainNumber);
//				checkAgainHash.remove(checkAgainNumber);//删除复检项
//			}
//		}
//	}
//	//qDebug() << "cigNumber :" << cigNumber <<endl;
//	if (cigNumber == 2)//两支烟
//	{
//		if ((hv_Row[0] < ps->mv_frame->nHeight / 2 && hv_Row[1] < ps->mv_frame->nHeight / 2) || (hv_Row[0] > ps->mv_frame->nHeight / 2 && hv_Row[1] > ps->mv_frame->nHeight / 2))//都在上半区或者下半区，剔除
//		{
//			return Err_SameSide2Cig;
//		}
//		//第一支检验标志
//		cigUpCheck = true;
//		if (checkAgainHash.contains(checkAgainNumber))
//		{
//			cigDownCheck = true;
//			resultCheckAgain = checkAgainHash.value(checkAgainNumber);
//			checkAgainHash.remove(checkAgainNumber);//删除复检项
//		}
//	}
//
//	//if (cigUpCheck)//检验上烟
//	if (cigUpCheck||cigDownCheck)//有烟要检
//	{
//		int regionNumber = 0;//区域中目标在第几个
//		int nowCheckNumber = 1;//当前流程处理烟支1:up;2:down
//		if (cigUpCheck)
//		{
//			nowCheckNumber = 1;//上
//		}
//		else
//		{
//			nowCheckNumber = 2;//下
//		}
//		//第一支算法流程
//		GenRectangle2(&ho_Rectangle1, HTuple(hv_Row[regionNumber]), HTuple(hv_Column[regionNumber]),
//			HTuple(hv_Phi[regionNumber]), HTuple(hv_Length1[regionNumber]), HTuple(hv_Length2[regionNumber]));
//		//截取图片
//		ReduceDomain(ps->ho_Cam_Image, ho_Rectangle1, &ho_ImageReduced1);
//		/*showEffect(ho_ImageReduced1);
//		return 0;*/
//		//切割图像1二值化
//		Threshold(ho_ImageReduced1, &ho_Region, para1.cig1_thr, 255);
//
//		ClosingRectangle1(ho_Region, &ho_Region1Closing, 5, 5);
//		//判断图像1的矩形度，如果矩形度不够直接标记
//		SelectShape(ho_Region1Closing, &ho_SelectedRectangularityRegions, (HTuple("convexity").Append("rectangularity")),
//			"and", (HTuple(0.9).Append(0.95)), (HTuple(2).Append(2)));
//
//		CountObj(ho_SelectedRectangularityRegions, &hv_RectangleNumber);
//
//		if (hv_RectangleNumber.I() == 0)
//		{
//			Rectangularity(ho_Region1Closing, &hv_Rectangularity);
//			Convexity(ho_Region1Closing, &hv_Convexity);
//			if (nowCheckNumber == 1)//上烟举行度不符
//			{
//				result1 = result1 | 0x00000001;//最低位置1，矩形度和凸包度不合格,上烟
//			}
//			else
//			{
//				result2 = result2 | 0x00000001;//最低位置1，矩形度和凸包度不合格,下烟
//			}
//			
//
//			HTuple hv_Convexity_length, hv_Rectangularity_length;//仅计算用
//			TupleLength(hv_Convexity, &hv_Convexity_length);//仅计算用
//			TupleLength(hv_Rectangularity, &hv_Rectangularity_length);//仅计算用
//
//		}
//		else
//		{
//			if (nowCheckNumber == 1 || (nowCheckNumber == 2 && resultCheckAgain == 0x000000002))
//			{
//				//烟内孔洞判断
//				hv_smallestCheckArea = (HTuple)para1.smallestCheckArea;
//				//反选区域
//				Complement(ho_Region, &ho_RegionComplement);
//				Connection(ho_RegionComplement, &ho_ConnectedRegionComplement);
//				SelectShape(ho_ConnectedRegionComplement, &ho_SelectedResionsComplement,
//					(HTuple("area").Append("column1")), "and", hv_smallestCheckArea.TupleConcat(HTuple(hv_Column[regionNumber]) - HTuple(hv_Length1[regionNumber])),
//					((HTuple(hv_Length1[regionNumber]) * HTuple(hv_Length2[regionNumber])) * 2).TupleConcat((HTuple(hv_Column[regionNumber]) + HTuple(hv_Length1[regionNumber])) - 100));
//
//				CountObj(ho_SelectedResionsComplement, &hv_NumberNG);
//				//expendTime = getExpendTime(QStartCount);
//				//qDebug() << "task checkCigHoles expend" << expendTime << endl;
//
//				int NGNumber = hv_NumberNG.I();
//			}
//			if (0 != (int(hv_NumberNG > 0)))
//			{
//
//				HTuple hv_area, hv_row, hv_column;
//				if (nowCheckNumber == 1)
//				{
//					result1 = result1 | 0x00000002;//第二位置1，有内部缺陷
//				}
//				else
//				{
//					result2 = result2 | 0x00000002;//第二位置1，有内部缺陷
//				}
//				
//				AreaCenter(ho_SelectedResionsComplement, &hv_area, &hv_row, &hv_column);//仅用于显示
//
//			}
//			if (nowCheckNumber == 1 || (nowCheckNumber == 2 && resultCheckAgain == 0x000000004))
//			{
//				//搭口缺陷判断
//				HObject ho_div_region, ho_div_image, ho_div_thr_region, ho_gen_region, ho_diff_region, ho_diff_region_connection, ho_div_result;
//				HTuple hv_canny_region_left_up_Row, hv_canny_region_left_up_Column, hv_canny_region_right_down_Row, hv_canny_region_right_down_Column;
//				HTuple hv_out_Row, hv_out_Column, hv_out_phi, hv_out_length1, hv_out_length2, hv_div_result_number;
//				hv_canny_region_left_up_Row = 0;
//				hv_canny_region_left_up_Column = para1.divding_x1;
//				hv_canny_region_right_down_Row = ps->mv_frame->nHeight / 2;
//				hv_canny_region_right_down_Column = para1.divding_x2;
//				GenRectangle1(&ho_div_region, hv_canny_region_left_up_Row, hv_canny_region_left_up_Column, hv_canny_region_right_down_Row, hv_canny_region_right_down_Column);
//				ReduceDomain(ps->ho_Cam_Image, ho_div_region, &ho_div_image);
//				Threshold(ho_div_image, &ho_div_thr_region, para1.div_thr, 255);
//				SmallestRectangle2(ho_div_thr_region, &hv_out_Row, &hv_out_Column, &hv_out_phi, &hv_out_length1, &hv_out_length2);
//				GenRectangle2(&ho_gen_region, hv_out_Row, hv_out_Column, hv_out_phi, hv_out_length1, hv_out_length2);
//				Difference(ho_gen_region, ho_div_thr_region, &ho_diff_region);
//				Connection(ho_diff_region, &ho_diff_region_connection);
//				SelectShape(ho_diff_region_connection, &ho_div_result, "area", "and", para1.div_diff_area, 9999);
//				CountObj(ho_div_result, &hv_div_result_number);
//				
//				if (0 != (int(hv_div_result_number > 0)))//搭口缺陷
//				{
//					if (nowCheckNumber == 1)
//					{
//						result1 = result1 | 0x00000004;//第3位置1，有搭口缺陷
//					}
//					else
//					{
//						result2 = result2 | 0x00000004;//第3位置1，有搭口缺陷
//					}
//					
//				}
//				
//			}
//			
//		}
//	}
//	if (cigDownCheck)//检验下烟
//	{
//		int regionNumber = 0;//区域中目标在第几个
//		if (cigNumber == 2)
//		{
//			regionNumber = 1;//两支烟时取下面那支
//		}
//		//算法流程
//	}
//
//	return result1;
//}


LONGLONG process2_1::initTime() {
	LONGLONG Qpart1;
	QueryPerformanceFrequency(&litmp);
	dfFreq = (double)litmp.QuadPart;
	QueryPerformanceCounter(&litmp);
	Qpart1 = litmp.QuadPart;//开始计时
	return Qpart1;
}
LONGLONG process2_1::getExpendTime(LONGLONG startQpart)
{
	LONGLONG expendTime, Qpart2;//毫秒ms
	double dfMins, dfTime;
	QueryPerformanceCounter(&litmp);
	Qpart2 = litmp.QuadPart;//结束计时
	dfMins = (double)(Qpart2 - startQpart);
	dfTime = dfMins / dfFreq;
	expendTime = dfTime * 1000000;//us秒
	return expendTime;
}
