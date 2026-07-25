#pragma once

#include "BoundedQueue.h"
#include "InspectionContracts.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cigvision {

// Deterministic SDK-free model for P6-02. It models several recorded camera
// streams feeding one inspection worker. It is not a claim about MVS callback,
// GPU, PLC or production-machine timing.
enum class RealtimeFrameDisposition {
    Queued = 0,
    Processed,
    DroppedQueueFull,
    DroppedOldest,
    DuplicateFrameId,
    Invalid,
    ProcessingFailed,
    AfterStop,
    CancelledOnStop
};

enum class RealtimeSimulationRunState {
    Failed = 0,
    Completed,
    CompletedWithErrors,
    Stopped
};

struct RealtimeInputFrame {
    FramePacket frame;
    // Absolute virtual arrival time. Input order is retained so deliberately
    // shuffled recordings remain observable.
    TimestampMicros arrivalAt = 0;
    // -1 uses defaultProcessingMicros; zero is an instantaneous duration.
    TimestampMicros processingMicros = -1;
    bool ngCandidate = false;
};

struct RealtimeSimulationOptions {
    std::size_t queueCapacity = 4;
    QueueOverflowPolicy overflowPolicy = QueueOverflowPolicy::RejectNewest;
    TimestampMicros initialClockMicros = 1;
    TimestampMicros defaultProcessingMicros = 100;
    TimestampMicros rejectDelayMicros = 0;
    std::string targetOutput = "simulation-reject";
    // Zero disables stopping. A frame exactly on the boundary is not admitted.
    TimestampMicros stopAtMicros = 0;
    bool drainOnStop = true;
};

struct RealtimeFrameTrace {
    std::uint64_t frameId = 0;
    std::string stationId;
    std::string cameraId;
    std::uint32_t cigaretteNumber = 0;
    TimestampMicros capturedAt = 0;
    TimestampMicros arrivalAt = 0;
    TimestampMicros admittedAt = 0;
    TimestampMicros startedAt = 0;
    TimestampMicros completedAt = 0;
    TimestampMicros queueWaitMicros = 0;
    TimestampMicros endToEndMicros = 0;
    std::size_t queueDepthAtArrival = 0;
    bool ngCandidate = false;
    InspectionDecision decision = InspectionDecision::Unknown;
    RealtimeFrameDisposition disposition = RealtimeFrameDisposition::Queued;
    std::vector<std::string> sequenceFlags;
    RejectCommand command;
    RejectExecutionResult execution;
    std::string reasonCode;
    std::string reasonMessage;
};

struct RealtimeCameraStatistics {
    // offered here means valid, unique frames admitted to the queue boundary.
    std::uint64_t offered = 0;
    std::uint64_t accepted = 0;
    std::uint64_t processed = 0;
    std::uint64_t dropped = 0;
    std::uint64_t duplicateCigarettes = 0;
    std::uint64_t cigaretteGaps = 0;
    std::uint64_t cigaretteOutOfOrder = 0;
};

struct RealtimeSimulationStatistics {
    std::uint64_t offered = 0;
    std::uint64_t accepted = 0;
    std::uint64_t processed = 0;
    std::uint64_t dropped = 0;
    std::uint64_t duplicateFrameIds = 0;
    std::uint64_t invalid = 0;
    std::uint64_t processingFailures = 0;
    std::uint64_t afterStop = 0;
    std::uint64_t cancelled = 0;
    std::uint64_t sourceErrors = 0;
    std::uint64_t arrivalOutOfOrder = 0;
    // Gaps are observed forward jumps in replay order, not a proof that a
    // later out-of-order frame can never fill the number.
    std::uint64_t frameIdGaps = 0;
    std::uint64_t frameIdOutOfOrder = 0;
    std::uint64_t duplicateCigarettes = 0;
    std::uint64_t cigaretteGaps = 0;
    std::uint64_t cigaretteOutOfOrder = 0;
    std::uint64_t maxQueueDepth = 0;
    std::uint64_t maxPipelineDepth = 0;
    std::uint64_t ngCandidates = 0;
    std::uint64_t commands = 0;
    std::uint64_t simulatedRejects = 0;
    std::uint64_t rejectFailures = 0;
    TimestampMicros maxQueueWaitMicros = 0;
    TimestampMicros p95QueueWaitMicros = 0;
    TimestampMicros maxEndToEndMicros = 0;
    TimestampMicros p95EndToEndMicros = 0;
    std::map<std::string, RealtimeCameraStatistics> cameras;

