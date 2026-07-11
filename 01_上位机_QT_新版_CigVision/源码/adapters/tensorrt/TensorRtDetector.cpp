#include "TensorRtDetector.h"

#include <NvInfer.h>
#include <cuda_runtime_api.h>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cigvision {
namespace {

constexpr std::size_t kClassCount = 9;
constexpr std::size_t kOutputRows = 300;
constexpr std::size_t kOutputColumns = 6;
constexpr char kPreprocessMode[] = "stretch-rgb-f32";

class TensorRtLogger final : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* message) noexcept override
    {
        if (severity <= Severity::kERROR && message != nullptr) {
            try {
                std::lock_guard<std::mutex> lock(mutex_);
                lastError_ = message;
            }
            catch (...) {
            }
        }
    }

    std::string lastError() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastError_;
    }

private:
    mutable std::mutex mutex_;
    std::string lastError_;
};

template <typename T>
struct TensorRtDeleter {
    void operator()(T* object) const noexcept
    {
        delete object;
    }
};

template <typename T>
using TensorRtPtr = std::unique_ptr<T, TensorRtDeleter<T>>;

class DetectFailure final : public std::runtime_error {
public:
    DetectFailure(std::string code, std::string message)
        : std::runtime_error(std::move(message)), code_(std::move(code)) {}

    const std::string& code() const noexcept { return code_; }

private:
    std::string code_;
};

std::string cudaMessage(const char* operation, cudaError_t status)
{
    std::ostringstream stream;
    stream << operation << " failed: " << cudaGetErrorName(status) << " ("
           << cudaGetErrorString(status) << ')';
    return stream.str();
}

void checkCudaForConstruction(cudaError_t status, const char* operation)
{
    if (status != cudaSuccess) {
        throw std::runtime_error(cudaMessage(operation, status));
    }
}

void checkCudaForDetection(cudaError_t status, const char* operation)
{
    if (status != cudaSuccess) {
        throw DetectFailure("CUDA_RUNTIME_ERROR", cudaMessage(operation, status));
    }
}

void checkCudaForCleanup(cudaError_t status, const char* operation) noexcept
{
    if (status != cudaSuccess) {
        const char* name = cudaGetErrorName(status);
        const char* description = cudaGetErrorString(status);
        std::fprintf(stderr, "TensorRtDetector cleanup: %s failed: %s (%s)\n",
            operation, name != nullptr ? name : "unknown CUDA error",
            description != nullptr ? description : "no CUDA error description");
    }
}

std::vector<char> readEngineFile(const std::string& path)
{
    std::ifstream input(path.c_str(), std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("cannot open TensorRT engine: " + path);
    }

    const std::ifstream::pos_type end = input.tellg();
    if (end <= std::ifstream::pos_type(0)) {
        throw std::runtime_error("TensorRT engine is empty or unreadable: " + path);
    }
    if (static_cast<unsigned long long>(end) >
        static_cast<unsigned long long>((std::numeric_limits<std::size_t>::max)())) {
        throw std::runtime_error("TensorRT engine is too large to load: " + path);
    }

    std::vector<char> bytes(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("failed to read complete TensorRT engine: " + path);
    }
    return bytes;
}

void validateConfig(const TensorRtDetectorConfig& config)
{
    if (config.enginePath.empty()) {
        throw std::runtime_error("enginePath is required");
    }
    if (config.inputTensorName.empty() || config.outputTensorName.empty()) {
        throw std::runtime_error("inputTensorName and outputTensorName are required");
    }
    if (config.inputTensorName != "images" || config.outputTensorName != "output0") {
        throw std::runtime_error("tensor names must be 'images' and 'output0'");
    }
    if (config.inputTensorName == config.outputTensorName) {
        throw std::runtime_error("input and output tensor names must differ");
    }
    if (config.inputWidth != 992 || config.inputHeight != 992) {
        throw std::runtime_error("TensorRtDetector requires a 992x992 input configuration");
    }
    if (!std::isfinite(config.confidenceThreshold) ||
        config.confidenceThreshold < 0.0F || config.confidenceThreshold > 1.0F) {
        throw std::runtime_error("confidenceThreshold must be finite and within [0, 1]");
    }
    if (config.classNames.size() != kClassCount) {
        throw std::runtime_error("classNames must contain exactly 9 entries");
    }
    for (std::size_t i = 0; i < config.classNames.size(); ++i) {
        if (config.classNames[i].empty()) {
            throw std::runtime_error("classNames entries must be non-empty");
        }
    }
    for (std::int32_t classId : config.disabledClassIds) {
        if (classId < 0 || classId >= static_cast<std::int32_t>(kClassCount)) {
            throw std::runtime_error("disabledClassIds contains an ID outside [0, 8]");
        }
    }
    if (config.detectorVersion.empty()) {
        throw std::runtime_error("detectorVersion is required");
    }
    if (config.preprocessMode != kPreprocessMode) {
        throw std::runtime_error("preprocessMode must be stretch-rgb-f32");
    }
}

