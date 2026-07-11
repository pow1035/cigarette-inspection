#pragma once

#include "core/OfflineInspection.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <map>
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
};

class QtImageListFrameSource final : public IFrameSource {
public:
    explicit QtImageListFrameSource(std::vector<QtOfflineInput> inputs);
    bool start(std::string& errorMessage) override;
    void stop() noexcept override;
    bool tryRead(FramePacket& frame, std::string& errorMessage) override;

private:
    std::vector<QtOfflineInput> inputs_;
    std::size_t index_ = 0;
    bool running_ = false;
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
    OfflineInspectionWorker(QStringList files, QString outputDirectory);
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
    OfflineInspectionSession session_;
};

int runOfflineBatchManifest(const QString& manifestPath, const QString& outputDirectory);
int runTensorRtBatchManifest(const QString& manifestPath, const QString& outputDirectory,
    const QString& detectorConfigPath);

} // namespace cigvision
