#include "core/BoundedQueue.h"
#include "core/InspectionInterfaces.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace cigvision;

#define CHECK_TRUE(expression) checkTrue((expression), #expression, __FILE__, __LINE__)

void checkTrue(bool condition, const char* expression, const char* file, int line)
{
    if (!condition) {
        throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
            " check failed: " + expression);
    }
}

FramePacket makeFrame()
{
    FramePacket frame;
    frame.frameId = 42;
    frame.stationId = "component-1";
    frame.cameraId = "camera-1";
    frame.cigaretteNumber = 7;
    frame.capturedAt = 1'720'000'000'000'000;
    frame.width = 2;
    frame.height = 2;
    frame.strideBytes = 2;
    frame.pixelFormat = PixelFormat::Mono8;
    frame.pixels = { 1, 2, 3, 4 };
    return frame;
}

Detection makeDetection()
{
    Detection detection;
    detection.classId = 3;
    detection.className = "filter-damage";
    detection.confidence = 0.92F;
    detection.box = { 1.0F, 2.0F, 10.0F, 12.0F };
    detection.detectorVersion = "fake-detector-v1";
    return detection;
}

class FakeFrameSource final : public IFrameSource {
public:
    explicit FakeFrameSource(FramePacket frame) : frame_(std::move(frame)) {}

    bool start(std::string& errorMessage) override
    {
        errorMessage.clear();
        running_ = true;
        return true;
    }

    void stop() noexcept override
    {
        running_ = false;
    }

    bool tryRead(FramePacket& frame, std::string& errorMessage) override
    {
        if (!running_) {
            errorMessage = "source is stopped";
            return false;
        }
        errorMessage.clear();
        frame = frame_;
        return true;
    }

private:
    FramePacket frame_;
    bool running_ = false;
};

class FakeDetector final : public IDetector {
public:
    DetectionBatch detect(const FramePacket& frame) override
    {
        DetectionBatch batch;
        batch.frameId = frame.frameId;
        batch.detections.push_back(makeDetection());
        batch.elapsedMicros = 120;
        batch.detectorVersion = "fake-detector-v1";
        return batch;
    }
};

class FakeResultSink final : public IInspectionResultSink {
public:
    bool store(const InspectionResult& result, std::string& errorMessage) override
    {
        errorMessage.clear();
        stored_ = result;
        storedAny_ = true;
        return true;
    }

    bool storedAny() const { return storedAny_; }
    const InspectionResult& stored() const { return stored_; }

private:
    InspectionResult stored_;
    bool storedAny_ = false;
};

class FakeRejectOutput final : public IRejectOutput {
public:
    RejectExecutionResult execute(const RejectCommand& command) override
    {
        RejectExecutionResult result;
        result.frameId = command.frameId;
        result.status = command.mode == RejectMode::Simulation
            ? RejectExecutionStatus::Simulated
            : RejectExecutionStatus::Failed;
        result.completedAt = command.scheduledAt;
        if (command.mode == RejectMode::Real) {
            result.errorCode = "REAL_OUTPUT_DISABLED_IN_CONTRACT_TEST";
        }
        return result;
    }
};

