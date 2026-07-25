#include "QtOfflineInspection.h"
#include "adapters/tensorrt/TensorRtDetector.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>

namespace cigvision {
namespace {

std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString frameBaseName(std::uint64_t frameId)
{
    return QStringLiteral("frame-%1").arg(static_cast<qulonglong>(frameId), 8, 10,
        QLatin1Char('0'));
}

bool hasDuplicateTopLevelJsonKey(const QByteArray& bytes, QString& duplicateKey)
{
    QSet<QString> keys;
    int depth = 0;
    for (int index = 0; index < bytes.size(); ++index) {
        const char character = bytes.at(index);
        if (character == '{' || character == '[') {
            ++depth;
            continue;
        }
        if (character == '}' || character == ']') {
            --depth;
            continue;
        }
        if (character != '"') {
            continue;
        }

        const int tokenStart = index;
        bool terminated = false;
        for (++index; index < bytes.size(); ++index) {
            if (bytes.at(index) == '\\') {
                ++index;
                continue;
            }
            if (bytes.at(index) == '"') {
                terminated = true;
                break;
            }
        }
        if (!terminated || depth != 1) {
            continue;
        }
        int next = index + 1;
        while (next < bytes.size() &&
            (bytes.at(next) == ' ' || bytes.at(next) == '\t' ||
                bytes.at(next) == '\r' || bytes.at(next) == '\n')) {
            ++next;
        }
        if (next >= bytes.size() || bytes.at(next) != ':') {
            continue;
        }

        const QByteArray token = bytes.mid(tokenStart, index - tokenStart + 1);
        QJsonParseError keyError;
        const QJsonDocument keyDocument = QJsonDocument::fromJson(
            QByteArray("[") + token + QByteArray("]"), &keyError);
        if (keyError.error != QJsonParseError::NoError ||
            !keyDocument.isArray() || keyDocument.array().size() != 1 ||
            !keyDocument.array().at(0).isString()) {
            duplicateKey = QStringLiteral("<invalid-key>");
            return true;
        }
        const QString key = keyDocument.array().at(0).toString();
        if (keys.contains(key)) {
            duplicateKey = key;
            return true;
        }
        keys.insert(key);
    }
    duplicateKey.clear();
    return false;
}

QString decisionName(InspectionDecision decision)
{
    switch (decision) {
    case InspectionDecision::Ok: return QStringLiteral("OK");
    case InspectionDecision::Ng: return QStringLiteral("NG");
    case InspectionDecision::Error: return QStringLiteral("ERROR");
    default: return QStringLiteral("UNKNOWN");
    }
}

bool isExactJsonInteger(const QJsonValue& value)
{
    return value.isDouble() && std::isfinite(value.toDouble()) &&
        std::floor(value.toDouble()) == value.toDouble();
}

QJsonObject resultJson(const InspectionResult& result, const FramePacket* frame)
{
    QJsonObject root;
    root.insert(QStringLiteral("frameId"), static_cast<qint64>(result.frameId));
    root.insert(QStringLiteral("decision"), decisionName(result.decision));
    root.insert(QStringLiteral("elapsedMicros"), static_cast<qint64>(result.elapsedMicros));
    root.insert(QStringLiteral("parameterVersion"), QString::fromUtf8(result.parameterVersion.c_str()));
    root.insert(QStringLiteral("parameterSha256"),
        QString::fromLatin1(result.parameterSha256.c_str()));
    root.insert(QStringLiteral("errorCode"), QString::fromUtf8(result.errorCode.c_str()));
    root.insert(QStringLiteral("errorMessage"), QString::fromUtf8(result.errorMessage.c_str()));
    if (frame != nullptr) {
        root.insert(QStringLiteral("stationId"), QString::fromUtf8(frame->stationId.c_str()));
        root.insert(QStringLiteral("cameraId"), QString::fromUtf8(frame->cameraId.c_str()));
        // Retain the P3 field for backward-compatible evidence consumers.
        root.insert(QStringLiteral("sourceFile"), QString::fromUtf8(frame->cameraId.c_str()));
        root.insert(QStringLiteral("cigaretteNumber"), static_cast<qint64>(frame->cigaretteNumber));
        root.insert(QStringLiteral("capturedAtMicros"), static_cast<qint64>(frame->capturedAt));
        root.insert(QStringLiteral("width"), static_cast<qint64>(frame->width));
        root.insert(QStringLiteral("height"), static_cast<qint64>(frame->height));
    }
    QJsonArray defects;
    for (const Detection& detection : result.defects) {
        QJsonObject item;
        item.insert(QStringLiteral("classId"), detection.classId);
        item.insert(QStringLiteral("className"), QString::fromUtf8(detection.className.c_str()));
        item.insert(QStringLiteral("confidence"), detection.confidence);
        item.insert(QStringLiteral("detectorVersion"),
            QString::fromUtf8(detection.detectorVersion.c_str()));
        QJsonObject box;
        box.insert(QStringLiteral("x"), detection.box.x);
        box.insert(QStringLiteral("y"), detection.box.y);
        box.insert(QStringLiteral("width"), detection.box.width);
        box.insert(QStringLiteral("height"), detection.box.height);
        item.insert(QStringLiteral("box"), box);
        defects.append(item);
    }
    root.insert(QStringLiteral("defects"), defects);
    return root;
}

QImage imageFromFrame(const FramePacket& frame)
{
    QImage::Format format = QImage::Format_Invalid;
    if (frame.pixelFormat == PixelFormat::Mono8) {
        format = QImage::Format_Grayscale8;
    } else if (frame.pixelFormat == PixelFormat::RGB8) {
        format = QImage::Format_RGB888;
    }
    if (format == QImage::Format_Invalid || frame.pixels.empty()) {
        return QImage();
    }
    QImage borrowed(frame.pixels.data(), static_cast<int>(frame.width),
        static_cast<int>(frame.height), static_cast<int>(frame.strideBytes), format);
    return borrowed.copy();
}

QJsonObject statisticsJson(const InspectionStatistics& statistics)
{
    QJsonObject object;
    object.insert(QStringLiteral("received"), static_cast<qint64>(statistics.received));
    object.insert(QStringLiteral("processed"), static_cast<qint64>(statistics.processed));
    object.insert(QStringLiteral("ok"), static_cast<qint64>(statistics.ok));
    object.insert(QStringLiteral("ng"), static_cast<qint64>(statistics.ng));
    object.insert(QStringLiteral("error"), static_cast<qint64>(statistics.error));
    object.insert(QStringLiteral("sourceErrors"), static_cast<qint64>(statistics.sourceErrors));
    object.insert(QStringLiteral("detectorErrors"), static_cast<qint64>(statistics.detectorErrors));
    object.insert(QStringLiteral("observerErrors"), static_cast<qint64>(statistics.observerErrors));
    object.insert(QStringLiteral("saveFailures"), static_cast<qint64>(statistics.saveFailures));
    object.insert(QStringLiteral("dropped"), static_cast<qint64>(statistics.dropped));
    return object;
}

class CallbackObserver final : public IInspectionObserver {
public:
    using Callback = std::function<void(const FramePacket&, const InspectionResult&,
        const InspectionStatistics&)>;
    explicit CallbackObserver(Callback callback) : callback_(std::move(callback)) {}
    void onResult(const FramePacket& frame, const InspectionResult& result,
        const InspectionStatistics& statistics) override
    {
        callback_(frame, result, statistics);
    }
private:
    Callback callback_;
};

bool loadManifest(const QString& manifestPath, std::vector<QtOfflineInput>& inputs,
    QString& errorMessage, bool requireExpected = true)
{
    inputs.clear();
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errorMessage = parseError.errorString();
        return false;
    }

