#include "pch.h"
#include "ImageProcess.h"
using namespace HalconCpp;

 /**
  * @brief 功能说明(不超过50字)
  * @param[in] param1 输入参数说明（注明单位/取值范围）
  * @param[out] param2 输出参数说明（需说明内存管理责任）
  * @param[in,out] param3 输入输出参数说明
  * @return 返回值说明（错误代码需对应错误码表）
  * @note 特别注意事项（线程安全/异常情况/性能警告）
  * @warning 重要警告信息（如不可重入等）
  * @since 版本号（例：v1.2.3）
  * @example 使用示例代码片段
  *
  * 典型DLL导出函数格式：
  * HRESULT APIENTRY MyFunction(__in DWORD dwParam, __out_opt LPSTR* pszResult);
  */



   /**
  * @brief 查找烟支左侧端点
  *@param[in] ho_image 输入图像
  * @param[in] rect_cneterX 中心点X坐标
  * @param[in] rect_cneterY 中心点Y坐标
  * @param[in] rect_width 宽度
  * @param[in] rect_height 高度
  * @param[in] sigma_code 西格玛值
  * @param[in] line_strength_start 边界强度起始值
  * @param[in] line_strength_step_length 边界强度步长
  * @param[out] left_pointX 左侧端点X坐标
  * @param[out] left_pointY 左侧端点Y坐标
  * @return 成功返回ErrorCode::Success，失败返回ErrorCode::Failed
  * @note 特别注意事项（线程安全/异常情况/性能警告）
  * @warning 重要警告信息（如不可重入等）
  * @since 版本号（例：v1.2.3）
  * @example 使用示例代码片段
  */
  IMAGEPROCESS_API ErrorCode FindLeftPoint(HObject ho_image, int rect_centerX, int rect_centerY, 
    int rect_width, int rect_height,int sigma_code, int line_strength_start, 
    int line_strength_step_length, int& left_pointX, int& left_pointY)
  {
    try
    {
        HTuple hv_image_width, hv_image_height;
        GetImageSize(ho_image, &hv_image_width, &hv_image_height);
        // 创建矩形区域
        HObject rect_region;
        GenRectangle2(&rect_region, rect_centerY, rect_centerX, 0, rect_width/2.0, rect_height/2.0);

        // 创建测量对象
        HTuple measureHandle;
        GenMeasureRectangle2(rect_centerY, rect_centerX, 0, rect_width/2.0, rect_height/2.0, 
            hv_image_width, hv_image_height, "nearest_neighbor", &measureHandle);

        HTuple hv_rowEdge, hv_columnEdge, hv_amplitude, hv_distance;
        HTuple hv_lineStrength = line_strength_start;

        // 循环查找边缘直到找到
        do {
            MeasurePos(ho_image, measureHandle, sigma_code, hv_lineStrength, "positive", "all", 
                &hv_rowEdge, &hv_columnEdge, &hv_amplitude, &hv_distance);
            hv_lineStrength += line_strength_step_length;
        } while (0 == (int(hv_rowEdge.TupleLength()) == 1));

        // 如果找到边缘点
        if (hv_rowEdge.TupleLength() == 1) {
            left_pointX = (int)hv_columnEdge[0].D();
            left_pointY = (int)hv_rowEdge[0].D();
            return ErrorCode::Success;
        }

        return ErrorCode::Failed;
    }
    catch (HException& exception)
    {
        OutputDebugStringA(exception.ErrorMessage().Text());
        return ErrorCode::Failed;
    }
  } 
