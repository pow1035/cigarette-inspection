#pragma once

#include "core/InspectionInterfaces.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace cigvision {

struct TensorRtDetectorConfig {
    std::string enginePath;
    std::string inputTensorName = "images";
    std::string outputTensorName = "output0";
    std::uint32_t inputWidth = 992;
    std::uint32_t inputHeight = 992;
    float confidenceThreshold = 0.25F;
    std::vector<std::string> classNames;
    std::unordered_set<std::int32_t> disabledClassIds;
    std::string detectorVersion;
    std::string preprocessMode = "stretch-rgb-f32";
};

class TensorRtDetector final : public IDetector {
public:
    using Config = TensorRtDetectorConfig;

    explicit TensorRtDetector(TensorRtDetectorConfig config);
    ~TensorRtDetector() override;

    TensorRtDetector(const TensorRtDetector&) = delete;
    TensorRtDetector& operator=(const TensorRtDetector&) = delete;
    TensorRtDetector(TensorRtDetector&&) = delete;
    TensorRtDetector& operator=(TensorRtDetector&&) = delete;

    DetectionBatch detect(const FramePacket& frame) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cigvision
