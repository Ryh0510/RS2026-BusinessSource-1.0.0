#pragma once

#include "RobotQtViewerWorkbenchPackageRegistry.h"
#include "RobotQtViewerWorkbenchContribution.h"

class QWidget;

namespace robot_qt_viewer
{
    bool registerSprayProcessWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeSprayProcessWorkbenchRuntimeContributionFactory(QWidget& statusPanel);
}
