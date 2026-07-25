#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace cigvision {

using TimestampMicros = std::int64_t;

enum class PixelFormat {
    Unknown = 0,
    Mono8,
    RGB8,
    BGR8
};

inline std::size_t bytesPerPixel(PixelFormat format)
{
    switch (format) {
    case PixelFormat::Mono8:
        return 1;
    case PixelFormat::RGB8:
    case PixelFormat::BGR8:
        return 3;
    default:
        return 0;
    }
}

struct FramePacket {
    std::uint64_t frameId = 0;
    std::string stationId;
    std::string cameraId;
    std::uint32_t cigaretteNumber = 0;
    TimestampMicros capturedAt = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t strideBytes = 0;
    PixelFormat pixelFormat = PixelFormat::Unknown;
    std::vector<std::uint8_t> pixels;

    bool validate(std::string* reason = nullptr) const
    {
        if (frameId == 0) {
            return fail(reason, "frameId must be non-zero");
        }
        if (stationId.empty() || cameraId.empty()) {
            return fail(reason, "stationId and cameraId are required");
        }
        if (capturedAt <= 0) {
            return fail(reason, "capturedAt must be positive");
        }
        if (width == 0 || height == 0) {
            return fail(reason, "width and height must be non-zero");
        }

        const std::size_t channelBytes = bytesPerPixel(pixelFormat);
        if (channelBytes == 0) {
            return fail(reason, "pixelFormat is unsupported");
        }
        if (width > (std::numeric_limits<std::size_t>::max)() / channelBytes) {
            return fail(reason, "row byte count overflows size_t");
        }

        const std::size_t minimumStride = static_cast<std::size_t>(width) * channelBytes;
        if (strideBytes < minimumStride) {
            return fail(reason, "strideBytes is smaller than one pixel row");
        }
        if (height > (std::numeric_limits<std::size_t>::max)() / strideBytes) {
            return fail(reason, "payload byte count overflows size_t");
        }

        const std::size_t requiredBytes = static_cast<std::size_t>(strideBytes) * height;
        if (pixels.size() < requiredBytes) {
            return fail(reason, "pixel buffer is smaller than strideBytes * height");
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

struct BoundingBox {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;

    bool isValid() const
    {
        return x >= 0.0F && y >= 0.0F && width > 0.0F && height > 0.0F;
    }
};

struct Detection {
    std::int32_t classId = -1;
    std::string className;
    float confidence = 0.0F;
    BoundingBox box;
    std::string detectorVersion;

    bool isValid() const
    {
        return classId >= 0 && !className.empty() && confidence >= 0.0F &&
            confidence <= 1.0F && box.isValid() && !detectorVersion.empty();
    }
};

struct DetectionBatch {
    std::uint64_t frameId = 0;
    std::vector<Detection> detections;
    std::uint64_t elapsedMicros = 0;
    std::string detectorVersion;
    std::string parameterVersion;
    std::string parameterSha256;
    std::string errorCode;
    std::string errorMessage;

    bool succeeded() const
    {
        return errorCode.empty();
    }
};

enum class InspectionDecision {
    Unknown = 0,
    Ok,
    Ng,
    Error
};

inline bool isValidInspectionDecision(InspectionDecision decision)
{
    switch (decision) {
    case InspectionDecision::Unknown:
    case InspectionDecision::Ok:
    case InspectionDecision::Ng:
    case InspectionDecision::Error:
        return true;
    default:
        return false;
    }
}

inline bool isSha256Identity(const std::string& value)
{
    if (value.size() != 64U) {
        return false;
    }
    for (char character : value) {
        const bool digit = character >= '0' && character <= '9';
        const bool lower = character >= 'a' && character <= 'f';
        const bool upper = character >= 'A' && character <= 'F';
        if (!digit && !lower && !upper) {
            return false;
        }
    }
    return true;
}

struct InspectionResult {
    std::uint64_t frameId = 0;
    InspectionDecision decision = InspectionDecision::Unknown;
    std::vector<Detection> defects;
    std::uint64_t elapsedMicros = 0;
    std::string parameterVersion;
    std::string parameterSha256;
    std::string errorCode;
    std::string errorMessage;

    bool validate(std::string* reason = nullptr) const
    {
        if (frameId == 0) {
            return fail(reason, "frameId must be non-zero");
        }
        if (parameterVersion.empty()) {
            return fail(reason, "parameterVersion is required");
        }
        if (!isSha256Identity(parameterSha256)) {
            return fail(reason, "parameterSha256 must be a SHA-256 identity");
        }
        if (!isValidInspectionDecision(decision)) {
            return fail(reason, "decision is outside the supported enum domain");
        }
        if (decision == InspectionDecision::Unknown) {
            return fail(reason, "decision must be explicit");
        }
        if (decision == InspectionDecision::Ok && !defects.empty()) {
            return fail(reason, "OK results cannot contain defects");
        }
        if (decision == InspectionDecision::Ng && defects.empty()) {
            return fail(reason, "NG results require at least one defect");
        }
        if (decision == InspectionDecision::Error && errorCode.empty()) {
            return fail(reason, "error results require an errorCode");
        }
        for (const Detection& defect : defects) {
            if (!defect.isValid()) {
                return fail(reason, "result contains an invalid detection");
            }
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

enum class RejectMode {
    Simulation = 0,
    Real
};

inline bool isValidRejectMode(RejectMode mode)
{
    switch (mode) {
    case RejectMode::Simulation:
    case RejectMode::Real:
        return true;
    default:
        return false;
    }
}

struct RejectCommand {
    std::uint64_t frameId = 0;
    std::uint32_t cigaretteNumber = 0;
    std::string targetOutput;
    TimestampMicros scheduledAt = 0;
    RejectMode mode = RejectMode::Simulation;

    bool validate(std::string* reason = nullptr) const
    {
        if (frameId == 0) {
            return fail(reason, "frameId must be non-zero");
        }
        if (targetOutput.empty()) {
            return fail(reason, "targetOutput is required");
        }
        if (scheduledAt <= 0) {
            return fail(reason, "scheduledAt must be positive");
        }
        if (!isValidRejectMode(mode)) {
            return fail(reason, "mode is outside the supported enum domain");
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

enum class RejectExecutionStatus {
    Skipped = 0,
    Simulated,
    Executed,
    Failed
};

struct RejectExecutionResult {
    std::uint64_t frameId = 0;
    RejectExecutionStatus status = RejectExecutionStatus::Skipped;
    TimestampMicros completedAt = 0;
    std::string errorCode;
    std::string errorMessage;
};

} // namespace cigvision
