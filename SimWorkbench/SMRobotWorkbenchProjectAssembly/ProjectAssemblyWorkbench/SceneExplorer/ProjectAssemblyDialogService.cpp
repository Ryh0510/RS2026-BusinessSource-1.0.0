#include "ProjectAssemblyDialogService.h"

#include <SceneEntityWorkflowController.h>

#include <RobotQtViewerFileDialog.h>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>

namespace robot_qt_viewer {

QString ProjectAssemblyDialogService::selectRobotPackageForImport(
    QWidget* parent,
    const QString& initialPath)
{
    return getOpenFileName(
        QStringLiteral("projectAssembly.robotPackage.import"),
        parent,
        QStringLiteral("Import robot package"),
        initialPath,
        QStringLiteral("Robot Package (*.rbt.json);;JSON Files (*.json);;All Files (*.*)"));
}

QString ProjectAssemblyDialogService::selectRobotPackageForExport(
    QWidget* parent,
    const QString& initialPath)
{
    return getSaveFileName(
        QStringLiteral("projectAssembly.robotPackage.export"),
        parent,
        QStringLiteral("Export robot package"),
        initialPath,
        QStringLiteral("Robot Package (*.rbt.json);;JSON Files (*.json);;All Files (*.*)"));
}

QString ProjectAssemblyDialogService::selectRobotForImport(QWidget* parent)
{
    return getOpenFileName(
        QStringLiteral("projectAssembly.robot.import"),
        parent,
        QStringLiteral("Import robot"),
        QString(),
        QStringLiteral("Robot Files (*.urdf *.xml);;URDF Files (*.urdf);;Simscape Files (*.xml);;All Files (*.*)"));
}

QString ProjectAssemblyDialogService::selectObjectForImport(QWidget* parent)
{
    return getOpenFileName(
        QStringLiteral("projectAssembly.object.import"),
        parent,
        QStringLiteral("Import object"),
        QString(),
        QStringLiteral("Object Meshes (*.stl *.STL *.obj *.OBJ *.dae *.DAE *.ply *.PLY);;STL Files (*.stl *.STL);;All Files (*.*)"));
}

bool ProjectAssemblyDialogService::selectStlImportUnit(QWidget* parent, SceneObjectImportUnit& unit)
{
    QInputDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("stlImportUnitDialog"));
    dialog.setWindowTitle(QStringLiteral("\u9009\u62e9 STL \u6a21\u578b\u5355\u4f4d"));
    dialog.setLabelText(QStringLiteral("STL \u6587\u4ef6\u4e2d\u7684\u5750\u6807\u5355\u4f4d\uff1a\n"
        "\u9009\u62e9\u6beb\u7c73\u65f6\uff0c\u6a21\u578b\u5c06\u6309 0.001 \u6362\u7b97\u4e3a\u7c73\u3002"));
    const QString meters = QStringLiteral("\u7c73 (m)");
    const QString millimeters = QStringLiteral("\u6beb\u7c73 (mm)");
    dialog.setComboBoxItems({meters, millimeters});
    dialog.setComboBoxEditable(false);
    dialog.setTextValue(meters);
    dialog.setOkButtonText(QStringLiteral("\u5bfc\u5165"));
    dialog.setCancelButtonText(QStringLiteral("\u53d6\u6d88"));
    if(dialog.exec() != QDialog::Accepted) return false;
    unit = dialog.textValue() == millimeters ? SceneObjectImportUnit::Millimeters : SceneObjectImportUnit::Meters;
    return true;
}

QString ProjectAssemblyDialogService::selectPointCloudForImport(QWidget* parent)
{
    return getOpenFileName(
        QStringLiteral("projectAssembly.pointCloud.import"),
        parent,
        QStringLiteral("Import point cloud"),
        QString(),
        QStringLiteral("Point Cloud Files (*.pcd *.PCD);;PCD Files (*.pcd *.PCD);;All Files (*.*)"));
}

bool ProjectAssemblyDialogService::confirmDelete(
    QWidget* parent,
    SceneExplorerNodeKind kind,
    const QString& entityId)
{
    QString title;
    QString message;
    switch(kind) {
    case SceneExplorerNodeKind::ToolAsset:
        title = QStringLiteral("Delete Device Definition");
        message = QStringLiteral("Delete unused device definition %1? Definitions still in use must be unbound first.").arg(entityId);
        break;
    case SceneExplorerNodeKind::ToolAttachment:
        title = QStringLiteral("Delete Installed Device");
        message = QStringLiteral("Delete installed device %1? Its unused definition will also be removed; the mount frame is retained.").arg(entityId);
        break;
    case SceneExplorerNodeKind::Robot:
        title = QStringLiteral("Delete Robot");
        message = QStringLiteral(
            "Delete robot %1?\n\nRelated mount frames, attachments, sensors, tools, and collision references will also be removed.")
                      .arg(entityId);
        break;
    case SceneExplorerNodeKind::Object:
        title = QStringLiteral("Delete Object");
        message = QStringLiteral(
            "Delete object %1?\n\nRelated collision references will also be removed.")
                      .arg(entityId);
        break;
    case SceneExplorerNodeKind::PointCloud:
        title = QStringLiteral("Delete Point Cloud");
        message = QStringLiteral(
            "Delete point cloud %1?\n\nRelated collision references will also be removed.")
                      .arg(entityId);
        break;
    default:
        return false;
    }

    return QMessageBox::question(
               parent,
               title,
               message,
               QMessageBox::Yes | QMessageBox::No,
               QMessageBox::No) == QMessageBox::Yes;
}

QString ProjectAssemblyDialogService::requestPointCloudName(
    QWidget* parent,
    const QString& currentName,
    bool* accepted)
{
    return QInputDialog::getText(
               parent,
               QStringLiteral("Rename Point Cloud"),
               QStringLiteral("Name:"),
               QLineEdit::Normal,
               currentName,
               accepted)
        .trimmed();
}

} // namespace robot_qt_viewer
