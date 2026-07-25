#pragma once

#include "InspectionContracts.h"
#include "ProductParameterProfile.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cigvision {

// SDK-free presentation state for the P7 product workflow. It deliberately
// contains no camera, TensorRT, Qt or hardware-output dependency.
enum class ProductRunMode {
    OfflineFixture = 0,
    Simulation,
    TensorRtOffline
};

enum class ProductRuntimeStatus {
    Idle = 0,
    Running,
    Stopping,
    Faulted
};

enum class ProductReviewOutcome {
    Unreviewed = 0,
    Confirmed,
    Corrected,
    Dismissed
};

enum class ProductDiagnosticSeverity {
    Information = 0,
    Warning,
    Error
};

inline bool isValidProductReviewOutcome(ProductReviewOutcome outcome)
{
    switch (outcome) {
    case ProductReviewOutcome::Unreviewed:
    case ProductReviewOutcome::Confirmed:
    case ProductReviewOutcome::Corrected:
    case ProductReviewOutcome::Dismissed:
        return true;
    default:
        return false;
    }
}

inline bool isValidProductDiagnosticSeverity(ProductDiagnosticSeverity severity)
{
    switch (severity) {
    case ProductDiagnosticSeverity::Information:
    case ProductDiagnosticSeverity::Warning:
    case ProductDiagnosticSeverity::Error:
        return true;
    default:
        return false;
    }
}

inline const char* productRunModeName(ProductRunMode mode)
{
    switch (mode) {
    case ProductRunMode::OfflineFixture: return "OFFLINE_FIXTURE";
    case ProductRunMode::Simulation: return "SIMULATION";
    case ProductRunMode::TensorRtOffline: return "TENSORRT_OFFLINE";
    default: return "UNKNOWN";
    }
}

inline const char* productRuntimeStatusName(ProductRuntimeStatus status)
{
    switch (status) {
    case ProductRuntimeStatus::Idle: return "IDLE";
    case ProductRuntimeStatus::Running: return "RUNNING";
    case ProductRuntimeStatus::Stopping: return "STOPPING";
    case ProductRuntimeStatus::Faulted: return "FAULTED";
    default: return "UNKNOWN";
    }
}

struct ProductRunConfiguration {
    std::string runId;
    std::string brandName;
    ProductRunMode mode = ProductRunMode::OfflineFixture;
    std::string detectorVersion;
    std::string parameterVersion;
    std::string modelSha256;
    std::size_t queueCapacity = 4;
    std::size_t recentResultCapacity = 100;
    std::size_t diagnosticCapacity = 200;
    std::size_t duplicateWindowCapacity = 4096;
    bool realIoEnabled = false;
    ProductParameterProfile configuredParameters;
    ProductParameterProfile appliedParameters;
    bool configuredParametersApplied = false;

