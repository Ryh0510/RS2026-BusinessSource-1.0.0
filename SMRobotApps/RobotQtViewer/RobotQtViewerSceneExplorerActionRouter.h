#pragma once

#include "SceneTreeIntentController.h"

#include <QString>

#include <functional>

class QWidget;

namespace robot_qt_viewer
{
    class CollisionConfigWorkbenchShellPort;
    class SceneExplorerWorkbenchShellPort;
    class ToolSetupWorkbenchShellPort;

    class RobotQtViewerSceneExplorerActionRouter
    {
    public:
        using StatusCallback = std::function<void(const QString&, int)>;
        using VoidCallback = std::function<void()>;
        using TransitionCallback = std::function<bool()>;
        using RobotLinkCallback = std::function<void(const QString&, const QString&)>;
        using ObjectCallback = std::function<void(const QString&)>;

        RobotQtViewerSceneExplorerActionRouter() = default;

        void setParentWidget(QWidget* parentWidget);
        void setSceneExplorerPort(SceneExplorerWorkbenchShellPort* port);
        void setToolSetupPort(ToolSetupWorkbenchShellPort* port);
        void setCollisionWorkbenchPort(CollisionConfigWorkbenchShellPort* port);
        void setEnterToolSetupWorkbenchCallback(TransitionCallback callback);
        void setDeleteSelectedEntityCallback(VoidCallback callback);
        void setReloadViewportCallback(VoidCallback callback);
        void setSelectRobotContextCallback(RobotLinkCallback callback);
        void setSelectObjectContextCallback(ObjectCallback callback);

        void handleAction(
            const SceneTreeIntentController::ContextMenuAction& action,
            StatusCallback statusCallback);

    private:
        QWidget* m_parentWidget = nullptr;
        SceneExplorerWorkbenchShellPort* m_sceneExplorerPort = nullptr;
        ToolSetupWorkbenchShellPort* m_toolSetupPort = nullptr;
        CollisionConfigWorkbenchShellPort* m_collisionWorkbenchPort = nullptr;
        TransitionCallback m_enterToolSetupWorkbenchCallback;
        VoidCallback m_deleteSelectedEntityCallback;
        VoidCallback m_reloadViewportCallback;
        RobotLinkCallback m_selectRobotContextCallback;
        ObjectCallback m_selectObjectContextCallback;
    };
}