TensorRtDetectorConfig validatedConfig(TensorRtDetectorConfig config)
{
    validateConfig(config);
    return config;
}

bool dimensionsEqual(const nvinfer1::Dims& dimensions,
    std::initializer_list<std::int64_t> expected)
{
    if (dimensions.nbDims != static_cast<std::int32_t>(expected.size())) {
        return false;
    }
    std::size_t index = 0;
    for (std::int64_t value : expected) {
        if (dimensions.d[index++] != value) {
            return false;
        }
    }
    return true;
}

std::string dimensionsText(const nvinfer1::Dims& dimensions)
{
    std::ostringstream stream;
    stream << '[';
    for (std::int32_t i = 0; i < dimensions.nbDims; ++i) {
        if (i != 0) {
            stream << ',';
        }
        stream << dimensions.d[i];
    }
    stream << ']';
    return stream.str();
}

std::uint64_t elapsedMicros(std::chrono::steady_clock::time_point startedAt)
{
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startedAt).count();
    return elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0U;
}

std::size_t checkedElementCount(std::uint32_t width, std::uint32_t height,
    std::size_t channels)
{
    const std::size_t w = width;
    const std::size_t h = height;
    if (w == 0U || h == 0U || channels == 0U) {
        throw std::runtime_error("configured input dimensions must be non-zero");
    }
    if (w > (std::numeric_limits<std::size_t>::max)() / h ||
        w * h > (std::numeric_limits<std::size_t>::max)() / channels) {
        throw std::runtime_error("configured input element count overflows size_t");
    }
    return w * h * channels;
}

} // namespace

class TensorRtDetector::Impl {
public:
    explicit Impl(TensorRtDetectorConfig config)
        : config_(validatedConfig(std::move(config))), inputElementCount_(checkedElementCount(
              config_.inputWidth, config_.inputHeight, 3)),
          hostInput_(inputElementCount_), hostOutput_(kOutputRows * kOutputColumns)
    {
        const std::vector<char> engineBytes = readEngineFile(config_.enginePath);

        runtime_.reset(nvinfer1::createInferRuntime(logger_));
        if (!runtime_) {
            throw std::runtime_error("createInferRuntime returned null");
        }
        engine_.reset(runtime_->deserializeCudaEngine(engineBytes.data(), engineBytes.size()));
        if (!engine_) {
            const std::string detail = logger_.lastError();
            throw std::runtime_error("failed to deserialize TensorRT engine" +
                (detail.empty() ? std::string() : ": " + detail));
        }

        validateEngine();
        context_.reset(engine_->createExecutionContext());
        if (!context_) {
            throw std::runtime_error("createExecutionContext returned null");
        }

        checkCudaForConstruction(cudaStreamCreate(&stream_), "cudaStreamCreate");
        try {
            checkCudaForConstruction(cudaMalloc(&deviceInput_,
                inputElementCount_ * sizeof(float)), "cudaMalloc(input)");
            checkCudaForConstruction(cudaMalloc(&deviceOutput_,
                hostOutput_.size() * sizeof(float)), "cudaMalloc(output)");
            if (!context_->setTensorAddress(config_.inputTensorName.c_str(), deviceInput_)) {
                throw std::runtime_error("setTensorAddress failed for input tensor '" +
                    config_.inputTensorName + "'");
            }
            if (!context_->setTensorAddress(config_.outputTensorName.c_str(), deviceOutput_)) {
                throw std::runtime_error("setTensorAddress failed for output tensor '" +
                    config_.outputTensorName + "'");
            }
        } catch (...) {
            releaseCudaResources();
            throw;
        }
    }

