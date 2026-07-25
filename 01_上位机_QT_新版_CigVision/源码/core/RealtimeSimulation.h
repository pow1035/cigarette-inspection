#pragma once

#include "OfflineInspection.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cigvision {

// Local replay is intentionally bounded so a malformed manifest cannot block a
// batch run for hours or years. Production cadence limits remain a separate,
// not-yet-confirmed decision.
constexpr TimestampMicros kMaxLocalReplayDelayMicros = 60000000;

enum class ReplayWaitStatus {
    Completed = 0,
    Cancelled,
    Failed
};

// A pacing boundary keeps replay timing independent from the file/image source.
// Production replay can use SteadyReplayPacer; tests can inject a deterministic
// or manually-cancelled implementation without sleeping.
class IReplayPacer {
public:
    virtual ~IReplayPacer() = default;
    virtual void reset() noexcept = 0;
    virtual ReplayWaitStatus wait(TimestampMicros duration,
        std::string& errorMessage) = 0;
    virtual void cancel() noexcept = 0;
};

class SteadyReplayPacer final : public IReplayPacer {
public:
    void reset() noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled_ = false;
    }

    ReplayWaitStatus wait(TimestampMicros duration,
        std::string& errorMessage) override
    {
        errorMessage.clear();
        if (duration < 0) {
            errorMessage = "replay duration must not be negative";
            return ReplayWaitStatus::Failed;
        }
        if (duration == 0) {
            return ReplayWaitStatus::Completed;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        if (cancelled_) {
            return ReplayWaitStatus::Cancelled;
        }
        const bool wasCancelled = condition_.wait_for(lock,
            std::chrono::microseconds(duration), [this] { return cancelled_; });
        return wasCancelled ? ReplayWaitStatus::Cancelled : ReplayWaitStatus::Completed;
    }

    void cancel() noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelled_ = true;
        }
        condition_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool cancelled_ = false;
};

struct ReplayFrame {
    FramePacket frame;
    // Delay before this frame is made available. The first frame normally has
    // zero delay; later entries make a recorded/local cadence explicit.
    TimestampMicros delayBeforeMicros = 0;
};

class ReplayFrameSource final : public IFrameSource {
public:
    ReplayFrameSource(std::vector<ReplayFrame> frames, IReplayPacer& pacer)
        : frames_(std::move(frames)), pacer_(pacer) {}

    bool start(std::string& errorMessage) override
    {
        errorMessage.clear();
        for (const ReplayFrame& entry : frames_) {
            if (entry.delayBeforeMicros < 0) {
                errorMessage = "replay frame delay must not be negative";
                return false;
            }
            if (entry.delayBeforeMicros > kMaxLocalReplayDelayMicros) {
                errorMessage = "replay frame delay exceeds the local safety limit";
                return false;
            }
            std::string validationError;
            if (!entry.frame.validate(&validationError)) {
                errorMessage = "invalid replay frame: " + validationError;
                return false;
            }
        }
        index_ = 0;
        running_.store(true);
        pacer_.reset();
        return true;
    }

    void stop() noexcept override
    {
        running_.store(false);
        pacer_.cancel();
    }

    bool tryRead(FramePacket& frame, std::string& errorMessage) override
    {
        errorMessage.clear();
        if (!running_.load() || index_ >= frames_.size()) {
            return false;
        }

        const ReplayFrame& entry = frames_[index_];
        if (entry.delayBeforeMicros > 0) {
            const ReplayWaitStatus waitStatus = pacer_.wait(
                entry.delayBeforeMicros, errorMessage);
            if (waitStatus == ReplayWaitStatus::Cancelled) {
                errorMessage.clear();
                running_.store(false);
                return false;
            }
            if (waitStatus == ReplayWaitStatus::Failed) {
                running_.store(false);
                return false;
            }
        }

        if (!running_.load()) {
            return false;
        }
        frame = entry.frame;
        ++index_;
        return true;
    }

    std::size_t index() const noexcept { return index_; }
    std::size_t size() const noexcept { return frames_.size(); }

private:
    std::vector<ReplayFrame> frames_;
    IReplayPacer& pacer_;
    std::size_t index_ = 0;
    std::atomic<bool> running_{ false };
};

class SimulationClock final : public IClock {
public:
    explicit SimulationClock(TimestampMicros initialMicros = 1)
        : nowMicros_(initialMicros)
    {
        if (initialMicros <= 0) {
            throw std::invalid_argument("simulation clock must start positive");
        }
    }

    TimestampMicros now() const noexcept override { return nowMicros_.load(); }

    void set(TimestampMicros value)
    {
        if (value <= 0) {
            throw std::invalid_argument("simulation clock must remain positive");
        }
        nowMicros_.store(value);
    }

    void advance(TimestampMicros delta)
    {
        if (delta < 0 || now() > (std::numeric_limits<TimestampMicros>::max)() - delta) {
            throw std::overflow_error("simulation clock overflow");
        }
        nowMicros_.fetch_add(delta);
    }

private:
    std::atomic<TimestampMicros> nowMicros_;
};

