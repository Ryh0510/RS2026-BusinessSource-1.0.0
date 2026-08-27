#include "SprayProcessWorkbenchContribution.h"

namespace robot_qt_viewer
{
    bool registerSprayProcessWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog,
        RobotQtViewerWorkbenchPackageSource source)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.spray-process");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Spray Process"), source)) ||
            !catalog.registerMode(makeRobotQtViewerWorkbenchMode(
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
