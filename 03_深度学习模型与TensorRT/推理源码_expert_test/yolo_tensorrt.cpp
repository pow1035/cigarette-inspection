/**
 * YOLO26 TensorRT C++ 推理引擎实现
 */

#include "yolo_tensorrt.h"
#include <algorithm>
#include <chrono>
#include <iostream>

YOLOTensorRT::YOLOTensorRT(const std::string& engine_path,
                           int input_size,
                           float conf_thresh,
                           float iou_thresh)
    : engine_path_(engine_path)
    , input_size_(input_size)
    , input_h_(input_size)
    , input_w_(input_size)
    , conf_thresh_(conf_thresh)
    , iou_thresh_(iou_thresh) {

    buffers_[0] = nullptr;
    buffers_[1] = nullptr;
}

YOLOTensorRT::~YOLOTensorRT() {
    // 释放CUDA资源
    if (buffers_[0]) cudaFree(buffers_[0]);
    if (buffers_[1]) cudaFree(buffers_[1]);
    if (stream_) cudaStreamDestroy(stream_);

    // 释放TensorRT资源
    if (context_) context_->destroy();
    if (engine_) engine_->destroy();
}

bool YOLOTensorRT::initialize() {
    std::cout << "初始化TensorRT引擎..." << std::endl;

    // 加载引擎
    if (!loadEngine(engine_path_)) {
        std::cerr << "加载引擎失败" << std::endl;
        return false;
    }

    // 创建执行上下文
    context_ = engine_->createExecutionContext();
    if (!context_) {
        std::cerr << "创建执行上下文失败" << std::endl;
        return false;
    }

    // 创建CUDA流
    cudaStreamCreate(&stream_);

    // 获取输入输出维度
    auto input_dims = engine_->getBindingDimensions(0);
    auto output_dims = engine_->getBindingDimensions(1);

    std::cout << "输入维度: [";
    for (int i = 0; i < input_dims.nbDims; i++) {
        std::cout << input_dims.d[i];
        if (i < input_dims.nbDims - 1) std::cout << ", ";
    }
    std::cout << "]" << std::endl;

    std::cout << "输出维度: [";
    for (int i = 0; i < output_dims.nbDims; i++) {
        std::cout << output_dims.d[i];
        if (i < output_dims.nbDims - 1) std::cout << ", ";
    }
    std::cout << "]" << std::endl;

    // 计算输出大小
    output_size_ = 1;
    for (int i = 0; i < output_dims.nbDims; i++) {
        output_size_ *= output_dims.d[i];
    }

    // 分配GPU内存
    size_t input_size = input_c_ * input_h_ * input_w_ * sizeof(float);
    size_t output_size = output_size_ * sizeof(float);

    cudaMalloc(&buffers_[0], input_size);
    cudaMalloc(&buffers_[1], output_size);

    std::cout << "✓ TensorRT引擎初始化成功" << std::endl;
    std::cout << "  输入尺寸: " << input_w_ << "x" << input_h_ << std::endl;
    std::cout << "  置信度阈值: " << conf_thresh_ << std::endl;
    std::cout << "  NMS IoU阈值: " << iou_thresh_ << std::endl;

    return true;
}

bool YOLOTensorRT::loadEngine(const std::string& engine_path) {
    std::ifstream file(engine_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "无法打开引擎文件: " << engine_path << std::endl;
        return false;
    }

    // 读取文件
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    file.close();

    // 反序列化引擎
    nvinfer1::IRuntime* runtime = nvinfer1::createInferRuntime(gLogger);
    engine_ = runtime->deserializeCudaEngine(buffer.data(), size);
    runtime->destroy();

    if (!engine_) {
        std::cerr << "反序列化引擎失败" << std::endl;
        return false;
    }

    std::cout << "✓ 引擎加载成功: " << engine_path << std::endl;
    std::cout << "  文件大小: " << size / 1024.0 / 1024.0 << " MB" << std::endl;

    return true;
}

cv::Mat YOLOTensorRT::preprocessImage(const cv::Mat& image) {
    auto start = std::chrono::high_resolution_clock::now();

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(input_w_, input_h_));

    // 转换为RGB并归一化到[0, 1]
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    auto end = std::chrono::high_resolution_clock::now();
    total_preprocess_time_ += std::chrono::duration<double, std::milli>(end - start).count();

    return rgb;
}

std::vector<Detection> YOLOTensorRT::detect(const cv::Mat& image) {
    inference_count_++;

    // 1. 预处理
    cv::Mat input = preprocessImage(image);

    // 2. 准备输入数据 (HWC -> CHW)
    std::vector<float> input_data(input_c_ * input_h_ * input_w_);
    for (int c = 0; c < input_c_; c++) {
        for (int h = 0; h < input_h_; h++) {
            for (int w = 0; w < input_w_; w++) {
                input_data[c * input_h_ * input_w_ + h * input_w_ + w] =
                    input.at<cv::Vec3f>(h, w)[c];
            }
        }
    }

    // 3. 复制到GPU
    cudaMemcpyAsync(buffers_[0], input_data.data(),
                    input_data.size() * sizeof(float),
                    cudaMemcpyHostToDevice, stream_);

    // 4. 推理
    auto infer_start = std::chrono::high_resolution_clock::now();
    context_->enqueueV2(buffers_, stream_, nullptr);
    cudaStreamSynchronize(stream_);
    auto infer_end = std::chrono::high_resolution_clock::now();
    total_inference_time_ += std::chrono::duration<double, std::milli>(infer_end - infer_start).count();

    // 5. 复制输出
    std::vector<float> output(output_size_);
    cudaMemcpyAsync(output.data(), buffers_[1],
                    output_size_ * sizeof(float),
                    cudaMemcpyDeviceToHost, stream_);
    cudaStreamSynchronize(stream_);

    // 6. 后处理
    return postprocess(output.data(), output_size_, image);
}

