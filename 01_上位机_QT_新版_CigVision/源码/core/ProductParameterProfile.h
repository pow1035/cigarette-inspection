#pragma once

#include "Sha256.h"

#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace cigvision {

enum class ProductParameterProfileKind {
    LegacyDeepLearningPage = 0,
    DeterministicFixture,
    TensorRtOffline
};

inline const char* productParameterProfileKindName(ProductParameterProfileKind kind)
{
    switch (kind) {
    case ProductParameterProfileKind::LegacyDeepLearningPage:
        return "LEGACY_DEEP_LEARNING_PAGE";
    case ProductParameterProfileKind::DeterministicFixture:
        return "DETERMINISTIC_FIXTURE";
    case ProductParameterProfileKind::TensorRtOffline:
        return "TENSORRT_OFFLINE";
    default:
        return "UNKNOWN";
    }
}

struct ProductClassParameter {
    std::int32_t classId = -1;
    std::string className;
    float confidenceThreshold = 0.0F;
    bool enabled = true;

    bool validate(std::string* reason = nullptr) const
    {
        if (classId < 0 || className.empty() || !std::isfinite(confidenceThreshold) ||
            confidenceThreshold < 0.0F || confidenceThreshold > 1.0F) {
            return fail(reason, "class ID, name and confidence threshold are invalid");
        }
        if (reason != nullptr) {
            reason->clear();
        }
        return true;
    }

private:
    static bool fail(std::string* reason, const char* message)
    {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    }
};

struct ProductParameterProfile {
    std::string schemaVersion = "cigvision-product-parameters-v1";
    ProductParameterProfileKind kind =
        ProductParameterProfileKind::DeterministicFixture;
    std::string profileId;
    std::string parameterVersion;
    std::string detectorVersion;
    std::string modelSha256;
    std::string inputTensorName;
    std::string outputTensorName;
    std::uint32_t inputWidth = 0;
    std::uint32_t inputHeight = 0;
    std::string preprocessMode;
    std::vector<ProductClassParameter> classes;

    bool validate(std::string* reason = nullptr) const
    {
        if (schemaVersion != "cigvision-product-parameters-v1" ||
            profileId.empty() || parameterVersion.empty()) {
            return fail(reason, "parameter schema, profile and version are required");
        }
        if (!validateClasses(reason)) {
            return false;
        }
        switch (kind) {
        case ProductParameterProfileKind::LegacyDeepLearningPage:
            if (!detectorVersion.empty() || !modelSha256.empty() ||
                !inputTensorName.empty() || !outputTensorName.empty() ||
                inputWidth != 0U || inputHeight != 0U || !preprocessMode.empty() ||
                classes.size() != 9U) {
                return fail(reason,
                    "legacy page profile must contain exactly nine class rules and no runtime detector fields");
            }
            break;
        case ProductParameterProfileKind::DeterministicFixture:
            if (detectorVersion.empty() || !modelSha256.empty() ||
                !inputTensorName.empty() || !outputTensorName.empty() ||
                inputWidth != 0U || inputHeight != 0U || !preprocessMode.empty() ||
                !classes.empty()) {
                return fail(reason,
                    "fixture profile must identify only its deterministic detector");
            }
            break;
        case ProductParameterProfileKind::TensorRtOffline:
            if (detectorVersion.empty() || !isSha256Hex(modelSha256) ||
                inputTensorName.empty() || outputTensorName.empty() ||
                inputTensorName == outputTensorName || inputWidth == 0U ||
                inputHeight == 0U || preprocessMode.empty() || classes.size() != 9U) {
                return fail(reason,
                    "TensorRT profile requires model, tensor, shape, preprocess and nine class rules");
            }
            break;
        default:
            return fail(reason, "parameter profile kind is unsupported");
        }
        if (reason != nullptr) {
            reason->clear();
        }
        return true;
    }

    std::string canonicalPayload() const
    {
        std::string result;
        append(result, "schemaVersion", schemaVersion);
        append(result, "kind", productParameterProfileKindName(kind));
        append(result, "profileId", profileId);
        append(result, "parameterVersion", parameterVersion);
        append(result, "detectorVersion", detectorVersion);
        append(result, "modelSha256", canonicalSha256(modelSha256));
        append(result, "inputTensorName", inputTensorName);
        append(result, "outputTensorName", outputTensorName);
        append(result, "inputWidth", std::to_string(inputWidth));
        append(result, "inputHeight", std::to_string(inputHeight));
        append(result, "preprocessMode", preprocessMode);
        append(result, "classCount", std::to_string(classes.size()));
        for (const ProductClassParameter& value : classes) {
            append(result, "classId", std::to_string(value.classId));
            append(result, "className", value.className);
            append(result, "confidenceThreshold", canonicalFloat(value.confidenceThreshold));
            append(result, "enabled", value.enabled ? "1" : "0");
        }
        return result;
    }

    std::string sha256() const
    {
        return sha256Hex(canonicalPayload());
    }

private:
    bool validateClasses(std::string* reason) const
    {
        for (std::size_t index = 0; index < classes.size(); ++index) {
            std::string classError;
            if (!classes[index].validate(&classError) ||
                classes[index].classId != static_cast<std::int32_t>(index)) {
                return fail(reason,
                    "class rules must be valid, unique and ordered from ID zero");
            }
        }
        return true;
    }

    static std::string canonicalFloat(float value)
    {
        static_assert(std::numeric_limits<float>::is_iec559 &&
            sizeof(float) == sizeof(std::uint32_t),
            "parameter identity requires IEEE-754 binary32");
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        static const char digits[] = "0123456789abcdef";
        std::string result(8U, '0');
        for (std::size_t index = 0; index < 8U; ++index) {
            const std::uint32_t shift = static_cast<std::uint32_t>((7U - index) * 4U);
            result[index] = digits[(bits >> shift) & 0x0fU];
        }
        return result;
    }

    static std::string canonicalSha256(const std::string& value)
    {
        std::string result = value;
        for (char& character : result) {
            if (character >= 'A' && character <= 'F') {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }
        return result;
    }

    static void append(std::string& destination, const char* key,
        const std::string& value)
    {
        destination += std::to_string(std::char_traits<char>::length(key));
        destination.push_back(':');
        destination += key;
        destination += std::to_string(value.size());
        destination.push_back(':');
        destination += value;
    }

    static bool fail(std::string* reason, const char* message)
    {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    }
};

} // namespace cigvision
