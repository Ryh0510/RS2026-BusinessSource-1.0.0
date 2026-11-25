#pragma once

#include "RobotQtViewerWorkbenchPackageRegistry.h"
#include "RobotQtViewerWorkbenchContribution.h"

class QWidget;

namespace robot_qt_viewer
{
    class IRobotQtViewerTelemetryPort;
    class IRobotQtViewerMotionPlanningViewportPort;
    bool registerDigitalTwinWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeDigitalTwinWorkbenchRuntimeContributionFactory(IRobotQtViewerTelemetryPort& telemetry,
            IRobotQtViewerMotionPlanningViewportPort& viewport);
}
