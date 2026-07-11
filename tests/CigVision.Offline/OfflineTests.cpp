#include "core/OfflineInspection.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace cigvision;

#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)

FramePacket frame(std::uint64_t id)
{
    FramePacket value;
    value.frameId = id;
    value.stationId = "offline";
    value.cameraId = "fixture";
    value.cigaretteNumber = static_cast<std::uint32_t>(id);
    value.capturedAt = 1000 + static_cast<TimestampMicros>(id);
    value.width = 2; value.height = 2; value.strideBytes = 2;
    value.pixelFormat = PixelFormat::Mono8;
    value.pixels = { 1, 2, 3, 4 };
    return value;
}

class Source final : public IFrameSource {
public:
    explicit Source(int count, bool firstError = false) : count_(count), firstError_(firstError) {}
    bool start(std::string& error) override { index_ = 0; errorEmitted_ = false; running_ = true; error.clear(); return true; }
    void stop() noexcept override { running_ = false; }
    bool tryRead(FramePacket& value, std::string& error) override
    {
        error.clear();
        if (!running_) return false;
        if (firstError_ && !errorEmitted_) { errorEmitted_ = true; error = "damaged image"; return false; }
        if (index_ >= count_) return false;
        value = frame(static_cast<std::uint64_t>(++index_));
        return true;
    }
private:
    int count_; bool firstError_; int index_ = 0; bool errorEmitted_ = false; bool running_ = false;
};

class Sink final : public IInspectionResultSink {
public:
    explicit Sink(bool fail = false) : fail_(fail) {}
    bool store(const InspectionResult& result, std::string& error) override
    { results.push_back(result); if (fail_) { error = "disk full"; return false; } return true; }
    bool fail_; std::vector<InspectionResult> results;
};

class Archive final : public IFrameArchive {
public:
    explicit Archive(bool fail = false) : fail_(fail) {}
    bool store(const FramePacket&, const InspectionResult&, std::string& error) override
    { if (fail_) { error = "archive denied"; return false; } ++stored; return true; }
    bool fail_; int stored = 0;
};

class SlowDetector final : public IDetector {
public:
    explicit SlowDetector(int delayMs = 0, bool invalid = false)
        : delayMs_(delayMs), invalid_(invalid) {}
    DetectionBatch detect(const FramePacket& input) override
    {
        if (delayMs_) std::this_thread::sleep_for(std::chrono::milliseconds(delayMs_));
        DetectionBatch batch;
        batch.frameId = invalid_ ? input.frameId + 1 : input.frameId;
        batch.detectorVersion = "test";
        return batch;
    }
private:
    int delayMs_; bool invalid_;
};

class Clock final : public IClock { public: TimestampMicros now() const noexcept override { return 1; } };

class ThrowingSource final : public IFrameSource {
public:
    bool start(std::string& error) override { error.clear(); return true; }
    void stop() noexcept override {}
    bool tryRead(FramePacket&, std::string&) override { throw std::runtime_error("source boom"); }
};

class ThrowingStartSource final : public IFrameSource {
public:
    bool start(std::string&) override { throw std::runtime_error("start boom"); }
    void stop() noexcept override {}
    bool tryRead(FramePacket&, std::string&) override { return false; }
};

class ThrowingSink final : public IInspectionResultSink {
public:
    bool store(const InspectionResult&, std::string&) override { throw std::runtime_error("sink boom"); }
};

class ThrowingArchive final : public IFrameArchive {
public:
    bool store(const FramePacket&, const InspectionResult&, std::string&) override
    { throw std::runtime_error("archive boom"); }
};

class ThrowingObserver final : public IInspectionObserver {
public:
    void onResult(const FramePacket&, const InspectionResult&, const InspectionStatistics&) override
    { throw std::runtime_error("observer boom"); }
};

OfflineRunSummary run(IFrameSource& source, IDetector& detector,
    IInspectionResultSink& sink, IFrameArchive& archive,
    OfflineInspectionSession& session, OfflineRunOptions options = OfflineRunOptions(),
    IInspectionObserver* observer = nullptr)
{
    Clock clock;
    return session.run(source, detector, sink, archive, clock, observer, options);
}

void normalFlow()
{
    Source source(4); DeterministicFixtureDetector detector; Sink sink; Archive archive;
    OfflineInspectionSession session; const auto summary = run(source, detector, sink, archive, session);
    CHECK(summary.state == OfflineRunState::Completed); CHECK(summary.statistics.processed == 4);
    CHECK(summary.statistics.ok == 2); CHECK(summary.statistics.ng == 2); CHECK(archive.stored == 4);
}

