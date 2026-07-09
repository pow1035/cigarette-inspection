// 备用方案：如果 TensorRT engine 输出已经是后处理格式
// 适用于输出格式为 [1, N, 6] 或 [N, 6] 的情况
// 其中 6 = [x1, y1, x2, y2, confidence, class_id]

std::vector<Detection> YOLOTensorRTv2::postprocess_already_processed(
    const float* output,
    int orig_width,
    int orig_height) {

    std::vector<Detection> detections;

    // 如果输出格式是 [1, max_det, 6] 或 [max_det, 6]
    // 其中 max_det 通常是 100 或 300
    // 输出已经包含：NMS 后的结果、归一化坐标等

    int max_detections = num_anchors_;  // 实际上这里应该是最大检测数，不是 8400
    int feature_size = 6;  // [x1, y1, x2, y2, conf, class]

    // 检查输出格式：可能是 [1, N, 6] 或 [N, 6]
    // 假设是 [N, 6] 格式

    for (int i = 0; i < max_detections; ++i) {
        const float* det = output + i * feature_size;

        float x1 = det[0];
        float y1 = det[1];
        float x2 = det[2];
        float y2 = det[3];
        float confidence = det[4];
        int class_id = static_cast<int>(det[5]);

        // 如果 confidence 为 0 或很小，说明这是填充的空检测
        if (confidence < 0.01f) break;  // 后面都是空的

        // 过滤低置信度检测
        if (confidence < conf_threshold_) continue;

        // 坐标可能已经是原始图像尺寸，也可能是归一化的
        // 需要根据实际值判断

        // 如果坐标都小于 1.0，说明是归一化坐标
        if (x2 <= 1.0f && y2 <= 1.0f) {
            x1 *= orig_width;
            y1 *= orig_height;
            x2 *= orig_width;
            y2 *= orig_height;
        }
        // 否则坐标已经是像素值，但可能需要缩放
        // 检查坐标范围
        else if (x2 > input_size_ || y2 > input_size_) {
            // 坐标可能是基于输入尺寸的，需要缩放到原始图像
            float scale_x = static_cast<float>(orig_width) / input_size_;
            float scale_y = static_cast<float>(orig_height) / input_size_;
            x1 *= scale_x;
            y1 *= scale_y;
            x2 *= scale_x;
            y2 *= scale_y;
        }

        // 裁剪到图像边界
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

    // 输出已经经过 NMS，不需要再次应用
    return detections;
}