    const QJsonObject root = document.object();
    if (!root.value(QStringLiteral("root")).isString() ||
        !root.value(QStringLiteral("samples")).isArray()) {
        errorMessage = QStringLiteral("manifest root and samples are required");
        return false;
    }
    const QDir manifestDirectory = QFileInfo(manifestPath).absoluteDir();
    const QDir sampleRoot(manifestDirectory.absoluteFilePath(root.value(QStringLiteral("root")).toString()));
    const QJsonArray samples = root.value(QStringLiteral("samples")).toArray();
    if (samples.isEmpty()) {
        errorMessage = QStringLiteral("manifest has no samples");
        return false;
    }
    QSet<QString> uniquePaths;
    const QRegularExpression sha256Pattern(QStringLiteral("^[0-9A-Fa-f]{64}$"));
    for (int sampleIndex = 0; sampleIndex < samples.size(); ++sampleIndex) {
        const QJsonValue value = samples.at(sampleIndex);
        if (!value.isObject()) {
            errorMessage = QStringLiteral("each manifest sample must be an object");
            return false;
        }
        const QJsonObject sample = value.toObject();
        const QJsonValue pathValue = sample.value(QStringLiteral("path"));
        const QJsonValue sha256Value = sample.value(QStringLiteral("sha256"));
        const QJsonValue expectedValue = sample.value(QStringLiteral("expected"));
        const QString relativePath = pathValue.toString();
        const QString sha256 = sha256Value.toString();
        const QString expected = expectedValue.toString();
        const bool expectedValid = expected == QStringLiteral("OK") ||
            expected == QStringLiteral("NG");
        if (!pathValue.isString() || relativePath.isEmpty() || !sha256Value.isString() ||
            !sha256Pattern.match(sha256).hasMatch() ||
            (requireExpected && (!expectedValue.isString() || !expectedValid)) ||
            (sample.contains(QStringLiteral("expected")) &&
                (!expectedValue.isString() || !expectedValid))) {
            errorMessage = QStringLiteral("sample path, SHA-256 or expected decision is invalid");
            return false;
        }
        QtOfflineInput input;
        input.path = QDir::cleanPath(sampleRoot.absoluteFilePath(relativePath));
        if (uniquePaths.contains(input.path)) {
            errorMessage = QStringLiteral("duplicate sample path");
            return false;
        }
        uniquePaths.insert(input.path);
        input.expectedSha256 = sha256.toUpper();
        input.expectedDecision = expected == QStringLiteral("OK") ? InspectionDecision::Ok :
            (expected == QStringLiteral("NG") ? InspectionDecision::Ng :
                InspectionDecision::Unknown);

        const QJsonValue stationValue = sample.value(QStringLiteral("stationId"));
        if (sample.contains(QStringLiteral("stationId"))) {
            input.stationId = stationValue.toString().trimmed();
            if (!stationValue.isString() || input.stationId.isEmpty()) {
                errorMessage = QStringLiteral("stationId must be a non-empty string");
                return false;
            }
        } else {
            input.stationId = QStringLiteral("offline");
        }

        const QJsonValue cameraValue = sample.value(QStringLiteral("cameraId"));
        if (sample.contains(QStringLiteral("cameraId"))) {
            input.cameraId = cameraValue.toString().trimmed();
            if (!cameraValue.isString() || input.cameraId.isEmpty()) {
                errorMessage = QStringLiteral("cameraId must be a non-empty string");
                return false;
            }
        } else {
            input.cameraId = QFileInfo(input.path).fileName();
        }

        const QJsonValue cigaretteValue = sample.value(QStringLiteral("cigaretteNumber"));
        if (sample.contains(QStringLiteral("cigaretteNumber"))) {
            if (!isExactJsonInteger(cigaretteValue) || cigaretteValue.toDouble() <= 0.0 ||
                cigaretteValue.toDouble() >
                    static_cast<double>((std::numeric_limits<std::uint32_t>::max)())) {
                errorMessage = QStringLiteral(
                    "cigaretteNumber must be an integer from 1 to UINT32_MAX");
                return false;
            }
            input.cigaretteNumber = static_cast<std::uint32_t>(cigaretteValue.toDouble());
        } else {
            input.cigaretteNumber = static_cast<std::uint32_t>(sampleIndex + 1);
        }

        const QJsonValue delayValue = sample.value(QStringLiteral("delayBeforeMicros"));
        if (sample.contains(QStringLiteral("delayBeforeMicros"))) {
            constexpr double maximumSafeJsonInteger = 9007199254740991.0;
            if (!isExactJsonInteger(delayValue) || delayValue.toDouble() < 0.0 ||
                delayValue.toDouble() > maximumSafeJsonInteger ||
                delayValue.toDouble() > static_cast<double>(kMaxLocalReplayDelayMicros)) {
                errorMessage = QStringLiteral(
                    "delayBeforeMicros must be an integer from 0 to 60000000 microseconds");
                return false;
            }
            input.delayBeforeMicros = static_cast<TimestampMicros>(delayValue.toDouble());
        }
        inputs.push_back(input);
    }
    errorMessage.clear();
    return true;
}

bool writeInputManifest(const QString& outputDirectory,
    const std::vector<QtOfflineInput>& inputs, QString& errorMessage)
{
    QJsonArray samples;
    for (std::size_t inputIndex = 0; inputIndex < inputs.size(); ++inputIndex) {
        const QtOfflineInput& input = inputs[inputIndex];
        QFile file(input.path);
        if (!file.open(QIODevice::ReadOnly)) {
            errorMessage = input.path + QStringLiteral(": ") + file.errorString();
            return false;
        }
        QJsonObject item;
        item.insert(QStringLiteral("sourcePath"), QFileInfo(input.path).absoluteFilePath());
        item.insert(QStringLiteral("sha256"), QString::fromLatin1(
            QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex()));
        item.insert(QStringLiteral("stationId"), input.stationId.isEmpty()
            ? QStringLiteral("offline") : input.stationId);
        item.insert(QStringLiteral("cameraId"), input.cameraId.isEmpty()
            ? QFileInfo(input.path).fileName() : input.cameraId);
        const std::uint32_t cigaretteNumber = input.cigaretteNumber == 0
            ? static_cast<std::uint32_t>(inputIndex + 1) : input.cigaretteNumber;
        item.insert(QStringLiteral("cigaretteNumber"), static_cast<qint64>(cigaretteNumber));
        item.insert(QStringLiteral("delayBeforeMicros"), static_cast<qint64>(
            input.delayBeforeMicros));
        if (input.expectedDecision != InspectionDecision::Unknown) {
            item.insert(QStringLiteral("expected"), decisionName(input.expectedDecision));
        }
        samples.append(item);
    }
    QJsonObject root;
    root.insert(QStringLiteral("samples"), samples);
    QSaveFile file(QDir(outputDirectory).filePath(QStringLiteral("input-manifest.json")));
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        errorMessage = file.errorString();
        return false;
    }
    errorMessage.clear();
    return true;
}

