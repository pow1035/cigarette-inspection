#include "core/OfflineInspection.h"
#include "core/BatchCommandLine.h"
#include "core/RealtimeSimulation.h"
#include "core/RealtimeLoadSimulation.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace cigvision;

#define CHECK(expression) check((expression), #expression, __FILE__, __LINE__)

void check(bool condition, const char* expression, const char* file, int line)
{
    if (!condition) {
        throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
            " check failed: " + expression);
    }
}

FramePacket makeFrame(std::uint64_t id, const std::string& camera,
    std::uint32_t cigaretteNumber)
{
    FramePacket frame;
    frame.frameId = id;
    frame.stationId = "station-1";
    frame.cameraId = camera;
    frame.cigaretteNumber = cigaretteNumber;
    frame.capturedAt = 1000 + static_cast<TimestampMicros>(id);
    frame.width = 2;
    frame.height = 2;
    frame.strideBytes = 2;
    frame.pixelFormat = PixelFormat::Mono8;
    frame.pixels = { 1, 2, 3, 4 };
    return frame;
}

InspectionResult makeNgResult(std::uint64_t frameId)
{
    Detection detection;
    detection.classId = 0;
    detection.className = "simulated-defect";
    detection.confidence = 0.9F;
    detection.box = { 0.0F, 0.0F, 1.0F, 1.0F };
    detection.detectorVersion = "simulation-test-v1";

    InspectionResult result;
    result.frameId = frameId;
    result.decision = InspectionDecision::Ng;
    result.defects.push_back(detection);
    result.parameterVersion = "simulation-test-v1";
    result.parameterSha256 = std::string(64, 'a');
    return result;
}

class RecordingPacer final : public IReplayPacer {
public:
    void reset() noexcept override { cancelled = false; }

    ReplayWaitStatus wait(TimestampMicros duration, std::string& error) override
    {
        if (duration < 0) {
            error = "negative duration";
            return ReplayWaitStatus::Failed;
        }
        waits.push_back(duration);
        return cancelled ? ReplayWaitStatus::Cancelled : ReplayWaitStatus::Completed;
    }

    void cancel() noexcept override { cancelled = true; }

    std::vector<TimestampMicros> waits;
    bool cancelled = false;
};

class BlockingPacer final : public IReplayPacer {
public:
    void reset() noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex);
        cancelled = false;
        waiting = false;
    }

    ReplayWaitStatus wait(TimestampMicros, std::string&) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        waiting = true;
        changed.notify_all();
        changed.wait(lock, [this] { return cancelled; });
        return ReplayWaitStatus::Cancelled;
    }

    void cancel() noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            cancelled = true;
        }
        changed.notify_all();
    }

    void waitUntilEntered()
    {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [this] { return waiting; });
    }

private:
    std::mutex mutex;
    std::condition_variable changed;
    bool waiting = false;
    bool cancelled = false;
};

class MemorySink final : public IInspectionResultSink {
public:
    bool store(const InspectionResult& result, std::string& error) override
    {
        error.clear();
        results.push_back(result);
        return true;
    }
    std::vector<InspectionResult> results;
};

class MemoryArchive final : public IFrameArchive {
public:
    bool store(const FramePacket&, const InspectionResult&, std::string& error) override
    {
        error.clear();
        ++stored;
        return true;
    }
    std::size_t stored = 0;
};

class ExecutedOutput final : public IRejectOutput {
public:
    RejectExecutionResult execute(const RejectCommand& command) override
    {
        RejectExecutionResult result;
        result.frameId = command.frameId;
        result.status = RejectExecutionStatus::Executed;
        result.completedAt = command.scheduledAt;
        return result;
    }
};

class ThrowingOutput final : public IRejectOutput {
public:
    RejectExecutionResult execute(const RejectCommand&) override
    {
        throw std::runtime_error("output failed");
    }
};

