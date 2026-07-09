/**
 * YOLO26 TensorRT C++ 推理引擎
 *
 * 支持:
 * - TensorRT FP16推理
 * - 自动NMS后处理
 * - 高性能图像预处理
 * - 批量推理支持
 */

#ifndef YOLO_TENSORRT_H
#define YOLO_TENSORRT_H

#include <opencv2/opencv.hpp>
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime.h>
#include <vector>
#include <string>
#include <memory>
#include <fstream>

// 检测结果结构
struct Detection {
    cv::Rect box;          // 边界框
    float confidence;      // 置信度
    int class_id;          // 类别ID
    std::string class_name; // 类别名称
};

// TensorRT日志类
class Logger : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cout << msg << std::endl;
        }
    }
} gLogger;


class YOLOTensorRT {
public:
    /**
     * 构造函数
     * @param engine_path TensorRT引擎文件路径
     * @param input_size 输入图像尺寸 (默认640)
     * @param conf_thresh 置信度阈值 (默认0.25)
     * @param iou_thresh NMS IoU阈值 (默认0.45)
     */
    YOLOTensorRT(const std::string& engine_path,
                 int input_size = 640,
                 float conf_thresh = 0.25f,
                 float iou_thresh = 0.45f);

    ~YOLOTensorRT();

    /**
     * 初始化引擎
     * @return 是否成功
     */
    bool initialize();

    /**
     * 推理单张图像
     * @param image 输入图像
     * @return 检测结果
     */
    std::vector<Detection> detect(const cv::Mat& image);

    /**
     * 批量推理
     * @param images 输入图像列表
     * @return 每张图像的检测结果
     */
    std::vector<std::vector<Detection>> detectBatch(const std::vector<cv::Mat>& images);

    /**
     * 设置类别名称
     * @param names 类别名称列表
     */
    void setClassNames(const std::vector<std::string>& names) {
        class_names_ = names;
    }

    /**
     * 获取性能统计
     */
    void printPerformanceStats() const;

private:
    // TensorRT相关
    std::string engine_path_;
    nvinfer1::ICudaEngine* engine_ = nullptr;
    nvinfer1::IExecutionContext* context_ = nullptr;

    // 输入输出缓冲区
    void* buffers_[2];  // 0=input, 1=output
    cudaStream_t stream_;

    // 模型参数
    int input_size_;
    int input_h_;
    int input_w_;
    int input_c_ = 3;
    int output_size_;
    int num_classes_ = 80;  // 根据实际模型修改

    // 推理参数
    float conf_thresh_;
    float iou_thresh_;
    std::vector<std::string> class_names_;

    // 性能统计
    mutable int inference_count_ = 0;
    mutable double total_preprocess_time_ = 0;
    mutable double total_inference_time_ = 0;
    mutable double total_postprocess_time_ = 0;

    // 内部函数
    bool loadEngine(const std::string& engine_path);
    cv::Mat preprocessImage(const cv::Mat& image);
    std::vector<Detection> postprocess(float* output, int output_size,
                                       const cv::Mat& original_image);
    float computeIoU(const cv::Rect& box1, const cv::Rect& box2);
    std::vector<int> nms(const std::vector<Detection>& detections);
};

#endif // YOLO_TENSORRT_H
