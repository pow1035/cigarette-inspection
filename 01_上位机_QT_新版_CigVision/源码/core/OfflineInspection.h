#pragma once

#include "BoundedQueue.h"
#include "InspectionInterfaces.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace cigvision {

class IFrameArchive {
public:
    virtual ~IFrameArchive() = default;
    virtual bool store(const FramePacket& frame, const InspectionResult& result,
        std::string& errorMessage) = 0;
};

struct InspectionStatistics {
    std::uint64_t received = 0;
    std::uint64_t processed = 0;
    std::uint64_t ok = 0;
    std::uint64_t ng = 0;
    std::uint64_t error = 0;
    std::uint64_t sourceErrors = 0;
    std::uint64_t detectorErrors = 0;
    std::uint64_t observerErrors = 0;
    std::uint64_t saveFailures = 0;
    std::uint64_t dropped = 0;
    std::map<std::int32_t, std::uint64_t> defectsByClass;

    bool isConsistent() const
    {
        return processed == ok + ng + error && processed + dropped <= received;
    }
};

class IInspectionObserver {
public:
    virtual ~IInspectionObserver() = default;
    virtual void onResult(const FramePacket& frame, const InspectionResult& result,
        const InspectionStatistics& statistics) = 0;
};

enum class OfflineRunState {
    Failed = 0,
    Completed,
    CompletedWithErrors,
    Stopped
};

struct OfflineRunOptions {
    std::size_t queueCapacity = 4;
    QueueOverflowPolicy overflowPolicy = QueueOverflowPolicy::RejectNewest;
    bool drainOnStop = true;
    std::string parameterVersion = "offline-fixture-v1";
};

struct OfflineRunSummary {
    OfflineRunState state = OfflineRunState::Failed;
    InspectionStatistics statistics;
    std::vector<std::string> issues;
};

class SystemClock final : public IClock {
public:
    TimestampMicros now() const noexcept override
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

// A deterministic chain-test detector. It is not a production inspection algorithm.
class DeterministicFixtureDetector final : public IDetector {
public:
    DetectionBatch detect(const FramePacket& frame) override
    {
        DetectionBatch batch;
        batch.frameId = frame.frameId;
        batch.detectorVersion = "deterministic-fixture-v1";
        if ((frame.frameId % 2U) == 0U) {
            Detection detection;
            detection.classId = 0;
            detection.className = "fixture-defect";
            detection.confidence = 1.0F;
            detection.box = { 0.0F, 0.0F, static_cast<float>(frame.width),
                static_cast<float>(frame.height) };
            detection.detectorVersion = batch.detectorVersion;
            batch.detections.push_back(detection);
        }
        return batch;
    }
};

inline InspectionResult decideInspection(const FramePacket& frame,
    const DetectionBatch& batch, const std::string& parameterVersion)
{
    InspectionResult result;
    result.frameId = frame.frameId;
    result.elapsedMicros = batch.elapsedMicros;
    result.parameterVersion = parameterVersion;

    std::string errorCode = batch.errorCode;
    std::string errorMessage = batch.errorMessage;
    if (batch.frameId != frame.frameId) {
        errorCode = "DETECTOR_FRAME_ID_MISMATCH";
        errorMessage = "detector result does not match the input frame";
    }

    if (errorCode.empty()) {
        for (const Detection& detection : batch.detections) {
            const float right = detection.box.x + detection.box.width;
            const float bottom = detection.box.y + detection.box.height;
            if (!detection.isValid() || !std::isfinite(right) || !std::isfinite(bottom) ||
                right > static_cast<float>(frame.width) ||
                bottom > static_cast<float>(frame.height)) {
                errorCode = "INVALID_DETECTION";
                errorMessage = "detector returned an invalid or out-of-frame box";
                break;
            }
        }
    }

    if (!errorCode.empty()) {
        result.decision = InspectionDecision::Error;
        result.errorCode = errorCode;
        result.errorMessage = errorMessage;
        return result;
    }

    result.defects = batch.detections;
    result.decision = result.defects.empty() ? InspectionDecision::Ok : InspectionDecision::Ng;
    return result;
}

class OfflineInspectionSession {
public:
    void requestStop() noexcept
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (running_.load()) {
            stopRequested_.store(true);
        }
    }
    bool isRunning() const noexcept { return running_.load(); }