void emptyInput()
{
    Source source(0); DeterministicFixtureDetector detector; Sink sink; Archive archive;
    OfflineInspectionSession session; const auto summary = run(source, detector, sink, archive, session);
    CHECK(summary.state == OfflineRunState::Completed); CHECK(summary.statistics.received == 0);
    CHECK(summary.statistics.processed == 0); CHECK(summary.statistics.isConsistent());
    CHECK(sink.results.empty()); CHECK(archive.stored == 0);
}

void inputAndDetectorErrors()
{
    Source source(1, true); SlowDetector detector(0, true); Sink sink; Archive archive;
    OfflineInspectionSession session; const auto summary = run(source, detector, sink, archive, session);
    CHECK(summary.state == OfflineRunState::CompletedWithErrors);
    CHECK(summary.statistics.sourceErrors == 1); CHECK(summary.statistics.error == 1);
}

void saveFailure()
{
    Source source(1); DeterministicFixtureDetector detector; Sink sink(true); Archive archive(true);
    OfflineInspectionSession session; const auto summary = run(source, detector, sink, archive, session);
    CHECK(summary.state == OfflineRunState::CompletedWithErrors); CHECK(summary.statistics.saveFailures == 1);
}

void queueFull()
{
    Source source(200); SlowDetector detector(1); Sink sink; Archive archive;
    OfflineRunOptions options; options.queueCapacity = 1;
    options.overflowPolicy = QueueOverflowPolicy::DropOldest;
    OfflineInspectionSession session;
    const auto summary = run(source, detector, sink, archive, session, options);
    CHECK(summary.statistics.dropped > 0); CHECK(summary.statistics.isConsistent());
}

void stopAndRepeat()
{
    OfflineInspectionSession session; Source source(500); SlowDetector detector(1); Sink sink; Archive archive;
    OfflineRunOptions options; options.queueCapacity = 4; options.drainOnStop = false;
    OfflineRunSummary first;
    std::thread running([&] { first = run(source, detector, sink, archive, session, options); });
    while (!session.isRunning()) std::this_thread::yield();
    session.requestStop(); running.join(); CHECK(first.state == OfflineRunState::Stopped);
    CHECK(first.statistics.processed < 500); CHECK(first.statistics.isConsistent());
    Source secondSource(2); DeterministicFixtureDetector secondDetector; Sink secondSink; Archive secondArchive;
    const auto second = run(secondSource, secondDetector, secondSink, secondArchive, session);
    CHECK(second.state == OfflineRunState::Completed); CHECK(second.statistics.processed == 2);
}

void collaboratorExceptions()
{
    DeterministicFixtureDetector detector;
    {
        ThrowingStartSource source; Sink sink; Archive archive; OfflineInspectionSession session;
        const auto summary = run(source, detector, sink, archive, session);
        CHECK(summary.state == OfflineRunState::Failed);
        CHECK(summary.statistics.sourceErrors == 1); CHECK(!session.isRunning());
    }
    {
        ThrowingSource source; Sink sink; Archive archive; OfflineInspectionSession session;
        const auto summary = run(source, detector, sink, archive, session);
        CHECK(summary.state == OfflineRunState::CompletedWithErrors);
        CHECK(summary.statistics.sourceErrors == 1);
    }
    {
        Source source(1); ThrowingSink sink; ThrowingArchive archive; OfflineInspectionSession session;
        const auto summary = run(source, detector, sink, archive, session);
        CHECK(summary.state == OfflineRunState::CompletedWithErrors);
        CHECK(summary.statistics.processed == 1); CHECK(summary.statistics.saveFailures == 1);
    }
    {
        Source source(1); Sink sink; Archive archive; ThrowingObserver observer;
        OfflineInspectionSession session;
        const auto summary = run(source, detector, sink, archive, session,
            OfflineRunOptions(), &observer);
        CHECK(summary.state == OfflineRunState::CompletedWithErrors);
        CHECK(summary.statistics.observerErrors == 1);
    }
}

int main()
{
    try {
        normalFlow(); std::cout << "PASS normal flow\n";
        emptyInput(); std::cout << "PASS empty input\n";
        inputAndDetectorErrors(); std::cout << "PASS input and detector errors\n";
        saveFailure(); std::cout << "PASS save failure\n";
        queueFull(); std::cout << "PASS queue full\n";
        stopAndRepeat(); std::cout << "PASS stop and repeat\n";
        collaboratorExceptions(); std::cout << "PASS collaborator exceptions\n";
        std::cout << "PASS all offline tests (7)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n'; return 1;
    }
}