bool writeSummary(const QString& outputDirectory, const OfflineRunSummary& summary,
    const QStringList& mismatches)
{
    QJsonObject root;
    root.insert(QStringLiteral("state"), static_cast<int>(summary.state));
    root.insert(QStringLiteral("statistics"), statisticsJson(summary.statistics));
    QJsonObject defectsByClass;
    for (const auto& item : summary.statistics.defectsByClass) {
        defectsByClass.insert(QString::number(item.first), static_cast<qint64>(item.second));
    }
    root.insert(QStringLiteral("defectsByClass"), defectsByClass);
    QJsonArray issues;
    for (const std::string& issue : summary.issues) {
        issues.append(QString::fromUtf8(issue.c_str()));
    }
    for (const QString& mismatch : mismatches) {
        issues.append(mismatch);
    }
    root.insert(QStringLiteral("issues"), issues);
    QSaveFile file(QDir(outputDirectory).filePath(QStringLiteral("summary.json")));
    return file.open(QIODevice::WriteOnly) &&
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0 && file.commit();
}

QString simulationTraceStatusName(SimulationRejectTraceStatus status)
{
    switch (status) {
    case SimulationRejectTraceStatus::Skipped: return QStringLiteral("SKIPPED");
    case SimulationRejectTraceStatus::Simulated: return QStringLiteral("SIMULATED");
    case SimulationRejectTraceStatus::Failed: return QStringLiteral("FAILED");
    default: return QStringLiteral("UNKNOWN");
    }
}

QString rejectExecutionStatusName(RejectExecutionStatus status)
{
    switch (status) {
    case RejectExecutionStatus::Skipped: return QStringLiteral("SKIPPED");
    case RejectExecutionStatus::Simulated: return QStringLiteral("SIMULATED");
    case RejectExecutionStatus::Executed: return QStringLiteral("EXECUTED");
    case RejectExecutionStatus::Failed: return QStringLiteral("FAILED");
    default: return QStringLiteral("UNKNOWN");
    }
}

QJsonObject simulationRejectCommandJson(const RejectCommand& command)
{
    QJsonObject object;
    object.insert(QStringLiteral("frameId"), static_cast<qint64>(command.frameId));
    object.insert(QStringLiteral("cigaretteNumber"),
        static_cast<qint64>(command.cigaretteNumber));
    object.insert(QStringLiteral("targetOutput"),
        QString::fromUtf8(command.targetOutput.c_str()));
    object.insert(QStringLiteral("scheduledAtMicros"),
        static_cast<qint64>(command.scheduledAt));
    object.insert(QStringLiteral("mode"), QStringLiteral("Simulation"));
    return object;
}

QJsonObject simulationExecutionJson(const RejectExecutionResult& execution)
{
    QJsonObject object;
    object.insert(QStringLiteral("frameId"), static_cast<qint64>(execution.frameId));
    object.insert(QStringLiteral("status"), rejectExecutionStatusName(execution.status));
    object.insert(QStringLiteral("completedAtMicros"),
        static_cast<qint64>(execution.completedAt));
    object.insert(QStringLiteral("errorCode"), QString::fromUtf8(execution.errorCode.c_str()));
    object.insert(QStringLiteral("errorMessage"),
        QString::fromUtf8(execution.errorMessage.c_str()));
    return object;
}

