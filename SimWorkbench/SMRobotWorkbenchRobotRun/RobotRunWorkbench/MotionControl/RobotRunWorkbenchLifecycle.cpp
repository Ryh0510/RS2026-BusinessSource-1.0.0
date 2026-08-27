#include "RobotRunWorkbenchLifecycle.h"

#include "MotionControlModuleController.h"

#include <RobotRuntime/RobotRunService.h>

namespace robot_qt_viewer
{
    bool registerRobotRunWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog,
        RobotQtViewerWorkbenchPackageSource source)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.robot-run");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Robot Run"), source)) ||
            !catalog.registerMode(makeRobotQtViewerWorkbenchMode(
               packageId,
               RobotQtViewerWorkbenchKind::Motion,
               QStringLiteral("robotRunWorkbench"),
               30,
               { QStringLiteral("smrobot.feature.robot-run") },
               { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }))) {
            return false;
        }
        return catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
            QStringLiteral("smrobot.feature.robot-run"),
            QStringLiteral("Robot Run"),
            packageId,
            { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion) }));
    }

    RobotRunWorkbenchLifecycle::RobotRunWorkbenchLifecycle(
        MotionControlModuleController& controller,
        robotruntime::IRobotRunService& runService)
        : m_controller(controller)
        , m_runService(runService)
    {
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotRunWorkbenchLifecycle::prepareDeactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult RobotRunWorkbenchLifecycle::deactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        return quiesce();
    }

    RobotQtViewerWorkbenchTransitionResult RobotRunWorkbenchLifecycle::activate(
        const RobotQtViewerWorkbenchActivationContext&)
    {
        m_controller.updateRowsFromScene();
        m_controller.refreshTrajectories();
        m_controller.refreshCollisionMonitor();
        return workbenchTransitionSucceeded();
    }

    void RobotRunWorkbenchLifecycle::releaseProject(
        const RobotQtViewerWorkbenchProjectReleaseContext&) noexcept
    {
        m_controller.clearRuntime();
    }

    void RobotRunWorkbenchLifecycle::shutdown(
        const RobotQtViewerWorkbenchShutdownContext&) noexcept
    {
        quiesce();
    }

    RobotQtViewerWorkbenchTransitionResult RobotRunWorkbenchLifecycle::quiesce()
    {
        m_controller.stopAllAutoMotion();
        const robotruntime::RobotRunExecutionSnapshot snapshot = m_runService.trajectorySnapshot();
        if(snapshot.state != robotruntime::RobotRunExecutionState::Running) {
            return workbenchTransitionSucceeded();
        }
        const robotruntime::RobotRunCommandResult pause = m_runService.pauseTrajectory();
        if(!pause.success) {
            return workbenchTransitionFailed(
                QString::fromStdString(pause.message),
                QStringLiteral("robot_run.trajectory.pause_failed"));
        }
        return workbenchTransitionSucceeded(QString::fromStdString(pause.message));
    }
}
