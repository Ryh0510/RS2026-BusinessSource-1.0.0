#include "CollisionConfigDialogService.h"

#include <RobotQtViewerFileDialog.h>

namespace robot_qt_viewer {

QString CollisionConfigDialogService::selectOverrideSidecarForSave(
    QWidget* parent,
    const QString& initialPath)
{
    return getSaveFileName(
        QStringLiteral("collisionConfig.override.save"),
        parent,
        QStringLiteral("Save collision override sidecar"),
        initialPath,
        QStringLiteral("Collision Override (*.collision.override.json *.json);;JSON Files (*.json);;All Files (*.*)"));
}

QString CollisionConfigDialogService::selectUrdfForExport(
    QWidget* parent,
    const QString& initialPath)
{
    return getSaveFileName(
        QStringLiteral("collisionConfig.urdf.export"),
        parent,
        QStringLiteral("Export robot URDF with collision"),
        initialPath,
        QStringLiteral("URDF Files (*.urdf);;All Files (*.*)"));
}

} // namespace robot_qt_viewer
