#include "core/OfflineInspection.h"
#include "core/ProductRuntimeState.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace cigvision;

namespace {

struct Options {
    std::string outputDirectory;
    double durationSeconds = 0.0;
    std::size_t framesPerSession = 512;
    int restart = 0;
    int round = 0;
    int iteration = 0;
};

std::uint64_t parseUnsigned(const std::string& text, const char* label)
{
    std::size_t consumed = 0;
    const unsigned long long value = std::stoull(text, &consumed, 10);
    if (consumed != text.size()) {
        throw std::invalid_argument(std::string(label) + " must be an integer");
    }
    return static_cast<std::uint64_t>(value);
}

double parsePositiveDouble(const std::string& text, const char* label)
{
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value) || value <= 0.0) {
        throw std::invalid_argument(std::string(label) + " must be finite and positive");
    }
    return value;
}

int parsePositiveInt(const std::string& text, const char* label)
{
    const std::uint64_t value = parseUnsigned(text, label);
    const std::uint64_t maximum = static_cast<std::uint64_t>(
        (std::numeric_limits<int>::max)());
    if (value == 0 || value > maximum) {
        throw std::invalid_argument(std::string(label) + " outside safe bounds");
    }
    return static_cast<int>(value);
}

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string name = argv[index];
        if (index + 1 >= argc) {
            throw std::invalid_argument("missing value for " + name);
        }
        const std::string value = argv[++index];
        if (name == "--output-dir") {
            options.outputDirectory = value;
        } else if (name == "--duration-seconds") {
            options.durationSeconds = parsePositiveDouble(value, "duration");
        } else if (name == "--frames-per-session") {
            const std::uint64_t parsed = parseUnsigned(value, "frames per session");
            if (parsed == 0 || parsed > 100000) {
                throw std::invalid_argument("frames per session outside safe bounds");
            }
            options.framesPerSession = static_cast<std::size_t>(parsed);
        } else if (name == "--restart") {
            options.restart = parsePositiveInt(value, "restart");
        } else if (name == "--round") {
            options.round = parsePositiveInt(value, "round");
        } else if (name == "--iteration") {
            options.iteration = parsePositiveInt(value, "iteration");
        } else {
            throw std::invalid_argument("unknown argument: " + name);
        }
    }
    if (options.outputDirectory.empty() || options.durationSeconds <= 0.0 ||
        options.restart <= 0 || options.round <= 0 || options.iteration <= 0) {
        throw std::invalid_argument("output, duration and run coordinates are required");
    }
    return options;
}

ProductParameterProfile fixtureProfile()
{
    ProductParameterProfile profile;
    profile.kind = ProductParameterProfileKind::DeterministicFixture;
    profile.profileId = "offline-fixture";
    profile.parameterVersion = "offline-fixture-v1";
    profile.detectorVersion = "deterministic-fixture-v1";
    return profile;
}

class SyntheticSource final : public IFrameSource {
public:
    SyntheticSource(std::size_t count, TimestampMicros timeBase)
        : count_(count), timeBase_(timeBase)
    {
    }

    bool start(std::string& errorMessage) override
    {
        index_ = 0;
        running_ = true;
        errorMessage.clear();
        return true;
    }

    void stop() noexcept override
    {
        running_ = false;
    }

    bool tryRead(FramePacket& frame, std::string& errorMessage) override
    {
        if (!running_ || index_ >= count_) {
            errorMessage.clear();
            return false;
        }
        ++index_;
        frame = FramePacket();
        frame.frameId = static_cast<std::uint64_t>(index_);
        frame.stationId = (index_ % 2U == 0U) ? "station-a" : "station-b";
        frame.cameraId = (index_ % 2U == 0U) ? "camera-1" : "camera-2";
        frame.cigaretteNumber = static_cast<std::uint32_t>(index_);
        frame.capturedAt = timeBase_ + static_cast<TimestampMicros>(index_);
        frame.width = 8;
        frame.height = 8;
        frame.strideBytes = 8;
        frame.pixelFormat = PixelFormat::Mono8;
        frame.pixels.assign(64, static_cast<std::uint8_t>(index_ & 0xFFU));
        errorMessage.clear();
        return true;
    }

private:
    std::size_t count_ = 0;
    std::size_t index_ = 0;
    TimestampMicros timeBase_ = 0;
    bool running_ = false;
};