    bool validate(std::string* reason = nullptr) const
    {
        if (runId.empty() || brandName.empty() || detectorVersion.empty() ||
            parameterVersion.empty()) {
            return fail(reason, "run, brand, detector and parameter identity are required");
        }
        switch (mode) {
        case ProductRunMode::OfflineFixture:
        case ProductRunMode::Simulation:
            if (!modelSha256.empty() && !isSha256(modelSha256)) {
                return fail(reason, "optional model SHA-256 is malformed");
            }
            break;
        case ProductRunMode::TensorRtOffline:
            if (!isSha256(modelSha256)) {
                return fail(reason, "TensorRT offline mode requires a model SHA-256");
            }
            break;
        default:
            return fail(reason, "run mode is outside the supported local domain");
        }
        if (realIoEnabled) {
            return fail(reason, "real IO is not available in the local product workflow");
        }
        std::string profileError;
        if (!configuredParameters.validate(&profileError) ||
            !appliedParameters.validate(&profileError)) {
            return fail(reason, "configured and applied parameter profiles must be valid");
        }
        if (appliedParameters.parameterVersion != parameterVersion ||
            appliedParameters.detectorVersion != detectorVersion ||
            appliedParameters.modelSha256 != modelSha256) {
            return fail(reason,
                "applied parameter profile does not match run detector identity");
        }
        if (configuredParametersApplied &&
            configuredParameters.sha256() != appliedParameters.sha256()) {
            return fail(reason,
                "configured parameters marked applied must match the applied profile");
        }
        if ((mode == ProductRunMode::OfflineFixture ||
                mode == ProductRunMode::Simulation) &&
            (appliedParameters.kind !=
                ProductParameterProfileKind::DeterministicFixture ||
                configuredParametersApplied)) {
            return fail(reason,
                "fixture and simulation runs must identify unapplied UI parameters");
        }
        if (mode == ProductRunMode::TensorRtOffline &&
            (appliedParameters.kind != ProductParameterProfileKind::TensorRtOffline ||
                !configuredParametersApplied)) {
            return fail(reason,
                "TensorRT run must apply its typed parameter profile");
        }
        if (queueCapacity == 0 || queueCapacity > 65536 ||
            recentResultCapacity == 0 || recentResultCapacity > 65536 ||
            diagnosticCapacity == 0 || diagnosticCapacity > 65536 ||
            duplicateWindowCapacity == 0 || duplicateWindowCapacity > 1048576) {
            return fail(reason, "runtime capacities are outside their safe bounds");
        }
        if (reason != nullptr) {
            reason->clear();
        }
        return true;
    }

private:
    static bool isSha256(const std::string& value)
    {
        if (value.size() != 64) {
            return false;
        }
        for (char character : value) {
            const unsigned char byte = static_cast<unsigned char>(character);
            if (std::isxdigit(byte) == 0) {
                return false;
            }
        }
        return true;
    }

    static bool fail(std::string* reason, const char* message)
    {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    }
};

struct ProductDefectSummary {
    std::int32_t classId = -1;
    std::string className;
    float confidence = 0.0F;

    bool validate() const
    {
        return classId >= 0 && !className.empty() && confidence >= 0.0F &&
            confidence <= 1.0F;
    }
};

struct ProductFrameResult {
    std::uint64_t frameId = 0;
    std::string stationId;
    std::string cameraId;
    std::uint32_t cigaretteNumber = 0;
    TimestampMicros capturedAtMicros = 0;
    TimestampMicros completedAtMicros = 0;
    InspectionDecision decision = InspectionDecision::Unknown;
    std::uint64_t elapsedMicros = 0;
    std::string parameterVersion;
    std::string parameterSha256;
    std::vector<ProductDefectSummary> defects;
    std::string errorCode;
    std::string errorMessage;

