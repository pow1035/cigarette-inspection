/**
 * YOLO26 TensorRT测试程序
 *
 * 功能:
 * - 测试单张图像推理
 * - 测试视频推理
 * - 性能基准测试
 */

#include "yolo_tensorrt.h"
#include <iostream>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

// 绘制检测结果
void drawDetections(cv::Mat& image, const std::vector<Detection>& detections) {
    for (const auto& det : detections) {
        // 绘制边界框
        cv::rectangle(image, det.box, cv::Scalar(0, 255, 0), 2);

        // 准备标签
        std::string label = det.class_name + " " +
                          std::to_string(static_cast<int>(det.confidence * 100)) + "%";

        // 绘制标签背景
        int baseline;
        cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX,
                                              0.5, 1, &baseline);

        cv::Point label_pos(det.box.x, det.box.y - 10);
        if (label_pos.y < 0) label_pos.y = det.box.y + label_size.height;

        cv::rectangle(image,
                     cv::Point(label_pos.x, label_pos.y - label_size.height),
                     cv::Point(label_pos.x + label_size.width, label_pos.y + baseline),
                     cv::Scalar(0, 255, 0), cv::FILLED);

        // 绘制标签文字
        cv::putText(image, label, label_pos, cv::FONT_HERSHEY_SIMPLEX,
                   0.5, cv::Scalar(0, 0, 0), 1);
    }
}

// 测试单张图像
void testImage(YOLOTensorRT& detector, const std::string& image_path,
               const std::string& output_path) {
    std::cout << "\n========== 图像推理测试 ==========" << std::endl;
    std::cout << "输入图像: " << image_path << std::endl;

    // 读取图像
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "无法读取图像: " << image_path << std::endl;
        return;
    }

    std::cout << "图像尺寸: " << image.cols << "x" << image.rows << std::endl;

    // 推理
    auto start = std::chrono::high_resolution_clock::now();
    auto detections = detector.detect(image);
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "检测到 " << detections.size() << " 个目标" << std::endl;
    std::cout << "推理用时: " << elapsed << " ms" << std::endl;

    // 打印检测结果
    for (size_t i = 0; i < detections.size(); i++) {
        const auto& det = detections[i];
        std::cout << "  [" << i << "] " << det.class_name
                  << " (" << det.confidence << ") "
                  << "at [" << det.box.x << ", " << det.box.y << ", "
                  << det.box.width << ", " << det.box.height << "]" << std::endl;
    }

    // 绘制并保存结果
    drawDetections(image, detections);
    cv::imwrite(output_path, image);
    std::cout << "结果已保存: " << output_path << std::endl;

    // 显示结果 (可选)
    cv::namedWindow("Detection Result", cv::WINDOW_NORMAL);
    cv::resizeWindow("Detection Result", 1280, 720);
    cv::imshow("Detection Result", image);
    std::cout << "\n按任意键继续..." << std::endl;
    cv::waitKey(0);
    cv::destroyAllWindows();
}

// 测试视频
void testVideo(YOLOTensorRT& detector, const std::string& video_path,
               const std::string& output_path) {
    std::cout << "\n========== 视频推理测试 ==========" << std::endl;
    std::cout << "输入视频: " << video_path << std::endl;

    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "无法打开视频: " << video_path << std::endl;
        return;
    }

    // 获取视频参数
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(cv::CAP_PROP_FPS);
    int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));

    std::cout << "视频尺寸: " << frame_width << "x" << frame_height << std::endl;
    std::cout << "帧率: " << fps << " FPS" << std::endl;
    std::cout << "总帧数: " << total_frames << std::endl;

    // 创建视频写入器
    cv::VideoWriter writer(output_path,
                          cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                          fps, cv::Size(frame_width, frame_height));

    cv::Mat frame;
    int frame_count = 0;
    double total_time = 0;

    std::cout << "处理中..." << std::endl;

    while (cap.read(frame)) {
        auto start = std::chrono::high_resolution_clock::now();
        auto detections = detector.detect(frame);
        auto end = std::chrono::high_resolution_clock::now();

        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        total_time += elapsed;
        frame_count++;

        // 绘制检测结果
        drawDetections(frame, detections);

        // 显示FPS
        std::string fps_text = "FPS: " + std::to_string(static_cast<int>(1000.0 / elapsed));
        cv::putText(frame, fps_text, cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);

        // 写入输出视频
        writer.write(frame);

        // 显示进度
        if (frame_count % 30 == 0) {
            std::cout << "已处理: " << frame_count << "/" << total_frames
                     << " 帧, 平均FPS: " << 1000.0 / (total_time / frame_count) << std::endl;
        }

        // 实时显示 (可选)
        cv::imshow("Video Detection", frame);
        if (cv::waitKey(1) == 27) break;  // ESC退出
    }

    cap.release();
    writer.release();
    cv::destroyAllWindows();

    std::cout << "\n视频处理完成" << std::endl;
    std::cout << "总帧数: " << frame_count << std::endl;
    std::cout << "平均推理时间: " << total_time / frame_count << " ms" << std::endl;
    std::cout << "平均FPS: " << 1000.0 / (total_time / frame_count) << std::endl;
    std::cout << "结果已保存: " << output_path << std::endl;
}