void replayPreservesMetadataAndPacing()
{
    RecordingPacer pacer;
    std::vector<ReplayFrame> entries = {
        { makeFrame(1, "camera-a", 101), 0 },
        { makeFrame(2, "camera-b", 202), 100 },
        { makeFrame(3, "camera-a", 103), 250 },
    };
    ReplayFrameSource source(std::move(entries), pacer);
    std::string error;
    CHECK(source.start(error));
    FramePacket frame;
    CHECK(source.tryRead(frame, error));
    CHECK(frame.cameraId == "camera-a" && frame.cigaretteNumber == 101);
    CHECK(source.tryRead(frame, error));
    CHECK(frame.frameId == 2 && frame.cameraId == "camera-b");
    CHECK(source.tryRead(frame, error));
    CHECK(frame.frameId == 3 && frame.cigaretteNumber == 103);
    CHECK(!source.tryRead(frame, error));
    CHECK(pacer.waits.size() == 2);
    CHECK(pacer.waits[0] == 100 && pacer.waits[1] == 250);
    source.stop();
}

void replayRejectsInvalidEntries()
{
    std::string error;
    RecordingPacer pacer;
    ReplayFrameSource empty({}, pacer);
    CHECK(empty.start(error));
    FramePacket emptyFrame;
    CHECK(!empty.tryRead(emptyFrame, error));
    CHECK(error.empty());

    ReplayFrameSource negative({ { makeFrame(1, "camera", 1), -1 } }, pacer);
    CHECK(!negative.start(error));
    CHECK(error.find("negative") != std::string::npos);

    ReplayFrameSource tooLong({ { makeFrame(1, "camera", 1),
        kMaxLocalReplayDelayMicros + 1 } }, pacer);
    CHECK(!tooLong.start(error));
    CHECK(error.find("safety limit") != std::string::npos);

    FramePacket invalid = makeFrame(1, "camera", 1);
    invalid.frameId = 0;
    ReplayFrameSource malformed({ { invalid, 0 } }, pacer);
    CHECK(!malformed.start(error));
    CHECK(error.find("invalid replay frame") != std::string::npos);
}

void replayStopCancelsPendingCadence()
{
    BlockingPacer pacer;
    ReplayFrameSource source({
        { makeFrame(1, "camera", 1), 0 },
        { makeFrame(2, "camera", 2), 1000 },
    }, pacer);
    std::string error;
    CHECK(source.start(error));
    FramePacket frame;
    CHECK(source.tryRead(frame, error));

    bool readResult = true;
    std::thread reader([&] { readResult = source.tryRead(frame, error); });
    pacer.waitUntilEntered();
    source.stop();
    reader.join();
    CHECK(!readResult);
    CHECK(error.empty());
}

void sessionStopCancelsBlockedReplaySource()
{
    BlockingPacer pacer;
    ReplayFrameSource source({
        { makeFrame(1, "camera", 1), 0 },
        { makeFrame(2, "camera", 2), 1000 },
    }, pacer);
    DeterministicFixtureDetector detector;
    MemorySink sink;
    MemoryArchive archive;
    SimulationClock clock(1000);
    OfflineInspectionSession session;
    OfflineRunSummary summary;
    std::thread running([&] {
        summary = session.run(source, detector, sink, archive, clock, nullptr);
    });
    pacer.waitUntilEntered();
    session.requestStop();
    running.join();
    CHECK(summary.state == OfflineRunState::Stopped);
    CHECK(summary.statistics.processed <= 1);
    CHECK(summary.statistics.isConsistent());
    CHECK(!session.isRunning());
}

