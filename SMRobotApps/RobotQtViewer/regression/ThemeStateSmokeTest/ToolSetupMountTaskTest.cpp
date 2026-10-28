#include "ToolSetupAppServices.h"
#include "ToolSetupModuleController.h"
#include "ToolSetupWidget.h"
#include "ToolSetupViewModelBuilder.h"
#include "SceneEntityWorkflowController.h"
#include "SceneTreeIntentController.h"
#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerEventHub.h"
#include "RobotQtViewerOperationStatus.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerViewportPreviewState.h"

#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidgetItem>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if(!condition) {
            throw std::runtime_error(message);
        }
    }

    class MountTestServices final : public robot_qt_viewer::ToolSetupAppServices
    {
    public:
        QString robotId = QStringLiteral("fanuc");
        QString linkName = QStringLiteral("flange");
        QString selectedRobotId() const override { return robotId; }
        QString selectedLinkName() const override { return linkName; }
        void setRobotMountContext(const QString& robot, const QString& link, const QString&) override
        {
            robotId = robot;
            linkName = link;
        }
        void setToolAttachmentContext(const QString& robot, const QString& link,
            const QString& mount, const QString&) override
        {
            setRobotMountContext(robot, link, mount);
        }
        robot_qt_viewer::ToolSetupViewportReloadResult reloadViewport(const QString&) override
        {
            return { true, QString() };
        }
    };
}

