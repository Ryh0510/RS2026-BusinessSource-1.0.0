#include "SceneTreeIntentController.h"

#include "SceneCollisionTargetResolver.h"

#include <SimulationProject/ProjectDocument.h>

#include <QPoint>
#include <QList>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace
{
    robot_qt_viewer::SceneTreeIntentController::ContextMenuActionView makeActionView(
        const QString& label,
        robot_qt_viewer::SceneTreeIntentController::ContextMenuActionKind kind,
        const robot_qt_viewer::SceneExplorerNodeRef& node,
        const QString& displayName,
        const QStringList& robotLinks,
        bool enabled,
        bool separatorBefore,
        const simulation_project::CollisionSelectionSetMemberDesc& collisionMember,
        bool hasCollisionMember)
    {
        robot_qt_viewer::SceneTreeIntentController::ContextMenuActionView view;
        view.label = label;
        view.enabled = enabled;
        view.separatorBefore = separatorBefore;
        view.action.kind = kind;
        view.action.node = node;
        view.action.displayName = displayName;
        view.action.robotLinks = robotLinks;
        view.action.collisionMember = collisionMember;
        view.action.hasCollisionMember = hasCollisionMember;
        return view;
    }
}

namespace robot_qt_viewer
{
    SceneExplorerNodeRef SceneTreeIntentController::nodeRefFromItem(const QTreeWidgetItem* item)
    {
        SceneExplorerNodeRef node;
        if(item == nullptr) {
            return node;
        }

        const QString type = item->data(0, kSceneExplorerRoleType).toString();
        node.kind = sceneExplorerNodeKindFromType(type);
        node.id = item->data(0, kSceneExplorerRoleId).toString();
        node.name = item->data(0, kSceneExplorerRoleName).toString();
        node.linkName = item->data(0, kSceneExplorerRoleLink).toString();
        return node;
    }

    QTreeWidgetItem* SceneTreeIntentController::treeItemAtOrCurrent(QTreeWidget* tree, const QPoint& pos)
    {
        if(tree == nullptr) {
            return nullptr;
        }

        QTreeWidgetItem* item = tree->itemAt(pos);
        if(isContextMenuTarget(nodeRefFromItem(item))) {
            return item;
        }

        item = tree->currentItem();
        return isContextMenuTarget(nodeRefFromItem(item)) ? item : nullptr;
    }

    QTreeWidgetItem* SceneTreeIntentController::actionableTreeItem(QTreeWidgetItem* item)
    {
        if(item == nullptr) {
            return nullptr;
        }
        const SceneExplorerNodeRef node = nodeRefFromItem(item);
        return isEntityActionable(node) ? item : actionableTreeItem(item->parent());
    }

    QStringList SceneTreeIntentController::robotLinksFromItem(QTreeWidgetItem* item)
    {
        QStringList links;
        if(item == nullptr) {
            return links;
        }

        const SceneExplorerNodeRef node = nodeRefFromItem(item);
        if(node.kind == SceneExplorerNodeKind::Link && !node.linkName.isEmpty()) {
            links.push_back(node.linkName);
            return links;
        }

        QList<QTreeWidgetItem*> pending;
        for(int i = 0; i < item->childCount(); ++i) {
            pending.push_back(item->child(i));
        }

        while(!pending.empty()) {
            QTreeWidgetItem* current = pending.takeLast();
            const SceneExplorerNodeRef currentNode = nodeRefFromItem(current);
            if(currentNode.kind == SceneExplorerNodeKind::Link &&
                !currentNode.linkName.isEmpty() &&
                links.indexOf(currentNode.linkName) < 0) {
                links.push_back(currentNode.linkName);
            }
            for(int i = 0; current != nullptr && i < current->childCount(); ++i) {
                pending.push_back(current->child(i));
            }
        }
        return links;
    }

    QString SceneTreeIntentController::displayNameFromItem(const QTreeWidgetItem* item)
    {
        if(item == nullptr) {
            return QString();
        }

        const SceneExplorerNodeRef node = nodeRefFromItem(item);
        return node.name.isEmpty() ? item->text(0) : node.name;
    }