void endToEndSimulationProducesOnlySimulatedRejects()
{
    RecordingPacer pacer;
    ReplayFrameSource source({
        { makeFrame(1, "camera-a", 101), 0 },
        { makeFrame(2, "camera-a", 102), 0 },
        { makeFrame(3, "camera-b", 203), 0 },
        { makeFrame(4, "camera-b", 204), 0 },
    }, pacer);
    DeterministicFixtureDetector detector;
    MemorySink sink;
    MemoryArchive archive;
    SimulationClock clock(2000);
    SimulationRejectOutput output(clock);
    SimulationRejectObserver observer(clock, output,
        { 50, "simulation-output-1" });
    OfflineInspectionSession session;
    OfflineRunSummary summary = session.run(source, detector, sink, archive, clock,
        &observer);

    CHECK(summary.state == OfflineRunState::Completed);
    CHECK(summary.statistics.received == 4);
    CHECK(summary.statistics.processed == 4);
    CHECK(summary.statistics.dropped == 0);
    CHECK(archive.stored == 4);
    CHECK(observer.statistics().observed == 4);
    CHECK(observer.statistics().ngCandidates == 2);
    CHECK(observer.statistics().commands == 2);
    CHECK(observer.statistics().simulated == 2);
    CHECK(observer.statistics().skipped == 2);
    CHECK(observer.statistics().failed == 0);
    CHECK(validateSimulationTrace(observer.traces(), observer.statistics()));
    CHECK(output.commands().size() == 2);
    CHECK(observer.traces().size() == 4);
    CHECK(observer.traces()[0].stationId == "station-1");
    CHECK(observer.traces()[0].cameraId == "camera-a");
    CHECK(observer.traces()[0].observedAt > 0);
    CHECK(observer.traces()[0].command.frameId == 0);
    CHECK(observer.traces()[1].command.targetOutput == "simulation-output-1");
    CHECK(observer.traces()[1].execution.completedAt ==
        observer.traces()[1].command.scheduledAt);
    std::vector<SimulationRejectTrace> incompleteTrace = observer.traces();
    incompleteTrace.pop_back();
    CHECK(!validateSimulationTrace(incompleteTrace, observer.statistics()));
    std::vector<SimulationRejectTrace> mismatchedTrace = observer.traces();
    mismatchedTrace[1].status = SimulationRejectTraceStatus::Skipped;
    CHECK(!validateSimulationTrace(mismatchedTrace, observer.statistics()));
    std::vector<SimulationRejectTrace> earlyReceiptTrace = observer.traces();
    earlyReceiptTrace[1].execution.completedAt =
        earlyReceiptTrace[1].command.scheduledAt - 1;
    CHECK(!validateSimulationTrace(earlyReceiptTrace, observer.statistics()));
    std::vector<SimulationRejectTrace> mismatchedCommandTrace = observer.traces();
    mismatchedCommandTrace[1].command.cigaretteNumber += 1;
    CHECK(!validateSimulationTrace(mismatchedCommandTrace, observer.statistics()));
    CHECK(output.commands()[0].frameId == 2 && output.commands()[0].cigaretteNumber == 102);
    CHECK(output.commands()[1].frameId == 4 && output.commands()[1].cigaretteNumber == 204);
    CHECK(output.commands()[0].mode == RejectMode::Simulation);
    CHECK(output.commands()[0].scheduledAt == 2050);
}

void unsafeAndThrowingOutputsAreRecordedAsFailures()
{
    SimulationClock clock(1000);
    SimulationRejectOutput validatingOutput(clock);
    RejectCommand invalidCommand;
    const RejectExecutionResult invalidExecution = validatingOutput.execute(invalidCommand);
    CHECK(invalidExecution.status == RejectExecutionStatus::Failed);
    CHECK(invalidExecution.errorCode == "INVALID_REJECT_COMMAND");

    const FramePacket frame = makeFrame(2, "camera", 22);
    const InspectionResult result = makeNgResult(frame.frameId);

    ExecutedOutput unsafe;
    SimulationRejectObserver unsafeObserver(clock, unsafe);
    unsafeObserver.onResult(frame, result, InspectionStatistics());
    CHECK(unsafeObserver.statistics().commands == 1);
    CHECK(unsafeObserver.statistics().simulated == 0);
    CHECK(unsafeObserver.statistics().failed == 1);
    CHECK(unsafeObserver.traces()[0].errorCode == "SIMULATION_OUTPUT_NOT_SIMULATED");
    CHECK(unsafeObserver.traces()[0].execution.errorCode ==
        "SIMULATION_OUTPUT_NOT_SIMULATED");
    CHECK(validateSimulationTrace(unsafeObserver.traces(), unsafeObserver.statistics()));

    ThrowingOutput throwing;
    SimulationRejectObserver throwingObserver(clock, throwing);
    throwingObserver.onResult(frame, result, InspectionStatistics());
    CHECK(throwingObserver.statistics().failed == 1);
    CHECK(throwingObserver.traces()[0].errorCode == "SIMULATION_OUTPUT_EXCEPTION");
    CHECK(throwingObserver.traces()[0].execution.errorCode == "SIMULATION_OUTPUT_EXCEPTION");
    CHECK(validateSimulationTrace(throwingObserver.traces(), throwingObserver.statistics()));

    InspectionResult mismatched = result;
    mismatched.frameId = 3;
    SimulationRejectObserver mismatchObserver(clock, unsafe);
    mismatchObserver.onResult(frame, mismatched, InspectionStatistics());
    CHECK(mismatchObserver.statistics().commands == 0);
    CHECK(mismatchObserver.statistics().failed == 1);
    CHECK(mismatchObserver.traces()[0].errorCode == "SIMULATION_FRAME_ID_MISMATCH");
    CHECK(validateSimulationTrace(mismatchObserver.traces(), mismatchObserver.statistics()));
}