    ~Impl()
    {
        releaseCudaResources();
    }

    DetectionBatch detect(const FramePacket& frame)
    {
        const auto startedAt = std::chrono::steady_clock::now();
        DetectionBatch batch;
        batch.frameId = frame.frameId;
        batch.detectorVersion = config_.detectorVersion;

        try {
            std::lock_guard<std::mutex> lock(mutex_);
            validateFrame(frame);
            preprocess(frame);
            infer();
            batch.detections = postprocess(frame);
        } catch (const DetectFailure& error) {
            batch.detections.clear();
            batch.errorCode = error.code();
            batch.errorMessage = error.what();
        } catch (const std::exception& error) {
            batch.detections.clear();
            batch.errorCode = "DETECTOR_RUNTIME_ERROR";
            batch.errorMessage = error.what();
        } catch (...) {
            batch.detections.clear();
            batch.errorCode = "DETECTOR_RUNTIME_ERROR";
            batch.errorMessage = "unknown TensorRtDetector runtime error";
        }

        batch.elapsedMicros = elapsedMicros(startedAt);
        return batch;
    }

private:
    void validateEngine() const
    {
        const std::int32_t tensorCount = engine_->getNbIOTensors();
        if (tensorCount != 2) {
            std::ostringstream message;
            message << "engine must expose exactly 2 I/O tensors, found " << tensorCount;
            throw std::runtime_error(message.str());
        }

        bool foundInput = false;
        bool foundOutput = false;
        for (std::int32_t i = 0; i < tensorCount; ++i) {
            const char* name = engine_->getIOTensorName(i);
            if (name == nullptr || *name == '\0') {
                throw std::runtime_error("getIOTensorName returned an invalid name");
            }
            const std::string tensorName(name);
            if (tensorName == config_.inputTensorName) {
                foundInput = true;
                validateTensor(tensorName, nvinfer1::TensorIOMode::kINPUT,
                    { 1, 3, static_cast<std::int64_t>(config_.inputHeight),
                        static_cast<std::int64_t>(config_.inputWidth) });
            } else if (tensorName == config_.outputTensorName) {
                foundOutput = true;
                validateTensor(tensorName, nvinfer1::TensorIOMode::kOUTPUT,
                    { 1, static_cast<std::int64_t>(kOutputRows),
                        static_cast<std::int64_t>(kOutputColumns) });
            } else {
                throw std::runtime_error("engine exposes unexpected I/O tensor '" +
                    tensorName + "'");
            }
        }
        if (!foundInput || !foundOutput) {
            throw std::runtime_error("engine does not expose both configured tensor names");
        }
    }

    void validateTensor(const std::string& name, nvinfer1::TensorIOMode expectedMode,
        std::initializer_list<std::int64_t> expectedDimensions) const
    {
        if (engine_->getTensorIOMode(name.c_str()) != expectedMode) {
            throw std::runtime_error("tensor '" + name + "' has the wrong I/O mode");
        }
        if (engine_->getTensorDataType(name.c_str()) != nvinfer1::DataType::kFLOAT) {
            throw std::runtime_error("tensor '" + name + "' must use FP32 data");
        }
        if (engine_->getTensorLocation(name.c_str()) != nvinfer1::TensorLocation::kDEVICE) {
            throw std::runtime_error("tensor '" + name + "' must use device memory");
        }
        const nvinfer1::Dims dimensions = engine_->getTensorShape(name.c_str());
        if (!dimensionsEqual(dimensions, expectedDimensions)) {
            throw std::runtime_error("tensor '" + name + "' has shape " +
                dimensionsText(dimensions) + ", expected fixed model shape");
        }
    }

