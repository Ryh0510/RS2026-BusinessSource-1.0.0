#include "RobotQtViewerSceneExplorerActionRouter.h"

#include "CollisionConfigWorkbenchShellPort.h"
#include "SceneCollisionTargetResolver.h"
#include "SceneExplorerWorkbenchShellPort.h"
#include "ToolSetupWorkbenchShellPort.h"

#include <utility>

namespace robot_qt_viewer
{
    void RobotQtViewerSceneExplorerActionRouter::setParentWidget(QWidget* parentWidget)
    {
        m_parentWidget = parentWidget;
    }

    void RobotQtViewerSceneExplorerActionRouter::setSceneExplorerPort(
        SceneExplorerWorkbenchShellPort* port)
    {
        m_sceneExplorerPort = port;
    }

    void RobotQtViewerSceneExplorerActionRouter::setToolSetupPort(ToolSetupWorkbenchShellPort* port)
    {
        m_toolSetupPort = port;
    }

    void RobotQtViewerSceneExplorerActionRouter::setCollisionWorkbenchPort(
        CollisionConfigWorkbenchShellPort* port)
    {
        m_collisionWorkbenchPort = port;
    }

    void RobotQtViewerSceneExplorerActionRouter::setEnterToolSetupWorkbenchCallback(
        TransitionCallback callback)
    {
        m_enterToolSetupWorkbenchCallback = std::move(callback);
    }

    void RobotQtViewerSceneExplorerActionRouter::setDeleteSelectedEntityCallback(VoidCallback callback)
    {
        m_deleteSelectedEntityCallback = std::move(callback);
    }

    void RobotQtViewerSceneExplorerActionRouter::setReloadViewportCallback(VoidCallback callback)
    {
        m_reloadViewportCallback = std::move(callback);
    }

    void RobotQtViewerSceneExplorerActionRouter::setSelectRobotContextCallback(RobotLinkCallback callback)
    {
        m_selectRobotContextCallback = std::move(callback);
    }

    void RobotQtViewerSceneExplorerActionRouter::setSelectObjectContextCallback(ObjectCallback callback)
    {
        m_selectObjectContextCallback = std::move(callback);
    }

