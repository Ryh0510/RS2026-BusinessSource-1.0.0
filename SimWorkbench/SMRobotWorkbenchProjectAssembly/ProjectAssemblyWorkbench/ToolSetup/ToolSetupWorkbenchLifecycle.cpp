#include "ToolSetupWorkbenchLifecycle.h"

#include "ToolSetupModuleController.h"

namespace robot_qt_viewer
{
    bool registerProjectAssemblyWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog,
        RobotQtViewerWorkbenchPackageSource source)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.project-assembly");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Project Assembly"), source))) {
            return false;
        }
        if(!catalog.registerMode(makeRobotQtViewerWorkbenchMode(
               packageId,
               RobotQtViewerWorkbenchKind::Browse,
               QStringLiteral("projectAssemblyWorkbench"),
               10,
               { QStringLiteral("smrobot.feature.project-assembly") })) ||
            !catalog.registerMode(makeRobotQtViewerWorkbenchMode(
               packageId,
               RobotQtViewerWorkbenchKind::ToolSetup,
               QStringLiteral("toolSetupWorkbench"),
               20,
               { QStringLiteral("smrobot.feature.tool-setup") },
               { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }))) {
            return false;
        }
        return catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
                   QStringLiteral("smrobot.feature.project-assembly"),
                   QStringLiteral("Project Assembly"),
                   packageId,
                   { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) })) &&
            catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
                   QStringLiteral("smrobot.feature.tool-setup"),
                   QStringLiteral("Tool Setup"),
                   packageId,
                   { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::ToolSetup) }));
    }

    ToolSetupWorkbenchLifecycle::ToolSetupWorkbenchLifecycle(
        ToolSetupModuleController& controller)
        : m_controller(controller)
    {
    }

    RobotQtViewerWorkbenchTransitionResult
    ToolSetupWorkbenchLifecycle::prepareDeactivate(
        const RobotQtViewerWorkbenchTransitionContext& context)
    {
        if(!m_controller.hasPendingTaskChanges()) {
            return workbenchTransitionSucceeded();
        }
        const bool restoreEditorTarget =
            context.cause != RobotQtViewerWorkbenchTransitionCause::ProjectReplacing &&
            context.cause != RobotQtViewerWorkbenchTransitionCause::ApplicationShutdown;
        if(!m_controller.resolvePendingTaskChanges(context.promptParent, restoreEditorTarget)) {
            return workbenchTransitionRejected(
                QStringLiteral("Tool Setup has unresolved changes."),
                QStringLiteral("tool_setup.pending_changes"));
        }
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult ToolSetupWorkbenchLifecycle::deactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        if(m_controller.hasPendingTaskChanges()) {
            return workbenchTransitionFailed(
                QStringLiteral("Tool Setup still has pending changes after preparation."),
                QStringLiteral("tool_setup.pending_after_prepare"));
        }
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult ToolSetupWorkbenchLifecycle::activate(
        const RobotQtViewerWorkbenchActivationContext&)
    {
        m_controller.updateToolFrameVisibility();
        return workbenchTransitionSucceeded();
    }

    void ToolSetupWorkbenchLifecycle::releaseProject(
        const RobotQtViewerWorkbenchProjectReleaseContext&) noexcept
    {
    }

    void ToolSetupWorkbenchLifecycle::shutdown(
        const RobotQtViewerWorkbenchShutdownContext&) noexcept
    {
    }
}
