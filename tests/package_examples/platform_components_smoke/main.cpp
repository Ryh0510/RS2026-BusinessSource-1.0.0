#include <AssetCore/AssetManager.h>
#include <AssetCore/MaterialDesc.h>
#include <AssetCore/ModelDesc.h>
#include <AssetCore/SubMeshDesc.h>
#include <CameraCore/CameraFactory.h>
#include <RenderCore/MaterialFactory.h>
#include <RobotRenderBridge/CollisionRenderBridge.h>
#include <SceneCore/SceneGraph.h>
#include <SensorCore/PointCloudProcessing.h>
#include <SensorCore/SensorTransform.h>
#include <SensorSimulation/SensorSimulationRuntime.h>
#include <SimulationProject/ProjectDocument.h>
#include <SimulationProject/ProjectValidation.h>
#include <SimulationRuntime/ProjectSimulationRuntime.h>
#include <SimulationRuntime/ProjectRuntimeTypes.h>
#include <VisualizationSDK/ProjectVisualizationSession.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool approxEqual(const Eigen::Vector3d& lhs, const Eigen::Vector3d& rhs)
    {
        return (lhs - rhs).norm() < 1.0e-9;
    }

    bool hasElement(
        const std::vector<smrobot::visualization::VisualizationElementInfo>& elements,
        const std::string& id,
        const std::string& kind,
        bool visible)
    {
        for(const auto& element : elements) {
            if(element.id == id && element.kind == kind && element.visible == visible) {
                return true;
            }
        }
        return false;
    }

    bool writeTextFile(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream stream(path, std::ios::binary);
        if(!stream) {
            return false;
        }
        stream << text;
        return static_cast<bool>(stream);
    }

    std::filesystem::path makeVisualizationSmokeAssetDir()
    {
        const std::filesystem::path assetDir =
            std::filesystem::current_path() / "visualization_sdk_smoke_assets";
        std::filesystem::create_directories(assetDir);
        return assetDir;
    }

    simulation_project::ProjectDocument makeVisualizationSmokeProject(
        const std::filesystem::path& robotUrdf,
        const std::filesystem::path& modelObj)
    {
        simulation_project::ProjectDocument document;
        document.assetSearchPaths = { "${PROJECT_DIR}" };

        simulation_project::RobotDesc robot;
        robot.id = "visualization-smoke-robot";
        robot.name = "Visualization Smoke Robot";
        robot.sourceType = "urdf";
        robot.sourcePath = robotUrdf.filename().generic_u8string();
        document.robots.push_back(robot);

        simulation_project::SceneObjectDesc object;
        object.id = "visualization-smoke-object";
        object.name = "Visualization Smoke Object";
        object.sourcePath = modelObj.filename().generic_u8string();
        object.transform.x = 0.25;
        object.transform.y = 0.5;
        object.visible = false;
        document.objects.push_back(object);

        simulation_project::RobotMountDesc mount;
        mount.id = "visualization-smoke-mount";
        mount.name = "Visualization Smoke Mount";
        mount.robotId = robot.id;
        mount.linkName = "tool0";
        document.robotMounts.push_back(mount);

        simulation_project::AttachmentFunctionalFrameDesc tcp;
        tcp.id = "visualization-smoke-tcp";
        tcp.name = "Visualization Smoke TCP";
        tcp.frameType = "tcp";
        tcp.primary = true;

        simulation_project::AttachmentAssetDesc asset;
        asset.id = "visualization-smoke-tool-asset";
        asset.name = "Visualization Smoke Tool Asset";
        asset.assetKind = "tool";
        asset.assetType = "smoke-tool";
        asset.visualPath = modelObj.filename().generic_u8string();
        asset.visible = false;
        asset.functionalFrames.push_back(tcp);
        document.attachmentAssets.push_back(asset);

        simulation_project::MountedAttachmentDesc attachment;
        attachment.id = "visualization-smoke-attachment";
        attachment.name = "Visualization Smoke Attachment";
        attachment.mountFrameId = mount.id;
        attachment.assetId = asset.id;
        document.mountedAttachments.push_back(attachment);

        return document;
    }

    int smokeAssetCore()
    {
        assetcore::ModelDesc model;
        assetcore::SubMeshDesc submesh;
        submesh.name = "assetcore-smoke-submesh";
        submesh.geometry.positions.emplace_back(0.0f, 0.0f, 0.0f);
        submesh.geometry.positions.emplace_back(1.0f, 0.0f, 0.0f);
        submesh.geometry.positions.emplace_back(0.0f, 1.0f, 0.0f);
        model.addSubMesh(submesh);

        if(model.subMeshCount() != 1 ||
           model.subMesh(0).geometry.positions.size() != 3 ||
           model.subMesh(0).geometry.hasNormals()) {
            std::cerr << "AssetCore ModelDesc smoke failed.\n";
            return 1;
        }

        std::string error;
        auto missing = assetcore::AssetManager::instance().tryLoadModel(
            "__smrobot_assetcore_missing_model__.obj",
            1.0f,
            &error);
        if(missing || error.empty()) {
            std::cerr << "AssetCore AssetManager missing-file smoke failed.\n";
            return 1;
        }

        std::cout << "AssetCore model description and loader smoke passed.\n";
        return 0;
    }

    int smokeCameraCore()
    {
        auto camera = cameracore::CameraFactory::createOrbitCamera();
        auto controller = cameracore::CameraFactory::createOrbitController(camera);
        if(!camera || !controller) {
            std::cerr << "CameraCore factory smoke failed.\n";
            return 1;
        }

        camera->setAspect(16.0f / 9.0f);
        camera->setUpAxis(cameracore::UpAxis::Z_UP);
        camera->lookAt(
            Eigen::Vector3f(1.0f, 2.0f, 3.0f),
            Eigen::Vector3f(0.0f, 0.0f, 0.0f),
            Eigen::Vector3f(0.0f, 0.0f, 1.0f));

        controller->onMouseMove(0.0f, 0.0f, 0);
        controller->onScroll(0.0f);

        const Eigen::Matrix4f view = camera->view();
        const Eigen::Matrix4f projection = camera->projection();
        if(!std::isfinite(view(0, 0)) || !std::isfinite(projection(0, 0))) {
            std::cerr << "CameraCore matrix smoke failed.\n";
            return 1;
        }

        std::cout << "CameraCore factory and interface smoke passed.\n";
        return 0;
    }

    int smokeSensorCore()
    {
        Eigen::Isometry3d sensorToWorld = Eigen::Isometry3d::Identity();
        sensorToWorld.translation() = Eigen::Vector3d(1.0, 2.0, 3.0);

        sensorcore::PointCloud cloud;
        cloud.frame.sensorId = "sensorcore-smoke";
        cloud.frame.sensorToWorld = sensorToWorld;
        cloud.points.push_back(Eigen::Vector3d(0.0, 0.0, 0.0));
        cloud.points.push_back(Eigen::Vector3d(2.0, 0.0, 0.0));

        const sensorcore::PointCloud worldCloud = sensorcore::transformPointCloudToWorld(cloud);
        if(worldCloud.points.size() != 2 ||
           !approxEqual(worldCloud.points.front(), Eigen::Vector3d(1.0, 2.0, 3.0))) {
            std::cerr << "SensorCore transform smoke failed.\n";
            return 1;
        }

        const sensorcore::PointCloudBounds bounds = sensorcore::computeBounds(worldCloud);
        if(!bounds.valid ||
           !approxEqual(bounds.min, Eigen::Vector3d(1.0, 2.0, 3.0)) ||
           !approxEqual(bounds.max, Eigen::Vector3d(3.0, 2.0, 3.0))) {
            std::cerr << "SensorCore bounds smoke failed.\n";
            return 1;
        }

        std::cout << "SensorCore transform and processing smoke passed.\n";
        return 0;
    }

    int smokeSimulationProject()
    {
        simulation_project::ProjectDocument document;
        std::string error;
        if(!simulation_project::validateProjectDocument(document, &error)) {
            std::cerr << "SimulationProject validation smoke failed: " << error << '\n';
            return 1;
        }

        std::cout << "SimulationProject document validation smoke passed.\n";
        return 0;
    }

    int smokeSimulationRuntime()
    {
        const simulation_runtime::Result ok = simulation_runtime::Result::ok();
        const simulation_runtime::Result error = simulation_runtime::Result::error("runtime-smoke");
        simulation_runtime::RuntimePointCloud cloud;
        cloud.documentId = "runtime-smoke-cloud";
        cloud.worldTransform = Eigen::Isometry3d::Identity();

        if(!ok.success || error.success || error.message != "runtime-smoke" || cloud.documentId.empty()) {
            std::cerr << "SimulationRuntime Result/runtime type smoke failed.\n";
            return 1;
        }

        std::cout << "SimulationRuntime Result/runtime type smoke passed.\n";
        return 0;
    }

    int smokeSensorSimulation()
    {
        sensorsimulation::SensorSimulationRuntime runtime;
        runtime.clear();

        if(runtime.rayScan("__missing_sensor__") != nullptr ||
           runtime.pointCloud("__missing_sensor__") != nullptr) {
            std::cerr << "SensorSimulation runtime clear/query smoke failed.\n";
            return 1;
        }

        std::cout << "SensorSimulation runtime smoke passed.\n";
        return 0;
    }

    int smokeRenderCore()
    {
        assetcore::MaterialDesc desc;
        desc.name = "rendercore-smoke-material";
        desc.type = assetcore::MaterialDesc::Type::Unlit;

        auto material = rendercore::MaterialFactory::createFromDesc(desc);
        if(!material) {
            std::cerr << "RenderCore material factory smoke failed.\n";
            return 1;
        }

        std::cout << "RenderCore material factory smoke passed.\n";
        return 0;
    }

    int smokeSceneCore()
    {
        scenecore::SceneGraph scene;
        scene.update();
        if(!scene.root() || scene.getNode("/root") == nullptr) {
            std::cerr << "SceneCore scene graph smoke failed.\n";
            return 1;
        }

        std::cout << "SceneCore scene graph smoke passed.\n";
        return 0;
    }

    int smokeRobotRenderBridge()
    {
        collision::CollisionDebugDrawData first;
        collision::CollisionDebugDrawData second;
        collision::CollisionDebugDrawDesc desc;
        desc.elementName = "robot-render-bridge-smoke";
        first.geometry.push_back(desc);

        const collision::CollisionDebugDrawData merged =
            robot_render::CollisionRenderBridge::merge({ first, second });
        if(merged.geometry.size() != 1) {
            std::cerr << "RobotRenderBridge collision merge smoke failed.\n";
            return 1;
        }

        std::cout << "RobotRenderBridge collision merge smoke passed.\n";
        return 0;
    }

    int smokeVisualizationSDK()
    {
        const std::filesystem::path assetDir = makeVisualizationSmokeAssetDir();
        const std::filesystem::path robotUrdf = assetDir / "visualization_smoke_robot.urdf";
        const std::filesystem::path modelObj = assetDir / "visualization_smoke_triangle.obj";

        if(!writeTextFile(
               robotUrdf,
               "<robot name=\"visualization_smoke_robot\">\n"
               "  <link name=\"base_link\"/>\n"
               "  <link name=\"tool0\"/>\n"
               "  <joint name=\"base_to_tool0\" type=\"fixed\">\n"
               "    <parent link=\"base_link\"/>\n"
               "    <child link=\"tool0\"/>\n"
               "    <origin xyz=\"0 0 1\" rpy=\"0 0 0\"/>\n"
               "  </joint>\n"
               "</robot>\n") ||
           !writeTextFile(
               modelObj,
               "o visualization_smoke_triangle\n"
               "v 0 0 0\n"
               "v 1 0 0\n"
               "v 0 1 0\n"
               "vt 0 0\n"
               "vt 1 0\n"
               "vt 0 1\n"
               "vn 0 0 1\n"
               "f 1/1/1 2/2/1 3/3/1\n")) {
            std::cerr << "VisualizationSDK smoke asset generation failed.\n";
            return 1;
        }

        simulation_runtime::ProjectSimulationRuntime runtime;
        const simulation_runtime::Result loadResult =
            runtime.loadProject(makeVisualizationSmokeProject(robotUrdf, modelObj), assetDir);
        if(!loadResult.success) {
            std::cerr << "VisualizationSDK runtime project load failed: " << loadResult.message << '\n';
            return 1;
        }
        if(runtime.robots().size() != 1 ||
           runtime.objects().size() != 1 ||
           runtime.mountedAttachments().size() != 1) {
            std::cerr << "VisualizationSDK runtime project smoke did not load robot/object/attachment.\n";
            return 1;
        }

        smrobot::visualization::ProjectVisualizationSession session;
        smrobot::visualization::VisualizationBuildOptions options;
        options.enableDefaultLighting = false;
        options.enableGrid = false;

        const simulation_runtime::Result buildResult = session.build(runtime, options);
        if(!buildResult.success) {
            std::cerr << "VisualizationSDK build smoke failed: " << buildResult.message << '\n';
            return 1;
        }
        if(session.gridEnabled()) {
            std::cerr << "VisualizationSDK grid option smoke failed.\n";
            return 1;
        }
        if(session.collisionDebugEnabled()) {
            std::cerr << "VisualizationSDK collision debug default smoke failed.\n";
            return 1;
        }

        const simulation_runtime::Result syncResult = session.sync(runtime);
        if(!syncResult.success) {
            std::cerr << "VisualizationSDK sync smoke failed: " << syncResult.message << '\n';
            return 1;
        }

        session.setObjectVisible("visualization-smoke-object", false);
        session.setAttachmentVisible("visualization-smoke-attachment", false);

        const smrobot::visualization::VisualizationStats stats = session.stats();
        if(stats.robotCount != 1 ||
           stats.objectCount != 1 ||
           stats.attachmentCount != 1 ||
           stats.nodeCount == 0) {
            std::cerr << "VisualizationSDK stats smoke failed.\n";
            return 1;
        }

        if(!hasElement(session.robots(), "visualization-smoke-robot", "robot", true) ||
           !hasElement(session.objects(), "visualization-smoke-object", "object", false) ||
           !hasElement(session.attachments(), "visualization-smoke-attachment", "tool", false)) {
            std::cerr << "VisualizationSDK readonly element info smoke failed.\n";
            return 1;
        }

        session.setOnlyAttachmentVisible("visualization-smoke-attachment");
        if(!hasElement(session.attachments(), "visualization-smoke-attachment", "tool", true)) {
            std::cerr << "VisualizationSDK attachment visibility smoke failed.\n";
            return 1;
        }

        smrobot::visualization::VisualizationBuildOptions unsupportedOptions;
        unsupportedOptions.enableCollisionDebug = true;
        const simulation_runtime::Result unsupportedResult =
            session.build(runtime, unsupportedOptions);
        if(unsupportedResult.success ||
           unsupportedResult.message.find("collision debug overlay is not supported") == std::string::npos) {
            std::cerr << "VisualizationSDK collision debug unsupported semantic smoke failed.\n";
            return 1;
        }

        session.clear();

        std::cout << "VisualizationSDK project visualization session smoke passed.\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    if(argc != 2) {
        std::cerr << "Usage: SMRobotPlatformComponentsSmoke <AssetCore|CameraCore|SensorCore|SimulationProject|SimulationRuntime|SensorSimulation|RenderCore|SceneCore|RobotRenderBridge|VisualizationSDK>\n";
        return 2;
    }

    const std::string component = argv[1];
    if(component == "AssetCore") {
        return smokeAssetCore();
    }
    if(component == "CameraCore") {
        return smokeCameraCore();
    }
    if(component == "SensorCore") {
        return smokeSensorCore();
    }
    if(component == "SimulationProject") {
        return smokeSimulationProject();
    }
    if(component == "SimulationRuntime") {
        return smokeSimulationRuntime();
    }
    if(component == "SensorSimulation") {
        return smokeSensorSimulation();
    }
    if(component == "RenderCore") {
        return smokeRenderCore();
    }
    if(component == "SceneCore") {
        return smokeSceneCore();
    }
    if(component == "RobotRenderBridge") {
        return smokeRobotRenderBridge();
    }
    if(component == "VisualizationSDK") {
        return smokeVisualizationSDK();
    }

    std::cerr << "Unknown component smoke: " << component << '\n';
    return 2;
}