/**
  * @brief 查找烟棒上下边界点（三组点与标准模版差值不超过阈值）
  *@param[in] ho_image 输入图像
  * @param[in] left_pointX 左侧端点X坐标
  * @param[in] left_pointY  左侧端点Y坐标
  * @param[in] rects_1st_distance 第一个矩形框到端点的水平距离
  * @param[in] rects_distance 矩形框之间的距离
  * @param[in] rects_height 矩形框的高度
  * @param[in] rects_width 矩形框的宽度
  * @param[in] sigma 西格玛值
  * @param[in] min_threshold 最小边界振幅
  * @param[in] model_int_rects3_array[3][3] 模版矩形数组，width,Y1,Y2
  * @param[in] set_new_model 设置新模版
  * @param[out] side_pointY1 上边界点Y1坐标
  * @param[out] side_pointY2 下边界点Y2坐标
  * @param[out] side_width 上下边界间宽度
  * @param[out] model_out_rects3_array[3][3] 模版矩形数组，width,Y1,Y2
  * @return 成功返回ErrorCode::Success，失败返回ErrorCode::Failed
  * @note 特别注意事项（线程安全/异常情况/性能警告）
  * @warning 重要警告信息（如不可重入等）
  * @since 版本号（例：v1.2.3）
  * @example 使用示例代码片段
  */
  IMAGEPROCESS_API ErrorCode FindSidePoint(HObject ho_image, int left_pointX, int left_pointY,
      int rects_1st_distance, int rects_distance, int rects_height, int rects_width, int sigma,
      int min_threshold, int model_int_rects3_array[3][3], bool set_new_model, int& side_pointY1, int& side_pointY2,
      int& side_width, int(& model_out_rects3_array)[3][3])
  {
    try
    {   
		HTuple hv_image_width, hv_image_height;
		GetImageSize(ho_image, &hv_image_width, &hv_image_height);

        // 创建测量对象
        HObject rect_region;
        HTuple measureHandle1, measureHandle2, measureHandle3;
        HTuple hv_MeasurePairs1_RowEdgeFirst1, hv_MeasurePairs1_ColumnEdgeFirst1, hv_MeasurePairs1_AmplitudeFirst1, hv_MeasurePairs1_RowEdgeSecond1, hv_MeasurePairs1_ColumnEdgeSecond1, hv_MeasurePairs1_AmplitudeSecond1, hv_MeasurePairs1_IntraDistance1_1, hv_MeasurePairs1_InterDistance1_2;
        HTuple hv_MeasurePairs2_RowEdgeFirst1, hv_MeasurePairs2_ColumnEdgeFirst1, hv_MeasurePairs2_AmplitudeFirst1, hv_MeasurePairs2_RowEdgeSecond1, hv_MeasurePairs2_ColumnEdgeSecond1, hv_MeasurePairs2_AmplitudeSecond1, hv_MeasurePairs2_IntraDistance1_1, hv_MeasurePairs2_InterDistance1_2;
        HTuple hv_MeasurePairs3_RowEdgeFirst1, hv_MeasurePairs3_ColumnEdgeFirst1, hv_MeasurePairs3_AmplitudeFirst1, hv_MeasurePairs3_RowEdgeSecond1, hv_MeasurePairs3_ColumnEdgeSecond1, hv_MeasurePairs3_AmplitudeSecond1, hv_MeasurePairs3_IntraDistance1_1, hv_MeasurePairs3_InterDistance1_2;
		GenMeasureRectangle2(left_pointY, left_pointX + rects_1st_distance, HTuple(90).TupleRad(),
            rects_height/2.0, rects_width/2.0, hv_image_width, hv_image_height, "nearest_neighbor", &measureHandle1);
        GenMeasureRectangle2(left_pointY, left_pointX + rects_1st_distance + rects_distance, HTuple(90).TupleRad(),
            rects_height / 2.0, rects_width / 2.0, hv_image_width, hv_image_height, "nearest_neighbor", &measureHandle2);
        GenMeasureRectangle2(left_pointY, left_pointX + rects_1st_distance + 2*rects_distance , HTuple(90).TupleRad(),
            rects_height / 2.0, rects_width / 2.0, hv_image_width, hv_image_height, "nearest_neighbor", &measureHandle3);
        MeasurePairs(ho_image, measureHandle1, sigma, min_threshold, 
      "positive", "all", &hv_MeasurePairs1_RowEdgeFirst1, &hv_MeasurePairs1_ColumnEdgeFirst1, &hv_MeasurePairs1_AmplitudeFirst1, 
      &hv_MeasurePairs1_RowEdgeSecond1, &hv_MeasurePairs1_ColumnEdgeSecond1, &hv_MeasurePairs1_AmplitudeSecond1, &hv_MeasurePairs1_IntraDistance1_1, 
      &hv_MeasurePairs1_InterDistance1_2); 
      MeasurePairs(ho_image, measureHandle2, sigma, min_threshold, 
      "positive", "all", &hv_MeasurePairs2_RowEdgeFirst1, &hv_MeasurePairs2_ColumnEdgeFirst1, &hv_MeasurePairs2_AmplitudeFirst1, 
      &hv_MeasurePairs2_RowEdgeSecond1, &hv_MeasurePairs2_ColumnEdgeSecond1, &hv_MeasurePairs2_AmplitudeSecond1, &hv_MeasurePairs2_IntraDistance1_1, 
      &hv_MeasurePairs2_InterDistance1_2); 
      MeasurePairs(ho_image, measureHandle3, sigma, min_threshold, 
      "positive", "all", &hv_MeasurePairs3_RowEdgeFirst1, &hv_MeasurePairs3_ColumnEdgeFirst1, &hv_MeasurePairs3_AmplitudeFirst1, 
      &hv_MeasurePairs3_RowEdgeSecond1, &hv_MeasurePairs3_ColumnEdgeSecond1, &hv_MeasurePairs3_AmplitudeSecond1, &hv_MeasurePairs3_IntraDistance1_1, 
      &hv_MeasurePairs3_InterDistance1_2); 
      //if(0 == (int(hv_MeasurePairs1_IntraDistance1_1.TupleLength()) == 1))
      int measurePairs1_number, measurePairs2_number, measurePairs3_number=0;
      int measurePairs1_Y1, measurePairs1_Y2,measurePairs1_width, measurePairs2_Y1, measurePairs2_Y2, measurePairs2_width, measurePairs3_Y1, measurePairs3_Y2, measurePairs3_width=0  ;
      measurePairs1_number = hv_MeasurePairs1_IntraDistance1_1.TupleLength();
      measurePairs2_number = hv_MeasurePairs2_IntraDistance1_1.TupleLength();
      measurePairs3_number = hv_MeasurePairs3_IntraDistance1_1.TupleLength();
      if(measurePairs1_number == 0 && measurePairs2_number == 0 && measurePairs3_number == 0)
      {
        return ErrorCode::Failed;//未找到烟棒上下边界点
      }
      if(measurePairs1_number == 1)
      {
        measurePairs1_Y1 = (int)hv_MeasurePairs1_RowEdgeFirst1[0].D();
        measurePairs1_Y2 = (int)hv_MeasurePairs1_RowEdgeSecond1[0].D();
        measurePairs1_width = (int)hv_MeasurePairs1_IntraDistance1_1[0].D();
      }
      if(measurePairs2_number == 1)
      {
        measurePairs2_Y1 = (int)hv_MeasurePairs2_RowEdgeFirst1[0].D();
        measurePairs2_Y2 = (int)hv_MeasurePairs2_RowEdgeSecond1[0].D();
        measurePairs2_width = (int)hv_MeasurePairs2_IntraDistance1_1[0].D();
      }
      if(measurePairs3_number == 1)
      {
        measurePairs3_Y1 = (int)hv_MeasurePairs3_RowEdgeFirst1[0].D();
        measurePairs3_Y2 = (int)hv_MeasurePairs3_RowEdgeSecond1[0].D();
        measurePairs3_width = (int)hv_MeasurePairs3_IntraDistance1_1[0].D();
      }
      
      if(measurePairs1_number > 1)
      {
        int temp_Y1 = 0;
        int temp_measurePairs1_Y1=0;
        int temp_measurePairs1_Y2=0;
        int temp_measurePairs1_width=0;
        for(int i = 0; i < measurePairs1_number; i++)//遍历所有边缘对
        {
            temp_Y1 = (int)hv_MeasurePairs1_IntraDistance1_1[i].D();//获取边缘对第i个边缘对内宽度
            if (model_int_rects3_array[0][0] == 0 and temp_Y1>30)//如果模版矩形1边缘对宽度为0，没有模版宽度，且边缘对内宽度大于30
            {
                temp_measurePairs1_Y1 = (int)hv_MeasurePairs1_RowEdgeFirst1[i].D();
                temp_measurePairs1_Y2 = (int)hv_MeasurePairs1_RowEdgeSecond1[i].D();
                temp_measurePairs1_width = (int)hv_MeasurePairs1_IntraDistance1_1[i].D();
                break;
            }
            if(model_int_rects3_array[0][0] >0 and (float)(abs(temp_Y1 - model_int_rects3_array[0][0]) )/(float)(model_int_rects3_array[0][0])<0.2)//有模版，按模版大小的80%计算
            {
                temp_measurePairs1_Y1 = (int)hv_MeasurePairs1_RowEdgeFirst1[i].D();
                temp_measurePairs1_Y2 = (int)hv_MeasurePairs1_RowEdgeSecond1[i].D();
                temp_measurePairs1_width = (int)hv_MeasurePairs1_IntraDistance1_1[i].D();
                break;
            }
        }
        measurePairs1_Y1 = temp_measurePairs1_Y1;
        measurePairs1_Y2 = temp_measurePairs1_Y2;
        measurePairs1_width = temp_measurePairs1_width;
      }
      if(measurePairs2_number > 1)
      {
        int temp_Y1 = 0;
        int temp_measurePairs2_Y1=0;
        int temp_measurePairs2_Y2=0;
        int temp_measurePairs2_width=0;
        for(int i = 0; i < measurePairs2_number; i++)//遍历所有边缘对
        {
            temp_Y1 = (int)hv_MeasurePairs2_IntraDistance1_1[i].D();//获取边缘对第i个边缘对内宽度
            if (model_int_rects3_array[1][0] ==0 and temp_Y1>30)//如果模版矩形2边缘对宽度为0，没有模版宽度，且边缘对内宽度大于30
            {
                temp_measurePairs2_Y1 = (int)hv_MeasurePairs2_RowEdgeFirst1[i].D();
                temp_measurePairs2_Y2 = (int)hv_MeasurePairs2_RowEdgeSecond1[i].D();
                temp_measurePairs2_width = (int)hv_MeasurePairs2_IntraDistance1_1[i].D();
                break;
            }
            if(model_int_rects3_array[1][0] >0 and (float)(abs(temp_Y1 - model_int_rects3_array[1][0]) )/(float)(model_int_rects3_array[1][0])<0.2)//有模版，按模版大小的80%计算
            {
                temp_measurePairs2_Y1 = (int)hv_MeasurePairs2_RowEdgeFirst1[i].D();
                temp_measurePairs2_Y2 = (int)hv_MeasurePairs2_RowEdgeSecond1[i].D();
                temp_measurePairs2_width = (int)hv_MeasurePairs2_IntraDistance1_1[i].D();
                break;
            }
        }
        measurePairs2_Y1 = temp_measurePairs2_Y1;
        measurePairs2_Y2 = temp_measurePairs2_Y2;
        measurePairs2_width = temp_measurePairs2_width;
      }
      if(measurePairs3_number > 1)
      {
        int temp_Y1 = 0;
        int temp_measurePairs3_Y1=0;
        int temp_measurePairs3_Y2=0;
        int temp_measurePairs3_width=0;
        for(int i = 0; i < measurePairs3_number; i++)//遍历所有边缘对
        {
            temp_Y1 = (int)hv_MeasurePairs3_IntraDistance1_1[i].D();//获取边缘对第i个边缘对内宽度
            if (model_int_rects3_array[2][0] ==0 and temp_Y1>30)//如果模版矩形3边缘对宽度为0，没有模版宽度，且边缘对内宽度大于30
            {
                temp_measurePairs3_Y1 = (int)hv_MeasurePairs3_RowEdgeFirst1[i].D();
                temp_measurePairs3_Y2 = (int)hv_MeasurePairs3_RowEdgeSecond1[i].D();
                temp_measurePairs3_width = (int)hv_MeasurePairs3_IntraDistance1_1[i].D();
                break;
            }
            if(model_int_rects3_array[2][0] >0 and (float)(abs(temp_Y1 - model_int_rects3_array[2][0]) )/(float)(model_int_rects3_array[2][0])<0.2)//有模版，按模版大小的80%计算
            {
                temp_measurePairs3_Y1 = (int)hv_MeasurePairs3_RowEdgeFirst1[i].D();
                temp_measurePairs3_Y2 = (int)hv_MeasurePairs3_RowEdgeSecond1[i].D();
                temp_measurePairs3_width = (int)hv_MeasurePairs3_IntraDistance1_1[i].D();
                break;
            }
        }
        measurePairs3_Y1 = temp_measurePairs3_Y1;
        measurePairs3_Y2 = temp_measurePairs3_Y2;
        measurePairs3_width = temp_measurePairs3_width;
      }
      if(measurePairs1_Y1>0)
      {
        side_pointY1 = measurePairs1_Y1;
        side_pointY2 = measurePairs1_Y2;
        side_width = measurePairs1_width;
        if(set_new_model)
        {
            model_out_rects3_array[0][0] = measurePairs1_width;
            model_out_rects3_array[0][1] = measurePairs1_Y1;
            model_out_rects3_array[0][2] = measurePairs1_Y2;
        }
      }else if(measurePairs2_Y1>0)
      {
        side_pointY1 = measurePairs2_Y1;
        side_pointY2 = measurePairs2_Y2;
        side_width = measurePairs2_width;
        if(set_new_model)
        {
            model_out_rects3_array[1][0] = measurePairs2_width;
            model_out_rects3_array[1][1] = measurePairs2_Y1;
            model_out_rects3_array[1][2] = measurePairs2_Y2;
        }
      }else if(measurePairs3_Y1>0)  
      {
        side_pointY1 = measurePairs3_Y1;
        side_pointY2 = measurePairs3_Y2;
        side_width = measurePairs3_width;
        if(set_new_model)
        {
            model_out_rects3_array[2][0] = measurePairs3_width;
            model_out_rects3_array[2][1] = measurePairs3_Y1;
            model_out_rects3_array[2][2] = measurePairs3_Y2;
        }
      }
      else
      {
        return ErrorCode::Failed;//未找到烟棒上下边界点
      }
   
    }
    catch (HException& exception)
    {
        OutputDebugStringA(exception.ErrorMessage().Text());
        return ErrorCode::Failed;
    }
    return ErrorCode::Success;
  }

  /**
  * @brief 烟棒区域暗点检测
  *@param[in] ho_image 输入图像
  * @param[in] left_pointX 左侧端点X坐标
  * @param[in] left_pointY  左侧端点Y坐标
  * @param[in] up_Y1 上点Y坐标
  * @param[in] down_Y2 下点Y坐标
  * @param[in] upCigBodyPositionParams 上烟检测区域
  * @param[in] downCigBodyPositionParams 下烟检测区域
  * @param[in] upCigStickDarkDefectParams 上烟棒暗点检测参数
  * @param[in] downCigStickDarkDefectParams 下烟棒暗点检测参数
  * @param[out] rects 缺陷标记框
  * @return 成功返回ErrorCode::Success，失败返回ErrorCode::Failed
  * @note 特别注意事项（线程安全/异常情况/性能警告）
  * @warning 重要警告信息（如不可重入等）
  * @since 版本号（例：v1.2.3）
  * @example 使用示例代码片段
  */
  IMAGEPROCESS_API ErrorCode StickDarkCheck(HObject ho_image, int left_pointX, int left_pointY, int up_Y1, int down_Y2,
      UpCigBodyPositionParams* upCigBodyPositionParams, DownCigBodyPositionParams* downCigBodyPositionParams,
      UpCigStickDarkDefectParams* upCigStickDarkDefectParams, DownCigStickDarkDefectParams* downCigStickDarkDefectParams,
      std::list<myRect>& rects) {
      try {
          HTuple width, height;
          HObject ho_rectangle,ho_reduceImage, ho_imageReduce, ho_imageMean, ho_RegionsDynThreshold, ho_RegionsConections, ho_SelectedRegions;
          GetImageSize(ho_image, &width, &height);
          int row1, column1, row2, column2;
          if (up_Y1 > down_Y2)//参数数据异常
          {
              return ErrorCode::Failed;
          }
          if (up_Y1 + upCigBodyPositionParams->innerEdgeThreshold< height)
          {
              row1 = up_Y1 + upCigBodyPositionParams->innerEdgeThreshold;
          }
          else
          {
              row1 = up_Y1;
          }
          if (left_pointX + upCigBodyPositionParams->bodyStartDistance < width)
          {
              column1 = left_pointX + upCigBodyPositionParams->bodyStartDistance;
          }
          else
          {
              column1 = left_pointX;
          }
          if (down_Y2> upCigBodyPositionParams->innerEdgeThreshold)
          {
              row2 = down_Y2 - upCigBodyPositionParams->innerEdgeThreshold;
          }
          else
          {
              row2 = down_Y2;
          }
          if (left_pointX + upCigBodyPositionParams->bodyStartDistance + upCigBodyPositionParams->bodyLength < width)
          {
              column2 = left_pointX + upCigBodyPositionParams->bodyStartDistance + upCigBodyPositionParams->bodyLength;
          }
          else
          {
              return ErrorCode::Failed;//参数数据异常
          }

          GenRectangle1(&ho_rectangle, row1, column1, row2, column2);
          ReduceDomain(ho_image, ho_rectangle, &ho_imageReduce);
          MeanImage(ho_imageReduce, &ho_imageMean, upCigStickDarkDefectParams->maskWidth, upCigStickDarkDefectParams->maskWidth);
          DynThreshold(ho_imageReduce, ho_imageMean, &ho_RegionsDynThreshold, upCigStickDarkDefectParams->offest, "dark");
          Connection(ho_RegionsDynThreshold, &ho_RegionsConections);
          SelectShape(ho_RegionsConections, &ho_SelectedRegions, "area", "and", upCigStickDarkDefectParams->darkPointAreasValue, 99999);
          return ErrorCode::Failed;
      }
      catch (HException& exception)
      {
          OutputDebugStringA(exception.ErrorMessage().Text());
          return ErrorCode::Failed;
      }
  }


