#include "yolo_trt_v2.h"
#include <algorithm>
#include <numeric>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

YOLOTensorRTv2::YOLOTensorRTv2(const std::string& model_path,
                               int input_size,
                               float conf_thresh,
                               float iou_thresh)
    : input_size_(input_size),
      conf_threshold_(conf_thresh),
      iou_threshold_(iou_thresh),
      initialized_(false) {

    buffers_[0] = nullptr;
    buffers_[1] = nullptr;
    cudaStreamCreate(&stream_);

    std::cout << "Initializing YOLO TensorRT v2..." << std::endl;
    std::cout << "Model: " << model_path << std::endl;

    // Check if file exists
    if (!fs::exists(model_path)) {
        std::cerr << "Error: Model file not found: " << model_path << std::endl;
        return;
    }

    // Check file extension
    std::string ext = fs::path(model_path).extension().string();

    if (ext == ".engine") {
        std::cout << "Loading pre-built engine..." << std::endl;
        initialized_ = loadEngine(model_path);
    }
    else if (ext == ".onnx") {
        std::cout << "Building engine from ONNX (this may take 5-10 minutes)..." << std::endl;

        // Check for cached engine
        std::string cache_path = model_path.substr(0, model_path.rfind('.')) + "_cache.engine";

        if (fs::exists(cache_path)) {
            std::cout << "Found cached engine: " << cache_path << std::endl;
            initialized_ = loadEngine(cache_path);
        }
        else {
            // Build new engine from ONNX
            initialized_ = buildEngineFromOnnx(model_path);

            if (initialized_) {
                std::cout << "Saving engine cache..." << std::endl;
                saveEngine(cache_path);
            }
        }
    }
    else {
        std::cerr << "Error: Unsupported file format: " << ext << std::endl;
        std::cerr << "Supported formats: .engine, .onnx" << std::endl;
        return;
    }

    if (initialized_) {
        std::cout << "TensorRT engine initialized successfully!" << std::endl;
    }
    else {
        std::cerr << "Failed to initialize TensorRT engine" << std::endl;
    }
}

YOLOTensorRTv2::~YOLOTensorRTv2() {
    if (buffers_[0]) cudaFree(buffers_[0]);
    if (buffers_[1]) cudaFree(buffers_[1]);
    if (stream_) cudaStreamDestroy(stream_);
}

