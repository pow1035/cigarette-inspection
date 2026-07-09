#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <memory>

struct Detection {
    cv::Rect box;
    float confidence;
    int class_id;
};

class YOLOOnnx {
public:
    YOLOOnnx(const std::string& model_path,
             int input_size = 992,
             float conf_thresh = 0.25f,
             float iou_thresh = 0.45f);
    ~YOLOOnnx();

    std::vector<Detection> detect(const cv::Mat& image);
    void setClassNames(const std::vector<std::string>& names);
    std::vector<std::string> getClassNames() const { return class_names_; }

private:
    void preprocess(const cv::Mat& image, std::vector<float>& input_tensor);
    std::vector<Detection> postprocess(const std::vector<float>& output,
                                       int orig_width, int orig_height);
    void nms(std::vector<Detection>& detections, float iou_threshold);
    float iou(const cv::Rect& a, const cv::Rect& b);

private:
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    Ort::SessionOptions session_options_;

    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
    std::vector<int64_t> input_shape_;
    std::vector<int64_t> output_shape_;

    int input_size_;
    float conf_threshold_;
    float iou_threshold_;
    std::vector<std::string> class_names_;
};
