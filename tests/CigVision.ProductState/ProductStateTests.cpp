#include "core/ProductRuntimeState.h"
#include "adapters/tensorrt/TensorRtDetector.h"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace cigvision;

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ProductParameterProfile legacyPageProfile()
{
    ProductParameterProfile profile;
    profile.kind = ProductParameterProfileKind::LegacyDeepLearningPage;
    profile.profileId = "legacy-deep-learning-page";
    profile.parameterVersion = "legacy-deep-learning-page-v1";
    const char* names[] = {
        "dakoucuoya", "feiyan", "jiamo", "lvzuizhezhou", "quezui",
        "yanbangposun", "yanbangzangwu", "wuzi", "jietou"
    };
    for (std::int32_t classId = 0; classId < 9; ++classId) {
        ProductClassParameter rule;
        rule.classId = classId;
        rule.className = names[classId];
        rule.confidenceThreshold = classId < 7 ? 0.5F : 1.0F;
        rule.enabled = classId < 7;
        profile.classes.push_back(rule);
    }
    return profile;
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

ProductParameterProfile tensorRtProfile()
{
    ProductParameterProfile profile;
    profile.kind = ProductParameterProfileKind::TensorRtOffline;
    profile.profileId = "tensorrt-product";
    profile.parameterVersion = "trt-parameters-v1";
    profile.detectorVersion = "tensorrt-detector-v1";
    profile.modelSha256 = std::string(64, 'A');
    profile.inputTensorName = "images";
    profile.outputTensorName = "output0";
    profile.inputWidth = 992;
    profile.inputHeight = 992;
    profile.preprocessMode = "stretch-rgb-f32";
    const ProductParameterProfile legacy = legacyPageProfile();
    profile.classes = legacy.classes;
    return profile;
}

ProductRunConfiguration configuration()
{
    ProductRunConfiguration value;
    value.runId = "local-run-001";
    value.brandName = "test-brand";
    value.mode = ProductRunMode::OfflineFixture;
    value.detectorVersion = "deterministic-fixture-v1";
    value.parameterVersion = "offline-fixture-v1";
    value.queueCapacity = 4;
    value.recentResultCapacity = 3;
    value.diagnosticCapacity = 2;
    value.duplicateWindowCapacity = 16;
    value.configuredParameters = legacyPageProfile();
    value.appliedParameters = fixtureProfile();
    value.configuredParametersApplied = false;
    return value;
}

ProductFrameResult frame(std::uint64_t frameId, InspectionDecision decision,
    const std::string& camera = "camera-a")
{
    ProductFrameResult value;
    value.frameId = frameId;
    value.stationId = "station-a";
    value.cameraId = camera;
    value.cigaretteNumber = static_cast<std::uint32_t>(1000U + frameId);
    value.capturedAtMicros = static_cast<TimestampMicros>(10000 + frameId * 100);
    value.completedAtMicros = value.capturedAtMicros + 20;
    value.decision = decision;
    value.elapsedMicros = 20;
    value.parameterVersion = "offline-fixture-v1";
    value.parameterSha256 = fixtureProfile().sha256();
    if (decision == InspectionDecision::Ng) {
        ProductDefectSummary defect;
        defect.classId = static_cast<std::int32_t>(frameId % 2U);
        defect.className = defect.classId == 0 ? "surface" : "filter";
        defect.confidence = 0.9F;
        value.defects.push_back(defect);
    } else if (decision == InspectionDecision::Error) {
        value.errorCode = "INJECTED_ERROR";
        value.errorMessage = "test error";
    }
    return value;
}

void configurationAndSafety()
{
    std::string error;
    ProductRuntimeState state;
    ProductRunConfiguration value = configuration();
    require(state.start(value, 100, error), "valid local configuration must start");
    require(state.snapshot().status == ProductRuntimeStatus::Running,
        "state must become running");

    ProductRunConfiguration unsafe = configuration();
    unsafe.runId = "unsafe";
    unsafe.realIoEnabled = true;
    ProductRuntimeState unsafeState;
    require(!unsafeState.start(unsafe, 100, error),
        "real IO must be rejected by the local product state");

    ProductRunConfiguration tensorRt = configuration();
    tensorRt.mode = ProductRunMode::TensorRtOffline;
    tensorRt.runId = "trt";
    require(!unsafeState.start(tensorRt, 100, error),
        "TensorRT mode without model identity must fail");
    tensorRt.modelSha256 = std::string(64, 'A');
    tensorRt.detectorVersion = "tensorrt-detector-v1";
    tensorRt.parameterVersion = "trt-parameters-v1";
    tensorRt.appliedParameters = tensorRtProfile();
    tensorRt.configuredParameters = tensorRt.appliedParameters;
    tensorRt.configuredParametersApplied = true;
    require(unsafeState.start(tensorRt, 100, error),
        "TensorRT mode with model identity must start");
}

void parameterProfileIdentity()
{
    require(sha256Hex("") ==
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855",
        "SHA-256 empty vector must match the standard");
    require(sha256Hex("abc") ==
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad",
        "SHA-256 abc vector must match the standard");

    std::string error;
    ProductParameterProfile legacy = legacyPageProfile();
    require(legacy.validate(&error), "legacy page profile must validate");
    require(isSha256Hex(legacy.sha256()), "profile identity must be SHA-256");
    require(legacy.sha256() ==
        "a973097f62c125cab024aa3db40ce304bb0e7ce57b22e1565daf5cf5032605a6",
        "legacy parameter profile must retain its canonical golden identity");
    ProductParameterProfile changed = legacy;
    changed.classes[0].confidenceThreshold = 0.6F;
    require(changed.sha256() != legacy.sha256(),
        "threshold changes must alter the profile identity");
    changed = legacy;
    changed.classes[7].enabled = true;
    require(changed.sha256() != legacy.sha256(),
        "enablement changes must alter the profile identity");
    changed = legacy;
    changed.classes[1].className = "renamed";
    require(changed.sha256() != legacy.sha256(),
        "class-name changes must alter the profile identity");
    changed = legacy;
    changed.classes[1].classId = 0;
    require(!changed.validate(&error),
        "duplicate or unordered class IDs must fail validation");

    TensorRtDetectorConfig adapter;
    adapter.enginePath = "not-part-of-canonical-identity.engine";
    adapter.inputTensorName = "images";
    adapter.outputTensorName = "output0";
    adapter.inputWidth = 992;
    adapter.inputHeight = 992;
    adapter.classConfidenceThresholds.assign(9, 0.25F);
    for (const ProductClassParameter& rule : legacy.classes) {
        adapter.classNames.push_back(rule.className);
    }
    adapter.disabledClassIds.insert(7);
    adapter.disabledClassIds.insert(8);
    adapter.detectorVersion = "tensorrt-detector-v1";
    adapter.parameterVersion = "tensorrt-parameters-v2";
    adapter.modelSha256 = std::string(64, 'a');
    adapter.preprocessMode = "stretch-rgb-f32";
    const ProductParameterProfile projected = tensorRtParameterProfile(adapter);
    require(projected.validate(&error), "TensorRT adapter profile must validate");
    require(projected.classes.size() == 9 &&
        !projected.classes[7].enabled && !projected.classes[8].enabled,
        "disabled class IDs must project into the typed profile");
    TensorRtDetectorConfig changedAdapter = adapter;
    changedAdapter.enginePath = "same-model-different-path.engine";
    require(tensorRtParameterProfile(changedAdapter).sha256() == projected.sha256(),
        "engine path must not affect canonical detector identity");
    changedAdapter = adapter;
    changedAdapter.classConfidenceThresholds[0] = 0.3F;
    require(tensorRtParameterProfile(changedAdapter).sha256() != projected.sha256(),
        "per-class threshold changes must alter detector identity");
    changedAdapter = adapter;
    changedAdapter.modelSha256[0] = 'b';
    require(tensorRtParameterProfile(changedAdapter).sha256() != projected.sha256(),
        "model changes must alter detector identity");
    changedAdapter = adapter;
    changedAdapter.modelSha256 = std::string(64, 'A');
    require(tensorRtParameterProfile(changedAdapter).sha256() == projected.sha256(),
        "SHA-256 hex case must not alter canonical detector identity");

    ProductRunConfiguration drift = configuration();
    drift.parameterVersion = "drifted";
    ProductRuntimeState state;
    require(!state.start(drift, 100, error),
        "run identity drift from applied profile must fail");
    ProductRunConfiguration markedApplied = configuration();
    markedApplied.configuredParametersApplied = true;
    require(!state.start(markedApplied, 100, error),
        "unapplied UI parameters cannot be marked as detector parameters");
}

void aggregationAndBoundedResults()
{
    ProductRuntimeState state;
    std::string error;
    require(state.start(configuration(), 100, error), "run must start");
    require(state.recordResult(frame(1, InspectionDecision::Ok), 1, error),
        "OK frame must be recorded");
    require(state.recordResult(frame(2, InspectionDecision::Ng), 4, error),
        "NG frame must be recorded");
    require(state.recordResult(frame(3, InspectionDecision::Error, "camera-b"), 2, error),
        "error frame must be recorded");
    require(state.recordResult(frame(4, InspectionDecision::Ng, "camera-b"), 0, error),
        "second NG frame must be recorded");

    const ProductRuntimeSnapshot snapshot = state.snapshot();
    require(snapshot.statistics.processed == 4 && snapshot.statistics.ok == 1 &&
        snapshot.statistics.ng == 2 && snapshot.statistics.error == 1,
        "decision totals must be aggregated");
    require(snapshot.statistics.maximumQueueDepth == 4 &&
        snapshot.statistics.totalElapsedMicros == 80 &&
        snapshot.statistics.maximumElapsedMicros == 20,
        "runtime measurements must be aggregated");
    require(snapshot.statistics.defectsByClass.at(0) == 2,
        "defect classes must be aggregated");
    require(snapshot.statistics.cameras.size() == 2 &&
        snapshot.statistics.isConsistent(), "per-camera totals must be consistent");
    require(snapshot.recentResults.size() == 3 &&
        snapshot.recentResults.front().frame.frameId == 2 &&
        snapshot.recentResults.back().frame.frameId == 4,
        "recent-result history must remain bounded");
}

void lifecycleAndNoPartialMutation()
{
    ProductRuntimeState state;
    std::string error;
    require(state.start(configuration(), 100, error), "run must start");
    require(state.recordResult(frame(1, InspectionDecision::Ok), 0, error),
        "first frame must record");
    const ProductRuntimeSnapshot before = state.snapshot();

    ProductFrameResult invalid = frame(2, InspectionDecision::Ng);
    invalid.defects.clear();
    require(!state.recordResult(invalid, 0, error), "invalid NG must fail");
    require(!state.recordResult(frame(1, InspectionDecision::Ok), 0, error),
        "duplicate frame must fail");
    ProductFrameResult wrongVersion = frame(2, InspectionDecision::Ok);
    wrongVersion.parameterVersion = "other";
    require(!state.recordResult(wrongVersion, 0, error),
        "parameter drift must fail");
    ProductFrameResult wrongHash = frame(2, InspectionDecision::Ok);
    wrongHash.parameterSha256 = std::string(64, 'f');
    require(!state.recordResult(wrongHash, 0, error),
        "parameter hash drift must fail");
    const ProductRuntimeSnapshot after = state.snapshot();
    require(after.revision == before.revision &&
        after.statistics.processed == before.statistics.processed,
        "rejected frames must not mutate dashboard state");

    require(state.requestStop(200, error), "running workflow must enter stopping");
    require(!state.recordResult(frame(2, InspectionDecision::Ok), 0, error),
        "stopping workflow must reject new results");
    require(state.completeStop(250, error), "stopping workflow must complete");
    require(state.snapshot().status == ProductRuntimeStatus::Idle,
        "completed workflow must return idle");
}

void reviewWorkflow()
{
    ProductRuntimeState state;
    std::string error;
    require(state.start(configuration(), 100, error), "run must start");
    require(state.recordResult(frame(10, InspectionDecision::Ng), 0, error),
        "NG must record");
    require(!state.reviewResult(10, ProductReviewOutcome::Confirmed, "", "", 12000, error),
        "anonymous review must fail");
    require(!state.reviewResult(10, static_cast<ProductReviewOutcome>(99),
        "reviewer-a", "", 12000, error), "invalid review enum must fail");
    require(state.reviewResult(10, ProductReviewOutcome::Confirmed,
        "reviewer-a", "confirmed", 12000, error), "review must succeed");
    require(state.reviewResult(10, ProductReviewOutcome::Corrected,
        "reviewer-b", "class corrected", 12100, error), "review revision must succeed");

    const ProductRuntimeSnapshot snapshot = state.snapshot();
    require(snapshot.statistics.reviewed == 1 &&
        snapshot.statistics.confirmed == 0 &&
        snapshot.statistics.corrected == 1 &&
        snapshot.statistics.isConsistent(), "review statistics must remain consistent");
    require(snapshot.recentResults.front().review.reviewer == "reviewer-b",
        "latest review attribution must be retained");
}

void diagnosticsFaultAndRestart()
{
    ProductRuntimeState state;
    std::string error;
    require(state.start(configuration(), 100, error), "run must start");
    ProductDiagnosticEvent invalidSeverity;
    invalidSeverity.occurredAtMicros = 150;
    invalidSeverity.severity = static_cast<ProductDiagnosticSeverity>(99);
    invalidSeverity.component = "storage";
    invalidSeverity.code = "INVALID";
    invalidSeverity.message = "invalid severity";
    require(!state.appendDiagnostic(invalidSeverity, error),
        "invalid diagnostic severity must fail");
    for (int index = 0; index < 3; ++index) {
        ProductDiagnosticEvent event;
        event.occurredAtMicros = 200 + index;
        event.component = "storage";
        event.code = "TEST_" + std::to_string(index);
        event.message = "diagnostic";
        require(state.appendDiagnostic(event, error), "diagnostic must append");
    }
    require(state.snapshot().diagnostics.size() == 2,
        "diagnostic history must remain bounded");
    require(state.fail("RUNTIME_FAILURE", "injected failure", 300, error),
        "active workflow must fault");
    require(state.snapshot().status == ProductRuntimeStatus::Faulted,
        "fault must be visible");

    ProductRunConfiguration next = configuration();
    next.runId = "local-run-002";
    require(state.start(next, 400, error), "faulted state must support a clean restart");
    const ProductRuntimeSnapshot restarted = state.snapshot();
    require(restarted.status == ProductRuntimeStatus::Running &&
        restarted.statistics.processed == 0 && restarted.recentResults.empty() &&
        restarted.diagnostics.empty(), "restart must reset run-local state");
}

void cameraIdentityDoesNotCollide()
{
    ProductRuntimeState state;
    std::string error;
    require(state.start(configuration(), 100, error), "run must start");
    ProductFrameResult first = frame(1, InspectionDecision::Ok, "c");
    first.stationId = "a/b";
    ProductFrameResult second = frame(2, InspectionDecision::Ok, "b/c");
    second.stationId = "a";
    require(state.recordResult(first, 0, error), "first camera must record");
    require(state.recordResult(second, 0, error), "second camera must record");
    const ProductRuntimeSnapshot snapshot = state.snapshot();
    require(snapshot.statistics.cameras.size() == 2 &&
        snapshot.statistics.isConsistent(),
        "station and camera identities must not collide through separators");
}

void concurrentRecording()
{
    ProductRuntimeState state;
    ProductRunConfiguration value = configuration();
    value.runId = "concurrent";
    value.recentResultCapacity = 64;
    value.duplicateWindowCapacity = 1024;
    value.queueCapacity = 64;
    std::string error;
    require(state.start(value, 100, error), "concurrent run must start");

    std::atomic<int> failures{ 0 };
    std::vector<std::thread> threads;
    for (std::uint64_t worker = 0; worker < 4; ++worker) {
        threads.emplace_back([&state, &failures, worker] {
            for (std::uint64_t index = 0; index < 100; ++index) {
                const std::uint64_t frameId = worker * 100 + index + 1;
                ProductFrameResult value = frame(frameId,
                    (frameId % 2U) == 0U ? InspectionDecision::Ng :
                        InspectionDecision::Ok,
                    "camera-" + std::to_string(worker));
                std::string recordError;
                if (!state.recordResult(value, static_cast<std::size_t>(frameId % 8U),
                        recordError)) {
                    ++failures;
                }
            }
        });
    }
    for (std::thread& worker : threads) {
        worker.join();
    }
    const ProductRuntimeSnapshot snapshot = state.snapshot();
    require(failures.load() == 0 && snapshot.statistics.processed == 400 &&
        snapshot.statistics.ok == 200 && snapshot.statistics.ng == 200 &&
        snapshot.statistics.isConsistent() && snapshot.recentResults.size() == 64,
        "concurrent recording must be safe and bounded");
}

} // namespace

int main()
{
    const std::vector<std::pair<const char*, void(*)()>> tests = {
        { "configuration and safety", configurationAndSafety },
        { "parameter profile identity", parameterProfileIdentity },
        { "aggregation and bounded results", aggregationAndBoundedResults },
        { "lifecycle and no partial mutation", lifecycleAndNoPartialMutation },
        { "review workflow", reviewWorkflow },
        { "diagnostics fault and restart", diagnosticsFaultAndRestart },
        { "camera identity does not collide", cameraIdentityDoesNotCollide },
        { "concurrent recording", concurrentRecording },
    };
    std::size_t passed = 0;
    for (const auto& test : tests) {
        try {
            test.second();
            ++passed;
            std::cout << "[PASS] " << test.first << '\n';
        }
        catch (const std::exception& error) {
            std::cerr << "[FAIL] " << test.first << ": " << error.what() << '\n';
        }
    }
    std::cout << passed << '/' << tests.size() << " product state tests passed\n";
    return passed == tests.size() ? 0 : 1;
}