void verifyToolSetupMountTasks()
{
    using namespace robot_qt_viewer;
    for(const auto& type : {kSceneExplorerNodeRobot, kSceneExplorerNodeObject,
            kSceneExplorerNodePointCloud, kSceneExplorerNodeLink, kSceneExplorerNodeRobotMount,
            kSceneExplorerNodeObjectFrame, kSceneExplorerNodeToolAttachment, kSceneExplorerNodeToolAsset}) {
        QTreeWidgetItem item;
        item.setData(0, kSceneExplorerRoleType, type);
        item.setData(0, kSceneExplorerRoleId, QStringLiteral("test_item"));
        for(const auto scope : {SceneExplorerActionScope::ProjectAssembly, SceneExplorerActionScope::CollisionConfig}) {
            const auto menu = SceneTreeIntentController::contextMenuModelFromItem(&item, false, scope);
            bool showFound = false;
            bool hideFound = false;
            bool deleteFound = false;
            bool roleFound = false;
            bool tcpFound = false;
            for(const auto& entry : menu.actions) {
                showFound |= entry.action.kind == SceneTreeIntentController::ContextMenuActionKind::ShowItem;
                hideFound |= entry.action.kind == SceneTreeIntentController::ContextMenuActionKind::HideItem;
                deleteFound |= entry.action.kind == SceneTreeIntentController::ContextMenuActionKind::DeleteObject;
                roleFound |= entry.action.kind == SceneTreeIntentController::ContextMenuActionKind::SetAsWorkpiece;
                tcpFound |= entry.action.kind == SceneTreeIntentController::ContextMenuActionKind::EditTcp;
            }
            require(showFound && hideFound, "Every renderable tree item must offer Show and Hide in both workbenches");
            if(scope == SceneExplorerActionScope::ProjectAssembly &&
                (type == kSceneExplorerNodeObject || type == kSceneExplorerNodeToolAttachment || type == kSceneExplorerNodeToolAsset))
                require(roleFound, "Objects, installed devices and definitions must offer explicit workpiece role");
            if(type == kSceneExplorerNodeToolAttachment && scope == SceneExplorerActionScope::ProjectAssembly)
                require(tcpFound, "Installed tool context menu must offer TCP editing");
            if(type == kSceneExplorerNodeToolAsset && scope == SceneExplorerActionScope::ProjectAssembly) {
                require(deleteFound, "Device definition context menu must expose deletion");
            }
        }
    }
    simulation_project::ProjectDocument document;
    simulation_project::RobotDesc robot;
    robot.id = "fanuc";
    robot.name = "FANUC";
    robot.sourceType = "urdf";
    robot.sourcePath = "fanuc.urdf";
    document.robots.push_back(robot);
    simulation_project::ProjectSession session;
    session.setDocument(document, {}, false, false);
    RobotQtViewerEventHub hub;
    RobotQtViewerDocumentController documents(session, hub);
    RobotQtViewerSelectionModel selection(hub);
    RobotQtViewerViewportPreviewState preview(hub);
    RobotQtViewerOperationStatusStore status(hub);
    RobotQtViewerDocumentContext context(session, documents, selection, preview, hub, status);
    MountTestServices services;
    ToolSetupWidget widget;
    ToolSetupModuleController controller(widget, context, services);
    for(const auto kind : { RobotQtViewerEventKind::SelectionChanged,
             RobotQtViewerEventKind::ProjectDocumentChanged }) {
        hub.subscribe(kind, &controller, [&](const RobotQtViewerEvent& event) {
            controller.handleEvent(event, services.robotId, services.linkName);
        });
    }
    QString lastMessage;
    QObject::connect(&controller, &ToolSetupModuleController::statusMessageRequested,
        &widget, [&](const QString& message, int) {
            lastMessage = message;
            std::cout << "Mount task: " << message.toStdString() << '\n';
        });
    selection.selectRobotLink(services.robotId, services.linkName);
    controller.refresh(services.robotId, services.linkName);
    auto* apply = widget.findChild<QPushButton*>(QStringLiteral("toolSetupApplyTaskButton"));
    auto* cancel = widget.findChild<QPushButton*>(QStringLiteral("toolSetupCancelTaskButton"));
    require(apply != nullptr && cancel != nullptr, "Mount task buttons are missing");

    const auto beginDraft = [&]() {
        controller.createRobotMountForSelectedLink();
        require(controller.hasPendingTaskChanges() && apply->isEnabled(),
            "New mount must be an editable pending task");
        require(widget.currentMountId() == QStringLiteral("fanuc_flange_mount"),
            "New mount must select its draft id");
    };
    beginDraft();
    require(session.document().robotMounts.empty() && !session.isDirty(),
        "Draft creation must not persist a mount or dirty the project");
    cancel->click();
    require(session.document().robotMounts.empty() && !session.isDirty() &&
            !controller.hasPendingTaskChanges(),
        "Cancel must discard the new mount without changing the project");

    beginDraft();
    widget.setMountFrameNameEditor(true, true, QString());
    apply->click();
    require(session.document().robotMounts.empty() && controller.hasPendingTaskChanges(),
        "An empty mount name must be rejected without discarding the draft");
    simulation_project::TransformDesc transform;
    transform.x = 0.012;
    transform.roll = 3.141592653589793;
    transform.pitch = -1.5707963267948966;
    widget.setMountFrameNameEditor(true, true, QStringLiteral("FanucToolMount"));
    widget.setMountTransformEditor(true, true, QStringLiteral("Frame Transform"), transform);
    apply->click();
    require(session.document().robotMounts.size() == 1,
        "Create must commit the pending mount instead of requiring a persisted selection");
    const auto created = session.document().robotMounts.front();
    require(created.id == "FanucToolMount" && created.name == created.id &&
            created.robotId == "fanuc" && created.linkName == "flange" &&
            std::abs(created.linkToMount.x - transform.x) < 1.e-9 &&
            std::abs(created.linkToMount.roll - transform.roll) < 1.e-6 &&
            std::abs(created.linkToMount.pitch - transform.pitch) < 1.e-6,
        "Create must preserve the edited name, owner link, translation and rotation");
    require(session.isDirty() && !controller.hasPendingTaskChanges() &&
            selection.payload().mountId == QStringLiteral("FanucToolMount"),
        "Create must dirty the project, end the task and select the persisted mount");

    controller.focusRobotMountTask(services.robotId, services.linkName, QStringLiteral("FanucToolMount"));
    require(buildToolSetupPanelView(session.document(), services.robotId, services.linkName,
                QStringLiteral("FanucToolMount"), {}, {}, {}, {}, {}).addMountEnabled,
        "An existing mount on the selected link must not disable adding another mount");
    transform.z = 0.025;
    widget.setMountTransformEditor(true, true, QStringLiteral("Frame Transform"), transform);
    apply->click();
    require(session.document().robotMounts.size() == 1 &&
            std::abs(session.document().robotMounts.front().linkToMount.z - transform.z) < 1.e-9,
        "Editing an existing mount must update it without inserting another mount");

    beginDraft();
    widget.setMountFrameNameEditor(true, true, QStringLiteral("FanucToolMount"));
    apply->click();
    require(session.document().robotMounts.size() == 1 && controller.hasPendingTaskChanges() &&
            lastMessage.startsWith(QStringLiteral("Mount frame update failed:")),
        "A duplicate name must be rejected by the project service and keep the draft editable");
    widget.setMountFrameNameEditor(true, true, QStringLiteral("SecondMount"));
    apply->click();
    require(session.document().robotMounts.size() == 2 && !controller.hasPendingTaskChanges(),
        "A rejected draft must be creatable after choosing a unique name");
    controller.setMountFrameVisible(QStringLiteral("FanucToolMount"), true);
    controller.setMountFrameVisible(QStringLiteral("SecondMount"), true);
    require(preview.lastMutation().pinnedRobotMountFrameIds.contains(QStringLiteral("FanucToolMount")) &&
            preview.lastMutation().pinnedRobotMountFrameIds.contains(QStringLiteral("SecondMount")),
        "Two mount frames must be independently displayable at the same time");
    controller.setMountFrameVisible(QStringLiteral("SecondMount"), false);
    require(preview.lastMutation().pinnedRobotMountFrameIds.contains(QStringLiteral("FanucToolMount")) &&
            !preview.lastMutation().pinnedRobotMountFrameIds.contains(QStringLiteral("SecondMount")) &&
            preview.lastMutation().setRobotMountFrameVisibility && !preview.lastMutation().mountFrameVisible,
        "Hiding the current mount must retain the other visible mount");

    QTemporaryDir savedProject;
    require(savedProject.isValid(), "Mount save/load test needs a temporary directory");
    const auto path = std::filesystem::path(savedProject.path().toStdWString()) / "fanuc.sys.json";
    std::string error;
    require(session.saveToPath(path, true, &error), "Created mounts must save to a system project");
    simulation_project::ProjectSession reloaded;
    require(reloaded.loadFromPath(path, nullptr, &error) &&
            reloaded.document().robotMounts.size() == 2 &&
            reloaded.document().robotMounts.front().id == "FanucToolMount" &&
            reloaded.document().robotMounts.front().linkName == "flange" &&
            std::abs(reloaded.document().robotMounts.front().linkToMount.z - transform.z) < 1.e-9,
        "Created and edited mount data must survive saving and reopening the project");

    auto deletionFixture = session.document();
    simulation_project::AttachmentAssetDesc asset;
    asset.id = "A2";
    asset.name = "A2";
    asset.assetType = "workpiece";
    deletionFixture.attachmentAssets.push_back(asset);
    session.setDocument(deletionFixture, {}, false, false);
    SceneEntityWorkflowController workflow(context);
    const auto deleted = workflow.deleteEntity(SceneEntityKind::AttachmentAsset, QStringLiteral("A2"));
    require(deleted.success && session.document().attachmentAssets.empty() && session.isDirty(),
        "An unused device definition must be deletable and dirty the document");
    require(session.document().robotMounts.size() == 2,
        "Deleting a definition must preserve unrelated mount frames");
    deletionFixture = session.document();
    deletionFixture.attachmentAssets.push_back(asset);
    simulation_project::MountedAttachmentDesc installed;
    installed.id = "a2_attachment";
    installed.assetId = asset.id;
    installed.mountFrameId = "FanucToolMount";
    deletionFixture.mountedAttachments.push_back(installed);
    session.setDocument(deletionFixture, {}, false, false);
    require(!workflow.deleteEntity(SceneEntityKind::AttachmentAsset, QStringLiteral("A2")).success &&
            session.document().mountedAttachments.size() == 1,
        "Deleting an in-use definition must not orphan its installed device");
    require(workflow.deleteEntity(SceneEntityKind::MountedAttachment, QStringLiteral("a2_attachment")).success &&
            session.document().mountedAttachments.empty() &&
            session.document().attachmentAssets.empty() && session.document().robotMounts.size() == 2,
        "Deleting an installed device must remove its unused definition and retain mount frames");
}
