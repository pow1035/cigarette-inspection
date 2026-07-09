#pragma once

#include <opencv2/opencv.hpp>
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime.h>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>

struct Detection {
    cv::Rect box;
    float confidence;
    int class_id;
};

// TensorRT Logger
class Logger : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cout << "[TRT] " << msg << std::endl;
        }
    }
};

class YOLOTensorRTv2 {
public:
    /**
     * Constructor - supports both .engine and .onnx files
     * @param model_path Path to .engine or .onnx file
     * @param input_size Input size (default 992)
     * @param conf_thresh Confidence threshold
     * @param iou_thresh NMS IOU threshold
     */
    YOLOTensorRTv2(const std::string& model_path,
                   int input_size = 992,
                   float conf_thresh = 0.25f,
                   float iou_thresh = 0.45f);

    ~YOLOTensorRTv2();

    std::vector<Detection> detect(const cv::Mat& image);
    void setClassNames(const std::vector<std::string>& names) { class_names_ = names; }
    std::vector<std::string> getClassNames() const { return class_names_; }

private:
    // Build engine from ONNX
    bool buildEngineFromOnnx(const std::string& onnx_path);

    // Load pre-built engine
    bool loadEngine(const std::string& engine_path);

    // Save engine to file
    bool saveEngine(const std::string& engine_path);

    // Preprocessing
    void preprocess(const cv::Mat& image, float* input_buffer);

    // Postprocessing
    std::vector<Detection> postprocess(const float* output, int orig_width, int orig_height);

    // NMS
    void nms(std::vector<Detection>& detections, float iou_threshold);
    float iou(const cv::Rect& a, const cv::Rect& b);

private:
    Logger logger_;
    std::unique_ptr<nvinfer1::IRuntime> runtime_;
    std::unique_ptr<nvinfer1::ICudaEngine> engine_;
    std::unique_ptr<nvinfer1::IExecutionContext> context_;

    void* buffers_[2];  // input and output buffers
    cudaStream_t stream_;

    int input_size_;
    float conf_threshold_;
    float iou_threshold_;
    std::vector<std::string> class_names_;

    size_t input_buffer_size_;
    size_t output_buffer_size_;

    // YOLO output dimensions
    int num_classes_;      // Number of classes (e.g., 80 for COCO)
    int num_anchors_;      // Number of anchor points (e.g., 8400)
    int output_features_;  // Output feature dimension (e.g., 84 = 4 + 80)

    bool initialized_;
};
