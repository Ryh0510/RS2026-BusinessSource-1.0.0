#include "DigitalTwinWorkbenchContribution.h"
#include "DigitalTwinTelemetryPanel.h"
#include "DigitalTwinTelemetryController.h"

namespace robot_qt_viewer
{
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeDigitalTwinWorkbenchRuntimeContributionFactory(
        IRobotQtViewerTelemetryPort& telemetry, IRobotQtViewerMotionPlanningViewportPort& viewport)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::DigitalTwin);
        return {
            workbenchId,
            [workbenchId, &telemetry, &viewport](QWidget* parent) {
                auto* panel = new DigitalTwinTelemetryPanel(parent);
                new DigitalTwinTelemetryController(*panel, telemetry, &viewport);
                return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                    workbenchId,
                    [panel]() { return panel; },
                    std::make_unique<RobotQtViewerNoOpWorkbenchLifecycle>(),
                    RobotQtViewerWorkbenchLifecyclePolicy{
                        RobotQtViewerWorkbenchExecutionPolicy::MayContinueHeadless,
                        RobotQtViewerWorkbenchReactivationPolicy::ManualResume },
                    std::make_unique<RobotQtViewerNoOpLanguageParticipant>(),
                    false, RobotQtViewerBasicWorkbenchRuntimeContribution::PanelTitleResolver{}, [&telemetry]() { telemetry.stop(); });
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