class FakeClock final : public IClock {
public:
    TimestampMicros now() const noexcept override { return 1'720'000'000'000'500; }
};

void testFramePacketOwnsPixelsAndValidatesShape()
{
    FramePacket original = makeFrame();
    FramePacket copy = original;
    original.pixels[0] = 99;

    CHECK_TRUE(copy.pixels[0] == 1);
    CHECK_TRUE(copy.validate());

    copy.strideBytes = 1;
    std::string reason;
    CHECK_TRUE(!copy.validate(&reason));
    CHECK_TRUE(!reason.empty());
}

void testDetectionAndInspectionResultContracts()
{
    const Detection detection = makeDetection();
    CHECK_TRUE(detection.isValid());

    InspectionResult result;
    result.frameId = 42;
    result.decision = InspectionDecision::Ng;
    result.defects.push_back(detection);
    result.elapsedMicros = 250;
    result.parameterVersion = "brand-a-v1";
    CHECK_TRUE(result.validate());

    result.decision = InspectionDecision::Ok;
    CHECK_TRUE(!result.validate());

    result.defects.clear();
    result.decision = static_cast<InspectionDecision>(99);
    CHECK_TRUE(!result.validate());
}

void testInterfacesAreReplaceableWithoutSdkDependencies()
{
    FakeFrameSource source(makeFrame());
    FakeDetector detector;
    FakeResultSink sink;
    FakeRejectOutput rejectOutput;
    FakeClock clock;

    std::string errorMessage;
    CHECK_TRUE(source.start(errorMessage));

    FramePacket frame;
    CHECK_TRUE(source.tryRead(frame, errorMessage));
    const DetectionBatch batch = detector.detect(frame);
    CHECK_TRUE(batch.succeeded());
    CHECK_TRUE(batch.frameId == frame.frameId);
    CHECK_TRUE(batch.detections.size() == 1);

    InspectionResult result;
    result.frameId = batch.frameId;
    result.decision = InspectionDecision::Ng;
    result.defects = batch.detections;
    result.elapsedMicros = batch.elapsedMicros;
    result.parameterVersion = "brand-a-v1";
    CHECK_TRUE(result.validate());
    CHECK_TRUE(sink.store(result, errorMessage));
    CHECK_TRUE(sink.storedAny());
    CHECK_TRUE(sink.stored().frameId == frame.frameId);

    RejectCommand command;
    command.frameId = result.frameId;
    command.cigaretteNumber = frame.cigaretteNumber;
    command.targetOutput = "simulated-reject-1";
    command.scheduledAt = clock.now();
    CHECK_TRUE(command.mode == RejectMode::Simulation);
    CHECK_TRUE(command.validate());

    RejectCommand invalidCommand = command;
    invalidCommand.mode = static_cast<RejectMode>(99);
    CHECK_TRUE(!invalidCommand.validate());

    const RejectExecutionResult execution = rejectOutput.execute(command);
    CHECK_TRUE(execution.frameId == frame.frameId);
    CHECK_TRUE(execution.status == RejectExecutionStatus::Simulated);
    source.stop();
}

void testBoundedQueueMakesOverflowAndShutdownObservable()
{
    BoundedQueue<int> queue(2, QueueOverflowPolicy::DropOldest);
    CHECK_TRUE(queue.tryPush(1) == QueuePushResult::Pushed);
    CHECK_TRUE(queue.tryPush(2) == QueuePushResult::Pushed);
    CHECK_TRUE(queue.tryPush(3) == QueuePushResult::DroppedOldest);
    CHECK_TRUE(queue.droppedCount() == 1);

    int value = 0;
    CHECK_TRUE(queue.tryPop(value));
    CHECK_TRUE(value == 2);
    CHECK_TRUE(queue.tryPop(value));
    CHECK_TRUE(value == 3);
    CHECK_TRUE(!queue.tryPop(value));

    queue.close();
    CHECK_TRUE(queue.isClosed());
    CHECK_TRUE(queue.tryPush(4) == QueuePushResult::RejectedClosed);
    CHECK_TRUE(!queue.waitPop(value));
}

void testBoundedQueuePreservesRejectedMoveOwnership()
{
    BoundedQueue<std::unique_ptr<int>> queue(1, QueueOverflowPolicy::RejectNewest);
    std::unique_ptr<int> first(new int(1));
    CHECK_TRUE(queue.tryPush(std::move(first)) == QueuePushResult::Pushed);
    CHECK_TRUE(first.get() == nullptr);

    std::unique_ptr<int> rejected(new int(2));
    CHECK_TRUE(queue.tryPush(std::move(rejected)) == QueuePushResult::RejectedFull);
    CHECK_TRUE(rejected.get() != nullptr);
    CHECK_TRUE(*rejected == 2);

    std::unique_ptr<int> stored;
    CHECK_TRUE(queue.tryPop(stored));
    CHECK_TRUE(stored.get() != nullptr && *stored == 1);

    queue.close();
    CHECK_TRUE(queue.tryPush(std::move(rejected)) == QueuePushResult::RejectedClosed);
    CHECK_TRUE(rejected.get() != nullptr && *rejected == 2);
}

void testBoundedQueueWakesWaitersAndDrainsAfterClose()
{
    BoundedQueue<int> emptyQueue(2);
    std::atomic<int> waiting(0);
    std::atomic<int> stopped(0);
    std::vector<std::thread> waiters;
    for (int index = 0; index < 2; ++index) {
        waiters.emplace_back([&emptyQueue, &waiting, &stopped] {
            ++waiting;
            int value = 0;
            if (!emptyQueue.waitPop(value)) {
                ++stopped;
            }
        });
    }
    while (waiting.load() != 2) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    emptyQueue.close();
    for (std::thread& waiter : waiters) {
        waiter.join();
    }
    CHECK_TRUE(stopped.load() == 2);

    BoundedQueue<int> drainingQueue(2);
    CHECK_TRUE(drainingQueue.tryPush(7) == QueuePushResult::Pushed);
    drainingQueue.close();
    int value = 0;
    CHECK_TRUE(drainingQueue.waitPop(value));
    CHECK_TRUE(value == 7);
    CHECK_TRUE(!drainingQueue.waitPop(value));
}

void testBoundedQueueConcurrentProducerConsumer()
{
    const int itemCount = 1000;
    BoundedQueue<int> queue(32, QueueOverflowPolicy::RejectNewest);
    std::atomic<int> consumed(0);
    std::atomic<long long> sum(0);

    std::thread consumer([&queue, &consumed, &sum] {
        int value = 0;
        while (queue.waitPop(value)) {
            ++consumed;
            sum += value;
        }
    });

    for (int value = 1; value <= itemCount; ++value) {
        while (queue.tryPush(value) == QueuePushResult::RejectedFull) {
            std::this_thread::yield();
        }
    }
    queue.close();
    consumer.join();

    CHECK_TRUE(consumed.load() == itemCount);
    CHECK_TRUE(sum.load() == static_cast<long long>(itemCount) * (itemCount + 1) / 2);
}

void runTest(const char* name, const std::function<void()>& test, int& passed)
{
    test();
    ++passed;
    std::cout << "PASS " << name << '\n';
}

} // namespace

int main()
{
    int passed = 0;
    try {
        runTest("FramePacket owns pixels and validates shape",
            testFramePacketOwnsPixelsAndValidatesShape, passed);
        runTest("Detection and InspectionResult contracts",
            testDetectionAndInspectionResultContracts, passed);
        runTest("Interfaces are replaceable without SDK dependencies",
            testInterfacesAreReplaceableWithoutSdkDependencies, passed);
        runTest("BoundedQueue overflow and shutdown",
            testBoundedQueueMakesOverflowAndShutdownObservable, passed);
        runTest("BoundedQueue preserves rejected move ownership",
            testBoundedQueuePreservesRejectedMoveOwnership, passed);
        runTest("BoundedQueue wakes waiters and drains after close",
            testBoundedQueueWakesWaitersAndDrainsAfterClose, passed);
        runTest("BoundedQueue concurrent producer and consumer",
            testBoundedQueueConcurrentProducerConsumer, passed);
    }
    catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }

    std::cout << "PASS all contract tests (" << passed << ")\n";
    return 0;
}
