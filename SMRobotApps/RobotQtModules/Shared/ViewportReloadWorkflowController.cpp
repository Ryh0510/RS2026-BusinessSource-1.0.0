#include "ViewportReloadWorkflowController.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerOperationStatus.h"
#include "RobotQtViewerViewportServices.h"

#include <SimulationProject/ProjectDocument.h>
#include <SimulationProject/ProjectSession.h>
#include <SimulationProject/RuntimePaths.h>

#include <chrono>
#include <data_path.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace
{
    long long elapsedMilliseconds(const std::chrono::steady_clock::time_point& start)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
            .count();
    }

    void printProfileRow(const char* stage, long long ms, const QString& detail)
    {
        std::ostringstream out;
        out << "| " << std::left << std::setw(32) << stage
            << " | " << std::right << std::setw(10) << ms
            << " ms | " << detail.toStdString() << "\n";
        std::cout << out.str();
    }
}

namespace robot_qt_viewer
{
    ViewportReloadWorkflowController::ViewportReloadWorkflowController(RobotQtViewerDocumentContext& context)
        : m_context(context)
    {
    }

    ViewportReloadWorkflowResult ViewportReloadWorkflowController::reload(
        const QString& sourceId,
        const QString& operationId)
    {
        const auto reloadStart = std::chrono::steady_clock::now();

        ViewportReloadWorkflowResult result;
        result.sourceId = sourceId;
        result.operationId = operationId;

        if(!operationId.isEmpty()) {
            m_context.operationStatusStore().reportProgress(
                operationId,
                QStringLiteral("status.operation.projectOpen.step.viewport"),
                QStringLiteral("Rebuilding viewport scene"),
                3,
                3,
                2,
                3);
        }

        const auto requestStart = std::chrono::steady_clock::now();
        m_context.documentController().publishViewportReloadRequested(sourceId);
        printProfileRow("Viewport publish request", elapsedMilliseconds(requestStart), sourceId);

        RobotQtViewerViewportServices* viewportServices = m_context.viewportServices();
        if(viewportServices == nullptr) {
            result.errorMessage = QStringLiteral("Viewport is not available.");
            result.elapsedMs = elapsedMilliseconds(reloadStart);
            printProfileRow("Viewport reload total", result.elapsedMs, result.errorMessage);
            m_context.documentController().publishViewportReloaded(false, sourceId);
            if(!operationId.isEmpty()) {
                m_context.operationStatusStore().fail(
                    operationId,
                    QStringLiteral("status.operation.projectOpen.failed"),
                    QStringLiteral("Failed to open %1"),
                    QStringLiteral("project.open.viewport_unavailable"),
                    result.errorMessage);
            }
            return result;
        }

        std::filesystem::path basePath = simulation_project::RuntimePaths::applicationRoot();
        if(!m_context.projectSession().path().empty()) {
            basePath = m_context.projectSession().path().parent_path();
        }

        result.showCollisionGeometry = false;

        const auto loadStart = std::chrono::steady_clock::now();
        const RobotQtViewerViewportLoadResult loadResult =
            viewportServices->loadProjectDocument(m_context.document(), basePath);
        printProfileRow(
            "Viewport load document",
            elapsedMilliseconds(loadStart),
            loadResult.success ? QString::fromStdWString(basePath.wstring()) : loadResult.errorMessage);
        if(!loadResult.success) {
            result.errorMessage = loadResult.errorMessage.isEmpty()
                ? QStringLiteral("Failed to rebuild viewport scene.")
                : QString("Failed to rebuild viewport scene: %1").arg(loadResult.errorMessage);
            result.elapsedMs = elapsedMilliseconds(reloadStart);
            printProfileRow("Viewport reload total", result.elapsedMs, result.errorMessage);
            m_context.documentController().publishViewportReloaded(false, sourceId);
            if(!operationId.isEmpty()) {
                m_context.operationStatusStore().fail(
                    operationId,
                    QStringLiteral("status.operation.projectOpen.failed"),
                    QStringLiteral("Failed to open %1"),
                    QStringLiteral("project.open.viewport_rebuild_failed"),
                    result.errorMessage);
            }
            return result;
        }

        const auto visibilityStart = std::chrono::steady_clock::now();
        viewportServices->setCollisionGeometryVisible(result.showCollisionGeometry);
        printProfileRow(
            "Viewport collision visible",
            elapsedMilliseconds(visibilityStart),
            result.showCollisionGeometry ? QStringLiteral("true") : QStringLiteral("false"));

        result.success = true;
        result.robotCount = static_cast<int>(m_context.document().robots.size());
        result.objectCount = static_cast<int>(m_context.document().objects.size());
        result.detectorCount = static_cast<int>(m_context.document().collision.detectors.size());
        result.elapsedMs = elapsedMilliseconds(reloadStart);
        printProfileRow(
            "Viewport reload total",
            result.elapsedMs,
            QString("robots=%1 objects=%2 detectors=%3")
                .arg(result.robotCount)
                .arg(result.objectCount)
                .arg(result.detectorCount));
        m_context.documentController().publishViewportReloaded(true, sourceId);
        if(!operationId.isEmpty()) {
            m_context.operationStatusStore().succeed(
                operationId,
                QStringLiteral("status.operation.projectOpen.succeeded"),
                QStringLiteral("Opened %1"));
        }
        return result;
    }
}
