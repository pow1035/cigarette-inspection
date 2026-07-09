#pragma once

// 参数结构体 - 烟端定位
struct UpCigTopPositionParams {
	int centerX, centerY, width, height;           // 端点坐标
	//double x1, y1, x2, y2;           // 端点坐标
	int sigmaCode;                // 端西格玛值
	int edgeGradientStartThreshold;    // 端边界起始梯度阈值
	int stepGradientThreshold;    // 端边界梯度阈值(步进值)
};
struct DownCigTopPositionParams {
	int centerX, centerY, width, height;           // 端点坐标
	//double x1, y1, x2, y2;           // 端点坐标
	int sigmaCode;                // 端西格玛值
	int edgeGradientStartThreshold;    // 端边界起始梯度阈值
	int stepGradientThreshold;    // 端边界梯度阈值(步进值)
};

// 参数结构体 - 烟端模版计算参数
struct UpCigTopModelPositionParams {
	int modelPositionX;        // 模版计算端点坐标X
	int modelPositionY;        // 模版计算端点坐标Y
	int modelWidth;             // 模版计算烟宽度
};
struct DownCigTopModelPositionParams {
	int modelPositionX;        // 模版计算端点坐标X
	int modelPositionY;        // 模版计算端点坐标Y
	int modelWidth;             // 模版计算烟宽度
};

// 参数结构体 - 框定位
struct UpBoxPositionParams {
	int frameStartDistance;       // 框1起始距离
	int framesHorizontalDistance;// 框到框水平距离
	int frameHeight;              // 框高度
	int frameWidth;               // 框宽度
	int frameSigmaCode;          // 框西格玛值
	int frameEdgeGradientThreshold; // 框边界梯度阈值
};
struct DownBoxPositionParams {
	int frameStartDistance;       // 框1起始距离
	int framesHorizontalDistance;// 框到框水平距离
	int frameHeight;              // 框高度
	int frameWidth;               // 框宽度
	int frameSigmaCode;          // 框西格玛值
	int frameEdgeGradientThreshold; // 框边界梯度阈值
};

//参数结构体 - 框定位模版
struct UpBoxModelPositionParams {
	int model1PositionX;        // 模版1计算端点坐标X
	int model1PositionY;        // 模版1计算端点坐标Y
	int model1Width;             // 模版1计算烟宽度
	int model2PositionX;        // 模版计算端点坐标X
	int model2PositionY;        // 模版计算端点坐标Y
	int model2Width;             // 模版计算烟宽度
	int model3PositionX;        // 模版计算端点坐标X    
	int model3PositionY;        // 模版计算端点坐标Y
	int model3Width;             // 模版计算烟宽度
};
struct DownBoxModelPositionParams {
	int model1PositionX;        // 模版计算端点坐标X
	int model1PositionY;        // 模版计算端点坐标Y
	int model1Width;             // 模版计算烟宽度
	int model2PositionX;        // 模版计算端点坐标X
	int model2PositionY;        // 模版计算端点坐标Y
	int model2Width;             // 模版计算烟宽度
	int model3PositionX;        // 模版计算端点坐标X    
	int model3PositionY;        // 模版计算端点坐标Y
	int model3Width;             // 模版计算烟宽度
};


// 参数结构体 - 上烟体定位
struct UpCigBodyPositionParams {
	int bodyStartDistance;       // 烟体起始距离
	int bodyLength;             // 烟体长度
	int innerEdgeThreshold;     // 内边缘阈值
};
// 参数结构体 - 下烟体定位
struct DownCigBodyPositionParams {
	int bodyStartDistance;       // 烟体起始距离
	int bodyLength;             // 烟体长度
	int innerEdgeThreshold;     // 内边缘阈值
};

// 参数结构体 - 上嘴棒定位
struct UpFilterPositionParams {
	int filterStartDistance;    // 嘴棒起始距离
	int filterLength;          // 嘴棒长度
	int innerEdgeThreshold;    // 内边缘阈值
};
// 参数结构体 - 上嘴棒定位
struct DownFilterPositionParams {
	int filterStartDistance;    // 嘴棒起始距离
	int filterLength;          // 嘴棒长度
	int innerEdgeThreshold;    // 内边缘阈值
};

// 参数结构体 - 上拼接定位
struct UpJointPositionParams {
	int jointStartDistance;       // 拼接到端距离
	int jointLength;              // 拼接框长
	bool isLinear;                   // 直线查找?
	bool isTopDown;                  // 顶帽/底帽算法?
	bool isDeepLearning;            // 深度学习?
};
// 参数结构体 - 下拼接定位
struct DownJointPositionParams {
	int jointStartDistance;       // 拼接到端距离
	int jointLength;              // 拼接框长
	bool isLinear;                   // 直线查找?
	bool isTopDown;                  // 顶帽/底帽算法?
	bool isDeepLearning;            // 深度学习?
};

// 参数结构体 - 上烟体缺陷
struct UpCigStickDarkDefectParams {
	int darkPointAreasValue;    // 暗点面积值
	int darkPointGrayValue;     // 暗点灰度值

	int maskWidth;				//滤波器宽度
	int offest;					//允许偏差
};
// 参数结构体 - 下烟体缺陷
struct DownCigStickDarkDefectParams {
	int darkPointAreasValue;    // 暗点面积值
	int darkPointGrayValue;     // 暗点灰度值
};

// 参数结构体 - 上嘴棒暗缺陷
struct UpCigFilterDarkDefectParams {
	int darkPointAreasValue;    // 暗点面积值
	int darkPointGrayValue;     // 暗点灰度值
};
// 参数结构体 - 下嘴棒暗缺陷
struct DownCigFilterDarkDefectParams {
	int darkPointAreasValue;    // 暗点面积值
	int darkPointGrayValue;     // 暗点灰度值
};

// 参数结构体 - 上滤嘴亮点检测
struct UpCigFilterWhiteDefectParams {
	int brightPointAreasValue;  // 亮点面积值
	int brightPointGrayValue;   // 亮点灰度值
};
// 参数结构体 - 下滤嘴亮点检测
struct DownCigFilterWhiteDefectParams {
	int brightPointAreasValue;  // 亮点面积值
	int brightPointGrayValue;   // 亮点灰度值
};

// 拼接搓牙检测参数结构体
struct UpJointDefectParams {
	int defectArea;            // 缺陷面积
};
struct DownJointDefectParams {
	int defectArea;            // 缺陷面积
};

// 烟支轮廓缺陷参数结构体
struct UpOutDefectParams {
	int outPix;            // 向外扩展像素
	double rectangularity;  //矩形度 
	double convexity;	   //凸度
};
struct DownOutDefectParams {
	int outPix;            // 向外扩展像素
	double rectangularity;  //矩形度 
	double convexity;	   //凸度
};

// 深度学习检测参数结构体
struct DeepLearningParams {
	double jointRollThreshold;      // 搭口搓牙阈值
	double flyingTobaccoThreshold;  // 飞烟阈值
	double tobaccoClipsThreshold;   // 夹末阈值
	double filterWrinkleThreshold;  // 滤嘴皱褶阈值
	double missingFilterThreshold;  // 缺嘴阈值
	double rodDamageThreshold;      // 烟棒破损阈值
	double rodStainThreshold;       // 烟棒脏污阈值
};


//通用结构体
struct myRect {
	int x;
	int y;
	int width;
	int height;
};