bool YOLOTensorRTv2::buildEngineFromOnnx(const std::string& onnx_path) {
    std::cout << "Building TensorRT engine from ONNX..." << std::endl;

    auto builder = std::unique_ptr<nvinfer1::IBuilder>(
        nvinfer1::createInferBuilder(logger_));
    if (!builder) {
        std::cerr << "Failed to create builder" << std::endl;
        return false;
    }

    const auto explicitBatch = 1U << static_cast<uint32_t>(
        nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    auto network = std::unique_ptr<nvinfer1::INetworkDefinition>(
        builder->createNetworkV2(explicitBatch));
    if (!network) {
        std::cerr << "Failed to create network" << std::endl;
        return false;
    }

    auto parser = std::unique_ptr<nvonnxparser::IParser>(
        nvonnxparser::createParser(*network, logger_));
    if (!parser) {
        std::cerr << "Failed to create parser" << std::endl;
        return false;
    }

    // Parse ONNX file
    std::cout << "Parsing ONNX file..." << std::endl;
    if (!parser->parseFromFile(onnx_path.c_str(),
                               static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) {
        std::cerr << "Failed to parse ONNX file" << std::endl;
        for (int i = 0; i < parser->getNbErrors(); ++i) {
            std::cerr << parser->getError(i)->desc() << std::endl;
        }
        return false;
    }

    // Build engine config
    auto config = std::unique_ptr<nvinfer1::IBuilderConfig>(
        builder->createBuilderConfig());
    if (!config) {
        std::cerr << "Failed to create builder config" << std::endl;
        return false;
    }

    // Set memory pool size (workspace)
    config->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, 12ULL << 30);  // 12GB

    // Enable FP16 if supported
    if (builder->platformHasFastFp16()) {
        std::cout << "Enabling FP16 mode" << std::endl;
        config->setFlag(nvinfer1::BuilderFlag::kFP16);
    }
    else {
        std::cout << "FP16 not supported, using FP32" << std::endl;
    }

    // Build engine
    std::cout << "Building engine (this will take several minutes)..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    auto serializedEngine = std::unique_ptr<nvinfer1::IHostMemory>(
        builder->buildSerializedNetwork(*network, *config));
    if (!serializedEngine) {
        std::cerr << "Failed to build engine" << std::endl;
        return false;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    std::cout << "Engine built successfully in " << duration.count() << " seconds" << std::endl;

    // Create runtime and deserialize engine
    runtime_ = std::unique_ptr<nvinfer1::IRuntime>(
        nvinfer1::createInferRuntime(logger_));
    if (!runtime_) {
        std::cerr << "Failed to create runtime" << std::endl;
        return false;
    }

    engine_ = std::unique_ptr<nvinfer1::ICudaEngine>(
        runtime_->deserializeCudaEngine(serializedEngine->data(), serializedEngine->size()));
    if (!engine_) {
        std::cerr << "Failed to deserialize engine" << std::endl;
        return false;
    }

    context_ = std::unique_ptr<nvinfer1::IExecutionContext>(
        engine_->createExecutionContext());
    if (!context_) {
        std::cerr << "Failed to create execution context" << std::endl;
        return false;
    }

    // Allocate buffers
    auto inputDims = engine_->getBindingDimensions(0);
    auto outputDims = engine_->getBindingDimensions(1);

    input_buffer_size_ = 1 * 3 * input_size_ * input_size_ * sizeof(float);
    output_buffer_size_ = outputDims.d[0] * outputDims.d[1] * outputDims.d[2] * sizeof(float);

    cudaMalloc(&buffers_[0], input_buffer_size_);
    cudaMalloc(&buffers_[1], output_buffer_size_);

    std::cout << "Input dimensions: " << inputDims.d[0] << "x" << inputDims.d[1]
              << "x" << inputDims.d[2] << "x" << inputDims.d[3] << std::endl;
    std::cout << "Output dimensions: " << outputDims.d[0] << "x" << outputDims.d[1]
              << "x" << outputDims.d[2] << std::endl;

    // Parse YOLO output dimensions
    // Check if output is raw format [1, features, anchors] or post-processed [1, max_det, 6]
    if (outputDims.d[1] > 80 && outputDims.d[2] > 1000) {
        // Raw format: [1, features, anchors] e.g. [1, 84, 8400]
        output_features_ = outputDims.d[1];  // 84
        num_anchors_ = outputDims.d[2];      // 8400
        num_classes_ = output_features_ - 4; // 80

        std::cout << "Detected RAW YOLO format:" << std::endl;
        std::cout << "  Classes: " << num_classes_ << std::endl;
        std::cout << "  Anchors: " << num_anchors_ << std::endl;
        std::cout << "  Features: " << output_features_ << std::endl;
    } else if (outputDims.d[2] == 6 || outputDims.d[2] == 7) {
        // Post-processed format: [1, max_det, 6] e.g. [1, 300, 6]
        num_anchors_ = outputDims.d[1];      // max_detections (e.g., 300)
        output_features_ = outputDims.d[2];  // 6 or 7
        num_classes_ = -1;  // Unknown, post-processed format

        std::cout << "Detected POST-PROCESSED format:" << std::endl;
        std::cout << "  Max detections: " << num_anchors_ << std::endl;
        std::cout << "  Features per detection: " << output_features_ << std::endl;
        std::cout << "  Format: [x1, y1, x2, y2, conf, class]" << std::endl;
    } else {
        std::cerr << "Warning: Unknown output format!" << std::endl;
        output_features_ = outputDims.d[1];
        num_anchors_ = outputDims.d[2];
        num_classes_ = 0;
    }

    return true;
}

bool YOLOTensorRTv2::loadEngine(const std::string& engine_path) {
    std::cout << "Loading engine from: " << engine_path << std::endl;

    std::ifstream file(engine_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "Failed to open engine file" << std::endl;
        return false;
    }

    file.seekg(0, std::ifstream::end);
    size_t size = file.tellg();
    file.seekg(0, std::ifstream::beg);

    std::vector<char> engineData(size);
    file.read(engineData.data(), size);
    file.close();

    std::cout << "Engine size: " << size / (1024.0 * 1024.0) << " MB" << std::endl;

    runtime_ = std::unique_ptr<nvinfer1::IRuntime>(
        nvinfer1::createInferRuntime(logger_));
    if (!runtime_) {
        std::cerr << "Failed to create runtime" << std::endl;
        return false;
    }

    engine_ = std::unique_ptr<nvinfer1::ICudaEngine>(
        runtime_->deserializeCudaEngine(engineData.data(), size));
    if (!engine_) {
        std::cerr << "Failed to deserialize engine" << std::endl;
        return false;
    }

    context_ = std::unique_ptr<nvinfer1::IExecutionContext>(
        engine_->createExecutionContext());
    if (!context_) {
        std::cerr << "Failed to create execution context" << std::endl;
        return false;
    }

    // Allocate buffers
    auto inputDims = engine_->getBindingDimensions(0);
    auto outputDims = engine_->getBindingDimensions(1);

    input_buffer_size_ = 1 * 3 * input_size_ * input_size_ * sizeof(float);
    output_buffer_size_ = outputDims.d[0] * outputDims.d[1] * outputDims.d[2] * sizeof(float);

    cudaMalloc(&buffers_[0], input_buffer_size_);
    cudaMalloc(&buffers_[1], output_buffer_size_);

    std::cout << "Buffers allocated successfully" << std::endl;

    // Parse YOLO output dimensions
    if (outputDims.d[1] > 80 && outputDims.d[2] > 1000) {
        // Raw format
        output_features_ = outputDims.d[1];
        num_anchors_ = outputDims.d[2];
        num_classes_ = output_features_ - 4;
        std::cout << "RAW format: " << num_classes_ << " classes, " << num_anchors_ << " anchors" << std::endl;
    } else if (outputDims.d[2] == 6 || outputDims.d[2] == 7) {
        // Post-processed format
        num_anchors_ = outputDims.d[1];
        output_features_ = outputDims.d[2];
        num_classes_ = -1;
        std::cout << "POST-PROCESSED format: max " << num_anchors_ << " detections" << std::endl;
    }

    return true;
}

bool YOLOTensorRTv2::saveEngine(const std::string& engine_path) {
    if (!engine_) {
        std::cerr << "No engine to save" << std::endl;
        return false;
    }

    auto serializedEngine = std::unique_ptr<nvinfer1::IHostMemory>(
        engine_->serialize());
    if (!serializedEngine) {
        std::cerr << "Failed to serialize engine" << std::endl;
        return false;
    }

    std::ofstream file(engine_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "Failed to create engine file" << std::endl;
        return false;
    }

    file.write(static_cast<const char*>(serializedEngine->data()),
               serializedEngine->size());
    file.close();

    std::cout << "Engine saved to: " << engine_path << std::endl;
    std::cout << "Size: " << serializedEngine->size() / (1024.0 * 1024.0) << " MB" << std::endl;

    return true;
}

void YOLOTensorRTv2::preprocess(const cv::Mat& image, float* input_buffer) {
    // Resize
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(input_size_, input_size_));

    // BGR to RGB
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

    // Normalize to [0, 1]
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    // Convert HWC to CHW
    std::vector<cv::Mat> channels(3);
    cv::split(rgb, channels);

    for (int c = 0; c < 3; ++c) {
        memcpy(input_buffer + c * input_size_ * input_size_,
               channels[c].data,
               input_size_ * input_size_ * sizeof(float));
    }
}