void invalidCigaretteAndScheduleOverflowNeverCreateCommands()
{
    SimulationClock clock(1000);
    SimulationRejectOutput output(clock);
    SimulationRejectObserver observer(clock, output, { 10, "simulation-output" });
    FramePacket invalidNumber = makeFrame(2, "camera", 0);
    observer.onResult(invalidNumber, makeNgResult(2), InspectionStatistics());
    CHECK(observer.statistics().commands == 0);
    CHECK(observer.statistics().failed == 1);
    CHECK(observer.traces()[0].errorCode == "INVALID_CIGARETTE_NUMBER");

    clock.set((std::numeric_limits<TimestampMicros>::max)() - 2);
    FramePacket overflowFrame = makeFrame(4, "camera", 4);
    observer.onResult(overflowFrame, makeNgResult(4), InspectionStatistics());
    CHECK(observer.statistics().commands == 0);
    CHECK(observer.statistics().failed == 2);
    CHECK(observer.traces()[1].errorCode == "INVALID_REJECT_TIME");
}

void duplicateFrameIdsRemainObservableInSessionStatistics()
{
    RecordingPacer pacer;
    ReplayFrameSource source({
        { makeFrame(1, "camera", 1), 0 },
        { makeFrame(1, "camera", 2), 0 },
    }, pacer);
    DeterministicFixtureDetector detector;
    MemorySink sink;
    MemoryArchive archive;
    SimulationClock clock(1000);
    OfflineInspectionSession session;
    const OfflineRunSummary summary = session.run(source, detector, sink, archive, clock,
        nullptr);
    CHECK(summary.state == OfflineRunState::CompletedWithErrors);
    CHECK(summary.statistics.sourceErrors == 1);
    CHECK(summary.statistics.processed == 1);
    CHECK(summary.statistics.isConsistent());
}

void batchCliParsesSimulationAndLegacyModes()
{
    BatchCommandLine parsed;
    std::string error;
    CHECK(parseBatchCommandLine({
        "CigVision.exe", "--simulation-batch-manifest", "manifest.json",
        "--simulation-output", "trace-out", "--simulation-reject-delay-micros", "250",
        "--simulation-queue-capacity", "8", "--simulation-target-output", "  output-2  "
    }, parsed, error));
    CHECK(parsed.mode == BatchCommandMode::Simulation);
    CHECK(parsed.manifestPath == "manifest.json");
    CHECK(parsed.outputDirectory == "trace-out");
    CHECK(parsed.simulationRejectDelayMicros == 250);
    CHECK(parsed.simulationQueueCapacity == 8);
    CHECK(parsed.simulationTargetOutput == "output-2");

    CHECK(parseBatchCommandLine({
        "CigVision.exe", "--offline-batch-manifest", "fixture.json",
        "--offline-output", "fixture-out"
    }, parsed, error));
    CHECK(parsed.mode == BatchCommandMode::OfflineFixture);
    CHECK(parseBatchCommandLine({
        "CigVision.exe", "--tensorrt-batch-manifest", "trt.json",
        "--offline-output", "trt-out", "--detector-config", "detector.json"
    }, parsed, error));
    CHECK(parsed.mode == BatchCommandMode::TensorRt);
    CHECK(parseBatchCommandLine({ "CigVision.exe", "--offline" }, parsed, error));
    CHECK(parsed.mode == BatchCommandMode::None);
}