bool writeSimulationTrace(const QString& outputDirectory, const QString& manifestPath,
    TimestampMicros rejectDelayMicros, std::size_t queueCapacity,
    const QString& targetOutput, const OfflineRunSummary& summary,
    const SimulationRejectObserver& observer, QString& errorMessage)
{
    const SimulationRejectStatistics& simulationStatistics = observer.statistics();
    std::string traceValidationError;
    const bool traceValid = validateSimulationTrace(observer.traces(), simulationStatistics,
        &traceValidationError);
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), QStringLiteral("cigvision-simulation-trace-v1"));
    root.insert(QStringLiteral("mode"), QStringLiteral("simulation"));
    root.insert(QStringLiteral("simulation"), true);
    root.insert(QStringLiteral("realIoEnabled"), false);
    root.insert(QStringLiteral("manifestPath"), QFileInfo(manifestPath).absoluteFilePath());
    root.insert(QStringLiteral("runState"), static_cast<int>(summary.state));
    root.insert(QStringLiteral("traceCount"), static_cast<qint64>(observer.traces().size()));
    root.insert(QStringLiteral("traceComplete"), traceValid &&
        simulationStatistics.observed == summary.statistics.processed);
    root.insert(QStringLiteral("traceValidationError"),
        QString::fromUtf8(traceValidationError.c_str()));

    QJsonObject configuration;
    configuration.insert(QStringLiteral("detector"), QStringLiteral("deterministic-fixture-v1"));
    configuration.insert(QStringLiteral("rejectMode"), QStringLiteral("Simulation"));
    configuration.insert(QStringLiteral("simulation"), true);
    configuration.insert(QStringLiteral("rejectDelayMicros"),
        static_cast<qint64>(rejectDelayMicros));
    configuration.insert(QStringLiteral("queueCapacity"), static_cast<qint64>(queueCapacity));
    configuration.insert(QStringLiteral("targetOutput"), targetOutput);
    configuration.insert(QStringLiteral("overflowPolicy"), QStringLiteral("RejectNewest"));
    root.insert(QStringLiteral("configuration"), configuration);

    QJsonObject statistics;
    statistics.insert(QStringLiteral("received"), static_cast<qint64>(summary.statistics.received));
    statistics.insert(QStringLiteral("processed"), static_cast<qint64>(summary.statistics.processed));
    statistics.insert(QStringLiteral("ok"), static_cast<qint64>(summary.statistics.ok));
    statistics.insert(QStringLiteral("ng"), static_cast<qint64>(summary.statistics.ng));
    statistics.insert(QStringLiteral("error"), static_cast<qint64>(summary.statistics.error));
    statistics.insert(QStringLiteral("dropped"), static_cast<qint64>(summary.statistics.dropped));
    statistics.insert(QStringLiteral("observed"), static_cast<qint64>(simulationStatistics.observed));
    statistics.insert(QStringLiteral("ngCandidates"),
        static_cast<qint64>(simulationStatistics.ngCandidates));
    statistics.insert(QStringLiteral("commands"), static_cast<qint64>(simulationStatistics.commands));
    statistics.insert(QStringLiteral("simulated"), static_cast<qint64>(simulationStatistics.simulated));
    statistics.insert(QStringLiteral("skipped"), static_cast<qint64>(simulationStatistics.skipped));
    statistics.insert(QStringLiteral("failed"), static_cast<qint64>(simulationStatistics.failed));
    root.insert(QStringLiteral("statistics"), statistics);

    QJsonArray traces;
    for (const SimulationRejectTrace& trace : observer.traces()) {
        QJsonObject item;
        item.insert(QStringLiteral("frameId"), static_cast<qint64>(trace.frameId));
        item.insert(QStringLiteral("stationId"), QString::fromUtf8(trace.stationId.c_str()));
        item.insert(QStringLiteral("cameraId"), QString::fromUtf8(trace.cameraId.c_str()));
        item.insert(QStringLiteral("cigaretteNumber"),
            static_cast<qint64>(trace.cigaretteNumber));
        item.insert(QStringLiteral("observedAtMicros"), static_cast<qint64>(trace.observedAt));
        item.insert(QStringLiteral("decision"), decisionName(trace.decision));
        item.insert(QStringLiteral("status"), simulationTraceStatusName(trace.status));
        item.insert(QStringLiteral("simulation"), true);
        if (trace.command.frameId != 0) {
            item.insert(QStringLiteral("command"), simulationRejectCommandJson(trace.command));
        } else {
            item.insert(QStringLiteral("command"), QJsonValue(QJsonValue::Null));
        }
        item.insert(QStringLiteral("execution"), simulationExecutionJson(trace.execution));
        item.insert(QStringLiteral("errorCode"), QString::fromUtf8(trace.errorCode.c_str()));
        item.insert(QStringLiteral("errorMessage"),
            QString::fromUtf8(trace.errorMessage.c_str()));
        traces.append(item);
    }
    root.insert(QStringLiteral("traces"), traces);

    QSaveFile file(QDir(outputDirectory).filePath(QStringLiteral("simulation-trace.json")));
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 ||
        !file.commit()) {
        errorMessage = file.errorString().isEmpty()
            ? QStringLiteral("simulation trace write failed") : file.errorString();
        return false;
    }
    errorMessage.clear();
    return true;
}