std::vector<Detection> YOLOTensorRTv2::detect(const cv::Mat& image) {
    if (!initialized_) {
        std::cerr << "Engine not initialized" << std::endl;
        return {};
    }

    int orig_width = image.cols;
    int orig_height = image.rows;

    // Preprocess on CPU
    std::vector<float> input_host(1 * 3 * input_size_ * input_size_);
    preprocess(image, input_host.data());

    // Copy to GPU
    cudaMemcpyAsync(buffers_[0], input_host.data(), input_buffer_size_,
                   cudaMemcpyHostToDevice, stream_);

    // Inference
    context_->enqueueV2(buffers_, stream_, nullptr);

    // Copy output to CPU
    std::vector<float> output_host(output_buffer_size_ / sizeof(float));
    cudaMemcpyAsync(output_host.data(), buffers_[1], output_buffer_size_,
                   cudaMemcpyDeviceToHost, stream_);
    cudaStreamSynchronize(stream_);

    // Postprocess
    return postprocess(output_host.data(), orig_width, orig_height);
}

std::vector<Detection> YOLOTensorRTv2::postprocess(const float* output,
                                                    int orig_width,
                                                    int orig_height) {
    std::vector<Detection> detections;

    // Check output format: RAW [1, 84, 8400] or POST-PROCESSED [1, 300, 6]
    if (num_classes_ > 0) {
        // RAW YOLO format: [1, features, anchors]
        // where features = 4 (bbox) + num_classes
        std::vector<int> class_ids;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;

        float scale_x = static_cast<float>(orig_width) / input_size_;
        float scale_y = static_cast<float>(orig_height) / input_size_;

        // Iterate through all anchor points
        for (int i = 0; i < num_anchors_; ++i) {
            // Get bbox coordinates (in [features, anchors] layout)
            float cx = output[0 * num_anchors_ + i];
            float cy = output[1 * num_anchors_ + i];
            float w = output[2 * num_anchors_ + i];
            float h = output[3 * num_anchors_ + i];

            // Find max class score and corresponding class id
            float max_score = 0.0f;
            int max_class_id = 0;
            for (int c = 0; c < num_classes_; ++c) {
                float score = output[(4 + c) * num_anchors_ + i];
                if (score > max_score) {
                    max_score = score;
                    max_class_id = c;
                }
            }

            // Filter by confidence threshold
            if (max_score < conf_threshold_) continue;

            // Convert bbox to original image coordinates
            float x1 = (cx - w / 2.0f) * scale_x;
            float y1 = (cy - h / 2.0f) * scale_y;
            float x2 = (cx + w / 2.0f) * scale_x;
            float y2 = (cy + h / 2.0f) * scale_y;

            // Clip to image boundaries
            x1 = std::max(0.0f, std::min(x1, static_cast<float>(orig_width)));
            y1 = std::max(0.0f, std::min(y1, static_cast<float>(orig_height)));
            x2 = std::max(0.0f, std::min(x2, static_cast<float>(orig_width)));
            y2 = std::max(0.0f, std::min(y2, static_cast<float>(orig_height)));

            int box_w = static_cast<int>(x2 - x1);
            int box_h = static_cast<int>(y2 - y1);

            if (box_w > 0 && box_h > 0) {
                boxes.push_back(cv::Rect(static_cast<int>(x1), static_cast<int>(y1), box_w, box_h));
                confidences.push_back(max_score);
                class_ids.push_back(max_class_id);
            }
        }

        // Apply NMS
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, conf_threshold_, iou_threshold_, indices);

        // Collect final detections after NMS
        for (int idx : indices) {
            Detection det;
            det.box = boxes[idx];
            det.confidence = confidences[idx];
            det.class_id = class_ids[idx];
            detections.push_back(det);
        }
    } else {
        // POST-PROCESSED format: [1, max_det, 6]
        // Format: [x1, y1, x2, y2, confidence, class_id]
        // Output is already after NMS, coordinates in input space (need scaling)

        for (int i = 0; i < num_anchors_; ++i) {
            const float* det = output + i * output_features_;

            float x1 = det[0];
            float y1 = det[1];
            float x2 = det[2];
            float y2 = det[3];
            float confidence = det[4];
            int class_id = static_cast<int>(det[5]);

            // If confidence is 0, this is padding (no more detections)
            if (confidence < 0.01f) break;

            // Filter by threshold
            if (confidence < conf_threshold_) continue;

            // Coordinates are in input image space (992x992), need to scale to original image
            float scale_x = static_cast<float>(orig_width) / input_size_;
            float scale_y = static_cast<float>(orig_height) / input_size_;

            x1 *= scale_x;
            y1 *= scale_y;
            x2 *= scale_x;
            y2 *= scale_y;

            // Clip to boundaries
            x1 = std::max(0.0f, std::min(x1, static_cast<float>(orig_width)));
            y1 = std::max(0.0f, std::min(y1, static_cast<float>(orig_height)));
            x2 = std::max(0.0f, std::min(x2, static_cast<float>(orig_width)));
            y2 = std::max(0.0f, std::min(y2, static_cast<float>(orig_height)));

            int box_w = static_cast<int>(x2 - x1);
            int box_h = static_cast<int>(y2 - y1);

            if (box_w > 0 && box_h > 0) {
                Detection detection;
                detection.box = cv::Rect(static_cast<int>(x1), static_cast<int>(y1), box_w, box_h);
                detection.confidence = confidence;
                detection.class_id = class_id;
                detections.push_back(detection);
            }
        }

        // Optional: Apply NMS again with stricter threshold to remove duplicates
        // Model output already has NMS, but sometimes with loose IOU threshold
        if (detections.size() > 1) {
            nms(detections, iou_threshold_);
        }
    }

    return detections;
}