    bool isConsistent(std::size_t traceCount) const
    {
        if (traceCount != offered ||
            offered != accepted + duplicateFrameIds + invalid + afterStop ||
            accepted != processed + dropped + processingFailures + cancelled ||
            sourceErrors != duplicateFrameIds + invalid ||
            ngCandidates != simulatedRejects + rejectFailures ||
            commands != simulatedRejects) {
            return false;
        }

        std::uint64_t cameraOffered = 0;
        std::uint64_t cameraAccepted = 0;
        std::uint64_t cameraProcessed = 0;
        std::uint64_t cameraDropped = 0;
        for (const std::map<std::string, RealtimeCameraStatistics>::value_type& item : cameras) {
            cameraOffered += item.second.offered;
            cameraAccepted += item.second.accepted;
            cameraProcessed += item.second.processed;
            cameraDropped += item.second.dropped;
        }
        return cameraOffered == accepted && cameraAccepted == accepted &&
            cameraProcessed == processed &&
            cameraDropped == dropped;
    }
};

struct RealtimeSimulationSummary {
    RealtimeSimulationRunState state = RealtimeSimulationRunState::Failed;
    RealtimeSimulationStatistics statistics;
    std::vector<RealtimeFrameTrace> traces;
    std::vector<std::string> issues;
};

inline const char* realtimeFrameDispositionName(RealtimeFrameDisposition disposition)
{
    switch (disposition) {
    case RealtimeFrameDisposition::Queued: return "QUEUED";
    case RealtimeFrameDisposition::Processed: return "PROCESSED";
    case RealtimeFrameDisposition::DroppedQueueFull: return "DROPPED_QUEUE_FULL";
    case RealtimeFrameDisposition::DroppedOldest: return "DROPPED_OLDEST";
    case RealtimeFrameDisposition::DuplicateFrameId: return "DUPLICATE_FRAME_ID";
    case RealtimeFrameDisposition::Invalid: return "INVALID";
    case RealtimeFrameDisposition::ProcessingFailed: return "PROCESSING_FAILED";
    case RealtimeFrameDisposition::AfterStop: return "AFTER_STOP";
    case RealtimeFrameDisposition::CancelledOnStop: return "CANCELLED_ON_STOP";
    default: return "UNKNOWN";
    }
}

inline const char* realtimeSimulationRunStateName(RealtimeSimulationRunState state)
{
    switch (state) {
    case RealtimeSimulationRunState::Failed: return "FAILED";
    case RealtimeSimulationRunState::Completed: return "COMPLETED";
    case RealtimeSimulationRunState::CompletedWithErrors: return "COMPLETED_WITH_ERRORS";
    case RealtimeSimulationRunState::Stopped: return "STOPPED";
    default: return "UNKNOWN";
    }
}

class RealtimeLoadSimulator final {
private:
    struct Pending {
        std::size_t traceIndex = 0;
        TimestampMicros processingMicros = 0;
        TimestampMicros startedAt = 0;
        TimestampMicros completesAt = 0;
        bool ngCandidate = false;
    };