class CountingSink final : public IInspectionResultSink {
public:
    bool store(const InspectionResult& result, std::string& errorMessage) override
    {
        std::string validationError;
        if (!result.validate(&validationError)) {
            errorMessage = validationError;
            return false;
        }
        ++count;
        errorMessage.clear();
        return true;
    }

    std::uint64_t count = 0;
};

class CountingArchive final : public IFrameArchive {
public:
    bool store(const FramePacket& frame, const InspectionResult& result,
        std::string& errorMessage) override
    {
        std::string frameError;
        std::string resultError;
        if (!frame.validate(&frameError) || !result.validate(&resultError) ||
            frame.frameId != result.frameId) {
            errorMessage = frameError + resultError;
            return false;
        }
        ++count;
        errorMessage.clear();
        return true;
    }

    std::uint64_t count = 0;
};

class SyntheticClock final : public IClock {
public:
    explicit SyntheticClock(TimestampMicros initial) : value_(initial) {}

    TimestampMicros now() const noexcept override
    {
        return value_.fetch_add(1);
    }

private:
    mutable std::atomic<TimestampMicros> value_;
};

class ProductObserver final : public IInspectionObserver {
public:
    explicit ProductObserver(ProductRuntimeState& state) : state_(state) {}

    void onResult(const FramePacket& frame, const InspectionResult& result,
        const InspectionStatistics&) override
    {
        ProductFrameResult product;
        product.frameId = frame.frameId;
        product.stationId = frame.stationId;
        product.cameraId = frame.cameraId;
        product.cigaretteNumber = frame.cigaretteNumber;
        product.capturedAtMicros = frame.capturedAt;
        product.completedAtMicros = frame.capturedAt + 1;
        product.decision = result.decision;
        product.elapsedMicros = result.elapsedMicros;
        product.parameterVersion = result.parameterVersion;
        product.parameterSha256 = result.parameterSha256;
        product.errorCode = result.errorCode;
        product.errorMessage = result.errorMessage;
        for (const Detection& detection : result.defects) {
            ProductDefectSummary defect;
            defect.classId = detection.classId;
            defect.className = detection.className;
            defect.confidence = detection.confidence;
            product.defects.push_back(defect);
        }
        std::string error;
        if (!state_.recordResult(product, 0, error)) {
            throw std::runtime_error("product state rejected frame: " + error);
        }
    }

private:
    ProductRuntimeState& state_;
};

struct Totals {
    std::uint64_t sessions = 0;
    std::uint64_t received = 0;
    std::uint64_t processed = 0;
    std::uint64_t ok = 0;
    std::uint64_t ng = 0;
    std::uint64_t error = 0;
    std::uint64_t dropped = 0;
    std::uint64_t sourceErrors = 0;
    std::uint64_t detectorErrors = 0;
    std::uint64_t observerErrors = 0;
    std::uint64_t saveFailures = 0;
    std::uint64_t resultStores = 0;
    std::uint64_t archiveStores = 0;
    std::uint64_t productStateProcessed = 0;
    std::uint64_t productStateOk = 0;
    std::uint64_t productStateNg = 0;
    std::uint64_t productStateError = 0;
    std::uint64_t progressRecords = 0;
    std::size_t maximumQueueDepth = 0;
};

void addChecked(std::uint64_t& target, std::uint64_t value)
{
    if (target > (std::numeric_limits<std::uint64_t>::max)() - value) {
        throw std::overflow_error("soak counter overflow");
    }
    target += value;
}

