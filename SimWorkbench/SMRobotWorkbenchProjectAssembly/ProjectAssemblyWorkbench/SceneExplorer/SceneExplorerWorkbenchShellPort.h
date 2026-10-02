#pragma once

#include "SceneExplorerViewModel.h"
#include "SceneSelectionController.h"

#include <QHash>
#include <QString>
#include <QStringList>

class QWidget;

namespace robot_qt_viewer
{
    struct RobotQtViewerWorkbenchDescriptor;

    class SceneExplorerWorkbenchShellPort
    {
    public:
        virtual ~SceneExplorerWorkbenchShellPort() = default;

        virtual SceneExplorerNodeRef currentNode() const = 0;
        virtual SceneSelectionIntent selectionIntentForNode(const SceneExplorerNodeRef& node) const = 0;
        virtual SceneRobotSelectionContext robotSelectionContext(
            const QString& robotId,
            const QString& preferredLinkName,
            const QString& preferredMountId) const = 0;
        virtual QHash<QString, QStringList> robotLinksByRobotId() const = 0;
        virtual bool selectNode(const SceneExplorerNodeRef& node) = 0;
        virtual void focusTransformTask(const SceneExplorerNodeRef& node) = 0;
        virtual void createObjectFrameForObject(const QString& objectId) = 0;
        virtual void createCameraDefinition(QWidget* parentWidget = nullptr) = 0;
        virtual bool resolvePendingTransformPreviewIfTargetChanges(
            const SceneExplorerNodeRef& nextNode,
            QWidget* parentWidget) = 0;
        virtual bool linkFrameVisible(const QString& robotId, const QString& linkName) const = 0;
        virtual bool toggleLinkFrameVisible(const QString& robotId, const QString& linkName) = 0;
        virtual void setWorkbenchDescriptor(const RobotQtViewerWorkbenchDescriptor& descriptor) = 0;
        virtual void setRobotRuntime(
            const QString& robotId,
            const QString& robotName,
            const QStringList& links,
            const QStringList& joints,
            const QStringList& movableJoints,
            const QStringList& movableJointTypes) = 0;
        virtual void setSceneObjectRuntime(
            const QString& objectId,
            const QString& displayName) = 0;
        virtual void refreshViewModel() = 0;
        virtual void setSummaryText(const QString& text) = 0;
        virtual void clearRuntime() = 0;
    };
}
