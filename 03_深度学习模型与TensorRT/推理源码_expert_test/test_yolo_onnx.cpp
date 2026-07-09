#include "yolo_onnx.h"
#include <iostream>
#include <chrono>

void print_usage(const char* program_name) {
    std::cout << "Usage:" << std::endl;
    std::cout << "  Test image:      " << program_name << " image <model.onnx> <image.jpg>" << std::endl;
    std::cout << "  Test video:      " << program_name << " video <model.onnx> <video.mp4>" << std::endl;
    std::cout << "  Benchmark:       " << program_name << " benchmark <model.onnx> <image.jpg>" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " image models\\yanzhi20260115.onnx test.jpg" << std::endl;
    std::cout << "  " << program_name << " video models\\yanzhi20260115.onnx test.mp4" << std::endl;
    std::cout << "  " << program_name << " benchmark models\\yanzhi20260115.onnx test.jpg" << std::endl;
}

void test_image(const std::string& model_path, const std::string& image_path) {
    std::cout << "Loading model: " << model_path << std::endl;
    YOLOOnnx detector(model_path);

    std::cout << "Loading image: " << image_path << std::endl;
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "Error: Could not load image: " << image_path << std::endl;
        return;
    }

    std::cout << "Running detection..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    auto detections = detector.detect(image);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Detection complete in " << duration.count() << "ms" << std::endl;
    std::cout << "Found " << detections.size() << " objects" << std::endl;

    // Draw results
    for (const auto& det : detections) {
        cv::rectangle(image, det.box, cv::Scalar(0, 255, 0), 2);

        std::string label = "Class " + std::to_string(det.class_id) + ": " +
                           std::to_string(static_cast<int>(det.confidence * 100)) + "%";
        int baseline;
        cv::Size label_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

        cv::rectangle(image,
                     cv::Point(det.box.x, det.box.y - label_size.height - 5),
                     cv::Point(det.box.x + label_size.width, det.box.y),
                     cv::Scalar(0, 255, 0), -1);
        cv::putText(image, label,
                   cv::Point(det.box.x, det.box.y - 3),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }

    std::string output_path = "output_" + image_path.substr(image_path.find_last_of("/\\") + 1);
    cv::imwrite(output_path, image);
    std::cout << "Result saved to: " << output_path << std::endl;

    cv::imshow("Detection Result", image);
    std::cout << "Press any key to close..." << std::endl;
    cv::waitKey(0);
}

void test_video(const std::string& model_path, const std::string& video_path) {
    std::cout << "Loading model: " << model_path << std::endl;
    YOLOOnnx detector(model_path);

    std::cout << "Opening video: " << video_path << std::endl;
    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video: " << video_path << std::endl;
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

    std::cout << "Processing video... (Press 'q' to quit)" << std::endl;

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
            std::string label = "Class " + std::to_string(det.class_id) + ": " +
                               std::to_string(static_cast<int>(det.confidence * 100)) + "%";
            cv::putText(frame, label,
                       cv::Point(det.box.x, det.box.y - 5),
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
            std::cout << "Processed " << frame_count << " frames, avg FPS: "
                     << static_cast<int>(avg_fps) << std::endl;
        }
    }

    std::cout << "Processed " << frame_count << " frames" << std::endl;
    std::cout << "Average inference time: " << total_time / frame_count << "ms" << std::endl;
    std::cout << "Average FPS: " << frame_count / (total_time / 1000.0) << std::endl;
    std::cout << "Output saved to: " << output_path << std::endl;
}

void benchmark(const std::string& model_path, const std::string& image_path, int num_iterations = 100) {
    std::cout << "Loading model: " << model_path << std::endl;
    YOLOOnnx detector(model_path);

    std::cout << "Loading image: " << image_path << std::endl;
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "Error: Could not load image: " << image_path << std::endl;
        return;
    }

    std::cout << "Running " << num_iterations << " iterations for benchmarking..." << std::endl;

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

    // Calculate statistics
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
        } else if (mode == "video") {
            test_video(model_path, input_path);
        } else if (mode == "benchmark") {
            benchmark(model_path, input_path);
        } else {
            std::cerr << "Unknown mode: " << mode << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
