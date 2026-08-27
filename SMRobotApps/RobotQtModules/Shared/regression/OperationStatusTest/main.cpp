#include "RobotQtViewerEventHub.h"
#include "RobotQtViewerLocalization.h"
#include "RobotQtViewerOperationStatus.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>
#include <stdexcept>

namespace
{
    using namespace robot_qt_viewer;

    void require(bool condition, const char* message)
    {
        if(!condition) {
            throw std::runtime_error(message);
        }
    }

    void writeCatalog(const QString& path)
    {
        static const QByteArray catalog = R"json({
  "schema": "smrobot.localization.catalog",
  "version": 1,
  "locale": "en-US",
  "nativeName": "English",
  "fallbackLocale": "",
  "messages": {
    "status.operation.projectOpen.title": "Opening %1",
    "status.operation.projectOpen.step.read": "Reading project",
    "status.operation.projectOpen.succeeded": "Opened %1",
    "status.operation.running.withStep": "%1 - %2",
    "status.operation.running.stepProgress": "%1 (%2/%3)",
    "status.operation.state.running": "In progress",
    "status.operation.state.succeeded": "Succeeded",
    "status.operation.state.failed": "Failed",
    "status.operation.state.canceled": "Canceled",
    "status.operation.elapsed": " - %1 ms",
    "status.operation.historyItem": "%1  %2  %3"
  },
  "legacy": {}
})json";
        QFile file(path);
        require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "cannot create operation status catalog");
        require(file.write(catalog) == catalog.size(),
            "cannot write operation status catalog");
    }
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QObject receiver;
    RobotQtViewerEventHub eventHub;
    int eventCount = 0;
    RobotQtViewerOperationStatus lastEvent;
    eventHub.subscribe(
        RobotQtViewerEventKind::OperationStatusChanged,
        &receiver,
        [&](const RobotQtViewerEvent& event) {
            ++eventCount;
            lastEvent = event.operationStatus;
        });

    RobotQtViewerOperationStatusStore store(eventHub);
    const QString operationId = store.begin(
        QStringLiteral("test"),
        QStringLiteral("status.operation.projectOpen.title"),
        QStringLiteral("Opening %1"),
        QStringLiteral("stewart.sys.json"),
        3);
    require(!operationId.isEmpty() && eventCount == 1,
        "operation start was not published");
    require(store.reportProgress(
            operationId,
            QStringLiteral("status.operation.projectOpen.step.read"),
            QStringLiteral("Reading project"),
            1,
            3,
            1,
            3),
        "valid operation progress was rejected");
    require(!store.reportProgress(
            operationId,
            QStringLiteral("status.operation.projectOpen.step.read"),
            QStringLiteral("Reading project"),
            0,
            3,
            0,
            3),
        "non-monotonic operation progress was accepted");
    require(store.succeed(
            operationId,
            QStringLiteral("status.operation.projectOpen.succeeded"),
            QStringLiteral("Opened %1")),
        "operation success was rejected");
    require(!store.fail(
            operationId,
            QStringLiteral("status.operation.projectOpen.failed"),
            QStringLiteral("Failed to open %1")),
        "a second terminal operation state was accepted");
    require(lastEvent.state == RobotQtViewerOperationState::Succeeded && eventCount == 3,
        "operation lifecycle events are inconsistent");

    QTemporaryDir catalogDirectory;
    require(catalogDirectory.isValid(), "temporary catalog directory is unavailable");
    writeCatalog(catalogDirectory.filePath(QStringLiteral("base.en-US.i18n.json")));
    RobotQtViewerLocalizationService localization(catalogDirectory.path());
    QString error;
    require(localization.reloadCatalogs(&error), error.toUtf8().constData());
    require(localization.setLanguage(QStringLiteral("en-US"), false, &error),
        error.toUtf8().constData());

    RobotQtViewerOperationStatusPresenter presenter(localization);
    const RobotQtViewerOperationStatusViewModel viewModel =
        presenter.build(store.history(), operationId);
    require(viewModel.currentMessage == QStringLiteral("Opened stewart.sys.json"),
        "terminal operation summary was not localized");
    require(!viewModel.historyItems.isEmpty() && !viewModel.progressVisible,
        "terminal operation presentation is inconsistent");

    const QString failedOperationId = store.begin(
        QStringLiteral("testFailure"),
        QStringLiteral("status.operation.projectOpen.title"),
        QStringLiteral("Opening %1"),
        QStringLiteral("missing.sys.json"),
        3);
    require(store.fail(
            failedOperationId,
            QStringLiteral("status.operation.projectOpen.failed"),
            QStringLiteral("Failed to open %1"),
            QStringLiteral("project.open.read_failed"),
            QStringLiteral("file not found")),
        "operation failure was rejected");
    const RobotQtViewerOperationStatusViewModel failedViewModel =
        presenter.build(store.history(), failedOperationId);
    require(failedViewModel.currentMessage == QStringLiteral("Failed to open missing.sys.json") &&
            failedViewModel.timeoutMs == 8000,
        "failed operation presentation is inconsistent");

    std::cout << "Operation status tests passed.\n";
    return 0;
}