    bool validate(std::string* reason = nullptr) const
    {
        if (frameId == 0 || stationId.empty() || cameraId.empty() ||
            cigaretteNumber == 0) {
            return fail(reason, "frame identity is incomplete");
        }
        if (capturedAtMicros <= 0 || completedAtMicros < capturedAtMicros) {
            return fail(reason, "frame timestamps are invalid");
        }
        if (parameterVersion.empty() || !isSha256Hex(parameterSha256)) {
            return fail(reason, "parameter version and SHA-256 are required");
        }
        if (decision == InspectionDecision::Unknown ||
            !isValidInspectionDecision(decision)) {
            return fail(reason, "frame decision must be explicit");
        }
        if (decision == InspectionDecision::Ok && !defects.empty()) {
            return fail(reason, "OK frame cannot contain defects");
        }
        if (decision == InspectionDecision::Ng && defects.empty()) {
            return fail(reason, "NG frame requires at least one defect");
        }
        if (decision == InspectionDecision::Error && errorCode.empty()) {
            return fail(reason, "error frame requires an error code");
        }
        for (const ProductDefectSummary& defect : defects) {
            if (!defect.validate()) {
                return fail(reason, "frame contains an invalid defect summary");
            }
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

struct ProductCameraStatistics {
    std::uint64_t processed = 0;
    std::uint64_t ok = 0;
    std::uint64_t ng = 0;
    std::uint64_t error = 0;
};

struct ProductCameraKey {
    std::string stationId;
    std::string cameraId;

    bool operator<(const ProductCameraKey& other) const
    {
        return stationId < other.stationId ||
            (stationId == other.stationId && cameraId < other.cameraId);
    }
};

struct ProductRuntimeStatistics {
    std::uint64_t processed = 0;
    std::uint64_t ok = 0;
    std::uint64_t ng = 0;
    std::uint64_t error = 0;
    std::uint64_t reviewed = 0;
    std::uint64_t confirmed = 0;
    std::uint64_t corrected = 0;
    std::uint64_t dismissed = 0;
    std::uint64_t totalElapsedMicros = 0;
    std::uint64_t maximumElapsedMicros = 0;
    std::size_t maximumQueueDepth = 0;
    std::map<std::int32_t, std::uint64_t> defectsByClass;
    std::map<ProductCameraKey, ProductCameraStatistics> cameras;

    bool isConsistent() const
    {
        if (processed != ok + ng + error ||
            reviewed != confirmed + corrected + dismissed ||
            reviewed > processed) {
            return false;
        }
        std::uint64_t cameraProcessed = 0;
        std::uint64_t cameraOk = 0;
        std::uint64_t cameraNg = 0;
        std::uint64_t cameraError = 0;
        for (const std::map<ProductCameraKey, ProductCameraStatistics>::value_type& entry :
                cameras) {
            cameraProcessed += entry.second.processed;
            cameraOk += entry.second.ok;
            cameraNg += entry.second.ng;
            cameraError += entry.second.error;
        }
        return cameraProcessed == processed && cameraOk == ok &&
            cameraNg == ng && cameraError == error;
    }
};

struct ProductReviewRecord {
    ProductReviewOutcome outcome = ProductReviewOutcome::Unreviewed;
    std::string reviewer;
    std::string note;
    TimestampMicros reviewedAtMicros = 0;
};

struct ProductRecentResult {
    ProductFrameResult frame;
    ProductReviewRecord review;
};

struct ProductDiagnosticEvent {
    TimestampMicros occurredAtMicros = 0;
    ProductDiagnosticSeverity severity = ProductDiagnosticSeverity::Information;
    std::string component;
    std::string code;
    std::string message;
    std::uint64_t frameId = 0;

    bool validate() const
    {
        return occurredAtMicros > 0 && !component.empty() &&
            !code.empty() && !message.empty() &&
            isValidProductDiagnosticSeverity(severity);
    }
};

struct ProductRuntimeSnapshot {
    std::uint64_t revision = 0;
    ProductRuntimeStatus status = ProductRuntimeStatus::Idle;
    ProductRunConfiguration configuration;
    TimestampMicros startedAtMicros = 0;
    TimestampMicros stoppedAtMicros = 0;
    ProductRuntimeStatistics statistics;
    std::vector<ProductRecentResult> recentResults;
    std::vector<ProductDiagnosticEvent> diagnostics;
};

class ProductRuntimeState final {
public:
    bool start(const ProductRunConfiguration& configuration,
        TimestampMicros startedAtMicros, std::string& errorMessage)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string validationError;
        if (!configuration.validate(&validationError)) {
            errorMessage = validationError;
            return false;
        }
        if (startedAtMicros <= 0) {
            errorMessage = "run start time must be positive";
            return false;
        }
        if (status_ == ProductRuntimeStatus::Running ||
            status_ == ProductRuntimeStatus::Stopping) {
            errorMessage = "a product run is already active";
            return false;
        }

        configuration_ = configuration;
        status_ = ProductRuntimeStatus::Running;
        startedAtMicros_ = startedAtMicros;
        stoppedAtMicros_ = 0;
        statistics_ = ProductRuntimeStatistics();
        recentResults_.clear();
        diagnostics_.clear();
        duplicateWindow_.clear();
        duplicateIds_.clear();
        ++revision_;
        errorMessage.clear();
        return true;
    }

    bool recordResult(const ProductFrameResult& frame, std::size_t queueDepth,
        std::string& errorMessage)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string validationError;
        if (status_ != ProductRuntimeStatus::Running) {
            errorMessage = "product run is not accepting results";
            return false;
        }
        if (!frame.validate(&validationError)) {
            errorMessage = validationError;
            return false;
        }
        if (frame.parameterVersion != configuration_.parameterVersion) {
            errorMessage = "frame parameter version does not match the active run";
            return false;
        }
        if (frame.parameterSha256 != configuration_.appliedParameters.sha256()) {
            errorMessage = "frame parameter SHA-256 does not match the active run";
            return false;
        }
        if (queueDepth > configuration_.queueCapacity) {
            errorMessage = "reported queue depth exceeds the active capacity";
            return false;
        }
        if (duplicateIds_.find(frame.frameId) != duplicateIds_.end()) {
            errorMessage = "duplicate frame id inside the configured review window";
            return false;
        }
        if (statistics_.totalElapsedMicros >
            (std::numeric_limits<std::uint64_t>::max)() - frame.elapsedMicros) {
            errorMessage = "elapsed-time total would overflow";
            return false;
        }

        ++statistics_.processed;
        ProductCameraStatistics& camera = statistics_.cameras[cameraKey(frame)];
        ++camera.processed;
        switch (frame.decision) {
        case InspectionDecision::Ok:
            ++statistics_.ok;
            ++camera.ok;
            break;
        case InspectionDecision::Ng:
            ++statistics_.ng;
            ++camera.ng;
            for (const ProductDefectSummary& defect : frame.defects) {
                ++statistics_.defectsByClass[defect.classId];
            }
            break;
        case InspectionDecision::Error:
            ++statistics_.error;
            ++camera.error;
            break;
        default:
            errorMessage = "unsupported decision";
            return false;
        }
        statistics_.totalElapsedMicros += frame.elapsedMicros;
        statistics_.maximumElapsedMicros =
            (std::max)(statistics_.maximumElapsedMicros, frame.elapsedMicros);
        statistics_.maximumQueueDepth =
            (std::max)(statistics_.maximumQueueDepth, queueDepth);

        ProductRecentResult recent;
        recent.frame = frame;
        recentResults_.push_back(std::move(recent));
        while (recentResults_.size() > configuration_.recentResultCapacity) {
            recentResults_.pop_front();
        }

        duplicateWindow_.push_back(frame.frameId);
        duplicateIds_.insert(frame.frameId);
        while (duplicateWindow_.size() > configuration_.duplicateWindowCapacity) {
            duplicateIds_.erase(duplicateWindow_.front());
            duplicateWindow_.pop_front();
        }
        ++revision_;
        errorMessage.clear();
        return true;
    }

    bool reviewResult(std::uint64_t frameId, ProductReviewOutcome outcome,
        const std::string& reviewer, const std::string& note,
        TimestampMicros reviewedAtMicros, std::string& errorMessage)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!isValidProductReviewOutcome(outcome) ||
            outcome == ProductReviewOutcome::Unreviewed || reviewer.empty()) {
            errorMessage = "review outcome and reviewer are required";
            return false;
        }
        const auto found = std::find_if(recentResults_.begin(), recentResults_.end(),
            [frameId](const ProductRecentResult& value) {
                return value.frame.frameId == frameId;
            });
        if (found == recentResults_.end()) {
            errorMessage = "frame is not retained in the recent-result window";
            return false;
        }
        if (reviewedAtMicros < found->frame.completedAtMicros) {
            errorMessage = "review time precedes frame completion";
            return false;
        }