void batchCliRejectsConflictsAndInvalidValues()
{
    const auto rejects = [](const std::vector<std::string>& arguments) {
        BatchCommandLine parsed;
        std::string error;
        return !parseBatchCommandLine(arguments, parsed, error) && !error.empty();
    };
    CHECK(rejects({
        "CigVision.exe", "--offline-batch-manifest", "fixture.json",
        "--offline-output", "fixture-out", "--simulation-batch-manifest", "sim.json",
        "--simulation-output", "sim-out"
    }));
    CHECK(rejects({
        "CigVision.exe", "--simulation-batch-manifest", "sim.json"
    }));
    CHECK(rejects({
        "CigVision.exe", "--simulation-batch-manifest", "sim.json",
        "--simulation-output", "sim-out", "--simulation-reject-delay-micros", "-1"
    }));
    CHECK(rejects({
        "CigVision.exe", "--simulation-batch-manifest", "sim.json",
        "--simulation-output", "sim-out", "--simulation-queue-capacity", "0"
    }));
    CHECK(rejects({
        "CigVision.exe", "--simulation-batch-manifest", "sim.json",
        "--simulation-output", "sim-out", "--simulation-queue-capacity", "65537"
    }));
    CHECK(rejects({
        "CigVision.exe", "--simulation-batch-manifest", "sim.json",
        "--simulation-output", "sim-out", "--simulation-target-output", "   "
    }));
    CHECK(rejects({
        "CigVision.exe", "--simulation-batch-manifest", "sim.json",
        "--simulation-output", "sim-out", "--simulation-reject-delay-micros",
        "9223372036854775808"
    }));
    CHECK(rejects({
        "CigVision.exe", "--simulation-batch-manifest", "sim.json",
        "--simulation-output", "sim-out", "--simulation-reject-delay-micros", "60000001"
    }));
    CHECK(rejects({
        "CigVision.exe", "--offline-batch-manifest", "fixture.json",
        "--offline-output", "fixture-out", "--offline-output", "other-out"
    }));
    CHECK(rejects({
        "CigVision.exe", "--simulation-output", "sim-out"
    }));
}

RealtimeInputFrame loadEvent(std::uint64_t frameId, const std::string& camera,
    std::uint32_t cigaretteNumber, TimestampMicros arrivalAt,
    TimestampMicros processingMicros, bool ng = false)
{
    RealtimeInputFrame event;
    event.frame = makeFrame(frameId, camera, cigaretteNumber);
    event.frame.capturedAt = arrivalAt - 1;
    event.arrivalAt = arrivalAt;
    event.processingMicros = processingMicros;
    event.ngCandidate = ng;
    return event;
}

void multiCameraOrderingAndAccounting()
{
    const std::vector<RealtimeInputFrame> input = {
        loadEvent(1, "camera-a", 1, 100, 80),
        loadEvent(2, "camera-b", 1, 100, 80, true),
        loadEvent(4, "camera-a", 3, 110, 80),
        loadEvent(3, "camera-a", 2, 105, 80),
        loadEvent(4, "camera-a", 4, 120, 80),
    };
    RealtimeSimulationOptions options;
    options.queueCapacity = 8;
    options.rejectDelayMicros = 20;
    RealtimeLoadSimulator simulator;
    const RealtimeSimulationSummary summary = simulator.run(input, options);

    CHECK(summary.state == RealtimeSimulationRunState::CompletedWithErrors);
    CHECK(summary.statistics.offered == 5);
    CHECK(summary.statistics.accepted == 4);
    CHECK(summary.statistics.processed == 4);
    CHECK(summary.statistics.duplicateFrameIds == 1);
    CHECK(summary.statistics.arrivalOutOfOrder == 1);
    CHECK(summary.statistics.frameIdGaps == 1);
    CHECK(summary.statistics.frameIdOutOfOrder == 1);
    CHECK(summary.statistics.cigaretteGaps == 1);
    CHECK(summary.statistics.cigaretteOutOfOrder == 1);
    CHECK(summary.traces[2].sequenceFlags.size() == 2);
    CHECK(summary.traces[2].sequenceFlags[0] == "FRAME_ID_GAP");
    CHECK(summary.traces[2].sequenceFlags[1] == "CIGARETTE_GAP");
    CHECK(summary.traces[3].sequenceFlags.size() == 3);
    CHECK(summary.traces[3].sequenceFlags[0] == "ARRIVAL_OUT_OF_ORDER");
    CHECK(summary.traces[3].sequenceFlags[1] == "FRAME_ID_OUT_OF_ORDER");
    CHECK(summary.traces[3].sequenceFlags[2] == "CIGARETTE_OUT_OF_ORDER");
    CHECK(summary.statistics.ngCandidates == 1);
    CHECK(summary.statistics.commands == 1);
    CHECK(summary.statistics.simulatedRejects == 1);
    CHECK(summary.traces[1].command.mode == RejectMode::Simulation);
    CHECK(summary.traces[1].command.scheduledAt == 280);
    CHECK(summary.statistics.cameras.at("station-1/camera-a").processed == 3);
    CHECK(summary.statistics.cameras.at("station-1/camera-b").processed == 1);
    CHECK(validateRealtimeSimulationSummary(summary));
}