// 性能基准测试
void benchmark(YOLOTensorRT& detector, int warmup = 50, int iterations = 200) {
    std::cout << "\n========== 性能基准测试 ==========" << std::endl;
    std::cout << "预热: " << warmup << " 次" << std::endl;
    std::cout << "测试: " << iterations << " 次" << std::endl;

    // 创建随机图像
    cv::Mat dummy_image(640, 640, CV_8UC3);
    cv::randu(dummy_image, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));

    // 预热
    std::cout << "预热中..." << std::endl;
    for (int i = 0; i < warmup; i++) {
        detector.detect(dummy_image);
    }

    // 测试
    std::cout << "测试中..." << std::endl;
    std::vector<double> times;
    times.reserve(iterations);

    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        detector.detect(dummy_image);
        auto end = std::chrono::high_resolution_clock::now();

        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(elapsed);

        if ((i + 1) % 50 == 0) {
            std::cout << "  进度: " << (i + 1) << "/" << iterations << std::endl;
        }
    }

    // 统计
    std::sort(times.begin(), times.end());
    double sum = 0;
    for (double t : times) sum += t;
    double mean = sum / times.size();

    double variance = 0;
    for (double t : times) {
        variance += (t - mean) * (t - mean);
    }
    double stddev = std::sqrt(variance / times.size());

    std::cout << "\n性能统计:" << std::endl;
    std::cout << "  平均延迟: " << mean << " ms" << std::endl;
    std::cout << "  中位延迟: " << times[times.size() / 2] << " ms" << std::endl;
    std::cout << "  最小延迟: " << times.front() << " ms" << std::endl;
    std::cout << "  最大延迟: " << times.back() << " ms" << std::endl;
    std::cout << "  标准差: " << stddev << " ms" << std::endl;
    std::cout << "  P95延迟: " << times[static_cast<int>(times.size() * 0.95)] << " ms" << std::endl;
    std::cout << "  P99延迟: " << times[static_cast<int>(times.size() * 0.99)] << " ms" << std::endl;
    std::cout << "  平均吞吐量: " << 1000.0 / mean << " FPS" << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "   YOLO26 TensorRT C++ 推理测试" << std::endl;
    std::cout << "========================================" << std::endl;

    // 参数解析
    if (argc < 2) {
        std::cout << "\n用法:" << std::endl;
        std::cout << "  " << argv[0] << " <engine_path> [mode] [input] [output]" << std::endl;
        std::cout << "\n模式:" << std::endl;
        std::cout << "  image  - 图像推理 (需要input和output参数)" << std::endl;
        std::cout << "  video  - 视频推理 (需要input和output参数)" << std::endl;
        std::cout << "  bench  - 性能测试 (默认)" << std::endl;
        std::cout << "\n示例:" << std::endl;
        std::cout << "  " << argv[0] << " models/yanzhi20260115_4080s_fp16.engine bench" << std::endl;
        std::cout << "  " << argv[0] << " models/yanzhi20260115_4080s_fp16.engine image test.jpg result.jpg" << std::endl;
        std::cout << "  " << argv[0] << " models/yanzhi20260115_4080s_fp16.engine video test.mp4 result.mp4" << std::endl;
        return 1;
    }

    std::string engine_path = argv[1];
    std::string mode = argc > 2 ? argv[2] : "bench";

    // 检查引擎文件
    if (!fs::exists(engine_path)) {
        std::cerr << "\n错误: 引擎文件不存在: " << engine_path << std::endl;
        return 1;
    }

    // 初始化检测器
    std::cout << "\n初始化检测器..." << std::endl;
    YOLOTensorRT detector(engine_path, 640, 0.25f, 0.45f);

    // 设置类别名称 (根据你的数据集修改)
    std::vector<std::string> class_names = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck"
        // ... 添加所有类别
    };
    detector.setClassNames(class_names);

    if (!detector.initialize()) {
        std::cerr << "初始化失败" << std::endl;
        return 1;
    }

    // 执行测试
    if (mode == "image") {
        if (argc < 5) {
            std::cerr << "图像模式需要输入和输出路径" << std::endl;
            return 1;
        }
        testImage(detector, argv[3], argv[4]);
    }
    else if (mode == "video") {
        if (argc < 5) {
            std::cerr << "视频模式需要输入和输出路径" << std::endl;
            return 1;
        }
        testVideo(detector, argv[3], argv[4]);
    }
    else if (mode == "bench") {
        benchmark(detector, 50, 200);
    }
    else {
        std::cerr << "未知模式: " << mode << std::endl;
        return 1;
    }

    // 打印性能统计
    detector.printPerformanceStats();

    std::cout << "\n测试完成!" << std::endl;
    return 0;
}