    OfflineRunSummary run(IFrameSource& source, IDetector& detector,
        IInspectionResultSink& resultSink, IFrameArchive& frameArchive,
        IClock& clock, IInspectionObserver* observer,
        const OfflineRunOptions& options = OfflineRunOptions())
    {
        OfflineRunSummary summary;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            if (running_.load()) {
                summary.issues.push_back("offline session is already running");
                return summary;
            }
            stopRequested_.store(false);
            running_.store(true);
        }
        RunningGuard runningGuard(stateMutex_, running_);

        if (options.queueCapacity == 0 || options.parameterVersion.empty()) {
            summary.issues.push_back("invalid offline run options");
            return summary;
        }

        std::string sourceError;
        bool sourceStarted = false;
        try {
            sourceStarted = source.start(sourceError);
        }
        catch (const std::exception& error) {
            sourceError = std::string("source start exception: ") + error.what();
        }
        catch (...) {
            sourceError = "source start exception: unknown exception";
        }
        if (!sourceStarted) {
            ++summary.statistics.sourceErrors;
            summary.issues.push_back("source start failed: " + sourceError);
            return summary;
        }
        SourceGuard sourceGuard(source);

        BoundedQueue<FramePacket> queue(options.queueCapacity, options.overflowPolicy);
        std::mutex summaryMutex;

        std::thread consumer([&] {
            FramePacket frame;
            while (queue.waitPop(frame)) {
                if (stopRequested_.load() && !options.drainOnStop) {
                    std::lock_guard<std::mutex> lock(summaryMutex);
                    ++summary.statistics.dropped;
                    FramePacket discarded;
                    while (queue.tryPop(discarded)) {
                        ++summary.statistics.dropped;
                    }
                    break;
                }

                DetectionBatch batch;
                try {
                    batch = detector.detect(frame);
                }
                catch (const std::exception& error) {
                    batch.frameId = frame.frameId;
                    batch.errorCode = "DETECTOR_EXCEPTION";
                    batch.errorMessage = error.what();
                }
                catch (...) {
                    batch.frameId = frame.frameId;
                    batch.errorCode = "DETECTOR_EXCEPTION";
                    batch.errorMessage = "unknown detector exception";
                }

                InspectionResult result = decideInspection(frame, batch, options.parameterVersion);
                std::string validationError;
                if (!result.validate(&validationError)) {
                    result = InspectionResult();
                    result.frameId = frame.frameId;
                    result.decision = InspectionDecision::Error;
                    result.parameterVersion = options.parameterVersion;
                    result.errorCode = "INVALID_INSPECTION_RESULT";
                    result.errorMessage = validationError;
                }

                std::string resultError;
                std::string archiveError;
                bool resultStored = false;
                bool frameStored = false;
                try {
                    resultStored = resultSink.store(result, resultError);
                }
                catch (const std::exception& error) {
                    resultError = std::string("result sink exception: ") + error.what();
                }
                catch (...) {
                    resultError = "result sink exception: unknown exception";
                }
                try {
                    frameStored = frameArchive.store(frame, result, archiveError);
                }
                catch (const std::exception& error) {
                    archiveError = std::string("frame archive exception: ") + error.what();
                }
                catch (...) {
                    archiveError = "frame archive exception: unknown exception";
                }

                InspectionStatistics snapshot;
                {
                    std::lock_guard<std::mutex> lock(summaryMutex);
                    ++summary.statistics.processed;
                    if (result.decision == InspectionDecision::Ok) {
                        ++summary.statistics.ok;
                    } else if (result.decision == InspectionDecision::Ng) {
                        ++summary.statistics.ng;
                        for (const Detection& defect : result.defects) {
                            ++summary.statistics.defectsByClass[defect.classId];
                        }
                    } else {
                        ++summary.statistics.error;
                        ++summary.statistics.detectorErrors;
                    }
                    if (!resultStored || !frameStored) {
                        ++summary.statistics.saveFailures;
                        summary.issues.push_back("frame " + std::to_string(frame.frameId) +
                            " save failed: " + resultError + archiveError);
                    }
                    snapshot = summary.statistics;
                }
                if (observer != nullptr) {
                    try {
                        observer->onResult(frame, result, snapshot);
                    }
                    catch (const std::exception& error) {
                        std::lock_guard<std::mutex> lock(summaryMutex);
                        ++summary.statistics.observerErrors;
                        summary.issues.push_back("observer exception: " + std::string(error.what()));
                    }
                    catch (...) {
                        std::lock_guard<std::mutex> lock(summaryMutex);
                        ++summary.statistics.observerErrors;
                        summary.issues.push_back("observer exception: unknown exception");
                    }
                }
                (void)clock.now();
            }
        });

