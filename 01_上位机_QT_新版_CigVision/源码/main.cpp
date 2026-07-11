#include "CigVision.h"
#include <QtWidgets/QApplication>
#include <qwidget.h>
#include<qvboxlayout>
#include "adapters/qt/QtOfflineInspection.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    const QStringList arguments = QCoreApplication::arguments();
    const int tensorRtManifestIndex = arguments.indexOf(
        QStringLiteral("--tensorrt-batch-manifest"));
    const int detectorConfigIndex = arguments.indexOf(QStringLiteral("--detector-config"));
    const int manifestIndex = arguments.indexOf(QStringLiteral("--offline-batch-manifest"));
    const int outputIndex = arguments.indexOf(QStringLiteral("--offline-output"));
    const bool hasBatchArgument = tensorRtManifestIndex >= 0 || manifestIndex >= 0 ||
        outputIndex >= 0 || detectorConfigIndex >= 0;
    if (hasBatchArgument) {
        const bool tensorRtMode = tensorRtManifestIndex >= 0;
        const bool fixtureMode = manifestIndex >= 0;
        const auto hasSingleValue = [&arguments](const QString& option) {
            const int index = arguments.indexOf(option);
            return arguments.count(option) == 1 && index >= 0 && index + 1 < arguments.size() &&
                !arguments[index + 1].startsWith(QStringLiteral("--"));
        };
        const bool commonValid = tensorRtMode != fixtureMode &&
            hasSingleValue(QStringLiteral("--offline-output"));
        const bool modeValid = tensorRtMode
            ? hasSingleValue(QStringLiteral("--tensorrt-batch-manifest")) &&
                hasSingleValue(QStringLiteral("--detector-config")) && manifestIndex < 0
            : hasSingleValue(QStringLiteral("--offline-batch-manifest")) &&
                detectorConfigIndex < 0 && tensorRtManifestIndex < 0;
        if (!commonValid || !modeValid) {
            return 2;
        }
    }
    if (tensorRtManifestIndex >= 0) {
        return cigvision::runTensorRtBatchManifest(arguments[tensorRtManifestIndex + 1],
            arguments[outputIndex + 1], arguments[detectorConfigIndex + 1]);
    }
    if (manifestIndex >= 0) {
        return cigvision::runOfflineBatchManifest(arguments[manifestIndex + 1],
            arguments[outputIndex + 1]);
    }

    CigVision w(nullptr, arguments.contains(QStringLiteral("--offline")));
    
    //运行界面
    QVBoxLayout* layout = new QVBoxLayout(&w);
    


    //参数设置界面

    //统计查询界面 

    //系统设置 界面

    //w.showFullScreen();
    w.resize(1920, 1080);
    w.setFixedSize(1920, 1080);
    w.show();
    return a.exec();
}