    struct CameraSequence {
        bool hasLast = false;
        std::uint32_t lastCigarette = 0;
    };

public:
    RealtimeSimulationSummary run(const std::vector<RealtimeInputFrame>& input,
        const RealtimeSimulationOptions& options = RealtimeSimulationOptions()) const
    {
        RealtimeSimulationSummary summary;
        std::string optionError;
        if (!validateOptions(options, optionError)) {
            summary.issues.push_back(optionError);
            return summary;
        }

        std::deque<Pending> queue;
        Pending active;
        bool hasActive = false;
        bool stopped = false;
        TimestampMicros virtualNow = options.initialClockMicros;
        TimestampMicros previousArrival = 0;
        bool hasPreviousArrival = false;
        std::uint64_t previousFrameId = 0;
        bool hasPreviousFrameId = false;
        std::unordered_set<std::uint64_t> frameIds;
        std::map<std::string, CameraSequence> cameraSequences;
        std::vector<TimestampMicros> queueWaits;
        std::vector<TimestampMicros> endToEndLatencies;

        const auto cameraKey = [](const FramePacket& frame) {
            return frame.stationId + "/" + frame.cameraId;
        };
        const auto appendTrace = [&summary](const RealtimeInputFrame& event,
            TimestampMicros admittedAt) {
            RealtimeFrameTrace trace;
            trace.frameId = event.frame.frameId;
            trace.stationId = event.frame.stationId;
            trace.cameraId = event.frame.cameraId;
            trace.cigaretteNumber = event.frame.cigaretteNumber;
            trace.capturedAt = event.frame.capturedAt;
            trace.arrivalAt = event.arrivalAt;
            trace.admittedAt = admittedAt;
            trace.ngCandidate = event.ngCandidate;
            trace.execution.frameId = event.frame.frameId;
            trace.execution.status = RejectExecutionStatus::Skipped;
            trace.execution.completedAt = admittedAt > 0 ? admittedAt : 1;
            summary.traces.push_back(std::move(trace));
            return summary.traces.size() - 1U;
        };
        const auto updateDepth = [&summary, &queue, &hasActive]() {
            summary.statistics.maxQueueDepth = std::max(
                summary.statistics.maxQueueDepth,
                static_cast<std::uint64_t>(queue.size()));
            const std::uint64_t pipelineDepth = static_cast<std::uint64_t>(
                queue.size() + (hasActive ? 1U : 0U));
            summary.statistics.maxPipelineDepth = std::max(
                summary.statistics.maxPipelineDepth, pipelineDepth);
        };
        const auto markDrop = [&summary](RealtimeFrameTrace& trace,
            RealtimeFrameDisposition disposition, const char* code,
            const char* message) {
            trace.disposition = disposition;
            trace.reasonCode = code;
            trace.reasonMessage = message;
            trace.execution.status = RejectExecutionStatus::Skipped;
            trace.execution.completedAt = trace.admittedAt > 0 ? trace.admittedAt : 1;
            ++summary.statistics.dropped;
            ++summary.statistics.cameras[trace.stationId + "/" + trace.cameraId].dropped;
        };
        const auto markProcessingFailure = [&summary](
            RealtimeFrameTrace& trace, TimestampMicros at) {
            trace.disposition = RealtimeFrameDisposition::ProcessingFailed;
            trace.reasonCode = "PROCESSING_TIME_OVERFLOW";
            trace.reasonMessage = "processing duration exceeds virtual clock range";
            trace.execution.frameId = trace.frameId;
            trace.execution.status = RejectExecutionStatus::Failed;
            trace.execution.completedAt = at > 0 ? at : 1;
            trace.execution.errorCode = trace.reasonCode;
            trace.execution.errorMessage = trace.reasonMessage;
            ++summary.statistics.processingFailures;
        };
        const auto markCancelled = [&summary](RealtimeFrameTrace& trace,
            const char* code, const char* message, TimestampMicros at) {
            trace.disposition = RealtimeFrameDisposition::CancelledOnStop;
            trace.reasonCode = code;
            trace.reasonMessage = message;
            trace.execution.frameId = trace.frameId;
            trace.execution.status = RejectExecutionStatus::Skipped;
            trace.execution.completedAt = at > 0 ? at : 1;
            ++summary.statistics.cancelled;
        };

        const auto startPending = [&summary, &markProcessingFailure](Pending& pending,
            TimestampMicros startAt) {
            RealtimeFrameTrace& trace = summary.traces[pending.traceIndex];
            pending.startedAt = startAt;
            trace.startedAt = startAt;
            trace.disposition = RealtimeFrameDisposition::Queued;
            trace.queueWaitMicros = startAt >= trace.admittedAt
                ? startAt - trace.admittedAt : 0;
            if (pending.processingMicros >
                (std::numeric_limits<TimestampMicros>::max)() - startAt) {
                markProcessingFailure(trace, startAt);
                return false;
            }
            pending.completesAt = startAt + pending.processingMicros;
            return true;
        };

        const auto finishPending = [&summary, &options, &queueWaits,
            &endToEndLatencies](Pending& pending) {
            RealtimeFrameTrace& trace = summary.traces[pending.traceIndex];
            trace.completedAt = pending.completesAt;
            trace.disposition = RealtimeFrameDisposition::Processed;
            trace.decision = pending.ngCandidate ? InspectionDecision::Ng :
                InspectionDecision::Ok;
            trace.queueWaitMicros = pending.startedAt >= trace.admittedAt
                ? pending.startedAt - trace.admittedAt : 0;
            trace.endToEndMicros = pending.completesAt - trace.capturedAt;
            trace.execution.frameId = trace.frameId;
            trace.execution.status = RejectExecutionStatus::Skipped;
            trace.execution.completedAt = pending.completesAt;
            ++summary.statistics.processed;
            ++summary.statistics.cameras[trace.stationId + "/" + trace.cameraId].processed;

            queueWaits.push_back(trace.queueWaitMicros);
            endToEndLatencies.push_back(trace.endToEndMicros);
            summary.statistics.maxQueueWaitMicros = std::max(
                summary.statistics.maxQueueWaitMicros, trace.queueWaitMicros);
            summary.statistics.maxEndToEndMicros = std::max(
                summary.statistics.maxEndToEndMicros, trace.endToEndMicros);

            if (!pending.ngCandidate) {
                return;
            }
            ++summary.statistics.ngCandidates;
            if (trace.cigaretteNumber == 0) {
                trace.reasonCode = "INVALID_CIGARETTE_NUMBER";
                trace.reasonMessage = "NG replay frame has no cigarette number";
            } else if (options.rejectDelayMicros >
                (std::numeric_limits<TimestampMicros>::max)() - pending.completesAt) {
                trace.reasonCode = "INVALID_REJECT_TIME";
                trace.reasonMessage = "reject schedule exceeds virtual clock range";
            } else {
                trace.command.frameId = trace.frameId;
                trace.command.cigaretteNumber = trace.cigaretteNumber;
                trace.command.targetOutput = options.targetOutput;
                trace.command.scheduledAt = pending.completesAt + options.rejectDelayMicros;
                trace.command.mode = RejectMode::Simulation;
                std::string commandError;
                if (trace.command.validate(&commandError)) {
                    trace.execution.status = RejectExecutionStatus::Simulated;
                    trace.execution.completedAt = trace.command.scheduledAt;
                    ++summary.statistics.commands;
                    ++summary.statistics.simulatedRejects;
                    return;
                }
                trace.reasonCode = "INVALID_REJECT_COMMAND";
                trace.reasonMessage = commandError;
            }
            trace.execution.status = RejectExecutionStatus::Failed;
            trace.execution.errorCode = trace.reasonCode;
            trace.execution.errorMessage = trace.reasonMessage;
            ++summary.statistics.rejectFailures;
        };

        const auto startNextAt = [&]() {
            while (!hasActive && !queue.empty()) {
                active = queue.front();
                queue.pop_front();
                if (startPending(active, virtualNow)) {
                    hasActive = true;
                }
            }
            updateDepth();
        };
        const auto drainUntil = [&](TimestampMicros limit) {
            while (hasActive && active.completesAt <= limit) {
                virtualNow = active.completesAt;
                finishPending(active);
                hasActive = false;
                startNextAt();
            }
            if (virtualNow < limit) {
                virtualNow = limit;
            }
        };
        const auto completeAll = [&]() {
            while (hasActive) {
                drainUntil(active.completesAt);
            }
        };
        const auto cancelPipeline = [&]() {
            if (hasActive) {
                markCancelled(summary.traces[active.traceIndex],
                    "STOP_CANCELLED_ACTIVE", "active processing cancelled by stop", virtualNow);
                hasActive = false;
            }
            while (!queue.empty()) {
                const Pending pending = queue.front();
                queue.pop_front();
                markCancelled(summary.traces[pending.traceIndex],
                    "STOP_CANCELLED_QUEUED", "queued frame cancelled by stop", virtualNow);
            }
            updateDepth();
        };

        for (std::size_t inputIndex = 0; inputIndex < input.size(); ++inputIndex) {
            const RealtimeInputFrame& event = input[inputIndex];
            ++summary.statistics.offered;
            const bool arrivalWasOutOfOrder = hasPreviousArrival &&
                event.arrivalAt < previousArrival;
            if (arrivalWasOutOfOrder) {
                ++summary.statistics.arrivalOutOfOrder;
            }
            previousArrival = event.arrivalAt;
            hasPreviousArrival = true;

            if (stopped || (options.stopAtMicros > 0 &&
                    event.arrivalAt >= options.stopAtMicros)) {
                if (!stopped) {
                    drainUntil(options.stopAtMicros);
                    if (options.drainOnStop) {
                        completeAll();
                    } else {
                        cancelPipeline();
                    }
                    stopped = true;
                }
                const std::size_t traceIndex = appendTrace(event,
                    options.stopAtMicros > 0 ? options.stopAtMicros : virtualNow);
                RealtimeFrameTrace& trace = summary.traces[traceIndex];
                if (arrivalWasOutOfOrder) {
                    trace.sequenceFlags.push_back("ARRIVAL_OUT_OF_ORDER");
                }
                trace.disposition = RealtimeFrameDisposition::AfterStop;
                trace.reasonCode = "SOURCE_STOPPED";
                trace.reasonMessage = "frame arrived after the configured stop boundary";
                ++summary.statistics.afterStop;
                continue;
            }

            const TimestampMicros effectiveArrival = event.arrivalAt > virtualNow
                ? event.arrivalAt : virtualNow;
            drainUntil(effectiveArrival);
            const std::size_t traceIndex = appendTrace(event, effectiveArrival);
            RealtimeFrameTrace& trace = summary.traces[traceIndex];
            if (arrivalWasOutOfOrder) {
                trace.sequenceFlags.push_back("ARRIVAL_OUT_OF_ORDER");
            }
            std::string frameError;
            if (event.arrivalAt <= 0 || event.processingMicros < -1 ||
                !event.frame.validate(&frameError) ||
                event.frame.capturedAt > event.arrivalAt) {
                trace.disposition = RealtimeFrameDisposition::Invalid;
                trace.reasonCode = "INVALID_FRAME";
                if (event.arrivalAt <= 0) {
                    trace.reasonMessage = "arrivalAt must be positive";
                } else if (event.processingMicros < -1) {
                    trace.reasonMessage = "processingMicros must be -1 or non-negative";
                } else if (!frameError.empty()) {
                    trace.reasonMessage = frameError;
                } else {
                    trace.reasonMessage = "capturedAt must not be later than arrivalAt";
                }
                ++summary.statistics.invalid;
                ++summary.statistics.sourceErrors;
                continue;
            }
            if (!frameIds.insert(event.frame.frameId).second) {
                trace.disposition = RealtimeFrameDisposition::DuplicateFrameId;
                trace.reasonCode = "DUPLICATE_FRAME_ID";
                trace.reasonMessage = "frameId was already observed in this replay";
                ++summary.statistics.duplicateFrameIds;
                ++summary.statistics.sourceErrors;
                continue;
            }

            updateSequenceStatistics(summary.statistics, cameraSequences,
                event.frame, trace, previousFrameId, hasPreviousFrameId);
            const std::string key = cameraKey(event.frame);
            RealtimeCameraStatistics& camera = summary.statistics.cameras[key];
            ++camera.offered;
            ++camera.accepted;
            ++summary.statistics.accepted;
            trace.queueDepthAtArrival = queue.size();

            Pending pending;
            pending.traceIndex = traceIndex;
            pending.processingMicros = event.processingMicros < 0
                ? options.defaultProcessingMicros : event.processingMicros;
            pending.ngCandidate = event.ngCandidate;
            if (!hasActive) {
                active = pending;
                if (startPending(active, effectiveArrival)) {
                    hasActive = true;
                }
                updateDepth();
                continue;
            }

            if (queue.size() >= options.queueCapacity) {
                if (options.overflowPolicy == QueueOverflowPolicy::RejectNewest) {
                    markDrop(trace, RealtimeFrameDisposition::DroppedQueueFull,
                        "QUEUE_FULL_REJECT_NEWEST", "queue capacity reached");
                    updateDepth();
                    continue;
                }
                const Pending oldest = queue.front();
                queue.pop_front();
                markDrop(summary.traces[oldest.traceIndex],
                    RealtimeFrameDisposition::DroppedOldest,
                    "QUEUE_FULL_DROP_OLDEST", "oldest queued frame evicted");
            }
            queue.push_back(pending);
            trace.disposition = RealtimeFrameDisposition::Queued;
            updateDepth();
        }

        if (!stopped) {
            completeAll();
        }
        summary.statistics.p95QueueWaitMicros = percentile95(queueWaits);
        summary.statistics.p95EndToEndMicros = percentile95(endToEndLatencies);

        if (!summary.statistics.isConsistent(summary.traces.size())) {
            summary.state = RealtimeSimulationRunState::Failed;
            summary.issues.push_back("realtime simulation statistics invariant failed");
        } else if (stopped) {
            summary.state = RealtimeSimulationRunState::Stopped;
        } else if (hasAnomalies(summary.statistics)) {
            summary.state = RealtimeSimulationRunState::CompletedWithErrors;
        } else {
            summary.state = RealtimeSimulationRunState::Completed;
        }
        return summary;
    }

private:
    static bool validateOptions(const RealtimeSimulationOptions& options,
        std::string& errorMessage)
    {
        if (options.queueCapacity == 0 || options.queueCapacity > 65536U) {
            errorMessage = "realtime queue capacity must be from 1 to 65536";
            return false;
        }
        if (options.initialClockMicros <= 0 || options.defaultProcessingMicros < 0 ||
            options.rejectDelayMicros < 0 || options.targetOutput.empty() ||
            options.stopAtMicros < 0) {
            errorMessage = "realtime simulation options contain an invalid clock, duration or target";
            return false;
        }
        bool hasNonWhitespace = false;
        for (const char character : options.targetOutput) {
            if (std::isspace(static_cast<unsigned char>(character)) == 0) {
                hasNonWhitespace = true;
                break;
            }
        }
        if (!hasNonWhitespace || options.targetOutput.size() > 256U) {
            errorMessage = "realtime target output must contain 1 to 256 non-whitespace bytes";
            return false;
        }
        return true;
    }

