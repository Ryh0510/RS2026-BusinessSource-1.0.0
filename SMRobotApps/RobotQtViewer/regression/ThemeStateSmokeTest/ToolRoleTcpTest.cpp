#include "SceneEntitySemantics.h"
#include "SceneEntityWorkflowController.h"
#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerEventHub.h"
#include "RobotQtViewerOperationStatus.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerViewportPreviewState.h"
#include "TcpDefinitionDialog.h"
#include "ProjectScene.h"
#include <SimulationProject/ProjectDocumentService.h>
#include "UserCoordinateFrame.h"
#include "PlanningDataSource.h"
#include "SceneExplorerViewModelBuilder.h"
#include "ToolAssetEditorWidget.h"
#include "ToolTransformEditorWidget.h"
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <stdexcept>
#include <limits>

namespace {
void require(bool condition, const char* message) { if(!condition) throw std::runtime_error(message); }
}
void verifyToolRolesAndTcp()
{
    using namespace robot_qt_viewer;
    Eigen::Isometry3d fanuc, abb, generic;
    require(tcpPoseFromValues("fanuc", {100, 200, 300, 0, 0, 90, 0}, fanuc), "FANUC pose must parse");
    require(tcpPoseFromValues("abb", {100, 200, 300, std::sqrt(.5), 0, 0, std::sqrt(.5)}, abb), "ABB pose must parse");
    require(tcpPoseFromValues("generic", {100, 200, 300, 0, 0, 90, 0}, generic), "Generic pose must parse");
    require(fanuc.matrix().isApprox(abb.matrix(), 1e-12) && fanuc.matrix().isApprox(generic.matrix(), 1e-12),
        "FANUC WPR, ABB quaternion and generic RPY must represent the same pose");
    require((fanuc * Eigen::Vector3d::UnitX()).isApprox(Eigen::Vector3d(.1, 1.2, .3), 1e-12),
        "Controller XYZ uses millimeters and R=90 rotates X onto Y");
    require(!tcpPoseFromValues("abb", {0,0,0,0,0,0,0}, abb), "Zero ABB quaternion must be rejected");
    require(!tcpPoseFromValues("generic", {std::numeric_limits<double>::infinity(),0,0,0,0,0,0}, generic),
        "Non-finite TCP must be rejected");
    for(const auto& mode : {"fanuc", "abb", "generic"}) {
        require(tcpPoseFromValues(mode, tcpPoseToValues(mode, fanuc), generic) &&
                generic.matrix().isApprox(fanuc.matrix(), 1e-12), "TCP conversion must round-trip");
    }
    auto referenceToAsset = Eigen::Isometry3d::Identity();
    referenceToAsset.translation() = Eigen::Vector3d(.4, -.2, .1);
    referenceToAsset.linear() = Eigen::AngleAxisd(.7, Eigen::Vector3d::UnitZ()).toRotationMatrix();
    TcpDefinitionDialog dialog(tcpTransformDesc(fanuc), {{"flange", referenceToAsset}, {"ee_link", Eigen::Isometry3d::Identity()}},
        "fanuc", "flange");
    auto* mode = dialog.findChild<QComboBox*>("tcpConvention");
    auto* reference = dialog.findChild<QComboBox*>("tcpReference");
    require(mode && reference, "TCP convention and reference selectors must exist");
    mode->setCurrentIndex(mode->findData("abb"));
    reference->setCurrentText("ee_link");
    mode->setCurrentIndex(mode->findData("generic"));
    require(tcpTransformMatrix(dialog.assetToTcp()).matrix().isApprox(fanuc.matrix(), 1e-7),
        "Switching manufacturer and flange reference must preserve TCP despite nonzero mount offset");
    ToolAssetEditorWidget assetEditor;
    TcpDefinitionDialog frameDialog({}, {{"WORLD", Eigen::Isometry3d::Identity()}}, "generic", "WORLD");
    int applied = 0;
    simulation_project::TransformDesc appliedFrame;
    frameDialog.setUserFrameApplyHandler([&](const simulation_project::TransformDesc& value) {
        ++applied; appliedFrame = value; return true;
    });
    frameDialog.show();
    auto* buttons = frameDialog.findChild<QDialogButtonBox*>("definitionButtons");
    require(buttons != nullptr, "User frame must expose apply and close buttons");
    buttons->button(QDialogButtonBox::Ok)->click();
    require(applied == 1 && frameDialog.isVisible(), "Applying a user frame must keep the dialog open");
    frameDialog.findChild<QDoubleSpinBox*>("tcpValue0")->setValue(379);
    buttons->button(QDialogButtonBox::Ok)->click();
    require(applied == 2 && frameDialog.isVisible() && std::abs(appliedFrame.x-.379)<1e-10,
        "User frame must support repeated applies with millimeter conversion");
    buttons->button(QDialogButtonBox::Cancel)->click();
    require(!frameDialog.isVisible(), "Only explicit close must dismiss the user frame dialog");
    require(assetEditor.findChild<QComboBox*>("tcpConvention") != nullptr,
        "The existing asset definition panel must expose the TCP format dropdown");
    ToolTransformEditorWidget inlineTcp("TCP");
    inlineTcp.setTcpConventionEnabled(true);
    inlineTcp.setTransform(tcpTransformDesc(fanuc));
    auto* inlineMode = inlineTcp.findChild<QComboBox*>("tcpConvention");
    for(const auto* convention : {"abb", "fanuc", "generic"}) {
        inlineMode->setCurrentIndex(inlineMode->findData(convention));
        require(tcpTransformMatrix(inlineTcp.transform()).matrix().isApprox(fanuc.matrix(), 1e-7),
            "Inline TCP format switching must preserve pose and millimeter units");
    }

    simulation_project::ProjectDocument doc;
    simulation_project::RobotDesc robot; robot.id = "robot"; robot.name = "Robot"; robot.sourcePath = "robot.urdf"; robot.sourceType = "urdf";
    doc.robots.push_back(robot);
    simulation_project::RobotMountDesc mount; mount.id = "mount"; mount.name = "Mount"; mount.robotId = "robot"; mount.linkName = "ee_link";
    doc.robotMounts.push_back(mount);
    simulation_project::SceneObjectDesc gun; gun.id = "gun"; gun.name = "Gun"; gun.objectType = "workpiece"; gun.sourcePath = "gun.stl";
    gun.visible = false; gun.collisionEnabled = false;
    doc.objects.push_back(gun);
    auto part = gun; part.id = "part"; part.name = "Part"; part.visible = true; part.collisionEnabled = true; doc.objects.push_back(part);
    simulation_project::AttachmentAssetDesc asset; asset.id = "gun_asset"; asset.name = "Gun asset"; asset.assetKind = "tool";
    asset.assetType = "workpiece"; asset.visualPath = "gun.stl"; doc.attachmentAssets.push_back(asset);
    simulation_project::MountedAttachmentDesc attachment; attachment.id = "gun_attachment"; attachment.name = "Gun installed";
    attachment.assetId = asset.id; attachment.mountFrameId = mount.id; attachment.sourceObjectId = gun.id;
    doc.mountedAttachments.push_back(attachment);
    require(sceneObjectRole(doc, gun) == "tool" && sceneObjectRole(doc, part) == "workpiece",
        "Legacy source workpiece subtype must not misclassify a mounted gun as a workpiece");
    simulation_project::ProjectSession session; session.setDocument(doc, {}, false, false);
    RobotQtViewerEventHub hub;
    RobotQtViewerDocumentController documents(session, hub);
    RobotQtViewerSelectionModel selection(hub);
    RobotQtViewerViewportPreviewState preview(hub);
    RobotQtViewerOperationStatusStore status(hub);
    RobotQtViewerDocumentContext context(session, documents, selection, preview, hub, status);
    SceneEntityWorkflowController workflow(context);
    const auto roleResult = workflow.setEntityRole(SceneEntityKind::MountedAttachment, "gun_attachment", "workpiece");
    require(roleResult.success, roleResult.message.toStdString().c_str());
    require(sceneObjectRole(session.document(), session.document().objects.front()) == "workpiece", "Role change must reach source and installed view");
    require(workflow.setEntityRole(SceneEntityKind::Object, "part", "tool").success, "Standalone item role must be editable");
    require(workflow.setEntityRole(SceneEntityKind::Object, "part", "workpiece").success, "Standalone item can become a workpiece again");
    doc = session.document();
    auto other = attachment; other.id = "other_attachment"; other.sourceObjectId.clear(); doc.mountedAttachments.push_back(other);
    session.setDocument(doc, {}, false, false);
    require(workflow.setMountedTcp("gun_attachment", tcpTransformDesc(fanuc), "abb", "flange").success,
        "Installed tool TCP must be writable");
    const auto& installed = session.document().mountedAttachments.front();
    require(installed.assetId != asset.id && session.document().mountedAttachments.back().assetId == asset.id,
        "Editing instance TCP must isolate a shared definition");
    require(sceneObjectRole(session.document(), session.document().objects.front()) == "tool", "Setting TCP must mark the gun as a tool");
    const auto& stored = session.document().attachmentAssets.back().functionalFrames;
    require(stored.size() == 1 && stored.front().primary && stored.front().frameType == "tcp" &&
            tcpTransformMatrix(stored.front().assetMountToFrame).matrix().isApprox(fanuc.matrix(), 1e-12),
        "TCP must persist as canonical asset-local transform");
    QTemporaryDir directory;
    require(directory.isValid(), "TCP serialization fixture needs a directory");
    QFile mesh(directory.filePath("new_part.stl"));
    require(mesh.open(QIODevice::WriteOnly), "Import regression needs a mesh");
    mesh.write("solid part\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid part\n");
    mesh.close();
    const auto imported = workflow.importSceneObjectFromPath(
        std::filesystem::path(mesh.fileName().toStdWString()), "workpiece", SceneObjectImportUnit::Meters);
    require(imported.success, "Importing another workpiece after binding a gun must succeed");
    require(session.document().mountedAttachments.size() == 2 &&
        session.document().mountedAttachments.front().sourceObjectId == "gun" &&
        !session.document().objects.front().visible &&
        sceneObjectRole(session.document(), session.document().objects.front()) == "tool",
        "Workpiece import must preserve gun ownership and hidden source state");
    const auto path = std::filesystem::path(directory.path().toStdWString()) / "tcp.sys.json";
    std::string error;
    require(session.saveToPath(path, true, &error), "TCP and role must save");
    simulation_project::ProjectSession loaded;
    require(loaded.loadFromPath(path, nullptr, &error) && loaded.document().extensions.size() == 1 &&
        loaded.document().attachmentAssets.back().functionalFrames.front().frameType == "tcp" &&
        sceneObjectRole(loaded.document(), loaded.document().objects.front()) == "tool", "TCP and roles must survive reopening");
    const auto& reopenedDocument = loaded.document();
    const simulation_project::ProjectDocumentService reopenedService(reopenedDocument);
    const auto* reopenedTool = reopenedService.findMountedAttachment("gun_attachment");
    require(reopenedTool && reopenedService.findAttachmentAsset(reopenedTool->assetId) &&
        reopenedService.findRobotMount(reopenedTool->mountFrameId),
        "Read-only project must resolve installed tool, asset and mount after reopening for TCP editing");

    // Exercise real scene nodes: ViewportReloaded re-applies the inactive
    // coating view after import, which used to reveal every hidden source.
    QSurfaceFormat format; format.setVersion(3, 3); format.setProfile(QSurfaceFormat::CoreProfile);
    QOffscreenSurface surface; surface.setFormat(format); surface.create();
    QOpenGLContext gl; gl.setFormat(format);
    require(gl.create() && gl.makeCurrent(&surface), "Visibility regression requires OpenGL");
    QFile urdf(directory.filePath("robot.urdf"));
    require(urdf.open(QIODevice::WriteOnly), "Visibility regression needs a robot");
    urdf.write("<robot name=\"test\"><link name=\"ee_link\"/></robot>"); urdf.close();
    auto runtimeDoc = loaded.document();
    runtimeDoc.robots.front().sourcePath = urdf.fileName().toStdString();
    for(auto& object : runtimeDoc.objects) object.sourcePath = mesh.fileName().toStdString();
    for(auto& definition : runtimeDoc.attachmentAssets) definition.visualPath = mesh.fileName().toStdString();
    ProjectScene scene;
    scene.setProjectDocument(runtimeDoc, std::filesystem::path(directory.path().toStdWString()));
    require(scene.initialize(), "Visibility regression scene must initialize");
    auto expectHidden = [&] {
        bool visible = true;
        require(scene.sceneObjectVisible("gun", visible) && !visible,
            "Mounted gun source must stay hidden after coating state restoration");
    };
    expectHidden();
    scene.setCoatingAnalysisView(false); expectHidden();
    scene.setCoatingModelVisible("gun", true); expectHidden();
    scene.setCoatingModelVisibilities({{"gun", true}}); expectHidden();
    scene.clearCoatingModelVisibility("gun"); expectHidden();
    scene.setCoatingAnalysisView(true); scene.setCoatingAnalysisView(false); expectHidden();
    bool visible = false;
    require(scene.sceneObjectVisible(imported.entityId.toStdString(), visible) && visible,
        "Imported workpiece must remain visible while mounted gun source stays hidden");
    auto frameDoc=runtimeDoc;
    const auto originalPart=frameDoc.objects[1].transform;
    simulation_project::TransformDesc user;
    user.x=1; user.y=2; user.yaw=1.5707963267948966;
    const auto matrix=coordinateTransform(user);
    changeUserCoordinateFrame(frameDoc,user);
    require(coordinateTransform(frameDoc.objects[1].transform).matrix().isApprox(
        (matrix*coordinateTransform(originalPart)).matrix(),1e-10), "User frame must move workpieces by rigid composition");
    require(coordinateTransform(frameDoc.objects.front().transform).matrix().isApprox(
        coordinateTransform(runtimeDoc.objects.front().transform).matrix(),1e-10), "Mounted gun source must not move with user frame");
    changeUserCoordinateFrame(frameDoc,user);
    require(coordinateTransform(frameDoc.objects[1].transform).matrix().isApprox(
        (matrix*coordinateTransform(originalPart)).matrix(),1e-10), "Repeated absolute frame update must not accumulate transforms");
    require(scene.applyUserCoordinateFrameDocument(frameDoc), "User frame must apply without reloading the scene");
    frameDoc = scene.runtimeDocumentSnapshot();
    require(coordinateTransform(userCoordinateFrame(frameDoc)).matrix().isApprox(matrix.matrix(),1e-10),
        "Live scene snapshot must preserve the applied user frame for saving");
    require(coordinateTransform(frameDoc.objects[1].transform).matrix().isApprox(
        (matrix*coordinateTransform(originalPart)).matrix(),1e-10), "Live workpiece must follow applied frame");
    expectHidden();
    PlanningDataSnapshot pathSnapshot;
    pathSnapshot.modelObjectId="part"; pathSnapshot.modelPath="part.stl";
    spraytrajectory::SpraySegment segment; segment.points.emplace_back();
    segment.points.front().tcpPose.translation()=Eigen::Vector3d(.2,.3,.4);
    segment.points.front().jointValues={1,2,3};
    pathSnapshot.trajectory.segments.push_back(segment);
    PlanningDataSnapshot worldPath;
    require(planningDataToWorld(pathSnapshot,frameDoc,worldPath),"Local trajectory must resolve through moved workpiece");
    require(worldPath.trajectory.segments.front().points.front().tcpPose.matrix().isApprox(
        (matrix*coordinateTransform(originalPart)*segment.points.front().tcpPose).matrix(),1e-10),
        "Workpiece and trajectory must share the user frame");
    PlanningDataSource paths; paths.publish(worldPath); paths.rebaseUserFrame(matrix.inverse());
    require(paths.snapshot().trajectory.segments.front().points.front().jointValues.empty(),
        "Moving a frame must invalidate cached joint solutions");
    session.setDocument(frameDoc,{},false,false);
    const auto frameImport=workflow.importSceneObjectFromPath(std::filesystem::path(mesh.fileName().toStdWString()),"workpiece",SceneObjectImportUnit::Meters);
    require(frameImport.success && coordinateTransform(session.document().objects.back().transform).matrix().isApprox(matrix.matrix(),1e-10),
        "New workpiece must import at user origin");
    require(session.saveToPath(path,true,&error) && loaded.loadFromPath(path,nullptr,&error), "User frame must save and reopen");
    require(coordinateTransform(userCoordinateFrame(loaded.document())).matrix().isApprox(matrix.matrix(),1e-10),
        "Saved user frame must retain translation and rotation");
    auto managed=loaded.document();
    setCoordinateFrameState(managed,"user",false,false);
    setCoordinateFrameState(managed,"world",false,true);
    setCoordinateFrameState(managed,"tcp:gun_attachment",false,false);
    require(!coordinateFrameVisible(managed,"user") && !coordinateFrameVisible(managed,"world") &&
        !coordinateFrameDeleted(managed,"world"), "World frame can hide but cannot be deleted");
    const auto tree=buildSceneExplorerViewModel(managed,{},{});
    int frameChildren=0;
    for(const auto& node:tree.nodes) if(node.parentNodeId==QStringLiteral("group:coordinateFrames")) ++frameChildren;
    require(frameChildren>=3,"Coordinate manager must contain user, world and installed TCP frames");
    // Exercise ordinary Save as well as explicit Save As V3, including a later show operation.
    session.setDocument(managed,{},true,false);
    require(session.saveToPath(path,false,&error) && loaded.loadFromPath(path,nullptr,&error),
        "Ordinary save must retain hidden coordinate frames");
    for(const auto& id : {"user", "world", "tcp:gun_attachment"})
        require(!coordinateFrameVisible(loaded.document(),id) && !coordinateFrameDeleted(loaded.document(),id),
            "Hidden frames must remain hidden and defined after ordinary save/reopen");
    auto shown=loaded.document();
    setCoordinateFrameState(shown,"world",true,false);
    session.setDocument(shown,path,true,false);
    require(session.saveToPath(path,false,&error) && loaded.loadFromPath(path,nullptr,&error),
        "Updated frame visibility must overwrite saved state");
    require(coordinateFrameVisible(loaded.document(),"world") && !coordinateFrameVisible(loaded.document(),"user") &&
        !coordinateFrameVisible(loaded.document(),"tcp:gun_attachment"),
        "Showing one frame must preserve the other hidden frames");
    setCoordinateFrameState(managed,"user",false,true);
    const auto deletedTree=buildSceneExplorerViewModel(managed,{},{});
    for(const auto& node:deletedTree.nodes)
        require(node.ref.kind!=SceneExplorerNodeKind::UserFrame,"Deleted user frame must not return in the tree");
    session.setDocument(managed,{},false,false);
    require(session.saveToPath(path,true,&error) && loaded.loadFromPath(path,nullptr,&error),"Frame management state must save and reopen");
    require(coordinateFrameDeleted(loaded.document(),"user") && !coordinateFrameVisible(loaded.document(),"world") &&
        !coordinateFrameVisible(loaded.document(),"tcp:gun_attachment"),"Frame deletion and visibility must persist");
    auto library=loaded.document();
    for(const auto& id : {"input-one", "best-two"}) {
        const QJsonObject record{{"library_id",id},{"display_name",id},{"validation_status","incomplete_failed"},
            {"exports",QJsonObject{{"paq","# incomplete_failed\n0 0 0 1 0 0 0 50 0 0 0 0\n"}}}};
        library.extensions.push_back({std::string("rws.trajectory.")+id,1,
            QJsonDocument(record).toJson(QJsonDocument::Compact).toStdString()});
    }
    session.setDocument(library,path,true,false);
    require(session.saveToPath(path,false,&error) && loaded.loadFromPath(path,nullptr,&error),
        "Trajectory history including incomplete exports must survive save/reopen");
    const auto libraryTree=buildSceneExplorerViewModel(loaded.document(),{},{});
    int trajectories=0;
    for(const auto& node:libraryTree.nodes) if(node.ref.kind==SceneExplorerNodeKind::Trajectory) ++trajectories;
    require(trajectories==2,"Trajectory tree must retain both old input and new incomplete versions");
}