    void validateFrame(const FramePacket& frame) const
    {
        if (frame.pixelFormat != PixelFormat::Mono8 &&
            frame.pixelFormat != PixelFormat::RGB8 &&
            frame.pixelFormat != PixelFormat::BGR8) {
            throw DetectFailure("UNSUPPORTED_PIXEL_FORMAT",
                "FramePacket pixelFormat must be Mono8, RGB8, or BGR8");
        }
        std::string reason;
        if (!frame.validate(&reason)) {
            throw DetectFailure("INVALID_FRAME", "FramePacket validation failed: " + reason);
        }

        const std::size_t bytesPerPixelValue = bytesPerPixel(frame.pixelFormat);
        const std::size_t packedRowBytes = static_cast<std::size_t>(frame.width) *
            bytesPerPixelValue;
        if (frame.strideBytes < packedRowBytes) {
            throw DetectFailure("INVALID_FRAME_STRIDE",
                "FramePacket strideBytes is smaller than width * bytesPerPixel");
        }
        const std::size_t requiredBytes = static_cast<std::size_t>(frame.strideBytes) *
            static_cast<std::size_t>(frame.height);
        if (frame.pixels.size() < requiredBytes) {
            throw DetectFailure("INVALID_FRAME_BUFFER",
                "FramePacket pixels is smaller than strideBytes * height");
        }
    }