    static void updateSequenceStatistics(RealtimeSimulationStatistics& statistics,
        std::map<std::string, CameraSequence>& sequences, const FramePacket& frame,
        RealtimeFrameTrace& trace, std::uint64_t& previousFrameId,
        bool& hasPreviousFrameId)
    {
        if (hasPreviousFrameId) {
            if (frame.frameId < previousFrameId) {
                ++statistics.frameIdOutOfOrder;
                trace.sequenceFlags.push_back("FRAME_ID_OUT_OF_ORDER");
            } else if (frame.frameId > previousFrameId &&
                frame.frameId - previousFrameId > 1U) {
                statistics.frameIdGaps += frame.frameId - previousFrameId - 1U;
                trace.sequenceFlags.push_back("FRAME_ID_GAP");
            }
            if (frame.frameId > previousFrameId) {
                previousFrameId = frame.frameId;
            }
        } else {
            previousFrameId = frame.frameId;
            hasPreviousFrameId = true;
        }

        const std::string key = frame.stationId + "/" + frame.cameraId;
        CameraSequence& sequence = sequences[key];
        if (frame.cigaretteNumber == 0) {
            return;
        }
        if (sequence.hasLast) {
            if (frame.cigaretteNumber == sequence.lastCigarette) {
                ++statistics.duplicateCigarettes;
                ++statistics.cameras[key].duplicateCigarettes;
                trace.sequenceFlags.push_back("CIGARETTE_DUPLICATE");
            } else if (frame.cigaretteNumber < sequence.lastCigarette) {
                ++statistics.cigaretteOutOfOrder;
                ++statistics.cameras[key].cigaretteOutOfOrder;
                trace.sequenceFlags.push_back("CIGARETTE_OUT_OF_ORDER");
            } else {
                const std::uint64_t current = frame.cigaretteNumber;
                const std::uint64_t previous = sequence.lastCigarette;
                if (current > previous + 1U) {
                    const std::uint64_t gap = current - previous - 1U;
                    statistics.cigaretteGaps += gap;
                    statistics.cameras[key].cigaretteGaps += gap;
                    trace.sequenceFlags.push_back("CIGARETTE_GAP");
                }
                sequence.lastCigarette = frame.cigaretteNumber;
            }
        } else {
            sequence.hasLast = true;
            sequence.lastCigarette = frame.cigaretteNumber;
        }
    }