struct SimulationRejectOptions {
    TimestampMicros delayMicros = 0;
    std::string targetOutput = "simulation-reject";
};

enum class SimulationRejectTraceStatus {
    Skipped = 0,
    Simulated,
    Failed
};

struct SimulationRejectTrace {
    std::uint64_t frameId = 0;
    std::string stationId;
    std::string cameraId;
    std::uint32_t cigaretteNumber = 0;
    TimestampMicros observedAt = 0;
    InspectionDecision decision = InspectionDecision::Unknown;
    SimulationRejectTraceStatus status = SimulationRejectTraceStatus::Skipped;
    RejectCommand command;
    RejectExecutionResult execution;
    std::string errorCode;
    std::string errorMessage;
};

struct SimulationRejectStatistics {
    std::uint64_t observed = 0;
    std::uint64_t ngCandidates = 0;
    std::uint64_t commands = 0;
    std::uint64_t simulated = 0;
    std::uint64_t skipped = 0;
    std::uint64_t failed = 0;
};

inline bool validateSimulationTrace(const std::vector<SimulationRejectTrace>& traces,
    const SimulationRejectStatistics& statistics, std::string* reason = nullptr)
{
    const auto fail = [reason](const char* message) {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    };
    if (traces.size() != statistics.observed) {
        return fail("simulation trace count does not match observed count");
    }

    std::uint64_t skipped = 0;
    std::uint64_t simulated = 0;
    std::uint64_t failed = 0;
    std::uint64_t commands = 0;
    std::uint64_t ngCandidates = 0;
    for (const SimulationRejectTrace& trace : traces) {
        if (trace.frameId == 0 || trace.stationId.empty() || trace.cameraId.empty() ||
            trace.observedAt <= 0 || trace.execution.frameId != trace.frameId ||
            trace.execution.completedAt <= 0) {
            return fail("simulation trace is missing frame metadata or receipt binding");
        }
        if (trace.decision == InspectionDecision::Ng) {
            ++ngCandidates;
        }
        if (trace.command.frameId != 0) {
            ++commands;
            if (trace.command.frameId != trace.frameId ||
                trace.command.cigaretteNumber != trace.cigaretteNumber ||
                trace.command.mode != RejectMode::Simulation ||
                !trace.command.validate()) {
                return fail("simulation trace command is not Simulation-bound");
            }
        }
        switch (trace.status) {
        case SimulationRejectTraceStatus::Skipped:
            ++skipped;
            if (trace.decision == InspectionDecision::Ng || trace.command.frameId != 0 ||
                trace.execution.status != RejectExecutionStatus::Skipped) {
                return fail("skipped trace has a command or non-skipped receipt");
            }
            break;
        case SimulationRejectTraceStatus::Simulated:
            ++simulated;
            if (trace.decision != InspectionDecision::Ng || trace.command.frameId == 0 ||
                trace.execution.status != RejectExecutionStatus::Simulated ||
                trace.execution.completedAt < trace.command.scheduledAt) {
                return fail("simulated trace is missing an NG command or simulated receipt");
            }
            break;
        case SimulationRejectTraceStatus::Failed:
            ++failed;
            if (trace.errorCode.empty() || trace.execution.errorCode.empty() ||
                trace.execution.status != RejectExecutionStatus::Failed) {
                return fail("failed trace is missing an error or failed receipt");
            }
            break;
        default:
            return fail("simulation trace status is outside the supported domain");
        }
    }
    if (skipped != statistics.skipped || simulated != statistics.simulated ||
        failed != statistics.failed || commands != statistics.commands ||
        ngCandidates != statistics.ngCandidates || simulated > commands) {
        return fail("simulation trace statistics do not match trace entries");
    }
    if (reason != nullptr) {
        reason->clear();
    }
    return true;
}

// Safe local sink used by the P6 simulation path. It records commands and
// never writes a hardware output. A failing mode is useful for deterministic
// negative-path tests.
class SimulationRejectOutput final : public IRejectOutput {
public:
    explicit SimulationRejectOutput(IClock& clock, bool fail = false)
        : clock_(clock), fail_(fail) {}

    RejectExecutionResult execute(const RejectCommand& command) override
    {
        RejectExecutionResult result;
        result.frameId = command.frameId;
        result.completedAt = (std::max)(clock_.now(), command.scheduledAt);
        std::string validationError;
        if (!command.validate(&validationError)) {
            result.status = RejectExecutionStatus::Failed;
            result.errorCode = "INVALID_REJECT_COMMAND";
            result.errorMessage = validationError;
            return result;
        }
        if (command.mode != RejectMode::Simulation) {
            result.status = RejectExecutionStatus::Failed;
            result.errorCode = "REAL_OUTPUT_DISABLED_IN_SIMULATION";
            result.errorMessage = "simulation output accepts only Simulation commands";
            return result;
        }
        commands_.push_back(command);
        if (fail_) {
            result.status = RejectExecutionStatus::Failed;
            result.errorCode = "SIMULATION_OUTPUT_FAILURE";
            result.errorMessage = "injected simulation output failure";
            return result;
        }
        result.status = RejectExecutionStatus::Simulated;
        return result;
    }