    void preprocess(const FramePacket& frame)
    {
        const std::size_t destinationWidth = config_.inputWidth;
        const std::size_t destinationHeight = config_.inputHeight;
        const std::size_t planeSize = destinationWidth * destinationHeight;
        const int sourceType = frame.pixelFormat == PixelFormat::Mono8 ? CV_8UC1 : CV_8UC3;
        const cv::Mat source(static_cast<int>(frame.height), static_cast<int>(frame.width),
            sourceType, const_cast<std::uint8_t*>(frame.pixels.data()), frame.strideBytes);
        cv::Mat rgb;
        if (frame.pixelFormat == PixelFormat::Mono8) {
            cv::cvtColor(source, rgb, cv::COLOR_GRAY2RGB);
        } else if (frame.pixelFormat == PixelFormat::BGR8) {
            cv::cvtColor(source, rgb, cv::COLOR_BGR2RGB);
        } else {
            rgb = source;
        }
        cv::Mat resized;
        cv::resize(rgb, resized, cv::Size(static_cast<int>(destinationWidth),
            static_cast<int>(destinationHeight)), 0.0, 0.0, cv::INTER_LINEAR);
        for (std::size_t y = 0; y < destinationHeight; ++y) {
            const cv::Vec3b* row = resized.ptr<cv::Vec3b>(static_cast<int>(y));
            for (std::size_t x = 0; x < destinationWidth; ++x) {
                const std::size_t destinationIndex = y * destinationWidth + x;
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    hostInput_[channel * planeSize + destinationIndex] =
                        static_cast<float>(row[x][static_cast<int>(channel)]) / 255.0F;
                }
            }
        }
    }

    void infer()
    {
        checkCudaForDetection(cudaMemcpyAsync(deviceInput_, hostInput_.data(),
            hostInput_.size() * sizeof(float), cudaMemcpyHostToDevice, stream_),
            "cudaMemcpyAsync(input)");
        if (!context_->enqueueV3(stream_)) {
            throw DetectFailure("TENSORRT_ENQUEUE_FAILED", "TensorRT enqueueV3 returned false");
        }
        checkCudaForDetection(cudaMemcpyAsync(hostOutput_.data(), deviceOutput_,
            hostOutput_.size() * sizeof(float), cudaMemcpyDeviceToHost, stream_),
            "cudaMemcpyAsync(output)");
        checkCudaForDetection(cudaStreamSynchronize(stream_), "cudaStreamSynchronize");
    }

    std::vector<Detection> postprocess(const FramePacket& frame) const
    {
        std::vector<Detection> detections;
        detections.reserve(kOutputRows);
        const float inverseScaleX = static_cast<float>(frame.width) /
            static_cast<float>(config_.inputWidth);
        const float inverseScaleY = static_cast<float>(frame.height) /
            static_cast<float>(config_.inputHeight);

        for (std::size_t row = 0; row < kOutputRows; ++row) {
            const float* values = &hostOutput_[row * kOutputColumns];
            const float score = values[4];
            if (!std::isfinite(score) || score < 0.0F || score > 1.0F) {
                throw invalidOutput(row, "score must be finite and within [0, 1]");
            }
            if (score < config_.confidenceThreshold) {
                continue;
            }

            for (std::size_t column = 0; column < kOutputColumns; ++column) {
                if (!std::isfinite(values[column])) {
                    throw invalidOutput(row, "all six values must be finite");
                }
            }
            const float roundedClassId = std::round(values[5]);
            if (std::fabs(values[5] - roundedClassId) > 1.0e-4F ||
                roundedClassId < 0.0F || roundedClassId >= static_cast<float>(kClassCount)) {
                throw invalidOutput(row, "class ID must be an integer within [0, 8]");
            }
            const std::int32_t classId = static_cast<std::int32_t>(roundedClassId);
            if (values[2] <= values[0] || values[3] <= values[1]) {
                throw invalidOutput(row, "box must satisfy x2 > x1 and y2 > y1");
            }
            if (config_.disabledClassIds.count(classId) != 0U) {
                continue;
            }

            const float x1 = clamp(values[0] * inverseScaleX, 0.0F,
                static_cast<float>(frame.width));
            const float y1 = clamp(values[1] * inverseScaleY, 0.0F,
                static_cast<float>(frame.height));
            const float x2 = clamp(values[2] * inverseScaleX, 0.0F,
                static_cast<float>(frame.width));
            const float y2 = clamp(values[3] * inverseScaleY, 0.0F,
                static_cast<float>(frame.height));
            if (x2 <= x1 || y2 <= y1) {
                throw invalidOutput(row, "box is empty after mapping and clamping");
            }

            Detection detection;
            detection.classId = classId;
            detection.className = config_.classNames[static_cast<std::size_t>(classId)];
            detection.confidence = score;
            detection.box = { x1, y1, x2 - x1, y2 - y1 };
            detection.detectorVersion = config_.detectorVersion;
            detections.push_back(std::move(detection));
        }
        return detections;
    }

    static DetectFailure invalidOutput(std::size_t row, const char* reason)
    {
        std::ostringstream message;
        message << "TensorRT output row " << row << " is invalid: " << reason;
        return DetectFailure("INVALID_MODEL_OUTPUT", message.str());
    }

    static float clamp(float value, float minimum, float maximum)
    {
        return std::max(minimum, std::min(value, maximum));
    }

    void releaseCudaResources() noexcept
    {
        if (deviceOutput_ != nullptr) {
            checkCudaForCleanup(cudaFree(deviceOutput_), "cudaFree(output)");
            deviceOutput_ = nullptr;
        }
        if (deviceInput_ != nullptr) {
            checkCudaForCleanup(cudaFree(deviceInput_), "cudaFree(input)");
            deviceInput_ = nullptr;
        }
        if (stream_ != nullptr) {
            checkCudaForCleanup(cudaStreamDestroy(stream_), "cudaStreamDestroy");
            stream_ = nullptr;
        }
    }

    TensorRtDetectorConfig config_;
    std::size_t inputElementCount_ = 0;
    TensorRtLogger logger_;
    TensorRtPtr<nvinfer1::IRuntime> runtime_;
    TensorRtPtr<nvinfer1::ICudaEngine> engine_;
    TensorRtPtr<nvinfer1::IExecutionContext> context_;
    cudaStream_t stream_ = nullptr;
    void* deviceInput_ = nullptr;
    void* deviceOutput_ = nullptr;
    std::vector<float> hostInput_;
    std::vector<float> hostOutput_;
    std::mutex mutex_;
};

TensorRtDetector::TensorRtDetector(TensorRtDetectorConfig config)
    : impl_(new Impl(std::move(config))) {}

TensorRtDetector::~TensorRtDetector() = default;

DetectionBatch TensorRtDetector::detect(const FramePacket& frame)
{
    return impl_->detect(frame);
}

} // namespace cigvision