    static TimestampMicros percentile95(std::vector<TimestampMicros> values)
    {
        if (values.empty()) {
            return 0;
        }
        std::sort(values.begin(), values.end());
        const std::size_t rank = (values.size() * 95U + 99U) / 100U;
        return values[rank == 0 ? 0 : rank - 1U];
    }

    static bool hasAnomalies(const RealtimeSimulationStatistics& statistics)
    {
        return statistics.dropped != 0 || statistics.duplicateFrameIds != 0 ||
            statistics.invalid != 0 || statistics.processingFailures != 0 ||
            statistics.sourceErrors != 0 || statistics.arrivalOutOfOrder != 0 ||
            statistics.frameIdGaps != 0 || statistics.frameIdOutOfOrder != 0 ||
            statistics.duplicateCigarettes != 0 || statistics.cigaretteGaps != 0 ||
            statistics.cigaretteOutOfOrder != 0 || statistics.rejectFailures != 0;
    }
};

inline bool validateRealtimeSimulationSummary(const RealtimeSimulationSummary& summary,
    std::string* reason = nullptr)
{
    const auto fail = [reason](const char* message) {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    };
    if (summary.state == RealtimeSimulationRunState::Failed) {
        return fail("realtime simulation summary is failed");
    }
    if (!summary.statistics.isConsistent(summary.traces.size())) {
        return fail("realtime simulation count invariant failed");
    }

    std::uint64_t processed = 0;
    std::uint64_t dropped = 0;
    std::uint64_t duplicateFrameIds = 0;
    std::uint64_t invalid = 0;
    std::uint64_t processingFailures = 0;
    std::uint64_t afterStop = 0;
    std::uint64_t cancelled = 0;
    std::uint64_t ngCandidates = 0;
    std::uint64_t commands = 0;
    std::uint64_t simulatedRejects = 0;
    std::uint64_t rejectFailures = 0;
    std::uint64_t arrivalOutOfOrder = 0;
    std::uint64_t frameIdGaps = 0;
    std::uint64_t frameIdOutOfOrder = 0;
    std::uint64_t duplicateCigarettes = 0;
    std::uint64_t cigaretteGaps = 0;
    std::uint64_t cigaretteOutOfOrder = 0;
    TimestampMicros previousArrival = 0;
    bool hasPreviousArrival = false;
    std::uint64_t previousFrameId = 0;
    bool hasPreviousFrameId = false;
    std::map<std::string, std::uint32_t> previousCigarettes;
    std::map<std::string, bool> hasPreviousCigarette;
    std::map<std::string, RealtimeCameraStatistics> expectedCameras;
    std::vector<TimestampMicros> queueWaits;
    std::vector<TimestampMicros> endToEndLatencies;
    TimestampMicros maxQueueWaitMicros = 0;
    TimestampMicros maxEndToEndMicros = 0;
    for (const RealtimeFrameTrace& trace : summary.traces) {
        if (trace.stationId.empty() || trace.cameraId.empty() ||
            trace.execution.frameId != trace.frameId || trace.execution.completedAt <= 0) {
            return fail("realtime trace is missing frame or receipt binding");
        }
        std::vector<std::string> expectedSequenceFlags;
        if (hasPreviousArrival && trace.arrivalAt < previousArrival) {
            ++arrivalOutOfOrder;
            expectedSequenceFlags.push_back("ARRIVAL_OUT_OF_ORDER");
        }
        previousArrival = trace.arrivalAt;
        hasPreviousArrival = true;

        const bool acceptedDisposition =
            trace.disposition == RealtimeFrameDisposition::Processed ||
            trace.disposition == RealtimeFrameDisposition::DroppedQueueFull ||
            trace.disposition == RealtimeFrameDisposition::DroppedOldest ||
            trace.disposition == RealtimeFrameDisposition::ProcessingFailed ||
            trace.disposition == RealtimeFrameDisposition::CancelledOnStop;
        RealtimeCameraStatistics* expectedCamera = nullptr;
        if (acceptedDisposition) {
            const std::string cameraKey = trace.stationId + "/" + trace.cameraId;
            expectedCamera = &expectedCameras[cameraKey];
            ++expectedCamera->offered;
            ++expectedCamera->accepted;
            if (hasPreviousFrameId) {
                if (trace.frameId < previousFrameId) {
                    ++frameIdOutOfOrder;
                    expectedSequenceFlags.push_back("FRAME_ID_OUT_OF_ORDER");
                } else if (trace.frameId > previousFrameId &&
                    trace.frameId - previousFrameId > 1U) {
                    frameIdGaps += trace.frameId - previousFrameId - 1U;
                    expectedSequenceFlags.push_back("FRAME_ID_GAP");
                }
                if (trace.frameId > previousFrameId) {
                    previousFrameId = trace.frameId;
                }
            } else {
                previousFrameId = trace.frameId;
                hasPreviousFrameId = true;
            }

            if (trace.cigaretteNumber != 0) {
                const std::string key = trace.stationId + "/" + trace.cameraId;
                if (hasPreviousCigarette[key]) {
                    const std::uint32_t previous = previousCigarettes[key];
                    if (trace.cigaretteNumber == previous) {
                        ++duplicateCigarettes;
                        ++expectedCamera->duplicateCigarettes;
                        expectedSequenceFlags.push_back("CIGARETTE_DUPLICATE");
                    } else if (trace.cigaretteNumber < previous) {
                        ++cigaretteOutOfOrder;
                        ++expectedCamera->cigaretteOutOfOrder;
                        expectedSequenceFlags.push_back("CIGARETTE_OUT_OF_ORDER");
                    } else {
                        const std::uint64_t current = trace.cigaretteNumber;
                        if (current > static_cast<std::uint64_t>(previous) + 1U) {
                            const std::uint64_t gap = current - previous - 1U;
                            cigaretteGaps += gap;
                            expectedCamera->cigaretteGaps += gap;
                            expectedSequenceFlags.push_back("CIGARETTE_GAP");
                        }
                        previousCigarettes[key] = trace.cigaretteNumber;
                    }
                } else {
                    hasPreviousCigarette[key] = true;
                    previousCigarettes[key] = trace.cigaretteNumber;
                }
            }
        }
        if (trace.sequenceFlags != expectedSequenceFlags) {
            return fail("realtime sequence flags do not match trace order");
        }

        if (trace.command.frameId != 0) {
            ++commands;
            std::string commandError;
            if (trace.command.frameId != trace.frameId ||
                trace.command.cigaretteNumber != trace.cigaretteNumber ||
                trace.command.mode != RejectMode::Simulation ||
                !trace.command.validate(&commandError)) {
                return fail("realtime command is not Simulation-bound");
            }
        }
        switch (trace.disposition) {
        case RealtimeFrameDisposition::Processed:
            ++processed;
            ++expectedCamera->processed;
            if (trace.frameId == 0 || trace.capturedAt <= 0 || trace.arrivalAt <= 0 ||
                trace.admittedAt <= 0 || trace.startedAt <= 0 || trace.completedAt <= 0 ||
                trace.arrivalAt < trace.capturedAt || trace.admittedAt < trace.arrivalAt ||
                trace.startedAt < trace.admittedAt || trace.completedAt < trace.startedAt ||
                trace.queueWaitMicros != trace.startedAt - trace.admittedAt ||
                trace.endToEndMicros != trace.completedAt - trace.capturedAt ||
                (trace.decision != InspectionDecision::Ok &&
                    trace.decision != InspectionDecision::Ng) ||
                trace.execution.frameId != trace.frameId) {
                return fail("processed realtime trace is incomplete");
            }
            queueWaits.push_back(trace.queueWaitMicros);
            endToEndLatencies.push_back(trace.endToEndMicros);
            maxQueueWaitMicros = (std::max)(maxQueueWaitMicros, trace.queueWaitMicros);
            maxEndToEndMicros = (std::max)(maxEndToEndMicros, trace.endToEndMicros);
            if (trace.decision == InspectionDecision::Ng) {
                ++ngCandidates;
                if (trace.execution.status == RejectExecutionStatus::Simulated) {
                    ++simulatedRejects;
                    if (trace.command.frameId == 0 ||
                        trace.command.scheduledAt < trace.completedAt ||
                        trace.execution.completedAt < trace.command.scheduledAt) {
                        return fail("simulated reject has no command");
                    }
                } else if (trace.execution.status == RejectExecutionStatus::Failed) {
                    ++rejectFailures;
                    if (trace.reasonCode.empty() || trace.command.frameId != 0 ||
                        trace.execution.completedAt < trace.completedAt) {
                        return fail("failed reject has no reason");
                    }
                } else {
                    return fail("NG trace has no simulated or failed receipt");
                }
            } else if (trace.command.frameId != 0 ||
                trace.execution.status != RejectExecutionStatus::Skipped) {
                return fail("OK trace contains a reject command or receipt");
            }
            break;
        case RealtimeFrameDisposition::DroppedQueueFull:
        case RealtimeFrameDisposition::DroppedOldest:
            ++dropped;
            ++expectedCamera->dropped;
            if (trace.reasonCode.empty() || trace.command.frameId != 0 ||
                trace.execution.status != RejectExecutionStatus::Skipped) {
                return fail("dropped realtime trace is missing its reason");
            }
            break;
        case RealtimeFrameDisposition::DuplicateFrameId:
            ++duplicateFrameIds;
            if (trace.reasonCode.empty() || trace.command.frameId != 0 ||
                trace.execution.status != RejectExecutionStatus::Skipped) {
                return fail("duplicate realtime trace is missing its reason");
            }
            break;
        case RealtimeFrameDisposition::Invalid:
            ++invalid;
            if (trace.reasonCode.empty() || trace.command.frameId != 0 ||
                trace.execution.status != RejectExecutionStatus::Skipped) {
                return fail("invalid realtime trace is missing its reason");
            }
            break;
        case RealtimeFrameDisposition::ProcessingFailed:
            ++processingFailures;
            if (trace.reasonCode.empty() || trace.command.frameId != 0 ||
                trace.execution.status != RejectExecutionStatus::Failed ||
                trace.execution.errorCode != trace.reasonCode) {
                return fail("processing failure trace is incomplete");
            }
            break;
        case RealtimeFrameDisposition::AfterStop:
            ++afterStop;
            if (trace.reasonCode.empty() || trace.command.frameId != 0 ||
                trace.execution.status != RejectExecutionStatus::Skipped) {
                return fail("after-stop trace is incomplete");
            }
            break;
        case RealtimeFrameDisposition::CancelledOnStop:
            ++cancelled;
            if (trace.reasonCode.empty() || trace.command.frameId != 0 ||
                trace.execution.status != RejectExecutionStatus::Skipped) {
                return fail("cancelled trace is incomplete");
            }
            break;
        case RealtimeFrameDisposition::Queued:
        default:
            return fail("realtime trace contains an unfinished disposition");
        }
    }
    if (processed != summary.statistics.processed || dropped != summary.statistics.dropped ||
        duplicateFrameIds != summary.statistics.duplicateFrameIds ||
        invalid != summary.statistics.invalid ||
        processingFailures != summary.statistics.processingFailures ||
        afterStop != summary.statistics.afterStop ||
        cancelled != summary.statistics.cancelled ||
        ngCandidates != summary.statistics.ngCandidates ||
        commands != summary.statistics.commands ||
        simulatedRejects != summary.statistics.simulatedRejects ||
        rejectFailures != summary.statistics.rejectFailures) {
        return fail("realtime trace totals do not match statistics");
    }
    if (arrivalOutOfOrder != summary.statistics.arrivalOutOfOrder ||
        frameIdGaps != summary.statistics.frameIdGaps ||
        frameIdOutOfOrder != summary.statistics.frameIdOutOfOrder ||
        duplicateCigarettes != summary.statistics.duplicateCigarettes ||
        cigaretteGaps != summary.statistics.cigaretteGaps ||
        cigaretteOutOfOrder != summary.statistics.cigaretteOutOfOrder) {
        return fail("realtime sequence totals do not match statistics");
    }
    if (summary.statistics.cameras.size() != expectedCameras.size()) {
        return fail("realtime camera statistics contain an unexpected camera");
    }
    for (const std::map<std::string, RealtimeCameraStatistics>::value_type& item :
            expectedCameras) {
        const auto actual = summary.statistics.cameras.find(item.first);
        if (actual == summary.statistics.cameras.end() ||
            actual->second.offered != item.second.offered ||
            actual->second.accepted != item.second.accepted ||
            actual->second.processed != item.second.processed ||
            actual->second.dropped != item.second.dropped ||
            actual->second.duplicateCigarettes != item.second.duplicateCigarettes ||
            actual->second.cigaretteGaps != item.second.cigaretteGaps ||
            actual->second.cigaretteOutOfOrder != item.second.cigaretteOutOfOrder) {
            return fail("realtime per-camera sequence totals do not match statistics");
        }
    }
    const auto percentile95 = [](std::vector<TimestampMicros> values) {
        if (values.empty()) {
            return static_cast<TimestampMicros>(0);
        }
        std::sort(values.begin(), values.end());
        const std::size_t rank = (values.size() * 95U + 99U) / 100U;
        return values[rank == 0 ? 0 : rank - 1U];
    };
    if (summary.statistics.maxQueueWaitMicros != maxQueueWaitMicros ||
        summary.statistics.p95QueueWaitMicros != percentile95(queueWaits) ||
        summary.statistics.maxEndToEndMicros != maxEndToEndMicros ||
        summary.statistics.p95EndToEndMicros != percentile95(endToEndLatencies)) {
        return fail("realtime latency statistics do not match processed traces");
    }
    if (reason != nullptr) {
        reason->clear();
    }
    return true;
}

} // namespace cigvision
