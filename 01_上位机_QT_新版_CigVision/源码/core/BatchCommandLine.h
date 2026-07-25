#pragma once

#include "InspectionContracts.h"
#include "RealtimeSimulation.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cigvision {

enum class BatchCommandMode {
    None = 0,
    OfflineFixture,
    TensorRt,
    Simulation
};

struct BatchCommandLine {
    BatchCommandMode mode = BatchCommandMode::None;
    std::string manifestPath;
    std::string outputDirectory;
    std::string detectorConfigPath;
    TimestampMicros simulationRejectDelayMicros = 0;
    std::size_t simulationQueueCapacity = 4;
    std::string simulationTargetOutput = "simulation-reject";
};

namespace batch_cli_detail {

inline bool startsWithOptionPrefix(const std::string& value)
{
    return value.size() >= 2 && value[0] == '-' && value[1] == '-';
}

inline std::string trimAsciiWhitespace(const std::string& value)
{
    std::size_t begin = 0;
    while (begin < value.size() &&
        std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin &&
        std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

inline bool parseUnsignedDecimal(const std::string& value, std::uint64_t maximum,
    std::uint64_t& parsed)
{
    if (value.empty()) {
        return false;
    }
    std::uint64_t result = 0;
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
        if (result > (maximum - digit) / 10U) {
            return false;
        }
        result = result * 10U + digit;
    }
    parsed = result;
    return true;
}

inline bool hasValue(const std::map<std::string, std::string>& values,
    const char* option)
{
    return values.find(option) != values.end();
}

inline bool fail(std::string& errorMessage, const std::string& message)
{
    errorMessage = message;
    return false;
}

} // namespace batch_cli_detail

// Parses only CigVision batch options and leaves unrelated Qt/UI options alone.
// Every known option is single-use and value-bearing. Simulation has a separate
// output option so it cannot be confused with the fixture/TensorRT batch paths.
inline bool parseBatchCommandLine(const std::vector<std::string>& arguments,
    BatchCommandLine& parsed, std::string& errorMessage)
{
    using namespace batch_cli_detail;

    parsed = BatchCommandLine();
    errorMessage.clear();
    const std::vector<std::string> knownOptions = {
        "--offline-batch-manifest",
        "--tensorrt-batch-manifest",
        "--detector-config",
        "--offline-output",
        "--simulation-batch-manifest",
        "--simulation-output",
        "--simulation-reject-delay-micros",
        "--simulation-queue-capacity",
        "--simulation-target-output"
    };

    std::map<std::string, std::string> values;
    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        bool known = false;
        for (const std::string& option : knownOptions) {
            if (argument == option) {
                known = true;
                break;
            }
        }
        if (!known) {
            continue;
        }
        if (values.find(argument) != values.end()) {
            return fail(errorMessage, "duplicate batch option: " + argument);
        }
        if (index + 1 >= arguments.size() ||
            startsWithOptionPrefix(arguments[index + 1])) {
            return fail(errorMessage, "batch option requires one value: " + argument);
        }
        values.emplace(argument, arguments[++index]);
    }

    const bool fixtureMode = hasValue(values, "--offline-batch-manifest");
    const bool tensorRtMode = hasValue(values, "--tensorrt-batch-manifest");
    const bool simulationMode = hasValue(values, "--simulation-batch-manifest");
    const int modeCount = static_cast<int>(fixtureMode) + static_cast<int>(tensorRtMode) +
        static_cast<int>(simulationMode);
    if (modeCount == 0) {
        if (!values.empty()) {
            return fail(errorMessage, "batch options require exactly one batch manifest mode");
        }
        return true;
    }
    if (modeCount != 1) {
        return fail(errorMessage, "fixture, TensorRT and simulation batch modes are mutually exclusive");
    }

    const bool hasOfflineOutput = hasValue(values, "--offline-output");
    const bool hasDetectorConfig = hasValue(values, "--detector-config");
    const bool hasSimulationOutput = hasValue(values, "--simulation-output");
    const bool hasSimulationDelay = hasValue(values, "--simulation-reject-delay-micros");
    const bool hasSimulationCapacity = hasValue(values, "--simulation-queue-capacity");
    const bool hasSimulationTarget = hasValue(values, "--simulation-target-output");
    const bool hasAnySimulationSetting = hasSimulationOutput || hasSimulationDelay ||
        hasSimulationCapacity || hasSimulationTarget;

    if (fixtureMode) {
        if (!hasOfflineOutput || hasDetectorConfig || hasAnySimulationSetting) {
            return fail(errorMessage,
                "fixture batch requires --offline-output and forbids TensorRT/simulation options");
        }
        parsed.mode = BatchCommandMode::OfflineFixture;
        parsed.manifestPath = values["--offline-batch-manifest"];
        parsed.outputDirectory = values["--offline-output"];
    } else if (tensorRtMode) {
        if (!hasOfflineOutput || !hasDetectorConfig || hasAnySimulationSetting) {
            return fail(errorMessage,
                "TensorRT batch requires --offline-output and --detector-config and forbids simulation options");
        }
        parsed.mode = BatchCommandMode::TensorRt;
        parsed.manifestPath = values["--tensorrt-batch-manifest"];
        parsed.outputDirectory = values["--offline-output"];
        parsed.detectorConfigPath = values["--detector-config"];
    } else {
        if (!hasSimulationOutput || hasOfflineOutput || hasDetectorConfig) {
            return fail(errorMessage,
                "simulation batch requires --simulation-output and forbids fixture/TensorRT output options");
        }
        parsed.mode = BatchCommandMode::Simulation;
        parsed.manifestPath = values["--simulation-batch-manifest"];
        parsed.outputDirectory = values["--simulation-output"];

        if (hasSimulationDelay) {
            std::uint64_t delay = 0;
            if (!parseUnsignedDecimal(values["--simulation-reject-delay-micros"],
                    static_cast<std::uint64_t>(kMaxLocalReplayDelayMicros),
                    delay)) {
                return fail(errorMessage,
                    "simulation reject delay must be an integer from 0 to 60000000 microseconds");
            }
            parsed.simulationRejectDelayMicros = static_cast<TimestampMicros>(delay);
        }
        if (hasSimulationCapacity) {
            constexpr std::uint64_t maximumQueueCapacity = 65536U;
            std::uint64_t capacity = 0;
            if (!parseUnsignedDecimal(values["--simulation-queue-capacity"],
                    maximumQueueCapacity, capacity) || capacity == 0) {
                return fail(errorMessage,
                    "simulation queue capacity must be an integer from 1 to 65536");
            }
            parsed.simulationQueueCapacity = static_cast<std::size_t>(capacity);
        }
        if (hasSimulationTarget) {
            parsed.simulationTargetOutput = trimAsciiWhitespace(
                values["--simulation-target-output"]);
            if (parsed.simulationTargetOutput.empty() ||
                parsed.simulationTargetOutput.size() > 256) {
                return fail(errorMessage,
                    "simulation target output must contain 1 to 256 non-whitespace bytes");
            }
        }
    }

    if (trimAsciiWhitespace(parsed.manifestPath).empty() ||
        trimAsciiWhitespace(parsed.outputDirectory).empty() ||
        (parsed.mode == BatchCommandMode::TensorRt &&
            trimAsciiWhitespace(parsed.detectorConfigPath).empty())) {
        return fail(errorMessage, "batch paths must not be empty or whitespace-only");
    }
    return true;
}

} // namespace cigvision
