#pragma once

#include "core/OfflineInspection.h"
#include "core/RealtimeSimulation.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <cstddef>
#include <cstdint>
#include <map>
#include <atomic>
#include <utility>
#include <vector>

namespace cigvision {

struct QtOfflineInput {
    QtOfflineInput() = default;
    QtOfflineInput(QString inputPath, QString sha256, InspectionDecision decision)
        : path(std::move(inputPath)), expectedSha256(std::move(sha256)),
          expectedDecision(decision) {}
    QString path;
    QString expectedSha256;
    InspectionDecision expectedDecision = InspectionDecision::Unknown;
    QString stationId;
    QString cameraId;
    std::uint32_t cigaretteNumber = 0;
    TimestampMicros delayBeforeMicros = 0;
};

class QtImageListFrameSource final : public IFrameSource {
public:
    explicit QtImageListFrameSource(std::vector<QtOfflineInput> inputs,
        IReplayPacer* pacer = nullptr);
    bool start(std::string& errorMessage) override;
    void stop() noexcept override;
    bool tryRead(FramePacket& frame, std::string& errorMessage) override;

private:
    std::vector<QtOfflineInput> inputs_;
    IReplayPacer* pacer_ = nullptr;
    std::size_t index_ = 0;
    std::atomic<bool> running_{ false };
};

class QtAtomicResultSink final : public IInspectionResultSink {
public:
    explicit QtAtomicResultSink(QString outputDirectory);
    bool store(const InspectionResult& result, std::string& errorMessage) override;
    const std::map<std::uint64_t, InspectionDecision>& decisions() const { return decisions_; }

private:
    QString outputDirectory_;
    std::map<std::uint64_t, InspectionDecision> decisions_;
};

class QtAtomicFrameArchive final : public IFrameArchive {
public:
    explicit QtAtomicFrameArchive(QString outputDirectory, bool saveAnnotated = false);
    bool store(const FramePacket& frame, const InspectionResult& result,
        std::string& errorMessage) override;

private:
    QString outputDirectory_;
    bool saveAnnotated_ = false;
};

class OfflineInspectionWorker final : public QObject {
    Q_OBJECT
public:
    OfflineInspectionWorker(QStringList files, QString outputDirectory,
        std::string parameterVersion, std::string parameterSha256);
    void requestStop() noexcept { session_.requestStop(); }
    void publish(const FramePacket& frame, const InspectionResult& result,
        const InspectionStatistics& statistics);

public slots:
    void run();

signals:
    void frameProcessed(const QImage& image, const QString& resultText,
        const QVariantMap& statistics);
    void finished(const QString& message, bool success);

private:
    QStringList files_;
    QString outputDirectory_;
    std::string parameterVersion_;
    std::string parameterSha256_;
    OfflineInspectionSession session_;
};

int runOfflineBatchManifest(const QString& manifestPath, const QString& outputDirectory);
int runTensorRtBatchManifest(const QString& manifestPath, const QString& outputDirectory,
    const QString& detectorConfigPath);
int runSimulationBatchManifest(const QString& manifestPath, const QString& outputDirectory,
    TimestampMicros rejectDelayMicros, std::size_t queueCapacity,
    const QString& targetOutput);

} // namespace cigvision
