#pragma once

#include "RobotQtViewerWorkbenchContribution.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"

#include <QObject>
#include <functional>

namespace robot_qt_viewer
{
    class RobotQtViewerDocumentContext;
    class RobotQtViewerDocumentViewRegistry;
    struct CalibrationTranslationWorkbenchComposition
    {
        RobotQtViewerDocumentContext* documentContext = nullptr;
        RobotQtViewerDocumentViewRegistry* documentViewRegistry = nullptr;
        QObject* freeformDataSource = nullptr;
        std::function<void(const QString&, int)> showStatus;
    };

    bool registerCalibrationTranslationWorkbenchContribution(RobotQtViewerWorkbenchPackageRegistry& catalog);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeOwnedCalibrationTranslationWorkbenchRuntimeContributionFactory(
        CalibrationTranslationWorkbenchComposition composition);
}
