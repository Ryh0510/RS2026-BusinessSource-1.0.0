#pragma once

#include "RobotQtViewerWorkbenchPackageRegistry.h"

namespace robot_qt_viewer
{
    bool registerSprayProcessWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog,
        RobotQtViewerWorkbenchPackageSource source);
}
