#pragma once

#ifndef IMAGEPROCESS_EXPORTS
    #define IMAGEPROCESS_API __declspec(dllexport)
   
#endif
// 错误代码定义
enum class ErrorCode {
	Success = 0,
	Failed = -1,
	InvalidImage = -2,
	// 可以添加更多错误码
};
#include<paramStructs.h>
#include<list>
#include <HalconCpp.h>
using namespace HalconCpp;

// 辅助函数声明
IMAGEPROCESS_API bool IsValidImage(const HalconCpp::HObject& image);

// 导出函数声明
extern "C" {
    // 初始化函数
    IMAGEPROCESS_API ErrorCode Initialize();
    
    // 图像处理函数示例
    IMAGEPROCESS_API ErrorCode ProcessImage(const HalconCpp::HObject& inputImage, HalconCpp::HObject& outputImage);
    
    // 释放资源函数
    IMAGEPROCESS_API void Cleanup();
    //算法函数-烟棒左端点
    IMAGEPROCESS_API ErrorCode FindLeftPoint(HObject ho_image, int rect_centerX, int rect_centerY, 
        int rect_width, int rect_height,int sigma_code, int line_strength_start, 
        int line_strength_step_length, int& left_pointX, int& left_pointY);
    //算法函数-烟棒上下边界点
    IMAGEPROCESS_API ErrorCode FindSidePoint(HObject ho_image, int left_pointX, int left_pointY,
        int rects_1st_distance, int rects_distance, int rects_height, int rects_width, int sigma,
        int min_threshold, int model_int_rects3_array[3][3], bool set_new_model, int& side_pointY1, int& side_pointY2,
        int& side_width, int(&model_out_rects3_array)[3][3]);

    //算法函数-烟棒黑点检测
    IMAGEPROCESS_API ErrorCode StickDarkCheck(HObject ho_image, int left_pointX, int left_pointY, int up_Y1, int down_Y2,
        UpCigBodyPositionParams* upCigBodyPositionParams, DownCigBodyPositionParams* downCigBodyPositionParams,
        UpCigStickDarkDefectParams* upCigStickDarkDefectParams, DownCigStickDarkDefectParams* downCigStickDarkDefectParams,
        std::list<myRect>& rects);
}; 