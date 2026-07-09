#include "yolo_onnx.h"
#include <algorithm>
#include <numeric>
#include <iostream>

YOLOOnnx::YOLOOnnx(const std::string& model_path,
                   int input_size,
                   float conf_thresh,
                   float iou_thresh)
    : input_size_(input_size),
      conf_threshold_(conf_thresh),
      iou_threshold_(iou_thresh) {

    // Initialize ONNX Runtime environment
    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "YOLOInference");

    // Configure session options
    session_options_.SetIntraOpNumThreads(4);
    session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // Enable CUDA provider if available
    OrtCUDAProviderOptions cuda_options;
    cuda_options.device_id = 0;
    cuda_options.arena_extend_strategy = 0;
    cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
    cuda_options.do_copy_in_default_stream = 1;
    session_options_.AppendExecutionProvider_CUDA(cuda_options);

    // Create session
#ifdef _WIN32
    std::wstring w_model_path(model_path.begin(), model_path.end());
    session_ = std::make_unique<Ort::Session>(*env_, w_model_path.c_str(), session_options_);
#else
    session_ = std::make_unique<Ort::Session>(*env_, model_path.c_str(), session_options_);
#endif

    // Get input/output information
    Ort::AllocatorWithDefaultOptions allocator;

    // Input info
    auto input_name = session_->GetInputNameAllocated(0, allocator);
    input_names_.push_back(input_name.get());
    auto input_type_info = session_->GetInputTypeInfo(0);
    auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
    input_shape_ = input_tensor_info.GetShape();

    // Fix dynamic batch size
    if (input_shape_[0] == -1) {
        input_shape_[0] = 1;
    }

    // Output info
    auto output_name = session_->GetOutputNameAllocated(0, allocator);
    output_names_.push_back(output_name.get());
    auto output_type_info = session_->GetOutputTypeInfo(0);
    auto output_tensor_info = output_type_info.GetTensorTypeAndShapeInfo();
    output_shape_ = output_tensor_info.GetShape();

    std::cout << "ONNX Runtime initialized successfully" << std::endl;
    std::cout << "Input shape: [" << input_shape_[0] << ", "
              << input_shape_[1] << ", " << input_shape_[2] << ", "
              << input_shape_[3] << "]" << std::endl;
    std::cout << "Output shape: [" << output_shape_[0] << ", "
              << output_shape_[1] << ", " << output_shape_[2] << "]" << std::endl;
}

YOLOOnnx::~YOLOOnnx() {
    // Smart pointers handle cleanup automatically
}

void YOLOOnnx::preprocess(const cv::Mat& image, std::vector<float>& input_tensor) {
    // Resize image to input size
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(input_size_, input_size_));

    // Convert BGR to RGB
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

    // Normalize to [0, 1] and convert to float
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    // Convert HWC to CHW format
    input_tensor.resize(1 * 3 * input_size_ * input_size_);
    std::vector<cv::Mat> channels(3);
    cv::split(rgb, channels);

    for (int c = 0; c < 3; ++c) {
        std::memcpy(input_tensor.data() + c * input_size_ * input_size_,
                   channels[c].data,
                   input_size_ * input_size_ * sizeof(float));
    }
}

std::vector<Detection> YOLOOnnx::detect(const cv::Mat& image) {
    int orig_width = image.cols;
    int orig_height = image.rows;

    // Preprocess
    std::vector<float> input_tensor;
    preprocess(image, input_tensor);

    // Create input tensor
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor_obj = Ort::Value::CreateTensor<float>(
        memory_info,
        input_tensor.data(),
        input_tensor.size(),
        input_shape_.data(),
        input_shape_.size()
    );

    // Run inference
    auto output_tensors = session_->Run(
        Ort::RunOptions{nullptr},
        input_names_.data(),
        &input_tensor_obj,
        1,
        output_names_.data(),
        1
    );

    // Get output
    float* output_data = output_tensors[0].GetTensorMutableData<float>();
    size_t output_size = std::accumulate(output_shape_.begin(), output_shape_.end(),
                                         1, std::multiplies<int64_t>());
    std::vector<float> output(output_data, output_data + output_size);

    // Postprocess
    return postprocess(output, orig_width, orig_height);
}