void runSession(
    OfflineInspectionSession& session,
    ProductRuntimeState& productState,
    std::size_t frameCount,
    std::uint64_t sessionNumber,
    Totals& totals)
{
    const ProductParameterProfile profile = fixtureProfile();
    const std::string parameterSha = profile.sha256();
    ProductRunConfiguration configuration;
    configuration.runId = "local-soak-" + std::to_string(sessionNumber);
    configuration.brandName = "sdk-free-soak";
    configuration.mode = ProductRunMode::OfflineFixture;
    configuration.detectorVersion = profile.detectorVersion;
    configuration.parameterVersion = profile.parameterVersion;
    configuration.queueCapacity = 8;
    configuration.recentResultCapacity = 64;
    configuration.diagnosticCapacity = 64;
    configuration.duplicateWindowCapacity = frameCount + 1;
    configuration.realIoEnabled = false;
    configuration.configuredParameters = profile;
    configuration.appliedParameters = profile;
    configuration.configuredParametersApplied = false;
    const TimestampMicros timeBase = static_cast<TimestampMicros>(
        1'000'000 + (sessionNumber % 1000000ULL) * (frameCount + 10ULL));
    std::string error;
    if (!productState.start(configuration, timeBase, error)) {
        throw std::runtime_error("product run start failed: " + error);
    }

    SyntheticSource source(frameCount, timeBase + 1);
    DeterministicFixtureDetector detector(profile.parameterVersion, parameterSha);
    CountingSink sink;
    CountingArchive archive;
    SyntheticClock clock(timeBase + static_cast<TimestampMicros>(frameCount) + 100);
    ProductObserver observer(productState);
    OfflineRunOptions options;
    options.queueCapacity = configuration.queueCapacity;
    options.overflowPolicy = QueueOverflowPolicy::RejectNewest;
    options.drainOnStop = true;
    options.parameterVersion = profile.parameterVersion;
    options.parameterSha256 = parameterSha;
    const OfflineRunSummary summary = session.run(
        source, detector, sink, archive, clock, &observer, options);
    if (summary.state != OfflineRunState::Completed || !summary.issues.empty() ||
        !summary.statistics.isConsistent()) {
        throw std::runtime_error("offline session did not complete cleanly");
    }
    if (!productState.completeStop(
            timeBase + static_cast<TimestampMicros>(frameCount) + 1000, error)) {
        throw std::runtime_error("product run stop failed: " + error);
    }
    const ProductRuntimeSnapshot snapshot = productState.snapshot();
    if (snapshot.status != ProductRuntimeStatus::Idle ||
        !snapshot.statistics.isConsistent() ||
        snapshot.statistics.processed != summary.statistics.processed ||
        snapshot.statistics.ok != summary.statistics.ok ||
        snapshot.statistics.ng != summary.statistics.ng ||
        snapshot.statistics.error != summary.statistics.error ||
        sink.count != summary.statistics.processed ||
        archive.count != summary.statistics.processed) {
        throw std::runtime_error("session conservation check failed");
    }

    ++totals.sessions;
    addChecked(totals.received, summary.statistics.received);
    addChecked(totals.processed, summary.statistics.processed);
    addChecked(totals.ok, summary.statistics.ok);
    addChecked(totals.ng, summary.statistics.ng);
    addChecked(totals.error, summary.statistics.error);
    addChecked(totals.dropped, summary.statistics.dropped);
    addChecked(totals.sourceErrors, summary.statistics.sourceErrors);
    addChecked(totals.detectorErrors, summary.statistics.detectorErrors);
    addChecked(totals.observerErrors, summary.statistics.observerErrors);
    addChecked(totals.saveFailures, summary.statistics.saveFailures);
    addChecked(totals.resultStores, sink.count);
    addChecked(totals.archiveStores, archive.count);
    addChecked(totals.productStateProcessed, snapshot.statistics.processed);
    addChecked(totals.productStateOk, snapshot.statistics.ok);
    addChecked(totals.productStateNg, snapshot.statistics.ng);
    addChecked(totals.productStateError, snapshot.statistics.error);
    totals.maximumQueueDepth = (std::max)(
        totals.maximumQueueDepth, snapshot.statistics.maximumQueueDepth);
}

void writeProgress(
    std::ofstream& stream,
    const Options& options,
    double elapsedSeconds,
    Totals& totals)
{
    ++totals.progressRecords;
    stream << std::fixed << std::setprecision(6)
        << "{\"schemaVersion\":\"cigvision-local-soak-progress-v1\""
        << ",\"restart\":" << options.restart
        << ",\"round\":" << options.round
        << ",\"iteration\":" << options.iteration
        << ",\"sequence\":" << totals.progressRecords
        << ",\"elapsedSeconds\":" << elapsedSeconds
        << ",\"sessionsCompleted\":" << totals.sessions
        << ",\"framesProcessed\":" << totals.processed
        << ",\"ok\":" << totals.ok
        << ",\"ng\":" << totals.ng
        << ",\"error\":" << totals.error
        << ",\"productStateProcessed\":" << totals.productStateProcessed
        << ",\"productStateOk\":" << totals.productStateOk
        << ",\"productStateNg\":" << totals.productStateNg
        << ",\"productStateError\":" << totals.productStateError
        << ",\"realIoEnabled\":false"
        << ",\"realRejectEnabled\":false"
        << ",\"invariantsPassed\":true}\n";
    stream.flush();
    if (!stream) {
        throw std::runtime_error("cannot append runtime progress");
    }
}