        if (found->review.outcome != ProductReviewOutcome::Unreviewed) {
            decrementReview(found->review.outcome);
        } else {
            ++statistics_.reviewed;
        }
        incrementReview(outcome);
        found->review.outcome = outcome;
        found->review.reviewer = reviewer;
        found->review.note = note;
        found->review.reviewedAtMicros = reviewedAtMicros;
        ++revision_;
        errorMessage.clear();
        return true;
    }

    bool appendDiagnostic(const ProductDiagnosticEvent& event,
        std::string& errorMessage)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (configuration_.runId.empty()) {
            errorMessage = "no product run has been started";
            return false;
        }
        if (!event.validate()) {
            errorMessage = "diagnostic event is invalid";
            return false;
        }
        diagnostics_.push_back(event);
        while (diagnostics_.size() > configuration_.diagnosticCapacity) {
            diagnostics_.pop_front();
        }
        ++revision_;
        errorMessage.clear();
        return true;
    }

    bool requestStop(TimestampMicros requestedAtMicros, std::string& errorMessage)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != ProductRuntimeStatus::Running) {
            errorMessage = "only a running product workflow can be stopped";
            return false;
        }
        if (requestedAtMicros < startedAtMicros_) {
            errorMessage = "stop request precedes run start";
            return false;
        }
        status_ = ProductRuntimeStatus::Stopping;
        ++revision_;
        errorMessage.clear();
        return true;
    }

    bool completeStop(TimestampMicros stoppedAtMicros, std::string& errorMessage)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != ProductRuntimeStatus::Running &&
            status_ != ProductRuntimeStatus::Stopping) {
            errorMessage = "no active product workflow can be completed";
            return false;
        }
        if (stoppedAtMicros < startedAtMicros_) {
            errorMessage = "run completion precedes run start";
            return false;
        }
        status_ = ProductRuntimeStatus::Idle;
        stoppedAtMicros_ = stoppedAtMicros;
        ++revision_;
        errorMessage.clear();
        return true;
    }

    bool fail(const std::string& code, const std::string& message,
        TimestampMicros failedAtMicros, std::string& errorMessage)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != ProductRuntimeStatus::Running &&
            status_ != ProductRuntimeStatus::Stopping) {
            errorMessage = "no active product workflow can be faulted";
            return false;
        }
        if (code.empty() || message.empty() || failedAtMicros < startedAtMicros_) {
            errorMessage = "fault identity and timestamp are invalid";
            return false;
        }
        ProductDiagnosticEvent event;
        event.occurredAtMicros = failedAtMicros;
        event.severity = ProductDiagnosticSeverity::Error;
        event.component = "runtime";
        event.code = code;
        event.message = message;
        diagnostics_.push_back(event);
        while (diagnostics_.size() > configuration_.diagnosticCapacity) {
            diagnostics_.pop_front();
        }
        status_ = ProductRuntimeStatus::Faulted;
        stoppedAtMicros_ = failedAtMicros;
        ++revision_;
        errorMessage.clear();
        return true;
    }

    ProductRuntimeSnapshot snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ProductRuntimeSnapshot result;
        result.revision = revision_;
        result.status = status_;
        result.configuration = configuration_;
        result.startedAtMicros = startedAtMicros_;
        result.stoppedAtMicros = stoppedAtMicros_;
        result.statistics = statistics_;
        result.recentResults.assign(recentResults_.begin(), recentResults_.end());
        result.diagnostics.assign(diagnostics_.begin(), diagnostics_.end());
        return result;
    }

