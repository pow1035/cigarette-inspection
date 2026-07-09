#include "yolo_trt_v2.h"
#include <iostream>
#include <chrono>
#include <numeric>

void print_usage(const char* prog) {
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << prog << " image <model.onnx|.engine> <image.jpg>" << std::endl;
    std::cout << "  " << prog << " video <model.onnx|.engine> <video.mp4>" << std::endl;
    std::cout << "  " << prog << " benchmark <model.onnx|.engine> <image.jpg>" << std::endl;
}

void test_image(const std::string& model_path, const std::string& image_path) {
    std::cout << "\n========== Image Detection ==========" << std::endl;

    YOLOTensorRTv2 detector(model_path);

    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "Failed to load image: " << image_path << std::endl;
        return;
    }

    std::cout << "Image size: " << image.cols << "x" << image.rows << std::endl;
    std::cout << "Running detection..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    auto detections = detector.detect(image);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Inference time: " << duration.count() << " ms" << std::endl;
    std::cout << "Detections: " << detections.size() << std::endl;

    // Draw results
    for (const auto& det : detections) {
        cv::rectangle(image, det.box, cv::Scalar(0, 255, 0), 2);
        std::string label = "Class " + std::to_string(det.class_id) +
                           ": " + std::to_string(static_cast<int>(det.confidence * 100)) + "%";
        cv::putText(image, label, cv::Point(det.box.x, det.box.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    }

    std::string output_path = "output_" + image_path.substr(image_path.find_last_of("/\\") + 1);
    cv::imwrite(output_path, image);
    std::cout << "Result saved to: " << output_path << std::endl;

    cv::imshow("Detection", image);
    cv::waitKey(0);
}

void test_video(const std::string& model_path, const std::string& video_path) {
    std::cout << "\n========== Video Detection ==========" << std::endl;

    YOLOTensorRTv2 detector(model_path);

    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open video: " << video_path << std::endl;
        return;
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    int width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    std::cout << "Video: " << width << "x" << height << " @ " << fps << " FPS" << std::endl;

    std::string output_path = "output_" + video_path.substr(video_path.find_last_of("/\\") + 1);
    cv::VideoWriter writer(output_path, cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                          fps, cv::Size(width, height));

    cv::Mat frame;
    int frame_count = 0;
    double total_time = 0;

    std::cout << "Processing... (Press 'q' to quit)" << std::endl;

    while (cap.read(frame)) {
        auto start = std::chrono::high_resolution_clock::now();
        auto detections = detector.detect(frame);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        total_time += duration.count();
        frame_count++;

        // Draw results
        for (const auto& det : detections) {
            cv::rectangle(frame, det.box, cv::Scalar(0, 255, 0), 2);
            std::string label = "Class " + std::to_string(det.class_id);
            cv::putText(frame, label, cv::Point(det.box.x, det.box.y - 5),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        }

        // Display FPS
        double avg_fps = frame_count / (total_time / 1000.0);
        std::string fps_text = "FPS: " + std::to_string(static_cast<int>(avg_fps));
        cv::putText(frame, fps_text, cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);

        writer.write(frame);
        cv::imshow("Video Detection", frame);

        if (cv::waitKey(1) == 'q') break;

        if (frame_count % 30 == 0) {
            std::cout << "Frame " << frame_count << ", avg FPS: "
                     << static_cast<int>(avg_fps) << std::endl;
        }
    }

    std::cout << "\nProcessed " << frame_count << " frames" << std::endl;
    std::cout << "Average inference time: " << total_time / frame_count << " ms" << std::endl;
    std::cout << "Average FPS: " << frame_count / (total_time / 1000.0) << std::endl;
    std::cout << "Output saved to: " << output_path << std::endl;
}

void benchmark(const std::string& model_path, const std::string& image_path) {
    std::cout << "\n========== Benchmark ==========" << std::endl;

    YOLOTensorRTv2 detector(model_path);

    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "Failed to load image: " << image_path << std::endl;
        return;
    }

    int num_iterations = 100;
    std::cout << "Running " << num_iterations << " iterations..." << std::endl;

    // Warmup
    std::cout << "Warmup..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        detector.detect(image);
    }

    // Benchmark
    std::vector<double> times;
    times.reserve(num_iterations);

    for (int i = 0; i < num_iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto detections = detector.detect(image);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        times.push_back(duration.count() / 1000.0);

        if ((i + 1) % 10 == 0) {
            std::cout << "Progress: " << (i + 1) << "/" << num_iterations << std::endl;
        }
    }

    // Statistics
    std::sort(times.begin(), times.end());
    double min_time = times.front();
    double max_time = times.back();
    double avg_time = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    double median_time = times[times.size() / 2];
    double p95_time = times[static_cast<size_t>(times.size() * 0.95)];
    double p99_time = times[static_cast<size_t>(times.size() * 0.99)];

    std::cout << "\n========================================" << std::endl;
    std::cout << "Benchmark Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Iterations: " << num_iterations << std::endl;
    std::cout << "Image size: " << image.cols << "x" << image.rows << std::endl;
    std::cout << std::endl;
    std::cout << "Inference Time (ms):" << std::endl;
    std::cout << "  Min:    " << min_time << std::endl;
    std::cout << "  Max:    " << max_time << std::endl;
    std::cout << "  Mean:   " << avg_time << std::endl;
    std::cout << "  Median: " << median_time << std::endl;
    std::cout << "  P95:    " << p95_time << std::endl;
    std::cout << "  P99:    " << p99_time << std::endl;
    std::cout << std::endl;
    std::cout << "Throughput:" << std::endl;
    std::cout << "  Average FPS: " << 1000.0 / avg_time << std::endl;
    std::cout << "  Peak FPS:    " << 1000.0 / min_time << std::endl;
    std::cout << "========================================" << std::endl;

    if (avg_time <= 5.0) {
        std::cout << "\nPERFORMANCE TARGET MET! (<=5ms)" << std::endl;
    }
    else {
        std::cout << "\nAverage inference time: " << avg_time << " ms" << std::endl;
        std::cout << "Target: 5 ms" << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    std::string model_path = argv[2];
    std::string input_path = argv[3];

    try {
        if (mode == "image") {
            test_image(model_path, input_path);
        }
        else if (mode == "video") {
            test_video(model_path, input_path);
        }
        else if (mode == "benchmark") {
            benchmark(model_path, input_path);
        }
        else {
            std::cerr << "Unknown mode: " << mode << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