    SceneTreeIntentController::ContextMenuModel SceneTreeIntentController::contextMenuModelFromItem(
        QTreeWidgetItem* item,
        bool hasCollisionSelectionSets)
    {
        return contextMenuModelFromItem(
            item,
            hasCollisionSelectionSets,
            SceneExplorerActionScope::ProjectAssembly);
    }

    SceneTreeIntentController::ContextMenuModel SceneTreeIntentController::contextMenuModelFromItem(
        QTreeWidgetItem* item,
        bool hasCollisionSelectionSets,
        SceneExplorerActionScope actionScope)
    {
        (void)hasCollisionSelectionSets;
        ContextMenuModel model;
        if(item == nullptr) {
            return model;
        }

        const SceneExplorerNodeRef node = nodeRefFromItem(item);
        if(!isContextMenuTarget(node)) {
            return model;
        }

        model.node = node;
        const QString displayName = displayNameFromItem(item);
        const QStringList robotLinks = robotLinksFromItem(actionableTreeItem(item));
        simulation_project::CollisionSelectionSetMemberDesc collisionMember;
        const bool hasCollisionMember = collisionSelectionSetMemberFromItem(item, collisionMember);

        const bool showProjectAssemblyActions =
            actionScope == SceneExplorerActionScope::ProjectAssembly;
        const bool showCollisionActions =
            actionScope == SceneExplorerActionScope::CollisionConfig;
        if(node.kind == SceneExplorerNodeKind::Trajectory) {
            model.actions.push_back(makeActionView(QStringLiteral("\u663e\u793a\u6b64\u8f68\u8ff9"),ContextMenuActionKind::ShowTrajectory,node,displayName,robotLinks,true,false,collisionMember,false));
            model.actions.push_back(makeActionView(QStringLiteral("\u5bfc\u51fa\u8f68\u8ff9..."),ContextMenuActionKind::ExportTrajectory,node,displayName,robotLinks,true,false,collisionMember,false));
            model.actions.push_back(makeActionView(QStringLiteral("\u5220\u9664\u8f68\u8ff9"),ContextMenuActionKind::DeleteTrajectory,node,displayName,robotLinks,true,false,collisionMember,false));
            return model;
        }
        if(node.kind == SceneExplorerNodeKind::UserFrame || node.kind == SceneExplorerNodeKind::CoordinateFrame) {
            model.actions.push_back(makeActionView(QStringLiteral("\u663e\u793a"), ContextMenuActionKind::ShowItem,node,displayName,robotLinks,true,false,collisionMember,hasCollisionMember));
            model.actions.push_back(makeActionView(QStringLiteral("\u9690\u85cf"), ContextMenuActionKind::HideItem,node,displayName,robotLinks,true,false,collisionMember,hasCollisionMember));
            if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::UserFrame) model.actions.push_back(makeActionView(
                QStringLiteral("\u7f16\u8f91\u7528\u6237\u5750\u6807\u7cfb..."),
                ContextMenuActionKind::EditUserFrame,node,displayName,robotLinks,
                true,false,collisionMember,hasCollisionMember));
            if(showProjectAssemblyActions && node.id != QStringLiteral("world")) model.actions.push_back(makeActionView(
                QStringLiteral("\u5220\u9664\u5750\u6807\u7cfb"), ContextMenuActionKind::DeleteCoordinateFrame,node,displayName,robotLinks,true,false,collisionMember,hasCollisionMember));
            if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::CoordinateFrame && node.id != QStringLiteral("world")) model.actions.push_back(makeActionView(
                QStringLiteral("\u8bbe\u7f6e TCP..."), ContextMenuActionKind::EditTcp,node,displayName,robotLinks,true,false,collisionMember,hasCollisionMember));
            return model;
        }

        const bool frameTarget = node.kind == SceneExplorerNodeKind::Link ||
            node.kind == SceneExplorerNodeKind::RobotMount || node.kind == SceneExplorerNodeKind::ObjectFrame;
        model.actions.push_back(makeActionView(
            frameTarget ? QStringLiteral("\u663e\u793a\u5750\u6807\u7cfb") : QStringLiteral("\u663e\u793a"),
            ContextMenuActionKind::ShowItem, node, displayName, robotLinks,
            true, false, collisionMember, hasCollisionMember));
        model.actions.push_back(makeActionView(
            frameTarget ? QStringLiteral("\u9690\u85cf\u5750\u6807\u7cfb") : QStringLiteral("\u9690\u85cf"),
            ContextMenuActionKind::HideItem, node, displayName, robotLinks,
            true, false, collisionMember, hasCollisionMember));
        if(showProjectAssemblyActions && (node.kind == SceneExplorerNodeKind::Object ||
            node.kind == SceneExplorerNodeKind::ToolAttachment || node.kind == SceneExplorerNodeKind::ToolAsset)) {
            model.actions.push_back(makeActionView(QStringLiteral("\u8bbe\u4e3a\u5de5\u5177"),
                ContextMenuActionKind::SetAsTool, node, displayName, robotLinks,
                true, true, collisionMember, hasCollisionMember));
            model.actions.push_back(makeActionView(QStringLiteral("\u8bbe\u4e3a\u5de5\u4ef6"),
                ContextMenuActionKind::SetAsWorkpiece, node, displayName, robotLinks,
                true, false, collisionMember, hasCollisionMember));
        }
        if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::ToolAttachment) {
            model.actions.push_back(makeActionView(QStringLiteral("\u8bbe\u7f6e TCP..."),
                ContextMenuActionKind::EditTcp, node, displayName, robotLinks,
                true, false, collisionMember, hasCollisionMember));
        }

        if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::Robot) {
            model.actions.push_back(makeActionView(
                QStringLiteral("Edit Mount Frames..."),
                ContextMenuActionKind::ConfigureRobotFlange,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Move Base..."),
                ContextMenuActionKind::MoveRobotBase,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Delete Robot..."),
                ContextMenuActionKind::DeleteRobot,
                node,
                displayName,
                robotLinks,
                true,
                true,
                collisionMember,
                hasCollisionMember));
        } else if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::Link) {
            model.actions.push_back(makeActionView(
                QStringLiteral("Add Mount Frame..."),
                ContextMenuActionKind::ConfigureRobotFlange,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
        } else if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::RobotMount) {
            model.actions.push_back(makeActionView(
                QStringLiteral("Edit Mount Frame..."),
                ContextMenuActionKind::ConfigureRobotFlange,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Bind Item..."),
                ContextMenuActionKind::BindItemToMount,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
        } else if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::Object) {
            model.actions.push_back(makeActionView(
                QStringLiteral("Add Object Frame..."),
                ContextMenuActionKind::AddObjectFrame,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Move Object..."),
                ContextMenuActionKind::MoveSceneObject,
                node,
                displayName,
                robotLinks,
                true,
                true,
                collisionMember,
                hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Create Tool Asset From Object..."),
                ContextMenuActionKind::CreateToolAssetFromObject,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Bind To Mount..."),
                ContextMenuActionKind::BindObjectToMount,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Delete Object..."),
                ContextMenuActionKind::DeleteObject,
                node,
                displayName,
                robotLinks,
                true,
                true,
                collisionMember,
                hasCollisionMember));
        } else if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::ObjectFrame) {
            model.actions.push_back(makeActionView(
                QStringLiteral("Edit Object Frame..."),
                ContextMenuActionKind::EditObjectFrame,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Bind To Mount..."),
                ContextMenuActionKind::BindObjectToMount,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
        } else if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::ToolAttachment) {
            model.actions.push_back(makeActionView(
                QStringLiteral("Delete Installed Device..."),
                ContextMenuActionKind::DeleteObject, node, displayName, robotLinks,
                true, true, collisionMember, hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Unbind Attachment..."),
                ContextMenuActionKind::UnbindMountedAttachment,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
        } else if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::ToolAsset) {
            model.actions.push_back(makeActionView(
                QStringLiteral("Delete Device Definition..."),
                ContextMenuActionKind::DeleteObject, node, displayName, robotLinks,
                true, true, collisionMember, hasCollisionMember));
        } else if(showProjectAssemblyActions && node.kind == SceneExplorerNodeKind::PointCloud) {
            model.actions.push_back(makeActionView(
                QStringLiteral("Rename Point Cloud..."),
                ContextMenuActionKind::RenamePointCloud,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Move Point Cloud..."),
                ContextMenuActionKind::MoveSceneObject,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
            model.actions.push_back(makeActionView(
                QStringLiteral("Delete Point Cloud..."),
                ContextMenuActionKind::DeletePointCloud,
                node,
                displayName,
                robotLinks,
                true,
                true,
                collisionMember,
                hasCollisionMember));
        }

        if(node.kind == SceneExplorerNodeKind::Link) {
            model.actions.push_back(makeActionView(
                QStringLiteral("Show Frame"),
                ContextMenuActionKind::ShowLinkFrame,
                node,
                displayName,
                robotLinks,
                true,
                false,
                collisionMember,
                hasCollisionMember));
        }

        if(showCollisionActions) {
            const bool canConfigureCollisionModel =
                sceneExplorerNodeKindCanConfigureCollisionModel(node.kind) && hasCollisionMember;
            if(canConfigureCollisionModel) {
                model.actions.push_back(makeActionView(
                    QStringLiteral("Configure Collision Model..."),
                    ContextMenuActionKind::ConfigureCollisionModel,
                    node,
                    displayName,
                    robotLinks,
                    true,
                    !model.actions.empty(),
                    collisionMember,
                    hasCollisionMember));
            }
            if(hasCollisionMember) {
                model.actions.push_back(makeActionView(
                    QStringLiteral("Add to Detector Set A"),
                    ContextMenuActionKind::AddToDetectorSetA,
                    node,
                    displayName,
                    robotLinks,
                    true,
                    false,
                    collisionMember,
                    hasCollisionMember));
                model.actions.push_back(makeActionView(
                    QStringLiteral("Add to Detector Set B"),
                    ContextMenuActionKind::AddToDetectorSetB,
                    node,
                    displayName,
                    robotLinks,
                    true,
                    false,
                    collisionMember,
                    hasCollisionMember));
            }
        }
        return model;
    }

    bool SceneTreeIntentController::collisionSelectionSetMemberFromItem(
        const QTreeWidgetItem* item,
        simulation_project::CollisionSelectionSetMemberDesc& member,
        QString* errorMessage)
    {
        member = simulation_project::CollisionSelectionSetMemberDesc();
        if(item == nullptr) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("No tree item selected.");
            }
            return false;
        }

        return sceneExplorerNodeToCollisionSelectionMember(nodeRefFromItem(item), member, errorMessage);
    }

    bool SceneTreeIntentController::isContextMenuTarget(const SceneExplorerNodeRef& node)
    {
        if(node.id.isEmpty()) {
            return false;
        }
            return node.kind == SceneExplorerNodeKind::Trajectory || node.kind == SceneExplorerNodeKind::CoordinateFrame || node.kind == SceneExplorerNodeKind::UserFrame || node.kind == SceneExplorerNodeKind::Robot ||
            node.kind == SceneExplorerNodeKind::Object ||
            node.kind == SceneExplorerNodeKind::ObjectFrame ||
            node.kind == SceneExplorerNodeKind::PointCloud ||
            node.kind == SceneExplorerNodeKind::Link ||
            node.kind == SceneExplorerNodeKind::RobotMount ||
            node.kind == SceneExplorerNodeKind::ToolAttachment ||
            node.kind == SceneExplorerNodeKind::ToolAsset;
    }

    bool SceneTreeIntentController::isEntityActionable(const SceneExplorerNodeRef& node)
    {
        if(node.id.isEmpty()) {
            return false;
        }
        return node.kind == SceneExplorerNodeKind::Robot ||
            node.kind == SceneExplorerNodeKind::Object ||
            node.kind == SceneExplorerNodeKind::PointCloud;
    }
}