private:
    static ProductCameraKey cameraKey(const ProductFrameResult& frame)
    {
        ProductCameraKey result;
        result.stationId = frame.stationId;
        result.cameraId = frame.cameraId;
        return result;
    }

    void incrementReview(ProductReviewOutcome outcome)
    {
        switch (outcome) {
        case ProductReviewOutcome::Confirmed: ++statistics_.confirmed; break;
        case ProductReviewOutcome::Corrected: ++statistics_.corrected; break;
        case ProductReviewOutcome::Dismissed: ++statistics_.dismissed; break;
        default: break;
        }
    }

    void decrementReview(ProductReviewOutcome outcome)
    {
        switch (outcome) {
        case ProductReviewOutcome::Confirmed: --statistics_.confirmed; break;
        case ProductReviewOutcome::Corrected: --statistics_.corrected; break;
        case ProductReviewOutcome::Dismissed: --statistics_.dismissed; break;
        default: break;
        }
    }

    mutable std::mutex mutex_;
    std::uint64_t revision_ = 0;
    ProductRuntimeStatus status_ = ProductRuntimeStatus::Idle;
    ProductRunConfiguration configuration_;
    TimestampMicros startedAtMicros_ = 0;
    TimestampMicros stoppedAtMicros_ = 0;
    ProductRuntimeStatistics statistics_;
    std::deque<ProductRecentResult> recentResults_;
    std::deque<ProductDiagnosticEvent> diagnostics_;
    std::deque<std::uint64_t> duplicateWindow_;
    std::unordered_set<std::uint64_t> duplicateIds_;
};

} // namespace cigvision