    const std::vector<RejectCommand>& commands() const noexcept { return commands_; }

private:
    IClock& clock_;
    bool fail_ = false;
    std::vector<RejectCommand> commands_;
};

class SimulationRejectObserver final : public IInspectionObserver {
public:
    SimulationRejectObserver(IClock& clock, IRejectOutput& output,
        SimulationRejectOptions options = SimulationRejectOptions())
        : clock_(clock), output_(output), options_(std::move(options))
    {
        if (options_.delayMicros < 0) {
            throw std::invalid_argument("simulation reject delay must not be negative");
        }
        if (options_.targetOutput.empty()) {
            throw std::invalid_argument("simulation reject target output is required");
        }
    }

    void onResult(const FramePacket& frame, const InspectionResult& result,
        const InspectionStatistics&) override
    {
        ++statistics_.observed;
        SimulationRejectTrace trace;
        trace.frameId = frame.frameId;
        trace.stationId = frame.stationId;
        trace.cameraId = frame.cameraId;
        trace.cigaretteNumber = frame.cigaretteNumber;
        trace.observedAt = clock_.now();
        trace.decision = result.decision;
        if (result.decision == InspectionDecision::Ng) {
            ++statistics_.ngCandidates;
        }

        if (result.frameId != frame.frameId) {
            failTrace(trace, "SIMULATION_FRAME_ID_MISMATCH",
                "inspection result does not match the replay frame");
            return;
        }
        std::string validationError;
        if (!result.validate(&validationError)) {
            failTrace(trace, "INVALID_INSPECTION_RESULT", validationError);
            return;
        }

        if (result.decision != InspectionDecision::Ng) {
            trace.status = SimulationRejectTraceStatus::Skipped;
            trace.execution.frameId = frame.frameId;
            trace.execution.status = RejectExecutionStatus::Skipped;
            trace.execution.completedAt = trace.observedAt;
            ++statistics_.skipped;
            traces_.push_back(std::move(trace));
            return;
        }

        const TimestampMicros now = trace.observedAt;
        if (frame.cigaretteNumber == 0) {
            failTrace(trace, "INVALID_CIGARETTE_NUMBER", "cigarette number must be non-zero");
            return;
        }
        if (now <= 0 || options_.delayMicros >
                (std::numeric_limits<TimestampMicros>::max)() - now) {
            failTrace(trace, "INVALID_REJECT_TIME", "reject schedule is outside clock range");
            return;
        }

        trace.command.frameId = frame.frameId;
        trace.command.cigaretteNumber = frame.cigaretteNumber;
        trace.command.targetOutput = options_.targetOutput;
        trace.command.scheduledAt = now + options_.delayMicros;
        trace.command.mode = RejectMode::Simulation;
        std::string commandValidationError;
        if (!trace.command.validate(&commandValidationError)) {
            failTrace(trace, "INVALID_REJECT_COMMAND", commandValidationError);
            return;
        }
        ++statistics_.commands;

        try {
            trace.execution = output_.execute(trace.command);
        }
        catch (const std::exception& error) {
            failTrace(trace, "SIMULATION_OUTPUT_EXCEPTION", error.what());
            return;
        }
        catch (...) {
            failTrace(trace, "SIMULATION_OUTPUT_EXCEPTION", "unknown output exception");
            return;
        }
        if (trace.execution.frameId != frame.frameId) {
            failTrace(trace, "SIMULATION_FRAME_ID_MISMATCH",
                "simulation output receipt does not match the frame");
            return;
        }
        if (trace.execution.status != RejectExecutionStatus::Simulated) {
            failTrace(trace, "SIMULATION_OUTPUT_NOT_SIMULATED",
                "simulation path returned a non-simulated execution status");
            return;
        }
        trace.status = SimulationRejectTraceStatus::Simulated;
        ++statistics_.simulated;
        traces_.push_back(std::move(trace));
    }

    const SimulationRejectStatistics& statistics() const noexcept { return statistics_; }
    const std::vector<SimulationRejectTrace>& traces() const noexcept { return traces_; }

private:
    void failTrace(SimulationRejectTrace& trace, const std::string& code,
        const std::string& message)
    {
        trace.status = SimulationRejectTraceStatus::Failed;
        trace.errorCode = code;
        trace.errorMessage = message;
        trace.execution.frameId = trace.frameId;
        trace.execution.status = RejectExecutionStatus::Failed;
        trace.execution.completedAt = clock_.now();
        trace.execution.errorCode = trace.errorCode;
        trace.execution.errorMessage = trace.errorMessage;
        ++statistics_.failed;
        traces_.push_back(std::move(trace));
    }

    IClock& clock_;
    IRejectOutput& output_;
    SimulationRejectOptions options_;
    SimulationRejectStatistics statistics_;
    std::vector<SimulationRejectTrace> traces_;
};

} // namespace cigvision
