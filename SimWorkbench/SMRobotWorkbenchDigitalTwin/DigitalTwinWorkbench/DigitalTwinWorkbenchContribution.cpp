#include "DigitalTwinWorkbenchContribution.h"

namespace robot_qt_viewer
{
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeDigitalTwinWorkbenchRuntimeContributionFactory(QWidget& statusPanel)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::DigitalTwin);
        return {
            workbenchId,
            [workbenchId, &statusPanel](QWidget*) {
                return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                    workbenchId,
                    [&statusPanel]() { return &statusPanel; },
                    std::make_unique<RobotQtViewerNoOpWorkbenchLifecycle>(),
                    RobotQtViewerWorkbenchLifecyclePolicy{
                        RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
                        RobotQtViewerWorkbenchReactivationPolicy::ManualResume },
                    std::make_unique<RobotQtViewerNoOpLanguageParticipant>(),
                    true);
            }
        };
    }

    bool registerDigitalTwinWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.digital-twin");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Digital Twin"))) ||
            !catalog.registerWorkbench(makeRobotQtViewerWorkbench(
               packageId,
               RobotQtViewerWorkbenchKind::DigitalTwin,
               QStringLiteral("digitalTwinWorkbench"),
               80,
               { QStringLiteral("smrobot.feature.digital-twin") },
               { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }))) {
            return false;
        }
        return catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
            QStringLiteral("smrobot.feature.digital-twin"),
            QStringLiteral("Digital Twin"),
            packageId,
            { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::DigitalTwin) }));
    }
}
