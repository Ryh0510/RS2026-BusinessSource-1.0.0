#include "SprayProcessWorkbenchContribution.h"

namespace robot_qt_viewer
{
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeSprayProcessWorkbenchRuntimeContributionFactory(QWidget& statusPanel)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::SprayProcess);
        return {
            workbenchId,
            [workbenchId, &statusPanel](QWidget*) {
                return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                    workbenchId,
                    [&statusPanel]() { return &statusPanel; },
                    std::make_unique<RobotQtViewerNoOpWorkbenchLifecycle>(),
                    RobotQtViewerWorkbenchLifecyclePolicy{
                        RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
                        RobotQtViewerWorkbenchReactivationPolicy::PackageDefined },
                    std::make_unique<RobotQtViewerNoOpLanguageParticipant>(),
                    true);
            }
        };
    }

    bool registerSprayProcessWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.spray-process");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Spray Process"))) ||
            !catalog.registerWorkbench(makeRobotQtViewerWorkbench(
               packageId,
               RobotQtViewerWorkbenchKind::SprayProcess,
               QStringLiteral("sprayProcessWorkbench"),
               60,
               { QStringLiteral("smrobot.feature.spray-process") },
               { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }))) {
            return false;
        }
        return catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
            QStringLiteral("smrobot.feature.spray-process"),
            QStringLiteral("Spray Process"),
            packageId,
            { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::SprayProcess) }));
    }
}