    void RobotQtViewerSceneExplorerActionRouter::handleAction(
        const SceneTreeIntentController::ContextMenuAction& action,
        StatusCallback statusCallback)
    {
        const auto showStatus = [&](const QString& message, int timeoutMs) {
            if(statusCallback) {
                statusCallback(message, timeoutMs);
            }
        };

        switch(action.kind) {
        case SceneTreeIntentController::ContextMenuActionKind::ConfigureRobotFlange:
            if(m_toolSetupPort == nullptr) {
                showStatus(QStringLiteral("Frame Editor is not available."), 3000);
                return;
            }
            if(m_enterToolSetupWorkbenchCallback) {
                if(!m_enterToolSetupWorkbenchCallback()) {
                    showStatus(QStringLiteral("Tool Setup was not opened."), 3000);
                    return;
                }
            }
            if(action.node.kind == SceneExplorerNodeKind::RobotMount && m_sceneExplorerPort != nullptr) {
                const SceneSelectionIntent intent = m_sceneExplorerPort->selectionIntentForNode(action.node);
                if(intent.kind != SceneSelectionIntentKind::SelectRobotMount) {
                    showStatus(QStringLiteral("Mount frame is not available."), 3000);
                    return;
                }
                m_toolSetupPort->focusRobotMountTask(intent.robotId, intent.linkName, intent.mountId);
            } else if(action.node.kind == SceneExplorerNodeKind::Link && !action.node.linkName.isEmpty()) {
                m_toolSetupPort->focusRobotMountTask(action.node.id, action.node.linkName);
                m_toolSetupPort->createRobotMountForSelectedLink();
            } else {
                m_toolSetupPort->focusRobotMountTask(
                    action.node.id,
                    action.node.linkName.isEmpty() && !action.robotLinks.isEmpty()
                        ? action.robotLinks.first()
                        : action.node.linkName);
            }
            break;
        case SceneTreeIntentController::ContextMenuActionKind::MoveRobotBase:
            if(m_sceneExplorerPort != nullptr) {
                m_sceneExplorerPort->focusTransformTask(action.node);
            }
            break;
        case SceneTreeIntentController::ContextMenuActionKind::DeleteRobot:
            if(m_deleteSelectedEntityCallback) {
                m_deleteSelectedEntityCallback();
            }
            break;
        case SceneTreeIntentController::ContextMenuActionKind::MoveSceneObject:
            if(m_sceneExplorerPort != nullptr) {
                m_sceneExplorerPort->focusTransformTask(action.node);
            }
            break;
        case SceneTreeIntentController::ContextMenuActionKind::AddObjectFrame:
            if(m_sceneExplorerPort != nullptr) {
                m_sceneExplorerPort->createObjectFrameForObject(action.node.id);
            }
            break;
        case SceneTreeIntentController::ContextMenuActionKind::EditObjectFrame:
            if(m_sceneExplorerPort != nullptr) {
                m_sceneExplorerPort->focusTransformTask(action.node);
            }
            break;
        case SceneTreeIntentController::ContextMenuActionKind::BindItemToMount:
            if(m_toolSetupPort == nullptr) {
                showStatus(QStringLiteral("Frame Editor is not available."), 3000);
                return;
            }
            if(m_enterToolSetupWorkbenchCallback) {
                if(!m_enterToolSetupWorkbenchCallback()) {
                    showStatus(QStringLiteral("Tool Setup was not opened."), 3000);
                    return;
                }
            }
            m_toolSetupPort->focusObjectBindingTask(action.node.id);
            break;
        case SceneTreeIntentController::ContextMenuActionKind::BindObjectToMount:
            if(m_toolSetupPort == nullptr) {
                showStatus(QStringLiteral("Frame Editor is not available."), 3000);
                return;
            }
            if(m_enterToolSetupWorkbenchCallback) {
                if(!m_enterToolSetupWorkbenchCallback()) {
                    showStatus(QStringLiteral("Tool Setup was not opened."), 3000);
                    return;
                }
            }
            m_toolSetupPort->focusObjectBindingTask(
                QString(),
                action.node.id,
                action.node.kind == SceneExplorerNodeKind::ObjectFrame
                    ? action.node.linkName
                    : QString());
            break;
        case SceneTreeIntentController::ContextMenuActionKind::CreateToolAssetFromObject:
            if(m_toolSetupPort != nullptr) {
                m_toolSetupPort->createToolAssetFromSceneObject(action.node.id);
            }
            break;
        case SceneTreeIntentController::ContextMenuActionKind::UnbindMountedAttachment:
            if(m_toolSetupPort == nullptr) {
                showStatus(QStringLiteral("Frame Editor is not available."), 3000);
                return;
            }
            m_toolSetupPort->unbindMountedAttachment(action.node.id);
            break;
        case SceneTreeIntentController::ContextMenuActionKind::DeleteObject:
        case SceneTreeIntentController::ContextMenuActionKind::DeletePointCloud:
            if(m_deleteSelectedEntityCallback) {
                m_deleteSelectedEntityCallback();
            }
            break;
        case SceneTreeIntentController::ContextMenuActionKind::ShowLinkFrame:
            showStatus(QStringLiteral("Link frame display is handled by the main window."), 3000);
            break;
        case SceneTreeIntentController::ContextMenuActionKind::ConfigureCollisionModel:
            if(!sceneExplorerNodeKindCanConfigureCollisionModel(action.node.kind)) {
                showStatus(QStringLiteral("Select a robot, link, object, or attachment to configure collision geometry."), 3000);
                return;
            }
            if(m_collisionWorkbenchPort == nullptr) {
                showStatus(QStringLiteral("Collision configuration is not available."), 3000);
                return;
            }
            if((action.node.kind == SceneExplorerNodeKind::Robot ||
                    action.node.kind == SceneExplorerNodeKind::Link) &&
                m_selectRobotContextCallback) {
                const QString linkName = !action.node.linkName.isEmpty()
                    ? action.node.linkName
                    : (!action.robotLinks.isEmpty() ? action.robotLinks.first() : QString());
                if(linkName.isEmpty()) {
                    showStatus(QStringLiteral("Select a robot link to configure collision geometry."), 3000);
                    return;
                }
                m_selectRobotContextCallback(action.node.id, linkName);
            } else if(action.node.kind == SceneExplorerNodeKind::Object && m_selectObjectContextCallback) {
                m_selectObjectContextCallback(action.node.id);
            } else if(action.node.kind == SceneExplorerNodeKind::ToolAttachment &&
                m_toolSetupPort != nullptr) {
                m_toolSetupPort->selectToolAttachmentById(action.node.id.toStdString());
            }
            m_collisionWorkbenchPort->showCollisionModelConfiguration();
            showStatus(QStringLiteral("Collision model configuration opened."), 3000);
            break;
        case SceneTreeIntentController::ContextMenuActionKind::AddToDetectorSetA:
            if(!action.hasCollisionMember) {
                showStatus(QStringLiteral("Select a robot, link, object, or attachment."), 3000);
                return;
            }
            if(m_collisionWorkbenchPort != nullptr) {
                m_collisionWorkbenchPort->addMemberToDetectorDraftSet(
                    QStringLiteral("A"),
                    action.displayName,
                    action.robotLinks,
                    action.collisionMember);
            }
            break;
        case SceneTreeIntentController::ContextMenuActionKind::AddToDetectorSetB:
            if(!action.hasCollisionMember) {
                showStatus(QStringLiteral("Select a robot, link, object, or attachment."), 3000);
                return;
            }
            if(m_collisionWorkbenchPort != nullptr) {
                m_collisionWorkbenchPort->addMemberToDetectorDraftSet(
                    QStringLiteral("B"),
                    action.displayName,
                    action.robotLinks,
                    action.collisionMember);
            }
            break;
        }
    }
}