std::vector<RealtimeInputFrame> makeCapacityInput(std::size_t count)
{
    std::vector<RealtimeInputFrame> input;
    input.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint64_t frameId = static_cast<std::uint64_t>(index + 1);
        const std::string camera = (index % 2U) == 0U ? "camera-a" : "camera-b";
        input.push_back(loadEvent(frameId, camera,
            static_cast<std::uint32_t>(index / 2U + 1U),
            100 + static_cast<TimestampMicros>(index * 10U), 50,
            (index % 3U) == 0U));
    }
    return input;
}

void capacityMatrixAndLatency()
{
    const std::vector<RealtimeInputFrame> input = makeCapacityInput(20);
    RealtimeLoadSimulator simulator;
    struct Expectation {
        std::size_t capacity;
        std::uint64_t dropped;
        std::uint64_t processed;
        std::uint64_t maxQueueDepth;
        TimestampMicros p95QueueWait;
        TimestampMicros p95EndToEnd;
    };
    const std::vector<Expectation> expectations = {
        { 1U, 15U, 5U, 1U, 50, 101 },
        { 4U, 12U, 8U, 4U, 200, 251 },
        { 32U, 0U, 20U, 16U, 720, 771 },
    };
    std::uint64_t previousDrops = (std::numeric_limits<std::uint64_t>::max)();
    for (const Expectation& expected : expectations) {
        RealtimeSimulationOptions options;
        options.queueCapacity = expected.capacity;
        options.defaultProcessingMicros = 50;
        const RealtimeSimulationSummary summary = simulator.run(input, options);
        CHECK(summary.statistics.dropped == expected.dropped);
        CHECK(summary.statistics.processed == expected.processed);
        CHECK(summary.statistics.maxQueueDepth == expected.maxQueueDepth);
        CHECK(summary.statistics.p95QueueWaitMicros == expected.p95QueueWait);
        CHECK(summary.statistics.p95EndToEndMicros == expected.p95EndToEnd);
        CHECK(summary.statistics.isConsistent(summary.traces.size()));
        CHECK(validateRealtimeSimulationSummary(summary));
        CHECK(summary.statistics.dropped <= previousDrops);
        previousDrops = summary.statistics.dropped;
    }

    RealtimeSimulationOptions dropOldest;
    dropOldest.queueCapacity = 2;
    dropOldest.overflowPolicy = QueueOverflowPolicy::DropOldest;
    const RealtimeSimulationSummary oldest = simulator.run(input, dropOldest);
    CHECK(oldest.statistics.dropped == 14);
    CHECK(oldest.statistics.processed == 6);
    CHECK(oldest.statistics.p95QueueWaitMicros == 60);
    CHECK(oldest.statistics.p95EndToEndMicros == 111);
    CHECK(oldest.statistics.dropped > 0);
    CHECK(oldest.state == RealtimeSimulationRunState::CompletedWithErrors);
    bool sawOldest = false;
    for (const RealtimeFrameTrace& trace : oldest.traces) {
        if (trace.disposition == RealtimeFrameDisposition::DroppedOldest) {
            sawOldest = true;
            break;
        }
    }
    CHECK(sawOldest);
    CHECK(validateRealtimeSimulationSummary(oldest));
}

