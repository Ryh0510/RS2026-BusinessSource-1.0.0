#pragma once

#include "RobotQtViewerWorkbenchPackageRegistry.h"
#include "RobotQtViewerWorkbenchContribution.h"

class QWidget;

namespace robot_qt_viewer
{
    bool registerDigitalTwinWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeDigitalTwinWorkbenchRuntimeContributionFactory(QWidget& statusPanel);
}