// 辅助函数实现
IMAGEPROCESS_API bool IsValidImage(const HObject& image) {
    try {
        HTuple width, height;
        GetImageSize(image, &width, &height);
        return true;
    }
    catch (HException&) {
        return false;
    }
}

// 初始化函数实现
IMAGEPROCESS_API ErrorCode Initialize() {
    try {
        // 在这里进行必要的初始化
        // 例如：加载模型、初始化参数等
        return ErrorCode::Success;
    }
    catch (HException& exception) {
        // 处理HALCON异常
        OutputDebugStringA(exception.ErrorMessage().Text());
        return ErrorCode::Failed;
    }
}

// 图像处理函数实现
IMAGEPROCESS_API ErrorCode ProcessImage(const HObject& inputImage, HObject& outputImage) {
    try {
        // 检查输入图像是否有效
        if (!IsValidImage(inputImage)) {
            return ErrorCode::InvalidImage;
        }

        // 在这里添加你的图像处理代码
        // 例如：
        CopyImage(inputImage, &outputImage);
        // 添加更多处理步骤...

        return ErrorCode::Success;
    }
    catch (HException& exception) {
        OutputDebugStringA(exception.ErrorMessage().Text());
        return ErrorCode::Failed;
    }
}

// 清理函数实现
IMAGEPROCESS_API void Cleanup() {
    try {
        // 在这里进行资源清理
        // 例如：释放内存、关闭文件等
    }
    catch (HException& exception) {
        OutputDebugStringA(exception.ErrorMessage().Text());
    }
} 