std::vector<Detection> YOLOOnnx::postprocess(const std::vector<float>& output,
                                              int orig_width, int orig_height) {
    std::vector<Detection> detections;

    // YOLO output format: [batch, num_detections, 5 + num_classes]
    // where 5 = [x, y, w, h, objectness]
    int num_detections = output_shape_[1];
    int num_values = output_shape_[2];
    int num_classes = num_values - 5;

    float scale_x = static_cast<float>(orig_width) / input_size_;
    float scale_y = static_cast<float>(orig_height) / input_size_;

    for (int i = 0; i < num_detections; ++i) {
        const float* detection = output.data() + i * num_values;

        float objectness = detection[4];

        if (objectness < conf_threshold_) {
            continue;
        }

        // Find class with highest score
        const float* class_scores = detection + 5;
        int class_id = std::max_element(class_scores, class_scores + num_classes) - class_scores;
        float class_score = class_scores[class_id];
        float confidence = objectness * class_score;

        if (confidence < conf_threshold_) {
            continue;
        }

        // Convert from center format to corner format
        float cx = detection[0];
        float cy = detection[1];
        float w = detection[2];
        float h = detection[3];

        float x1 = (cx - w / 2) * scale_x;
        float y1 = (cy - h / 2) * scale_y;
        float x2 = (cx + w / 2) * scale_x;
        float y2 = (cy + h / 2) * scale_y;

        // Clamp to image boundaries
        x1 = std::max(0.0f, std::min(x1, static_cast<float>(orig_width)));
        y1 = std::max(0.0f, std::min(y1, static_cast<float>(orig_height)));
        x2 = std::max(0.0f, std::min(x2, static_cast<float>(orig_width)));
        y2 = std::max(0.0f, std::min(y2, static_cast<float>(orig_height)));

        Detection det;
        det.box = cv::Rect(static_cast<int>(x1), static_cast<int>(y1),
                          static_cast<int>(x2 - x1), static_cast<int>(y2 - y1));
        det.confidence = confidence;
        det.class_id = class_id;
        detections.push_back(det);
    }

    // Apply NMS
    nms(detections, iou_threshold_);

    return detections;
}

void YOLOOnnx::nms(std::vector<Detection>& detections, float iou_threshold) {
    // Sort by confidence
    std::sort(detections.begin(), detections.end(),
              [](const Detection& a, const Detection& b) {
                  return a.confidence > b.confidence;
              });

    std::vector<bool> keep(detections.size(), true);

    for (size_t i = 0; i < detections.size(); ++i) {
        if (!keep[i]) continue;

        for (size_t j = i + 1; j < detections.size(); ++j) {
            if (!keep[j]) continue;

            // Only suppress boxes of the same class
            if (detections[i].class_id != detections[j].class_id) continue;

            float overlap = iou(detections[i].box, detections[j].box);
            if (overlap > iou_threshold) {
                keep[j] = false;
            }
        }
    }

    // Remove suppressed detections
    std::vector<Detection> filtered;
    for (size_t i = 0; i < detections.size(); ++i) {
        if (keep[i]) {
            filtered.push_back(detections[i]);
        }
    }
    detections = filtered;
}

float YOLOOnnx::iou(const cv::Rect& a, const cv::Rect& b) {
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);

    int intersection = std::max(0, x2 - x1) * std::max(0, y2 - y1);
    int union_area = a.area() + b.area() - intersection;

    return union_area > 0 ? static_cast<float>(intersection) / union_area : 0.0f;
}

void YOLOOnnx::setClassNames(const std::vector<std::string>& names) {
    class_names_ = names;
}