std::vector<Detection> YOLOTensorRT::postprocess(float* output, int output_size,
                                                 const cv::Mat& original_image) {
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<Detection> detections;

    // YOLO输出格式: [batch, num_boxes, 4 + 1 + num_classes]
    // 4: box坐标, 1: objectness, num_classes: 类别概率
    int num_boxes = output_size / (5 + num_classes_);

    float scale_x = static_cast<float>(original_image.cols) / input_w_;
    float scale_y = static_cast<float>(original_image.rows) / input_h_;

    for (int i = 0; i < num_boxes; i++) {
        float* box_ptr = output + i * (5 + num_classes_);

        // 解析边界框 (中心点格式)
        float cx = box_ptr[0];
        float cy = box_ptr[1];
        float w = box_ptr[2];
        float h = box_ptr[3];
        float objectness = box_ptr[4];

        // 找到最大类别概率
        float max_class_prob = 0;
        int max_class_id = 0;
        for (int c = 0; c < num_classes_; c++) {
            if (box_ptr[5 + c] > max_class_prob) {
                max_class_prob = box_ptr[5 + c];
                max_class_id = c;
            }
        }

        float confidence = objectness * max_class_prob;

        if (confidence >= conf_thresh_) {
            // 转换为角点格式并缩放回原图
            int x1 = static_cast<int>((cx - w / 2) * scale_x);
            int y1 = static_cast<int>((cy - h / 2) * scale_y);
            int x2 = static_cast<int>((cx + w / 2) * scale_x);
            int y2 = static_cast<int>((cy + h / 2) * scale_y);

            // 边界检查
            x1 = std::max(0, std::min(x1, original_image.cols - 1));
            y1 = std::max(0, std::min(y1, original_image.rows - 1));
            x2 = std::max(0, std::min(x2, original_image.cols - 1));
            y2 = std::max(0, std::min(y2, original_image.rows - 1));

            Detection det;
            det.box = cv::Rect(x1, y1, x2 - x1, y2 - y1);
            det.confidence = confidence;
            det.class_id = max_class_id;
            det.class_name = max_class_id < class_names_.size() ?
                           class_names_[max_class_id] : std::to_string(max_class_id);

            detections.push_back(det);
        }
    }

    // NMS
    std::vector<int> keep_indices = nms(detections);
    std::vector<Detection> final_detections;
    for (int idx : keep_indices) {
        final_detections.push_back(detections[idx]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    total_postprocess_time_ += std::chrono::duration<double, std::milli>(end - start).count();

    return final_detections;
}

float YOLOTensorRT::computeIoU(const cv::Rect& box1, const cv::Rect& box2) {
    int x1 = std::max(box1.x, box2.x);
    int y1 = std::max(box1.y, box2.y);
    int x2 = std::min(box1.x + box1.width, box2.x + box2.width);
    int y2 = std::min(box1.y + box1.height, box2.y + box2.height);

    int inter_width = std::max(0, x2 - x1);
    int inter_height = std::max(0, y2 - y1);
    int inter_area = inter_width * inter_height;

    int box1_area = box1.width * box1.height;
    int box2_area = box2.width * box2.height;
    int union_area = box1_area + box2_area - inter_area;

    return static_cast<float>(inter_area) / union_area;
}

std::vector<int> YOLOTensorRT::nms(const std::vector<Detection>& detections) {
    std::vector<int> indices(detections.size());
    for (size_t i = 0; i < indices.size(); i++) {
        indices[i] = i;
    }

    // 按置信度降序排序
    std::sort(indices.begin(), indices.end(), [&detections](int i1, int i2) {
        return detections[i1].confidence > detections[i2].confidence;
    });

    std::vector<bool> suppressed(detections.size(), false);
    std::vector<int> keep;

    for (size_t i = 0; i < indices.size(); i++) {
        int idx = indices[i];
        if (suppressed[idx]) continue;

        keep.push_back(idx);

        for (size_t j = i + 1; j < indices.size(); j++) {
            int idx2 = indices[j];
            if (suppressed[idx2]) continue;

            if (detections[idx].class_id == detections[idx2].class_id) {
                float iou = computeIoU(detections[idx].box, detections[idx2].box);
                if (iou > iou_thresh_) {
                    suppressed[idx2] = true;
                }
            }
        }
    }

    return keep;
}

void YOLOTensorRT::printPerformanceStats() const {
    if (inference_count_ == 0) return;

    std::cout << "\n========== 性能统计 ==========" << std::endl;
    std::cout << "推理次数: " << inference_count_ << std::endl;
    std::cout << "平均预处理时间: " << total_preprocess_time_ / inference_count_ << " ms" << std::endl;
    std::cout << "平均推理时间: " << total_inference_time_ / inference_count_ << " ms" << std::endl;
    std::cout << "平均后处理时间: " << total_postprocess_time_ / inference_count_ << " ms" << std::endl;
    std::cout << "平均总时间: " << (total_preprocess_time_ + total_inference_time_ + total_postprocess_time_) / inference_count_ << " ms" << std::endl;
    std::cout << "平均FPS: " << 1000.0 / ((total_preprocess_time_ + total_inference_time_ + total_postprocess_time_) / inference_count_) << std::endl;
    std::cout << "==============================\n" << std::endl;
}
