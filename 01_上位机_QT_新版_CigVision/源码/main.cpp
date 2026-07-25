#include "CigVision.h"
#include <QtWidgets/QApplication>
#include <qwidget.h>
#include <qvboxlayout>
#include "core/BatchCommandLine.h"
#include "adapters/qt/QtOfflineInspection.h"

#include <string>
#include <vector>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    const QStringList arguments = QCoreApplication::arguments();
    std::vector<std::string> commandLine;
    commandLine.reserve(static_cast<std::size_t>(arguments.size()));
    for (const QString& argument : arguments) {
        const QByteArray bytes = argument.toUtf8();
        commandLine.emplace_back(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    }

    cigvision::BatchCommandLine batch;
    std::string batchError;
    if (!cigvision::parseBatchCommandLine(commandLine, batch, batchError)) {
        return 2;
    }
    if (batch.mode == cigvision::BatchCommandMode::OfflineFixture) {
        return cigvision::runOfflineBatchManifest(
            QString::fromUtf8(batch.manifestPath.c_str()),
            QString::fromUtf8(batch.outputDirectory.c_str()));
    }
    if (batch.mode == cigvision::BatchCommandMode::TensorRt) {
        return cigvision::runTensorRtBatchManifest(
            QString::fromUtf8(batch.manifestPath.c_str()),
            QString::fromUtf8(batch.outputDirectory.c_str()),
            QString::fromUtf8(batch.detectorConfigPath.c_str()));
    }
    if (batch.mode == cigvision::BatchCommandMode::Simulation) {
        return cigvision::runSimulationBatchManifest(
            QString::fromUtf8(batch.manifestPath.c_str()),
            QString::fromUtf8(batch.outputDirectory.c_str()),
            batch.simulationRejectDelayMicros, batch.simulationQueueCapacity,
            QString::fromUtf8(batch.simulationTargetOutput.c_str()));
    }

    // P5-P8 are local-only phases. The product UI therefore has no runtime
    // path that initializes cameras or DAQNavi; hardware work must be restored
    // in a separately approved future phase.
    CigVision w(nullptr, true);

    //运行界面
    QVBoxLayout* layout = new QVBoxLayout(&w);
    (void)layout;

    //参数设置界面
    //统计查询界面
    //系统设置 界面

    //w.showFullScreen();
    w.resize(1920, 1080);
    w.setFixedSize(1920, 1080);
    w.show();
    return a.exec();
}
