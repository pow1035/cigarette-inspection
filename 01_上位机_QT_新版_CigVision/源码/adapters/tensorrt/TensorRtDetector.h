#pragma once

#include "core/InspectionInterfaces.h"
#include "core/ProductParameterProfile.h"

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
    std::vector<float> classConfidenceThresholds;
    std::vector<std::string> classNames;
    std::unordered_set<std::int32_t> disabledClassIds;
    std::string detectorVersion;
    std::string parameterVersion = "tensorrt-parameters-v1";
    std::string modelSha256;
    std::string preprocessMode = "stretch-rgb-f32";
};

inline ProductParameterProfile tensorRtParameterProfile(
    const TensorRtDetectorConfig& config)
{
    ProductParameterProfile profile;
    profile.kind = ProductParameterProfileKind::TensorRtOffline;
    profile.profileId = "tensorrt-product";
    profile.parameterVersion = config.parameterVersion;
    profile.detectorVersion = config.detectorVersion;
    profile.modelSha256 = config.modelSha256;
    profile.inputTensorName = config.inputTensorName;
    profile.outputTensorName = config.outputTensorName;
    profile.inputWidth = config.inputWidth;
    profile.inputHeight = config.inputHeight;
    profile.preprocessMode = config.preprocessMode;
    for (std::size_t index = 0; index < config.classNames.size(); ++index) {
        ProductClassParameter rule;
        rule.classId = static_cast<std::int32_t>(index);
        rule.className = config.classNames[index];
        rule.confidenceThreshold =
            config.classConfidenceThresholds.size() == config.classNames.size()
            ? config.classConfidenceThresholds[index] : config.confidenceThreshold;
        rule.enabled = config.disabledClassIds.count(rule.classId) == 0U;
        profile.classes.push_back(rule);
    }
    return profile;
}

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
