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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

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

QString decisionName(InspectionDecision decision)
{
    switch (decision) {
    case InspectionDecision::Ok: return QStringLiteral("OK");
    case InspectionDecision::Ng: return QStringLiteral("NG");
    case InspectionDecision::Error: return QStringLiteral("ERROR");
    default: return QStringLiteral("UNKNOWN");
    }
}

QJsonObject resultJson(const InspectionResult& result, const FramePacket* frame)
{
    QJsonObject root;
    root.insert(QStringLiteral("frameId"), static_cast<qint64>(result.frameId));
    root.insert(QStringLiteral("decision"), decisionName(result.decision));
    root.insert(QStringLiteral("elapsedMicros"), static_cast<qint64>(result.elapsedMicros));
    root.insert(QStringLiteral("parameterVersion"), QString::fromUtf8(result.parameterVersion.c_str()));
    root.insert(QStringLiteral("errorCode"), QString::fromUtf8(result.errorCode.c_str()));
    root.insert(QStringLiteral("errorMessage"), QString::fromUtf8(result.errorMessage.c_str()));
    if (frame != nullptr) {
        root.insert(QStringLiteral("stationId"), QString::fromUtf8(frame->stationId.c_str()));
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
    for (const QJsonValue& value : samples) {
        if (!value.isObject()) {
            errorMessage = QStringLiteral("each manifest sample must be an object");
            return false;
        }
        const QJsonObject sample = value.toObject();
        const QString relativePath = sample.value(QStringLiteral("path")).toString();
        const QString sha256 = sample.value(QStringLiteral("sha256")).toString();
        const QString expected = sample.value(QStringLiteral("expected")).toString();
        const bool expectedValid = expected == QStringLiteral("OK") ||
            expected == QStringLiteral("NG");
        if (relativePath.isEmpty() || !sha256Pattern.match(sha256).hasMatch() ||
            (requireExpected && !expectedValid) || (!expected.isEmpty() && !expectedValid)) {
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
        inputs.push_back(input);
    }
    return true;
}

bool writeInputManifest(const QString& outputDirectory,
    const std::vector<QtOfflineInput>& inputs, QString& errorMessage)
{
    QJsonArray samples;
    for (const QtOfflineInput& input : inputs) {
        QFile file(input.path);
        if (!file.open(QIODevice::ReadOnly)) {
            errorMessage = input.path + QStringLiteral(": ") + file.errorString();
            return false;
        }
        QJsonObject item;
        item.insert(QStringLiteral("sourcePath"), QFileInfo(input.path).absoluteFilePath());
        item.insert(QStringLiteral("sha256"), QString::fromLatin1(
            QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex()));
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

bool loadTensorRtConfig(const QString& configPath, TensorRtDetectorConfig& config,
    QString& errorMessage)
{
    QFile file(configPath);
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
    const QString enginePath = root.value(QStringLiteral("enginePath")).toString();
    const QJsonArray classNames = root.value(QStringLiteral("classNames")).toArray();
    const QJsonArray disabledClassIds = root.value(QStringLiteral("disabledClassIds")).toArray();
    const QJsonValue inputWidth = root.value(QStringLiteral("inputWidth"));
    const QJsonValue inputHeight = root.value(QStringLiteral("inputHeight"));
    const auto isExactInteger = [](const QJsonValue& value) {
        return value.isDouble() && std::isfinite(value.toDouble()) &&
            std::floor(value.toDouble()) == value.toDouble();
    };
    if (enginePath.isEmpty() || classNames.size() != 9 ||
        !root.value(QStringLiteral("inputTensorName")).isString() ||
        !root.value(QStringLiteral("outputTensorName")).isString() ||
        !isExactInteger(inputWidth) || !isExactInteger(inputHeight) ||
        inputWidth.toDouble() != 992.0 || inputHeight.toDouble() != 992.0 ||
        !root.value(QStringLiteral("confidenceThreshold")).isDouble() ||
        !root.value(QStringLiteral("detectorVersion")).isString() ||
        !root.value(QStringLiteral("preprocessMode")).isString() ||
        !root.value(QStringLiteral("disabledClassIds")).isArray()) {
        errorMessage = QStringLiteral("TensorRT config is missing required typed fields");
        return false;
    }

    const QDir configDirectory = QFileInfo(configPath).absoluteDir();
    config.enginePath = utf8(QFileInfo(enginePath).isAbsolute() ? QDir::cleanPath(enginePath) :
        QDir::cleanPath(configDirectory.absoluteFilePath(enginePath)));
    config.inputTensorName = utf8(root.value(QStringLiteral("inputTensorName")).toString());
    config.outputTensorName = utf8(root.value(QStringLiteral("outputTensorName")).toString());
    config.inputWidth = 992U;
    config.inputHeight = 992U;
    config.confidenceThreshold = static_cast<float>(
        root.value(QStringLiteral("confidenceThreshold")).toDouble());
    config.detectorVersion = utf8(root.value(QStringLiteral("detectorVersion")).toString());
    config.preprocessMode = utf8(root.value(QStringLiteral("preprocessMode")).toString());
    for (const QJsonValue& value : classNames) {
        if (!value.isString() || value.toString().isEmpty()) {
            errorMessage = QStringLiteral("classNames must contain 9 non-empty strings");
            return false;
        }
        config.classNames.push_back(utf8(value.toString()));
    }
    for (const QJsonValue& value : disabledClassIds) {
        if (!isExactInteger(value) || value.toDouble() < 0.0 || value.toDouble() > 8.0) {
            errorMessage = QStringLiteral("disabledClassIds must contain integers");
            return false;
        }
        config.disabledClassIds.insert(value.toInt());
    }
    if (!QFileInfo(QString::fromUtf8(config.enginePath.c_str())).isFile()) {
        errorMessage = QStringLiteral("TensorRT engine file does not exist");
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

QtImageListFrameSource::QtImageListFrameSource(std::vector<QtOfflineInput> inputs)
    : inputs_(std::move(inputs))
{
}

bool QtImageListFrameSource::start(std::string& errorMessage)
{
    index_ = 0;
    running_ = true;
    errorMessage.clear();
    return true;
}

void QtImageListFrameSource::stop() noexcept
{
    running_ = false;
}

bool QtImageListFrameSource::tryRead(FramePacket& frame, std::string& errorMessage)
{
    errorMessage.clear();
    if (!running_ || index_ >= inputs_.size()) {
        return false;
    }
    const std::size_t currentIndex = index_++;
    const QtOfflineInput& input = inputs_[currentIndex];

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
    frame.stationId = "offline";
    frame.cameraId = utf8(QFileInfo(input.path).fileName());
    frame.cigaretteNumber = static_cast<std::uint32_t>(frame.frameId);
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

OfflineInspectionWorker::OfflineInspectionWorker(QStringList files, QString outputDirectory)
    : files_(std::move(files)), outputDirectory_(std::move(outputDirectory))
{
}

void OfflineInspectionWorker::publish(const FramePacket& frame,
    const InspectionResult& result, const InspectionStatistics& statistics)
{
    QVariantMap values;
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
    try {
        QtImageListFrameSource source(std::move(inputs));
        TensorRtDetector detector(detectorConfig);
        QtAtomicResultSink resultSink(outputDirectory);
        QtAtomicFrameArchive archive(outputDirectory, true);
        SystemClock clock;
        OfflineInspectionSession session;
        OfflineRunOptions options;
        options.queueCapacity = 4;
        options.parameterVersion = detectorConfig.detectorVersion;
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

} // namespace cigvision