bool loadTensorRtConfig(const QString& configPath, TensorRtDetectorConfig& config,
    QString& errorMessage)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = file.errorString();
        return false;
    }
    const QByteArray configBytes = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(configBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        errorMessage = parseError.errorString();
        return false;
    }
    QString duplicateKey;
    if (hasDuplicateTopLevelJsonKey(configBytes, duplicateKey)) {
        errorMessage = QStringLiteral("TensorRT config contains duplicate key: %1")
            .arg(duplicateKey);
        return false;
    }
    const QJsonObject root = document.object();
    const QSet<QString> allowedKeys = {
        QStringLiteral("schemaVersion"),
        QStringLiteral("enginePath"),
        QStringLiteral("modelSha256"),
        QStringLiteral("inputTensorName"),
        QStringLiteral("outputTensorName"),
        QStringLiteral("inputWidth"),
        QStringLiteral("inputHeight"),
        QStringLiteral("classConfidenceThresholds"),
        QStringLiteral("classNames"),
        QStringLiteral("disabledClassIds"),
        QStringLiteral("detectorVersion"),
        QStringLiteral("parameterVersion"),
        QStringLiteral("preprocessMode")
    };
    for (const QString& key : root.keys()) {
        if (!allowedKeys.contains(key)) {
            errorMessage = QStringLiteral("TensorRT config contains unknown field: %1").arg(key);
            return false;
        }
    }
    if (root.value(QStringLiteral("schemaVersion")).toString() !=
        QStringLiteral("cigvision-tensorrt-detector-v2")) {
        errorMessage = QStringLiteral("unsupported TensorRT config schemaVersion");
        return false;
    }
    const QString enginePath = root.value(QStringLiteral("enginePath")).toString();
    const QJsonArray classNames = root.value(QStringLiteral("classNames")).toArray();
    const QJsonArray classThresholds =
        root.value(QStringLiteral("classConfidenceThresholds")).toArray();
    const QJsonArray disabledClassIds = root.value(QStringLiteral("disabledClassIds")).toArray();
    const QJsonValue inputWidth = root.value(QStringLiteral("inputWidth"));
    const QJsonValue inputHeight = root.value(QStringLiteral("inputHeight"));
    const auto isExactInteger = [](const QJsonValue& value) {
        return value.isDouble() && std::isfinite(value.toDouble()) &&
            std::floor(value.toDouble()) == value.toDouble();
    };
    if (enginePath.isEmpty() || classNames.size() != 9 || classThresholds.size() != 9 ||
        !root.value(QStringLiteral("inputTensorName")).isString() ||
        !root.value(QStringLiteral("outputTensorName")).isString() ||
        !isExactInteger(inputWidth) || !isExactInteger(inputHeight) ||
        inputWidth.toDouble() != 992.0 || inputHeight.toDouble() != 992.0 ||
        !root.value(QStringLiteral("detectorVersion")).isString() ||
        !root.value(QStringLiteral("parameterVersion")).isString() ||
        !root.value(QStringLiteral("modelSha256")).isString() ||
        !root.value(QStringLiteral("preprocessMode")).isString() ||
        !root.value(QStringLiteral("disabledClassIds")).isArray()) {
        errorMessage = QStringLiteral("TensorRT config is missing required typed fields");
        return false;
    }

    TensorRtDetectorConfig loaded;
    const QDir configDirectory = QFileInfo(configPath).absoluteDir();
    loaded.enginePath = utf8(QFileInfo(enginePath).isAbsolute() ? QDir::cleanPath(enginePath) :
        QDir::cleanPath(configDirectory.absoluteFilePath(enginePath)));
    loaded.inputTensorName = utf8(root.value(QStringLiteral("inputTensorName")).toString());
    loaded.outputTensorName = utf8(root.value(QStringLiteral("outputTensorName")).toString());
    loaded.inputWidth = 992U;
    loaded.inputHeight = 992U;
    loaded.detectorVersion = utf8(root.value(QStringLiteral("detectorVersion")).toString());
    loaded.parameterVersion = utf8(root.value(QStringLiteral("parameterVersion")).toString());
    loaded.modelSha256 = utf8(root.value(QStringLiteral("modelSha256")).toString());
    loaded.preprocessMode = utf8(root.value(QStringLiteral("preprocessMode")).toString());
    QSet<QString> uniqueClassNames;
    for (const QJsonValue& value : classNames) {
        const QString className = value.toString();
        if (!value.isString() || className.isEmpty() || uniqueClassNames.contains(className)) {
            errorMessage = QStringLiteral("classNames must contain 9 unique non-empty strings");
            return false;
        }
        uniqueClassNames.insert(className);
        loaded.classNames.push_back(utf8(className));
    }
    for (const QJsonValue& value : classThresholds) {
        if (!value.isDouble() || !std::isfinite(value.toDouble()) ||
            value.toDouble() < 0.0 || value.toDouble() > 1.0) {
            errorMessage = QStringLiteral(
                "classConfidenceThresholds must contain 9 finite values within [0,1]");
            return false;
        }
        loaded.classConfidenceThresholds.push_back(
            static_cast<float>(value.toDouble()));
    }
    for (const QJsonValue& value : disabledClassIds) {
        if (!isExactInteger(value) || value.toDouble() < 0.0 || value.toDouble() > 8.0) {
            errorMessage = QStringLiteral("disabledClassIds must contain integers");
            return false;
        }
        if (!loaded.disabledClassIds.insert(value.toInt()).second) {
            errorMessage = QStringLiteral("disabledClassIds must not contain duplicates");
            return false;
        }
    }
    const QString loadedEnginePath = QString::fromUtf8(loaded.enginePath.c_str());
    QFile engineFile(loadedEnginePath);
    if (!QFileInfo(loadedEnginePath).isFile() || !engineFile.open(QIODevice::ReadOnly)) {
        errorMessage = QStringLiteral("TensorRT engine file does not exist");
        return false;
    }
    const QString actualModelSha256 = QString::fromLatin1(
        QCryptographicHash::hash(engineFile.readAll(), QCryptographicHash::Sha256).toHex());
    if (!isSha256Hex(loaded.modelSha256) ||
        actualModelSha256.compare(QString::fromLatin1(loaded.modelSha256.c_str()),
            Qt::CaseInsensitive) != 0) {
        errorMessage = QStringLiteral("TensorRT engine SHA-256 does not match modelSha256");
        return false;
    }
    loaded.modelSha256 = utf8(actualModelSha256);
    std::string profileError;
    if (!tensorRtParameterProfile(loaded).validate(&profileError)) {
        errorMessage = QString::fromUtf8(profileError.c_str());
        return false;
    }
    config = std::move(loaded);
    errorMessage.clear();
    return true;
}

QJsonObject tensorRtParameterProfileJson(const ProductParameterProfile& profile)
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"),
        QString::fromUtf8(profile.schemaVersion.c_str()));
    root.insert(QStringLiteral("kind"), QString::fromLatin1(
        productParameterProfileKindName(profile.kind)));
    root.insert(QStringLiteral("profileId"), QString::fromUtf8(profile.profileId.c_str()));
    root.insert(QStringLiteral("parameterVersion"),
        QString::fromUtf8(profile.parameterVersion.c_str()));
    root.insert(QStringLiteral("detectorVersion"),
        QString::fromUtf8(profile.detectorVersion.c_str()));
    root.insert(QStringLiteral("modelSha256"),
        QString::fromLatin1(profile.modelSha256.c_str()));
    root.insert(QStringLiteral("inputTensorName"),
        QString::fromUtf8(profile.inputTensorName.c_str()));
    root.insert(QStringLiteral("outputTensorName"),
        QString::fromUtf8(profile.outputTensorName.c_str()));
    root.insert(QStringLiteral("inputWidth"), static_cast<qint64>(profile.inputWidth));
    root.insert(QStringLiteral("inputHeight"), static_cast<qint64>(profile.inputHeight));
    root.insert(QStringLiteral("preprocessMode"),
        QString::fromUtf8(profile.preprocessMode.c_str()));
    root.insert(QStringLiteral("sha256"), QString::fromLatin1(profile.sha256().c_str()));
    QJsonArray classes;
    for (const ProductClassParameter& rule : profile.classes) {
        QJsonObject item;
        item.insert(QStringLiteral("classId"), rule.classId);
        item.insert(QStringLiteral("className"), QString::fromUtf8(rule.className.c_str()));
        item.insert(QStringLiteral("confidenceThreshold"), rule.confidenceThreshold);
        item.insert(QStringLiteral("enabled"), rule.enabled);
        classes.append(item);
    }
    root.insert(QStringLiteral("classes"), classes);
    return root;
}

bool writeTensorRtParameterProfile(const QString& outputDirectory,
    const ProductParameterProfile& profile, QString& errorMessage)
{
    QSaveFile file(QDir(outputDirectory).filePath(QStringLiteral("parameter-profile.json")));
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(tensorRtParameterProfileJson(profile)).toJson(
            QJsonDocument::Indented)) < 0 || !file.commit()) {
        errorMessage = file.errorString().isEmpty()
            ? QStringLiteral("parameter profile write failed") : file.errorString();
        return false;
    }
    errorMessage.clear();
    return true;
}