        std::unordered_set<std::uint64_t> frameIds;
        std::size_t consecutiveSourceErrors = 0;
        while (!stopRequested_.load()) {
            FramePacket frame;
            sourceError.clear();
            bool read = false;
            bool sourceException = false;
            try {
                read = source.tryRead(frame, sourceError);
            }
            catch (const std::exception& error) {
                sourceException = true;
                sourceError = std::string("source exception: ") + error.what();
            }
            catch (...) {
                sourceException = true;
                sourceError = "source exception: unknown exception";
            }
            if (!read) {
                if (sourceError.empty()) {
                    break;
                }
                std::lock_guard<std::mutex> lock(summaryMutex);
                ++summary.statistics.sourceErrors;
                summary.issues.push_back("source read failed: " + sourceError);
                if (sourceException || ++consecutiveSourceErrors >= 1024) {
                    summary.issues.push_back("source error limit reached");
                    break;
                }
                continue;
            }
            consecutiveSourceErrors = 0;

            std::string validationError;
            if (!frame.validate(&validationError) || !frameIds.insert(frame.frameId).second) {
                std::lock_guard<std::mutex> lock(summaryMutex);
                ++summary.statistics.sourceErrors;
                summary.issues.push_back("invalid or duplicate frame: " + validationError);
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(summaryMutex);
                ++summary.statistics.received;
            }
            QueuePushResult pushed = QueuePushResult::RejectedFull;
            do {
                pushed = queue.tryPush(std::move(frame));
                if (pushed == QueuePushResult::RejectedFull && !stopRequested_.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } while (pushed == QueuePushResult::RejectedFull && !stopRequested_.load());
            if (pushed == QueuePushResult::RejectedFull ||
                pushed == QueuePushResult::DroppedOldest) {
                std::lock_guard<std::mutex> lock(summaryMutex);
                ++summary.statistics.dropped;
            } else if (pushed == QueuePushResult::RejectedClosed) {
                break;
            }
        }

        queue.close();
        consumer.join();

        if (stopRequested_.load()) {
            summary.state = OfflineRunState::Stopped;
        } else if (!summary.statistics.isConsistent()) {
            summary.state = OfflineRunState::Failed;
            summary.issues.push_back("inspection statistics invariant failed");
        } else if (summary.statistics.sourceErrors != 0 ||
            summary.statistics.detectorErrors != 0 ||
            summary.statistics.observerErrors != 0 || summary.statistics.saveFailures != 0 ||
            summary.statistics.dropped != 0) {
            summary.state = OfflineRunState::CompletedWithErrors;
        } else {
            summary.state = OfflineRunState::Completed;
        }
        return summary;
    }

private:
    struct RunningGuard {
        RunningGuard(std::mutex& mutex, std::atomic<bool>& running)
            : mutex_(mutex), running_(running) {}
        ~RunningGuard()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_.store(false);
        }
        std::mutex& mutex_;
        std::atomic<bool>& running_;
    };
    struct SourceGuard {
        explicit SourceGuard(IFrameSource& source) : source_(source) {}
        ~SourceGuard() { source_.stop(); }
        IFrameSource& source_;
    };

    std::atomic<bool> stopRequested_{ false };
    std::atomic<bool> running_{ false };
    mutable std::mutex stateMutex_;
};

} // namespace cigvision
