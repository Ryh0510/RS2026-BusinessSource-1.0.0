#include <ProjectRuntimeTypes.h>
#include <ProjectSceneAttachmentVisualSystem.h>
#include <ProjectSceneCameraSystem.h>
#include <ProjectSceneCollisionPresentationSystem.h>
#include <ProjectSceneDocumentProjectionSystem.h>
#include <ProjectSceneEnvironmentSystem.h>
#include <ProjectSceneInteractionSystem.h>
#include <ProjectScenePreviewOverlayState.h>
#include <ProjectSceneStewartPresentationSystem.h>

#include <SimulationProject/ProjectDocument.h>
#include <RenderCore/Material.h>
#include <SceneCore/MaterialRenderState.h>
#include <SceneCore/SceneGraph.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    struct Checks
    {
        int failures = 0;

        void require(bool condition, const std::string& message)
        {
            if(condition) {
                std::cout << "[OK] " << message << "\n";
                return;
            }
            ++failures;
            std::cerr << "[FAIL] " << message << "\n";
        }
    };

    bool near(double a, double b)
    {
        return std::abs(a - b) < 1.0e-6;
    }

    void verifyNozzleCalibrationLifetime(Checks& checks)
    {
        RuntimeRobot source;
        source.sprayNozzleLinkName = "calibrated_nozzle";
        source.sprayNozzleLocalTransform.translation() = collision::Vec3(0.1, -0.2, 0.3);
        source.sprayNozzleLocalTransform.linear() = Eigen::AngleAxisd(
            0.4, collision::Vec3::UnitY()).toRotationMatrix();
        const auto expected = source.sprayNozzleLocalTransform;
        const auto check = [&](const RuntimeRobot& robot) {
            checks.require(robot.sprayNozzleLinkName == "calibrated_nozzle" &&
                robot.sprayNozzleLocalTransform.matrix().isApprox(expected.matrix()),
                "robot relocation preserves calibrated TCP link, position and orientation");
        };
        RuntimeRobot copied(source);
        check(copied);
        RuntimeRobot moved(std::move(copied));
        check(moved);
        RuntimeRobot assigned;
        assigned = source;
        check(assigned);
        RuntimeRobot moveAssigned;
        moveAssigned = std::move(assigned);
        check(moveAssigned);
        std::vector<RuntimeRobot> robots;
        robots.push_back(std::move(moved));
        robots.reserve(robots.capacity() + 8);
        check(robots.front());
    }

    void verifyCameraSystem(Checks& checks)
    {
        ProjectSceneCameraSystem::Bounds bounds;
        bounds.includePoint(collision::Vec3(-1.0, -2.0, -3.0));
        bounds.includePoint(collision::Vec3(3.0, 2.0, 1.0));
        checks.require(bounds.valid, "camera bounds become valid");
        checks.require(
            (bounds.center() - collision::Vec3(1.0, 0.0, -1.0)).norm() < 1.0e-6,
            "camera bounds preserve center");
        checks.require(
            near(bounds.radius(), std::sqrt(12.0)),
            "camera bounds preserve radius");

        ProjectSceneCameraSystem::LookAtState start;
        start.valid = true;
        start.position = Eigen::Vector3f(0.0f, 0.0f, 0.0f);
        ProjectSceneCameraSystem::LookAtState end = start;
        end.position = Eigen::Vector3f(2.0f, 4.0f, 6.0f);
        const auto middle =
            ProjectSceneCameraSystem::interpolateLookAt(start, end, 0.5);
        checks.require(
            (middle.position - Eigen::Vector3f(1.0f, 2.0f, 3.0f)).norm() < 1.0e-6f,
            "camera interpolation preserves midpoint");
    }

    void verifyInteractionSystem(Checks& checks)
    {
        ProjectSceneInteractionSystem interaction;
        interaction.selectJointFrame("robot_a", "joint_a");
        checks.require(
            interaction.selectedJointFrameName() == "joint_a",
            "joint frame selection is stored");

        interaction.selectSceneObject("object_a");
        checks.require(
            interaction.selectedJointFrameName().empty() &&
                interaction.selection().selectedSceneObjectId() == "object_a",
            "scene-object switch clears frame selection");

        checks.require(
            interaction.selectObjectFrame("object_a", "frame_a"),
            "valid object frame selection succeeds");
        interaction.selectRobotLink("robot_b", "link_b");
        checks.require(
            interaction.selectedObjectFrameId().empty() &&
                interaction.selection().selectedRobotId() == "robot_b",
            "robot-link switch clears object frame selection");

        interaction.setActivePreviewRobotMountId("mount_a");
        interaction.clearActivePreviewRobotMountId("mount_b");
        checks.require(
            interaction.activePreviewRobotMountId() == "mount_a",
            "unrelated mount clear preserves preview");
        interaction.clearActivePreviewRobotMountId("mount_a");
        checks.require(
            interaction.activePreviewRobotMountId().empty(),
            "matching mount clear resets preview");
    }

    void verifyAttachmentSystem(Checks& checks)
    {
        ProjectSceneAttachmentVisualSystem attachments;
        RuntimeToolAttachmentVisual first;
        first.documentId = "first";
        first.robotId = "robot";
        first.visible = true;
        RuntimeToolAttachmentVisual second;
        second.documentId = "second";
        second.robotId = "robot";
        second.visible = false;
        RuntimeToolAttachmentVisual third;
        third.documentId = "third";
        third.robotId = "robot";
        third.visible = true;
        attachments.visuals() = { first, second, third };
        attachments.selectInitialActive(0, 0);
        checks.require(
            attachments.cycleActive(false) && attachments.activeIndex() == 2,
            "attachment cycling skips hidden visuals");
        checks.require(
            !attachments.setActive("second"),
            "hidden attachment cannot become active");
        checks.require(
            attachments.setActive("first") && attachments.activeIndex() == 0,
            "visible attachment can become active");
        attachments.setActiveToolFrameRobot("robot");
        checks.require(
            attachments.activeToolFrameAttachment() != nullptr,
            "active tool frame resolves for owning robot");
        attachments.resetVisuals();
        checks.require(
            attachments.activeIndex() == static_cast<std::size_t>(-1),
            "attachment reset clears active index");
    }

    void verifyDocumentProjectionSystem(Checks& checks)
    {
        ProjectSceneDocumentProjectionSystem projection;

        RuntimeRobot regular;
        regular.documentId = "regular";
        regular.name = "Regular";
        regular.model.linkNames = { "base", "tool" };
        robot::RobotJoint movable;
        movable.name = "joint";
        movable.type = robot::JointType::Revolute;
        movable.dofIndex = 0;
        regular.model.joints.push_back(movable);

        RuntimeRobot follower;
        follower.documentId = "follower";
        follower.parallelFollowerEnabled = true;

        RuntimeRobot platform;
        platform.documentId = "platform";
        platform.name = "Platform";
        platform.parallelControlEnabled = true;

        projection.robots() = { regular, follower, platform };
        projection.rebuildRobotCatalog();
        checks.require(
            projection.robotCatalog().size() == 2,
            "document catalog omits Stewart follower fragments");
        checks.require(
            projection.robotCatalog().front().movableJoints.size() == 1 &&
                projection.robotCatalog().front().movableJoints.front().jointType == "revolute",
            "document catalog preserves movable joint type");
        checks.require(
            projection.robotCatalog().back().movableJoints.size() == 12,
            "document catalog exposes Stewart task-space controls");

        RuntimeSceneObject object;
        object.documentId = "object";
        object.name = "Object";
        projection.appendSceneObjectCatalog(object);
        projection.appendPointCloudCatalog(object);
        projection.removeObjectCatalogEntry("object");
        checks.require(
            projection.sceneObjectCatalog().empty() &&
                projection.pointCloudCatalog().empty(),
            "document projection removes both object catalog variants");
    }

    void verifyCollisionSystem(Checks& checks)
    {
        ProjectSceneCollisionPresentationSystem collisionSystem;
        auto& state = collisionSystem.state();
        state.visibleVariantIds["robot|link"] = "variant";
        state.visibleVariantFilters["robot|link"].variantId = "variant";
        state.visibleVariantFiltersByRuntimeLink["0|link"].variantId = "variant";
        state.queriesEnabled = true;
        state.showGeometry = true;
        state.runtimeBuilt = true;
        const std::uint64_t oldVersion = state.geometryOverlayCacheVersion;

        checks.require(
            collisionSystem.visibleVariantId("robot", "link") == "variant",
            "collision system resolves visible variant");
        collisionSystem.beginObjectPreview("object", "preview");
        checks.require(
            state.objectPreviewActive &&
                state.objectPreviewObjectId == "object" &&
                state.objectPreviewVariantId == "preview",
            "collision system begins object preview");
        collisionSystem.clearObjectPreview();
        checks.require(
            !state.objectPreviewActive &&
                state.objectPreviewObjectId.empty() &&
                state.objectPreviewVariantId.empty(),
            "collision system clears object preview");

        state.frameMetrics.overlayDetectorCount = 7;
        state.frameMetrics.overlayMs = 2.0;
        state.frameMetrics.resetOverlay();
        checks.require(
            state.frameMetrics.overlayDetectorCount == 0 &&
                near(state.frameMetrics.overlayMs, 0.0),
            "collision frame metrics reset together");

        collisionSystem.resetForProjectDocument();
        checks.require(
            state.visibleVariantIds.empty() &&
                state.visibleVariantFilters.empty() &&
                state.visibleVariantFiltersByRuntimeLink.empty() &&
                !state.queriesEnabled &&
                !state.showGeometry &&
                !state.runtimeBuilt &&
                state.geometryOverlayCacheVersion == oldVersion + 1,
            "collision document reset clears transient presentation state");
    }

    void verifyStewartSystem(Checks& checks)
    {
        RuntimeRobot first;
        first.sourceType = "Simscape Multibody";
        first.sourcePath = "models/Stewart.xml";
        RuntimeRobot second;
        second.sourceType = "simscape multibody";
        second.sourcePath = "MODELS/stewart.XML";
        RuntimeRobot other = second;
        other.sourcePath = "models/other.xml";

        checks.require(
            ProjectSceneStewartPresentationSystem::sameSource(first, second),
            "Stewart source matching is case insensitive");
        checks.require(
            !ProjectSceneStewartPresentationSystem::sameSource(first, other),
            "Stewart source matching rejects another model");

        simulation_project::RobotDesc desc;
        desc.sourceType = "urdf";
        desc.sourcePath = "robot.urdf";
        robot::RobotModel model;
        ProjectSceneStewartPresentationSystem::configureParallelControlIfNeeded(
            first,
            desc,
            model);
        checks.require(
            !first.parallelControlEnabled,
            "non-Stewart robot does not enable parallel control");
    }

    void verifyPreviewOverlay(Checks& checks)
    {
        simulation_project::ProjectDocument source;
        simulation_project::RobotMountDesc mount;
        mount.id = "mount";
        mount.robotId = "robot";
        mount.linkName = "tool";
        source.robotMounts.push_back(mount);

        simulation_project::AttachmentAssetDesc asset;
        asset.id = "asset";
        asset.name = "Original asset";
        source.attachmentAssets.push_back(asset);

        simulation_project::MountedAttachmentDesc attachment;
        attachment.id = "attachment";
        attachment.mountFrameId = mount.id;
        attachment.assetId = asset.id;
        source.mountedAttachments.push_back(attachment);

        simulation_project::SceneObjectDesc object;
        object.id = "object";
        simulation_project::ObjectFrameDesc frame;
        frame.id = "frame";
        frame.name = "Original frame";
        object.objectFrames.push_back(frame);
        source.objects.push_back(object);

        ProjectScenePreviewOverlayState overlay;
        mount.linkToMount.x = 1.25;
        overlay.setRobotMount(mount);
        simulation_project::TransformDesc attachmentTransform;
        attachmentTransform.z = 0.75;
        overlay.setAttachmentTransform(attachment.id, attachmentTransform);
        asset.name = "Preview asset";
        overlay.setAttachmentAsset(asset);
        frame.name = "Preview frame";
        frame.objectToFrame.y = -0.5;
        overlay.setObjectFrame(object.id, frame);

        const simulation_project::ProjectDocument preview = overlay.apply(source);
        checks.require(
            near(preview.robotMounts.front().linkToMount.x, 1.25) &&
                near(preview.mountedAttachments.front().mountToAssetMount.z, 0.75) &&
                preview.attachmentAssets.front().name == "Preview asset" &&
                preview.objects.front().objectFrames.front().name == "Preview frame",
            "preview overlay applies mount, attachment, asset, and object-frame deltas");
        checks.require(
            near(source.robotMounts.front().linkToMount.x, 0.0) &&
                near(source.mountedAttachments.front().mountToAssetMount.z, 0.0) &&
                source.attachmentAssets.front().name == "Original asset" &&
                source.objects.front().objectFrames.front().name == "Original frame",
            "preview overlay does not mutate the source document");

        overlay.removeRobotMount(mount.id);
        checks.require(
            overlay.apply(source).robotMounts.empty(),
            "preview overlay removes a mount without changing the source");
        overlay.setRobotMount(mount);
        checks.require(
            overlay.apply(source).robotMounts.size() == 1,
            "preview mount upsert cancels a pending removal");

        overlay.clear();
        const simulation_project::ProjectDocument restored = overlay.apply(source);
        checks.require(
            overlay.empty() &&
                restored.robotMounts.front().linkToMount.x ==
                    source.robotMounts.front().linkToMount.x &&
                restored.attachmentAssets.front().name ==
                    source.attachmentAssets.front().name,
            "clearing preview overlay restores the formal document projection");
    }

    void verifyEnvironmentSystem(Checks& checks)
    {
        ProjectSceneEnvironmentSystem environment;
        checks.require(
            environment.preset() == ProjectSceneEnvironmentPreset::Factory &&
                environment.hasGround() &&
                environment.primitiveCount() >= 20,
            "factory is the detailed default environment");

        environment.setPreset(ProjectSceneEnvironmentPreset::Workshop);
        checks.require(
            environment.hasGround() && environment.primitiveCount() >= 12,
            "workshop environment provides spatial fixtures");

        environment.setPreset(ProjectSceneEnvironmentPreset::Home);
        checks.require(
            environment.hasGround() && !environment.showsGrid() &&
                environment.primitiveCount() >= 20,
            "home environment provides domestic references without an industrial grid");

        environment.setSceneScale(0.1f);
        checks.require(
            near(environment.sceneScale(), 0.55),
            "environment scale clamps small robots to a usable room size");
        environment.setSceneScale(3.0f);
        checks.require(
            near(environment.sceneScale(), 1.5),
            "environment scale clamps large robots to a bounded room size");

        environment.setPreset(ProjectSceneEnvironmentPreset::Studio);
        checks.require(
            environment.hasGround() && environment.primitiveCount() >= 3,
            "studio environment keeps a neutral ground and backdrop");

        environment.setPreset(ProjectSceneEnvironmentPreset::None);
        checks.require(
            !environment.hasGround() && environment.primitiveCount() == 0,
            "grid-only environment has no solid presentation geometry");

        ProjectSceneEnvironmentPreset parsed = ProjectSceneEnvironmentPreset::None;
        checks.require(
            parseProjectSceneEnvironmentPreset("WoRkShOp", parsed) &&
                parsed == ProjectSceneEnvironmentPreset::Workshop &&
                !parseProjectSceneEnvironmentPreset("unknown", parsed),
            "environment preset ids are stable and validated");
    }

    void verifySceneLifecycle(Checks& checks)
    {
        scenecore::SceneGraph graph;
        auto parent = std::make_shared<scenecore::SceneNode>("custom-parent");
        auto child = std::make_shared<scenecore::SceneNode>("custom-child");
        parent->addChild(child);
        graph.addNode(parent);
        graph.registerNode(parent);
        checks.require(
            graph.getNode("/root/custom-parent") == parent &&
                graph.getNode("/root/custom-parent/custom-child") == child,
            "scene graph recursively registers custom nodes");
        checks.require(
            graph.removeNode(parent) && parent->parent() == nullptr &&
                graph.getNode("/root/custom-parent") == nullptr &&
                graph.getNode("/root/custom-parent/custom-child") == nullptr,
            "scene graph recursively unregisters removed nodes");
        checks.require(
            !graph.removeNode(graph.root()),
            "scene graph root cannot be removed");

        rendercore::Material material;
        scenecore::MaterialRenderState state;
        state.blend = scenecore::MaterialBlendMode::AlphaBlend;
        state.cull = scenecore::MaterialCullMode::None;
        state.depthWrite = false;
        state.renderOrder = 17;
        scenecore::setMaterialRenderState(&material, state);
        scenecore::MaterialRenderState observed;
        checks.require(
            scenecore::materialRenderState(&material, observed) &&
                observed.blend == scenecore::MaterialBlendMode::AlphaBlend &&
                observed.cull == scenecore::MaterialCullMode::None &&
                !observed.depthWrite && observed.renderOrder == 17,
            "material render state round-trips without changing Material layout");
        scenecore::clearMaterialRenderState(&material);
        checks.require(
            !scenecore::materialRenderState(&material, observed),
            "material render state is cleared with its custom mesh owner");
    }
}

int main()
{
    Checks checks;
    verifyNozzleCalibrationLifetime(checks);
    verifyCameraSystem(checks);
    verifyInteractionSystem(checks);
    verifyAttachmentSystem(checks);
    verifyDocumentProjectionSystem(checks);
    verifyCollisionSystem(checks);
    verifyStewartSystem(checks);
    verifyPreviewOverlay(checks);
    verifyEnvironmentSystem(checks);
    verifySceneLifecycle(checks);

    if(checks.failures != 0) {
        std::cerr << "ProjectScene systems regression failed with "
                  << checks.failures << " failure(s).\n";
        return 1;
    }
    std::cout << "ProjectScene systems regression passed.\n";
    return 0;
}