void writeTensorRtInitializationError(const QString& outputDirectory,
    const QString& errorMessage)
{
    QJsonObject root;
    root.insert(QStringLiteral("errorCode"), QStringLiteral("TENSORRT_INITIALIZATION_FAILED"));
    root.insert(QStringLiteral("errorMessage"), errorMessage);
    QSaveFile file(QDir(outputDirectory).filePath(QStringLiteral("initialization-error.json")));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.commit();
    }
}

} // namespace

QtImageListFrameSource::QtImageListFrameSource(std::vector<QtOfflineInput> inputs,
    IReplayPacer* pacer)
    : inputs_(std::move(inputs)), pacer_(pacer)
{
}

bool QtImageListFrameSource::start(std::string& errorMessage)
{
    if (inputs_.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        errorMessage = "too many replay inputs for cigarette numbering";
        return false;
    }
    for (const QtOfflineInput& input : inputs_) {
        if (input.delayBeforeMicros < 0) {
            errorMessage = "manifest delayBeforeMicros must not be negative";
            return false;
        }
        if (input.delayBeforeMicros > kMaxLocalReplayDelayMicros) {
            errorMessage = "manifest delayBeforeMicros exceeds the local safety limit";
            return false;
        }
    }
    index_ = 0;
    if (pacer_ != nullptr) {
        pacer_->reset();
    }
    running_.store(true);
    errorMessage.clear();
    return true;
}

void QtImageListFrameSource::stop() noexcept
{
    running_.store(false);
    if (pacer_ != nullptr) {
        pacer_->cancel();
    }
}

