#pragma once

#include "RobotQtViewerWorkbenchPackageRegistry.h"

#include <QString>

namespace robot_qt_viewer
{
    RobotQtViewerWorkbenchPackageRegistry makeRobotQtViewerBuiltWorkbenchCatalog(
        QString* errorMessage = nullptr);
}