void virtualStopAndRestart()
{
    const std::vector<RealtimeInputFrame> input = makeCapacityInput(20);
    RealtimeLoadSimulator simulator;
    RealtimeSimulationOptions stopOptions;
    stopOptions.queueCapacity = 4;
    stopOptions.defaultProcessingMicros = 100;
    stopOptions.stopAtMicros = 250;
    stopOptions.drainOnStop = false;
    const RealtimeSimulationSummary stopped = simulator.run(input, stopOptions);
    CHECK(stopped.state == RealtimeSimulationRunState::Stopped);
    CHECK(stopped.statistics.afterStop > 0);
    CHECK(stopped.statistics.cancelled > 0);
    CHECK(validateRealtimeSimulationSummary(stopped));

    RealtimeSimulationOptions drainOptions = stopOptions;
    drainOptions.drainOnStop = true;
    const RealtimeSimulationSummary drained = simulator.run(input, drainOptions);
    CHECK(drained.state == RealtimeSimulationRunState::Stopped);
    CHECK(drained.statistics.cancelled == 0);
    CHECK(drained.statistics.processed > stopped.statistics.processed);
    CHECK(validateRealtimeSimulationSummary(drained));

    const std::vector<RealtimeInputFrame> restartInput = {
        loadEvent(1, "camera-a", 1, 10, 5),
        loadEvent(2, "camera-a", 2, 20, 5, true),
    };
    RealtimeSimulationOptions restartOptions;
    restartOptions.initialClockMicros = 1;
    const RealtimeSimulationSummary restarted = simulator.run(restartInput, restartOptions);
    CHECK(restarted.state == RealtimeSimulationRunState::Completed);
    CHECK(restarted.statistics.processed == 2);
    CHECK(validateRealtimeSimulationSummary(restarted));
}

void invalidAndRejectSafety()
{
    RealtimeInputFrame invalid = loadEvent(1, "camera-a", 1, 100, 5);
    invalid.frame.frameId = 0;
    RealtimeInputFrame futureCapture = loadEvent(4, "camera-a", 4, 130, 5);
    futureCapture.frame.capturedAt = 131;
    RealtimeInputFrame zeroNumber = loadEvent(2, "camera-a", 0, 110, 5, true);
    RealtimeInputFrame hugeProcessing = loadEvent(3, "camera-a", 3, 120,
        (std::numeric_limits<TimestampMicros>::max)());
    const std::vector<RealtimeInputFrame> input = {
        invalid, zeroNumber, hugeProcessing, futureCapture
    };
    RealtimeSimulationOptions options;
    options.defaultProcessingMicros = 5;
    const RealtimeSimulationSummary summary = RealtimeLoadSimulator().run(input, options);
    CHECK(summary.state == RealtimeSimulationRunState::CompletedWithErrors);
    CHECK(summary.statistics.invalid == 2);
    CHECK(summary.statistics.processingFailures == 1);
    CHECK(summary.statistics.rejectFailures == 1);
    CHECK(summary.statistics.commands == 0);
    CHECK(validateRealtimeSimulationSummary(summary));

    RealtimeSimulationOptions badOptions;
    badOptions.queueCapacity = 0;
    const RealtimeSimulationSummary rejected = RealtimeLoadSimulator().run({}, badOptions);
    CHECK(rejected.state == RealtimeSimulationRunState::Failed);
    CHECK(!rejected.issues.empty());

    badOptions.queueCapacity = 65537;
    CHECK(RealtimeLoadSimulator().run({}, badOptions).state ==
        RealtimeSimulationRunState::Failed);
    badOptions.queueCapacity = 4;
    badOptions.targetOutput = "   ";
    CHECK(RealtimeLoadSimulator().run({}, badOptions).state ==
        RealtimeSimulationRunState::Failed);
}

