#include "CollisionConfigWorkbenchLifecycle.h"

#include "CollisionWorkbenchModuleController.h"

namespace robot_qt_viewer
{
    bool registerCollisionConfigWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog,
        RobotQtViewerWorkbenchPackageSource source)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.collision-config");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Collision Config"), source)) ||
            !catalog.registerMode(makeRobotQtViewerWorkbenchMode(
               packageId,
               RobotQtViewerWorkbenchKind::Collision,
               QStringLiteral("collisionConfigWorkbench"),
               40,
               { QStringLiteral("smrobot.feature.collision-config") },
               { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }))) {
            return false;
        }
        return catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
            QStringLiteral("smrobot.feature.collision-config"),
            QStringLiteral("Collision Configuration"),
            packageId,
            { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Collision) }));
    }

    CollisionConfigWorkbenchLifecycle::CollisionConfigWorkbenchLifecycle(
        CollisionWorkbenchModuleController& controller)
        : m_controller(controller)
    {
    }

    RobotQtViewerWorkbenchTransitionResult
    CollisionConfigWorkbenchLifecycle::prepareDeactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        if(m_controller.isCollisionModelConfigurationActive() &&
            !m_controller.canHandoffCollisionModelConfiguration()) {
            return workbenchTransitionRejected(
                QStringLiteral("Collision model configuration has pending changes."),
                QStringLiteral("collision_config.pending_changes"));
        }
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult CollisionConfigWorkbenchLifecycle::deactivate(
        const RobotQtViewerWorkbenchTransitionContext& context)
    {
        clearConfigurationPreview(context.sourceId);
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult CollisionConfigWorkbenchLifecycle::activate(
        const RobotQtViewerWorkbenchActivationContext&)
    {
        m_controller.showDetectorConfiguration();
        return workbenchTransitionSucceeded();
    }

    void CollisionConfigWorkbenchLifecycle::releaseProject(
        const RobotQtViewerWorkbenchProjectReleaseContext& context) noexcept
    {
        clearConfigurationPreview(context.sourceId);
    }

    void CollisionConfigWorkbenchLifecycle::shutdown(
        const RobotQtViewerWorkbenchShutdownContext& context) noexcept
    {
        clearConfigurationPreview(context.sourceId);
    }

    void CollisionConfigWorkbenchLifecycle::clearConfigurationPreview(
        const QString& sourceId) noexcept
    {
        if(m_controller.isCollisionModelConfigurationActive()) {
            m_controller.cancelCollisionModelConfigurationForHandoff(
                sourceId.isEmpty()
                    ? QStringLiteral("collisionWorkbenchLifecycle")
                    : sourceId);
        }
    }
}
