#include "RobotQtViewerWorkbenchLifecycle.h"

namespace robot_qt_viewer
{
    bool RobotQtViewerWorkbenchTransitionResult::succeeded() const
    {
        return status == RobotQtViewerWorkbenchTransitionStatus::Succeeded;
    }

    RobotQtViewerWorkbenchTransitionResult workbenchTransitionSucceeded(
        const QString& message)
    {
        return { RobotQtViewerWorkbenchTransitionStatus::Succeeded, message, QString() };
    }

    RobotQtViewerWorkbenchTransitionResult workbenchTransitionRejected(
        const QString& message,
        const QString& diagnosticCode)
    {
        return { RobotQtViewerWorkbenchTransitionStatus::Rejected, message, diagnosticCode };
    }

    RobotQtViewerWorkbenchTransitionResult workbenchTransitionFailed(
        const QString& message,
        const QString& diagnosticCode)
    {
        return { RobotQtViewerWorkbenchTransitionStatus::Failed, message, diagnosticCode };
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotQtViewerNoOpWorkbenchLifecycle::prepareDeactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult RobotQtViewerNoOpWorkbenchLifecycle::deactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult RobotQtViewerNoOpWorkbenchLifecycle::activate(
        const RobotQtViewerWorkbenchActivationContext&)
    {
        return workbenchTransitionSucceeded();
    }

    void RobotQtViewerNoOpWorkbenchLifecycle::releaseProject(
        const RobotQtViewerWorkbenchProjectReleaseContext&) noexcept
    {
    }

    void RobotQtViewerNoOpWorkbenchLifecycle::shutdown(
        const RobotQtViewerWorkbenchShutdownContext&) noexcept
    {
    }
}