void YOLOTensorRTv2::nms(std::vector<Detection>& detections, float iou_threshold) {
    std::sort(detections.begin(), detections.end(),
              [](const Detection& a, const Detection& b) {
                  return a.confidence > b.confidence;
              });

    std::vector<bool> keep(detections.size(), true);

    for (size_t i = 0; i < detections.size(); ++i) {
        if (!keep[i]) continue;

        for (size_t j = i + 1; j < detections.size(); ++j) {
            if (!keep[j]) continue;

            if (detections[i].class_id != detections[j].class_id) continue;

            float overlap = iou(detections[i].box, detections[j].box);
            if (overlap > iou_threshold) {
                keep[j] = false;
            }
        }
    }

    std::vector<Detection> filtered;
    for (size_t i = 0; i < detections.size(); ++i) {
        if (keep[i]) {
            filtered.push_back(detections[i]);
        }
    }
    detections = filtered;
}

float YOLOTensorRTv2::iou(const cv::Rect& a, const cv::Rect& b) {
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    int x2 = std::min(a.x + a.width, b.x + b.width);
    int y2 = std::min(a.y + a.height, b.y + b.height);

    int intersection = std::max(0, x2 - x1) * std::max(0, y2 - y1);
    int union_area = a.area() + b.area() - intersection;

    return union_area > 0 ? static_cast<float>(intersection) / union_area : 0.0f;
}