void realtimeTraceValidatorRejectsUnsafeMutation()
{
    const std::vector<RealtimeInputFrame> input = {
        loadEvent(1, "camera-a", 1, 100, 10, true),
    };
    RealtimeSimulationSummary summary = RealtimeLoadSimulator().run(input);
    CHECK(validateRealtimeSimulationSummary(summary));

    RealtimeSimulationSummary realMode = summary;
    realMode.traces[0].command.mode = RejectMode::Real;
    CHECK(!validateRealtimeSimulationSummary(realMode));

    RealtimeSimulationSummary wrongNumber = summary;
    ++wrongNumber.traces[0].command.cigaretteNumber;
    CHECK(!validateRealtimeSimulationSummary(wrongNumber));

    RealtimeSimulationSummary earlySchedule = summary;
    earlySchedule.traces[0].command.scheduledAt = earlySchedule.traces[0].completedAt - 1;
    CHECK(!validateRealtimeSimulationSummary(earlySchedule));

    RealtimeSimulationSummary wrongReceipt = summary;
    ++wrongReceipt.traces[0].execution.frameId;
    CHECK(!validateRealtimeSimulationSummary(wrongReceipt));

    RealtimeSimulationSummary unfinished = summary;
    unfinished.traces[0].disposition = RealtimeFrameDisposition::Queued;
    CHECK(!validateRealtimeSimulationSummary(unfinished));

    RealtimeSimulationSummary mismatched = summary;
    ++mismatched.statistics.processed;
    CHECK(!validateRealtimeSimulationSummary(mismatched));

    RealtimeSimulationSummary latencyMutation = summary;
    ++latencyMutation.statistics.p95QueueWaitMicros;
    CHECK(!validateRealtimeSimulationSummary(latencyMutation));

    const std::vector<RealtimeInputFrame> twoCameraInput = {
        loadEvent(1, "camera-a", 1, 100, 10, true),
        loadEvent(2, "camera-b", 1, 110, 10, false),
    };
    RealtimeSimulationSummary cameraMutation = RealtimeLoadSimulator().run(twoCameraInput);
    CHECK(validateRealtimeSimulationSummary(cameraMutation));
    ++cameraMutation.statistics.cameras.at("station-1/camera-a").processed;
    --cameraMutation.statistics.cameras.at("station-1/camera-b").processed;
    CHECK(!validateRealtimeSimulationSummary(cameraMutation));
}

} // namespace

int main()
{
    int passed = 0;
    try {
        replayPreservesMetadataAndPacing();
        std::cout << "PASS replay preserves metadata and pacing\n";
        ++passed;
        replayRejectsInvalidEntries();
        std::cout << "PASS replay rejects invalid entries\n";
        ++passed;
        replayStopCancelsPendingCadence();
        std::cout << "PASS replay stop cancels pending cadence\n";
        ++passed;
        sessionStopCancelsBlockedReplaySource();
        std::cout << "PASS session stop cancels blocked replay source\n";
        ++passed;
        endToEndSimulationProducesOnlySimulatedRejects();
        std::cout << "PASS end-to-end simulation produces only simulated rejects\n";
        ++passed;
        unsafeAndThrowingOutputsAreRecordedAsFailures();
        std::cout << "PASS unsafe and throwing outputs are recorded as failures\n";
        ++passed;
        invalidCigaretteAndScheduleOverflowNeverCreateCommands();
        std::cout << "PASS invalid cigarette and schedule overflow never create commands\n";
        ++passed;
        duplicateFrameIdsRemainObservableInSessionStatistics();
        std::cout << "PASS duplicate frame IDs remain observable\n";
        ++passed;
        batchCliParsesSimulationAndLegacyModes();
        std::cout << "PASS batch CLI parses simulation and legacy modes\n";
        ++passed;
        batchCliRejectsConflictsAndInvalidValues();
        std::cout << "PASS batch CLI rejects conflicts and invalid values\n";
        ++passed;
        multiCameraOrderingAndAccounting();
        std::cout << "PASS multi-camera ordering and accounting\n";
        ++passed;
        capacityMatrixAndLatency();
        std::cout << "PASS capacity matrix and latency\n";
        ++passed;
        virtualStopAndRestart();
        std::cout << "PASS virtual stop and restart\n";
        ++passed;
        invalidAndRejectSafety();
        std::cout << "PASS invalid and reject safety\n";
        ++passed;
        realtimeTraceValidatorRejectsUnsafeMutation();
        std::cout << "PASS realtime trace validator rejects unsafe mutation\n";
        ++passed;
        std::cout << "PASS all simulation tests (" << passed << ")\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