void writeSummary(const Options& options, double actualDuration, const Totals& totals)
{
    const std::string finalPath = options.outputDirectory + "/local-soak-summary.json";
    const std::string temporaryPath = finalPath + ".tmp";
    std::ofstream stream(temporaryPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("cannot open runtime summary staging file");
    }
    stream << std::fixed << std::setprecision(6);
    stream << "{\n"
        << "  \"schemaVersion\": \"cigvision-local-soak-runtime-v1\",\n"
        << "  \"sdkFree\": true,\n"
        << "  \"realIoEnabled\": false,\n"
        << "  \"realRejectEnabled\": false,\n"
        << "  \"productAcceptanceClaimed\": false,\n"
        << "  \"completed\": true,\n"
        << "  \"restart\": " << options.restart << ",\n"
        << "  \"round\": " << options.round << ",\n"
        << "  \"iteration\": " << options.iteration << ",\n"
        << "  \"targetDurationSeconds\": " << options.durationSeconds << ",\n"
        << "  \"durationSeconds\": " << actualDuration << ",\n"
        << "  \"sessionsCompleted\": " << totals.sessions << ",\n"
        << "  \"framesReceived\": " << totals.received << ",\n"
        << "  \"framesProcessed\": " << totals.processed << ",\n"
        << "  \"ok\": " << totals.ok << ",\n"
        << "  \"ng\": " << totals.ng << ",\n"
        << "  \"error\": " << totals.error << ",\n"
        << "  \"dropped\": " << totals.dropped << ",\n"
        << "  \"sourceErrors\": " << totals.sourceErrors << ",\n"
        << "  \"detectorErrors\": " << totals.detectorErrors << ",\n"
        << "  \"observerErrors\": " << totals.observerErrors << ",\n"
        << "  \"saveFailures\": " << totals.saveFailures << ",\n"
        << "  \"resultStores\": " << totals.resultStores << ",\n"
        << "  \"archiveStores\": " << totals.archiveStores << ",\n"
        << "  \"productStateProcessed\": " << totals.productStateProcessed << ",\n"
        << "  \"productStateOk\": " << totals.productStateOk << ",\n"
        << "  \"productStateNg\": " << totals.productStateNg << ",\n"
        << "  \"productStateError\": " << totals.productStateError << ",\n"
        << "  \"progressRecords\": " << totals.progressRecords << ",\n"
        << "  \"maximumQueueDepth\": " << totals.maximumQueueDepth << ",\n"
        << "  \"invariantsPassed\": true\n"
        << "}\n";
    stream.flush();
    if (!stream) {
        throw std::runtime_error("cannot write runtime summary");
    }
    stream.close();
    if (std::rename(temporaryPath.c_str(), finalPath.c_str()) != 0) {
        std::remove(temporaryPath.c_str());
        throw std::runtime_error("cannot atomically publish runtime summary");
    }
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const Options options = parseOptions(argc, argv);
        OfflineInspectionSession session;
        ProductRuntimeState productState;
        Totals totals;
        std::ofstream progress(
            (options.outputDirectory + "/local-soak-progress.ndjson").c_str(),
            std::ios::binary | std::ios::trunc);
        if (!progress) {
            throw std::runtime_error("cannot open runtime progress file");
        }
        const std::chrono::steady_clock::time_point started =
            std::chrono::steady_clock::now();
        do {
            runSession(
                session,
                productState,
                options.framesPerSession,
                totals.sessions + 1,
                totals);
            writeProgress(
                progress,
                options,
                std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - started).count(),
                totals);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count() < options.durationSeconds);
        const double actualDuration = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        if (totals.processed == 0 || totals.received != totals.processed ||
            totals.processed != totals.ok + totals.ng + totals.error ||
            totals.error != 0 || totals.dropped != 0 || totals.sourceErrors != 0 ||
            totals.detectorErrors != 0 || totals.observerErrors != 0 ||
            totals.saveFailures != 0 || totals.resultStores != totals.processed ||
            totals.archiveStores != totals.processed ||
            totals.productStateProcessed != totals.processed ||
            totals.productStateOk != totals.ok || totals.productStateNg != totals.ng ||
            totals.productStateError != totals.error ||
            totals.progressRecords != totals.sessions) {
            throw std::runtime_error("aggregate conservation check failed");
        }
        writeSummary(options, actualDuration, totals);
        std::cout << "CigVision SDK-free local soak completed: sessions="
            << totals.sessions << " frames=" << totals.processed
            << " duration=" << actualDuration << "s\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "CigVision local soak runtime failed: " << error.what() << '\n';
        return 1;
    }
}