bool QtImageListFrameSource::tryRead(FramePacket& frame, std::string& errorMessage)
{
    errorMessage.clear();
    if (!running_.load() || index_ >= inputs_.size()) {
        return false;
    }
    const std::size_t currentIndex = index_;
    const QtOfflineInput& input = inputs_[currentIndex];
    if (pacer_ != nullptr && input.delayBeforeMicros > 0) {
        const ReplayWaitStatus waitStatus = pacer_->wait(input.delayBeforeMicros, errorMessage);
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
    ++index_;

    QFile file(input.path);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = utf8(input.path + QStringLiteral(": ") + file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (!input.expectedSha256.isEmpty()) {
        const QString actual = QString::fromLatin1(
            QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()).toUpper();
        if (actual != input.expectedSha256) {
            errorMessage = utf8(input.path + QStringLiteral(": SHA-256 mismatch"));
            return false;
        }
    }

    QImage decoded;
    if (!decoded.loadFromData(bytes)) {
        errorMessage = utf8(input.path + QStringLiteral(": image decode failed"));
        return false;
    }
    const QImage image = decoded.convertToFormat(QImage::Format_RGB888);
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        errorMessage = utf8(input.path + QStringLiteral(": decoded image is empty"));
        return false;
    }

    frame.frameId = static_cast<std::uint64_t>(currentIndex + 1);
    frame.stationId = utf8(input.stationId.isEmpty() ? QStringLiteral("offline") : input.stationId);
    frame.cameraId = utf8(input.cameraId.isEmpty()
        ? QFileInfo(input.path).fileName() : input.cameraId);
    frame.cigaretteNumber = input.cigaretteNumber == 0
        ? static_cast<std::uint32_t>(frame.frameId) : input.cigaretteNumber;
    frame.capturedAt = std::max<TimestampMicros>(1,
        QFileInfo(input.path).lastModified().toMSecsSinceEpoch() * 1000);
    frame.width = static_cast<std::uint32_t>(image.width());
    frame.height = static_cast<std::uint32_t>(image.height());
    frame.strideBytes = frame.width * 3U;
    frame.pixelFormat = PixelFormat::RGB8;
    frame.pixels.resize(static_cast<std::size_t>(frame.strideBytes) * frame.height);
    for (std::uint32_t row = 0; row < frame.height; ++row) {
        std::memcpy(frame.pixels.data() + static_cast<std::size_t>(row) * frame.strideBytes,
            image.constScanLine(static_cast<int>(row)), frame.strideBytes);
    }
    return true;
}

QtAtomicResultSink::QtAtomicResultSink(QString outputDirectory)
    : outputDirectory_(std::move(outputDirectory))
{
    QDir().mkpath(outputDirectory_);
}

bool QtAtomicResultSink::store(const InspectionResult& result, std::string& errorMessage)
{
    decisions_[result.frameId] = result.decision;
    errorMessage.clear();
    return true;
}

QtAtomicFrameArchive::QtAtomicFrameArchive(QString outputDirectory, bool saveAnnotated)
    : outputDirectory_(std::move(outputDirectory)), saveAnnotated_(saveAnnotated)
{
    QDir().mkpath(outputDirectory_);
}

bool QtAtomicFrameArchive::store(const FramePacket& frame, const InspectionResult& result,
    std::string& errorMessage)
{
    const QImage image = imageFromFrame(frame);
    QSaveFile imageFile(QDir(outputDirectory_).filePath(frameBaseName(frame.frameId) +
        QStringLiteral(".png")));
    if (image.isNull() || !imageFile.open(QIODevice::WriteOnly) ||
        !image.save(&imageFile, "PNG") || !imageFile.commit()) {
        errorMessage = utf8(imageFile.errorString().isEmpty()
            ? QStringLiteral("image archive failed") : imageFile.errorString());
        return false;
    }
    QSaveFile resultFile(QDir(outputDirectory_).filePath(frameBaseName(frame.frameId) +
        QStringLiteral(".json")));
    if (!resultFile.open(QIODevice::WriteOnly) ||
        resultFile.write(QJsonDocument(resultJson(result, &frame)).toJson(
            QJsonDocument::Indented)) < 0 || !resultFile.commit()) {
        errorMessage = utf8(resultFile.errorString().isEmpty()
            ? QStringLiteral("result archive failed") : resultFile.errorString());
        return false;
    }
    if (saveAnnotated_) {
        QImage annotated = image;
        QPainter painter(&annotated);
        QPen pen(QColor(255, 48, 48));
        pen.setWidth(2);
        painter.setPen(pen);
        for (const Detection& detection : result.defects) {
            const QRectF rectangle(detection.box.x, detection.box.y,
                detection.box.width, detection.box.height);
            painter.drawRect(rectangle);
            painter.drawText(rectangle.topLeft() + QPointF(2.0, 14.0),
                QStringLiteral("%1 %2").arg(QString::fromUtf8(detection.className.c_str()))
                    .arg(detection.confidence, 0, 'f', 3));
        }
        painter.end();
        QSaveFile annotatedFile(QDir(outputDirectory_).filePath(
            frameBaseName(frame.frameId) + QStringLiteral("-annotated.png")));
        if (!annotatedFile.open(QIODevice::WriteOnly) ||
            !annotated.save(&annotatedFile, "PNG") || !annotatedFile.commit()) {
            errorMessage = utf8(annotatedFile.errorString().isEmpty()
                ? QStringLiteral("annotated image archive failed") : annotatedFile.errorString());
            return false;
        }
    }
    errorMessage.clear();
    return true;
}

OfflineInspectionWorker::OfflineInspectionWorker(QStringList files, QString outputDirectory,
    std::string parameterVersion, std::string parameterSha256)
    : files_(std::move(files)), outputDirectory_(std::move(outputDirectory)),
      parameterVersion_(std::move(parameterVersion)),
      parameterSha256_(std::move(parameterSha256))
{
}

void OfflineInspectionWorker::publish(const FramePacket& frame,
    const InspectionResult& result, const InspectionStatistics& statistics)
{
    QVariantMap values;
    values.insert(QStringLiteral("frameId"), static_cast<qulonglong>(frame.frameId));
    values.insert(QStringLiteral("stationId"), QString::fromUtf8(frame.stationId.c_str()));
    values.insert(QStringLiteral("cameraId"), QString::fromUtf8(frame.cameraId.c_str()));
    values.insert(QStringLiteral("cigaretteNumber"),
        static_cast<qulonglong>(frame.cigaretteNumber));
    values.insert(QStringLiteral("capturedAtMicros"),
        static_cast<qlonglong>(frame.capturedAt));
    values.insert(QStringLiteral("completedAtMicros"),
        static_cast<qlonglong>((std::max)(frame.capturedAt,
            static_cast<TimestampMicros>(QDateTime::currentMSecsSinceEpoch()) * 1000)));
    values.insert(QStringLiteral("elapsedMicros"),
        static_cast<qulonglong>(result.elapsedMicros));
    values.insert(QStringLiteral("parameterVersion"),
        QString::fromUtf8(result.parameterVersion.c_str()));
    values.insert(QStringLiteral("parameterSha256"),
        QString::fromLatin1(result.parameterSha256.c_str()));
    values.insert(QStringLiteral("errorCode"),
        QString::fromUtf8(result.errorCode.c_str()));
    values.insert(QStringLiteral("errorMessage"),
        QString::fromUtf8(result.errorMessage.c_str()));
    QVariantList defects;
    for (const Detection& defect : result.defects) {
        QVariantMap item;
        item.insert(QStringLiteral("classId"), defect.classId);
        item.insert(QStringLiteral("className"),
            QString::fromUtf8(defect.className.c_str()));
        item.insert(QStringLiteral("confidence"), defect.confidence);
        defects.push_back(item);
    }
    values.insert(QStringLiteral("defects"), defects);
    values.insert(QStringLiteral("processed"), static_cast<qulonglong>(statistics.processed));
    values.insert(QStringLiteral("ok"), static_cast<qulonglong>(statistics.ok));
    values.insert(QStringLiteral("ng"), static_cast<qulonglong>(statistics.ng));
    values.insert(QStringLiteral("error"), static_cast<qulonglong>(statistics.error));
    values.insert(QStringLiteral("saveFailures"), static_cast<qulonglong>(statistics.saveFailures));
    values.insert(QStringLiteral("dropped"), static_cast<qulonglong>(statistics.dropped));
    emit frameProcessed(imageFromFrame(frame), decisionName(result.decision), values);
}

void OfflineInspectionWorker::run()
{
    std::vector<QtOfflineInput> inputs;
    for (const QString& file : files_) {
        inputs.push_back({ file, QString(), InspectionDecision::Unknown });
    }
    QString manifestError;
    if (!writeInputManifest(outputDirectory_, inputs, manifestError)) {
        emit finished(QStringLiteral("Offline input manifest failed: %1").arg(manifestError), false);
        return;
    }
    QtImageListFrameSource source(std::move(inputs));
    DeterministicFixtureDetector detector;
    QtAtomicResultSink resultSink(outputDirectory_);
    QtAtomicFrameArchive archive(outputDirectory_);
    SystemClock clock;
    CallbackObserver observer([this](const FramePacket& frame, const InspectionResult& result,
        const InspectionStatistics& statistics) { publish(frame, result, statistics); });
    OfflineRunOptions options;
    options.queueCapacity = 4;
    options.drainOnStop = false;
    options.parameterVersion = parameterVersion_;
    options.parameterSha256 = parameterSha256_;
    const OfflineRunSummary summary = session_.run(source, detector, resultSink, archive,
        clock, &observer, options);
    const bool success = summary.state == OfflineRunState::Completed;
    emit finished(QStringLiteral("Offline chain test complete: %1, OK %2, NG %3, errors %4")
        .arg(static_cast<qulonglong>(summary.statistics.processed))
        .arg(static_cast<qulonglong>(summary.statistics.ok))
        .arg(static_cast<qulonglong>(summary.statistics.ng))
        .arg(static_cast<qulonglong>(summary.statistics.error)), success);
}

int runOfflineBatchManifest(const QString& manifestPath, const QString& outputDirectory)
{
    std::vector<QtOfflineInput> inputs;
    QString manifestError;
    if (!loadManifest(manifestPath, inputs, manifestError) || !QDir().mkpath(outputDirectory)) {
        return 2;
    }
    const std::vector<QtOfflineInput> expectedInputs = inputs;
    if (!writeInputManifest(outputDirectory, expectedInputs, manifestError)) {
        return 3;
    }
    QtImageListFrameSource source(std::move(inputs));
    DeterministicFixtureDetector detector;
    QtAtomicResultSink resultSink(outputDirectory);
    QtAtomicFrameArchive archive(outputDirectory);
    SystemClock clock;
    OfflineInspectionSession session;
    OfflineRunOptions options;
    options.queueCapacity = 4;
    const OfflineRunSummary summary = session.run(source, detector, resultSink, archive,
        clock, nullptr, options);

    QStringList mismatches;
    for (std::size_t index = 0; index < expectedInputs.size(); ++index) {
        const std::uint64_t frameId = static_cast<std::uint64_t>(index + 1);
        const auto actual = resultSink.decisions().find(frameId);
        if (actual == resultSink.decisions().end() ||
            actual->second != expectedInputs[index].expectedDecision) {
            mismatches.append(QStringLiteral("frame %1 decision mismatch").arg(frameId));
        }
    }
    const bool complete = summary.state == OfflineRunState::Completed && mismatches.isEmpty() &&
        summary.statistics.processed == expectedInputs.size();
    if (!writeSummary(outputDirectory, summary, mismatches)) {
        return 3;
    }
    return complete ? 0 : 1;
}

int runTensorRtBatchManifest(const QString& manifestPath, const QString& outputDirectory,
    const QString& detectorConfigPath)
{
    std::vector<QtOfflineInput> inputs;
    QString errorMessage;
    if (!loadManifest(manifestPath, inputs, errorMessage, false) ||
        !QDir().mkpath(outputDirectory)) {
        return 2;
    }
    const std::vector<QtOfflineInput> tracedInputs = inputs;
    if (!writeInputManifest(outputDirectory, tracedInputs, errorMessage)) {
        return 3;
    }

    TensorRtDetectorConfig detectorConfig;
    if (!loadTensorRtConfig(detectorConfigPath, detectorConfig, errorMessage)) {
        writeTensorRtInitializationError(outputDirectory, errorMessage);
        return 4;
    }
    const ProductParameterProfile parameterProfile =
        tensorRtParameterProfile(detectorConfig);
    if (!writeTensorRtParameterProfile(outputDirectory, parameterProfile, errorMessage)) {
        writeTensorRtInitializationError(outputDirectory, errorMessage);
        return 3;
    }
    try {
        QtImageListFrameSource source(std::move(inputs));
        TensorRtDetector detector(detectorConfig);
        QtAtomicResultSink resultSink(outputDirectory);
        QtAtomicFrameArchive archive(outputDirectory, true);
        SystemClock clock;
        OfflineInspectionSession session;
        OfflineRunOptions options;
        options.queueCapacity = 4;
        options.parameterVersion = parameterProfile.parameterVersion;
        options.parameterSha256 = parameterProfile.sha256();
        const OfflineRunSummary summary = session.run(source, detector, resultSink, archive,
            clock, nullptr, options);
        const bool complete = summary.state == OfflineRunState::Completed &&
            summary.statistics.processed == tracedInputs.size();
        if (!writeSummary(outputDirectory, summary, QStringList())) {
            return 3;
        }
        return complete ? 0 : 1;
    }
    catch (const std::exception& error) {
        writeTensorRtInitializationError(outputDirectory, QString::fromUtf8(error.what()));
        return 4;
    }
    catch (...) {
        writeTensorRtInitializationError(outputDirectory,
            QStringLiteral("unknown TensorRT initialization exception"));
        return 4;
    }
}

int runSimulationBatchManifest(const QString& manifestPath, const QString& outputDirectory,
    TimestampMicros rejectDelayMicros, std::size_t queueCapacity,
    const QString& targetOutput)
{
    std::vector<QtOfflineInput> inputs;
    QString errorMessage;
    if (!loadManifest(manifestPath, inputs, errorMessage, false) ||
        !QDir().mkpath(outputDirectory)) {
        return 2;
    }
    const std::vector<QtOfflineInput> tracedInputs = inputs;
    if (!writeInputManifest(outputDirectory, tracedInputs, errorMessage)) {
        return 3;
    }

    try {
        SteadyReplayPacer pacer;
        QtImageListFrameSource source(std::move(inputs), &pacer);
        DeterministicFixtureDetector detector(
            "simulation-fixture-v1",
            "4f92b0acb709f803e96e1e5007f542a0e5e0bca93f1fc7a71bb8547523f27065");
        QtAtomicResultSink resultSink(outputDirectory);
        QtAtomicFrameArchive archive(outputDirectory);
        SystemClock clock;
        SimulationRejectOutput output(clock);
        SimulationRejectOptions rejectOptions;
        rejectOptions.delayMicros = rejectDelayMicros;
        rejectOptions.targetOutput = utf8(targetOutput);
        SimulationRejectObserver observer(clock, output, rejectOptions);
        OfflineInspectionSession session;
        OfflineRunOptions options;
        options.queueCapacity = queueCapacity;
        options.parameterVersion = "simulation-fixture-v1";
        options.parameterSha256 =
            "4f92b0acb709f803e96e1e5007f542a0e5e0bca93f1fc7a71bb8547523f27065";
        const OfflineRunSummary summary = session.run(source, detector, resultSink, archive,
            clock, &observer, options);

        QStringList mismatches;
        for (std::size_t index = 0; index < tracedInputs.size(); ++index) {
            if (tracedInputs[index].expectedDecision == InspectionDecision::Unknown) {
                continue;
            }
            const std::uint64_t frameId = static_cast<std::uint64_t>(index + 1);
            const auto actual = resultSink.decisions().find(frameId);
            if (actual == resultSink.decisions().end() ||
                actual->second != tracedInputs[index].expectedDecision) {
                mismatches.append(QStringLiteral("frame %1 decision mismatch").arg(frameId));
            }
        }

        if (!writeSummary(outputDirectory, summary, mismatches)) {
            return 3;
        }
        if (!writeSimulationTrace(outputDirectory, manifestPath, rejectDelayMicros,
                queueCapacity, targetOutput, summary, observer, errorMessage)) {
            return 3;
        }

        const SimulationRejectStatistics& simulationStatistics = observer.statistics();
        const bool traceValid = validateSimulationTrace(observer.traces(), simulationStatistics);
        const bool complete = summary.state == OfflineRunState::Completed &&
            mismatches.isEmpty() && summary.statistics.processed == tracedInputs.size() &&
            simulationStatistics.observed == summary.statistics.processed &&
            traceValid &&
            simulationStatistics.failed == 0 &&
            simulationStatistics.commands == simulationStatistics.simulated &&
            output.commands().size() == simulationStatistics.commands;
        return complete ? 0 : 1;
    }
    catch (const std::exception& error) {
        QJsonObject root;
        root.insert(QStringLiteral("errorCode"), QStringLiteral("SIMULATION_INITIALIZATION_FAILED"));
        root.insert(QStringLiteral("errorMessage"), QString::fromUtf8(error.what()));
        QSaveFile file(QDir(outputDirectory).filePath(QStringLiteral("initialization-error.json")));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            file.commit();
        }
        return 4;
    }
    catch (...) {
        QJsonObject root;
        root.insert(QStringLiteral("errorCode"), QStringLiteral("SIMULATION_INITIALIZATION_FAILED"));
        root.insert(QStringLiteral("errorMessage"), QStringLiteral("unknown simulation exception"));
        QSaveFile file(QDir(outputDirectory).filePath(QStringLiteral("initialization-error.json")));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            file.commit();
        }
        return 4;
    }
}

} // namespace cigvision
