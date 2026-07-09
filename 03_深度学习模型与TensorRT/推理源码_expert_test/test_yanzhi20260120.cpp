#include "yolo_trt_v2.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <vector>
#include <iomanip>

namespace fs = std::filesystem;

void print_banner() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  YOLO TensorRT Inference Test" << std::endl;
    std::cout << "  Model: yanzhi20260120.engine" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void process_image(YOLOTensorRTv2& detector, const std::string& image_path, const std::string& output_dir) {
    std::cout << "\n------ Processing: " << image_path << " ------" << std::endl;

    // Load image
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "Failed to load image: " << image_path << std::endl;
        return;
    }

    std::cout << "Image size: " << image.cols << "x" << image.rows << std::endl;

    // Run detection with timing
    auto start = std::chrono::high_resolution_clock::now();
    auto detections = detector.detect(image);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Inference time: " << duration.count() << " ms" << std::endl;
    std::cout << "Detections found: " << detections.size() << std::endl;

    // Print detection details
    if (!detections.empty()) {
        std::cout << "\nDetection results:" << std::endl;
        std::cout << std::setw(5) << "No."
                  << std::setw(10) << "Class"
                  << std::setw(12) << "Confidence"
                  << std::setw(40) << "BBox [x, y, w, h]" << std::endl;
        std::cout << std::string(67, '-') << std::endl;

        for (size_t i = 0; i < detections.size(); ++i) {
            const auto& det = detections[i];
            std::cout << std::setw(5) << (i + 1)
                      << std::setw(10) << det.class_id
                      << std::setw(11) << std::fixed << std::setprecision(2)
                      << (det.confidence * 100) << "%"
                      << std::setw(10) << ""
                      << "[" << det.box.x << ", " << det.box.y << ", "
                      << det.box.width << ", " << det.box.height << "]" << std::endl;

            // Draw bounding box
            cv::rectangle(image, det.box, cv::Scalar(0, 255, 0), 2);

            // Draw label
            std::string label = "Class " + std::to_string(det.class_id) +
                               ": " + std::to_string(static_cast<int>(det.confidence * 100)) + "%";

            int baseline = 0;
            cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

            // Draw text background
            cv::rectangle(image,
                         cv::Point(det.box.x, det.box.y - textSize.height - 5),
                         cv::Point(det.box.x + textSize.width, det.box.y),
                         cv::Scalar(0, 255, 0), cv::FILLED);

            // Draw text
            cv::putText(image, label,
                       cv::Point(det.box.x, det.box.y - 5),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
        }
    } else {
        std::cout << "No detections found." << std::endl;
    }

    // Create output directory if it doesn't exist
    if (!fs::exists(output_dir)) {
        fs::create_directories(output_dir);
    }

    // Save result image
    fs::path input_path(image_path);
    std::string output_filename = "result_" + input_path.filename().string();
    std::string output_path = output_dir + "/" + output_filename;

    cv::imwrite(output_path, image);
    std::cout << "Result saved to: " << output_path << std::endl;

    // Display image
    cv::imshow("Detection Result - " + input_path.filename().string(), image);
    std::cout << "\nPress any key to continue..." << std::endl;
    cv::waitKey(0);
    cv::destroyAllWindows();
}

int main(int argc, char** argv) {
    print_banner();

    // Default paths
    std::string model_path = "yanzhi20260120.engine";
    std::string test_dir = "inference/test/";
    std::string output_dir = "inference/test_results";

    // Parse command line arguments
    if (argc >= 2) {
        model_path = argv[1];
    }
    if (argc >= 3) {
        test_dir = argv[2];
    }
    if (argc >= 4) {
        output_dir = argv[3];
    }

    std::cout << "Model:       " << model_path << std::endl;
    std::cout << "Test images: " << test_dir << std::endl;
    std::cout << "Output dir:  " << output_dir << std::endl;
    std::cout << std::endl;

    // Check if model file exists
    if (!fs::exists(model_path)) {
        std::cerr << "Error: Model file not found: " << model_path << std::endl;
        std::cerr << "\nUsage:" << std::endl;
        std::cerr << "  " << argv[0] << " [model_path] [test_dir] [output_dir]" << std::endl;
        std::cerr << "\nDefault:" << std::endl;
        std::cerr << "  model_path = yanzhi20260120.engine" << std::endl;
        std::cerr << "  test_dir   = inference/test/" << std::endl;
        std::cerr << "  output_dir = inference/test_results" << std::endl;
        return 1;
    }

    // Check if test directory exists
    if (!fs::exists(test_dir)) {
        std::cerr << "Error: Test directory not found: " << test_dir << std::endl;
        return 1;
    }

    try {
        // Initialize detector
        std::cout << "Loading TensorRT model..." << std::endl;
        YOLOTensorRTv2 detector(model_path, 992, 0.25f, 0.45f);
        std::cout << "Model loaded successfully!\n" << std::endl;

        // Find all image files in test directory
        std::vector<std::string> image_paths;
        std::vector<std::string> extensions = {".jpg", ".jpeg", ".png", ".bmp"};

        for (const auto& entry : fs::directory_iterator(test_dir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
                    image_paths.push_back(entry.path().string());
                }
            }
        }

        if (image_paths.empty()) {
            std::cerr << "No image files found in: " << test_dir << std::endl;
            return 1;
        }

        std::cout << "Found " << image_paths.size() << " image(s) to process.\n" << std::endl;

        // Process all images
        auto total_start = std::chrono::high_resolution_clock::now();

        for (const auto& image_path : image_paths) {
            process_image(detector, image_path, output_dir);
        }

        auto total_end = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);

        // Print summary
        std::cout << "\n========================================" << std::endl;
        std::cout << "Processing Summary" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Total images:    " << image_paths.size() << std::endl;
        std::cout << "Total time:      " << total_duration.count() << " ms" << std::endl;
        std::cout << "Average time:    " << total_duration.count() / image_paths.size() << " ms/image" << std::endl;
        std::cout << "Output saved to: " << output_dir << std::endl;
        std::cout << "========================================\n" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
